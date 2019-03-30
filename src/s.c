#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h> 
#include "comando.h"

#define MAX_FILE_DATA 1000

void gereComandos(int fd);
void executaComando(Comando comando);
void substituiBarrasPorDoisPontos(char* nome);
char* calculaDigest(pid_t pidCliente, char* ficheiroOrigem);
int criaAtalhoNaPastaMetadata(pid_t pidCliente, char* ficheiroNome, char*ficheiroDigest);
int comprimeFicheiro(pid_t pidCliente, char * ficheiroCaminhoAbsoluto, char * ficheiroDigest);
int eliminaFicheiro(pid_t pidCliente, char* ficheiroDigest);
void executaBackup(int pidCliente, char* ficheiroCaminhoAbsoluto, char* caminhoAbsoluto);
void copiaFicheiroDoPipe(int pidCliente, char * ficheiro, char * caminho);
void descomprimirEEnvia(pid_t pidCliente, char* ficheiroDigest, char* caminhoAbsoluto);
void executaRestore(int pidCliente, char* ficheiroCaminhoAbsoluto, char* caminhoAbsoluto);
void executaDelete(int pidCliente, char* ficheiroCaminhoAbsoluto);
void executaGc(int pidCliente);


// Read, Write, Execute (rwx)
int main () {
    int retMkf, fifo, res;
    Comando comando;
    pid_t pid;
    int fd;
    char caminhoHome[PATH_MAX];

    sprintf(caminhoHome, "%s/.Backup", getenv("HOME"));
    // cd ~/.Backup
    res = chdir(caminhoHome);
    if(res == -1) {
        perror("chdir");
        _exit(1);
    }

    // redirecionar os erros para o .Backup/erros.txt
    if((fd = open("erros.txt", O_CREAT | O_WRONLY, 0644)) == -1) {
        perror("erro a criar o ficheiro erros.txt");
        _exit(1);
    }
    dup2(fd, 2);
    close(fd);

    int pipeAnonimo [2];
    if(pipe(pipeAnonimo) == -1) {
        perror("erro a criar o pipe");
        _exit(1);
    }

    pid = fork();
    if(pid == 0) {
        close(pipeAnonimo[1]);
        gereComandos(pipeAnonimo[0]);
        _exit(1);
    }
    close(pipeAnonimo[0]);

    comando.op = GC; // só para entrar o ciclo
    while(comando.op != QUIT && (retMkf = mkfifo("fifoComandos", 0666)) != -1) { // criar o pipe com nome fifo
        fifo = open("fifoComandos",O_RDONLY); // abrir o fifo para leitura
        if(fifo == -1) { // tratamento de erro do fifo
            perror("open");
            _exit(1);
        }

        while(read(fifo, &comando, sizeof(Comando)) > 0) { // leitura um comando do pipe de comandos
            if(comando.op != QUIT) {
                res = write(pipeAnonimo[1], &comando, sizeof(Comando));
                if(res == -1) {
                    perror("write pipe anonimo");
                    _exit(1);
                }
            }
        }
        close(fifo);
        unlink("fifoComandos"); // elimina fifo
    }

    if ((retMkf == -1) && (errno != EEXIST)) { // tratamento de erro, verifica se já existe ou não o pipe fifo
        perror("mkfifo");
        _exit(1);
    }

    return 0;
}

void gereComandos(int fd) {
    int nComandos = 0;
    pid_t pid;
    Comando comando;
    while(read(fd, &comando, sizeof(Comando)) > 0) {
        while(nComandos >= 5) { // esperar que não haja mais que 5 operações simultâneas
            wait(NULL);
            nComandos--;
        }
        pid = fork();
        if(pid == 0) {
            executaComando(comando);
           _exit(1);
        }
        nComandos++;
    }
}

void executaComando(Comando comando) {
    int res;
    if(comando.op == BACKUP) {
        substituiBarrasPorDoisPontos(comando.ficheiroCaminhoAbsoluto);
        executaBackup(comando.cliente, comando.ficheiroCaminhoAbsoluto, comando.caminhoAbsoluto);
    }
    else if(comando.op == RESTORE) {
        substituiBarrasPorDoisPontos(comando.ficheiroCaminhoAbsoluto);
        executaRestore(comando.cliente, comando.ficheiroCaminhoAbsoluto, comando.caminhoAbsoluto);
    }
    else if(comando.op == DELETE) {
        substituiBarrasPorDoisPontos(comando.ficheiroCaminhoAbsoluto);
        executaDelete(comando.cliente, comando.ficheiroCaminhoAbsoluto);
    }
    else if(comando.op == GC) {
        executaGc(comando.cliente);
    }
    else {
        char msg[] = "Operação nao reconhecida %s!\n";
        res = write(2, "Operação nao reconhecida %s!\n", strlen(msg));
        if(res == -1) {
            perror("write executaComando");
            _exit(1);
        }
    }
}


void substituiBarrasPorDoisPontos(char* nome) {
    int i;
    for(i=0; nome[i] != '\0'; i++) {
        if(nome[i] == '/') {
            nome[i] = ':';
        }
    }
}




//*********************************************************************************
//*********************************************************************************
//*********************************************************************************
//
//
//                                      BACKUP
//
//
//*********************************************************************************
//*********************************************************************************
//*********************************************************************************





void executaBackup(int pidCliente, char* ficheiroCaminhoAbsoluto, char* caminhoAbsoluto) {
    // ler o ficheiro do fifo
    copiaFicheiroDoPipe(pidCliente, ficheiroCaminhoAbsoluto, caminhoAbsoluto);

    // calcular o digest
    char* digest  = calculaDigest(pidCliente, ficheiroCaminhoAbsoluto);
    char ficheiroDigest [NAME_MAX];
    ficheiroDigest[0] = '\0'; // marcar como uma string vazia
    strcat(ficheiroDigest, "data/");
    strcat(ficheiroDigest, digest);
    free(digest);

    if(access(ficheiroDigest, F_OK ) == -1 ) { // o ficheiroDigest não existe no data
        comprimeFicheiro(pidCliente, ficheiroCaminhoAbsoluto, ficheiroDigest); // "/user/ana/a.txt -> .Backup/data/abdi34324iusdf"
    }
    eliminaFicheiro(pidCliente, ficheiroCaminhoAbsoluto);
    criaAtalhoNaPastaMetadata(pidCliente, ficheiroCaminhoAbsoluto, ficheiroDigest);// execl("ln", "ln", "-s", digest.gz, atalho)

    kill(pidCliente, SIGUSR1);
    _exit(0); // OK!
}





void copiaFicheiroDoPipe(int pidCliente, char * ficheiro, char * caminho) {
    int fdLer, fdEscrever, res;
    ssize_t lido;
    char buffer[4096];
    char pipeDados[PATH_MAX + NAME_MAX];

    // abrir o novo ficheiro
    if((fdEscrever = open(ficheiro, O_CREAT | O_WRONLY, 0644)) == -1) {
        kill(pidCliente, SIGUSR2);
        perror("erro a criar o ficheiro do backup");
        _exit(1);
    }

    // abrir o pipe de dados do cliente
    sprintf(pipeDados, "%s/%d", caminho, pidCliente);
    if((fdLer = open(pipeDados, O_RDONLY)) == -1) {
        kill(pidCliente, SIGUSR2);
        perror("erro a ler pipe de dados");
        _exit(1);
    }

    while((lido = read(fdLer, buffer, 4096)) > 0) {
        res = write(fdEscrever, buffer, lido);
        if(res == -1) {
            perror("write copiaFicheiroDoPipe");
            _exit(1);
        }
    }

    close(fdEscrever);
    close(fdLer);
}


int comprimeFicheiro(pid_t pidCliente, char * ficheiroCaminhoAbsoluto, char * ficheiroDigest) { 
    pid_t pid;
    int fdLer, fdEscrever;

    pid = fork();

    if(pid == 0) { // FILHO
        // para ler
        if((fdLer = open(ficheiroCaminhoAbsoluto, O_RDONLY)) == -1) {
            kill(pidCliente, SIGUSR2);
            perror("erro a criar o ficheiro do backup");
            _exit(1);
        }

        // para escrever
        if((fdEscrever = open(ficheiroDigest, O_CREAT | O_WRONLY, 0644)) == -1) {
            kill(pidCliente, SIGUSR2);
            perror("erro a criar o ficheiro do backup");
            _exit(1);
        }

        dup2(fdLer, 0);
        dup2(fdEscrever, 1);
        close(fdLer);
        close(fdEscrever);

        execlp("gzip", "gzip", NULL);
        kill(pidCliente, SIGUSR2);
        perror("gzip");
        _exit(1);
    }
    else if (pid == -1) {
        kill(pidCliente, SIGUSR2);
        return 1;
    }
    else { // PAI
        wait(NULL);
        return 0;
    }
}


int criaAtalhoNaPastaMetadata(pid_t pidCliente, char* ficheiroNome, char* ficheiroDigest) {
    char atalho[PATH_MAX + NAME_MAX];
    atalho[0] = '\0';
    strcat(atalho, "metadata/");
    strcat(atalho, ficheiroNome);

    char originalData[PATH_MAX + NAME_MAX];
    originalData[0] = '\0';
    strcat(originalData, getenv("HOME"));
    strcat(originalData, "/.Backup/");
    strcat(originalData, ficheiroDigest);

    pid_t pid;

    pid = fork();
    if(pid == 0) {
        //Criar a flag -s para criar uma cópia simbólica do ficheiro na pasta data
        execlp("ln", "ln", "-s", originalData, atalho, NULL);
        kill(pidCliente, SIGUSR2);
        perror("ln");
        _exit(1);
    } 
    else if(pid == -1) {
        kill(pidCliente, SIGUSR2);
        return 1;
    } 
    else { // PAI
        wait(NULL);
        return 0;
    }
}

char* calculaDigest(pid_t pidCliente, char* ficheiro) {
    pid_t pid;
    int fd[2];
    int res = pipe(fd);
    if(res == -1) {
        perror("pipe digest");
        _exit(1);
    }

    pid = fork();
    char* digest = (char*) malloc(1024*sizeof(char));
    if(pid == 0) { // FILHO
        close(fd[0]);
        dup2(fd[1], 1);
        close(fd[1]);
        execlp("sha1sum", "sha1sum", ficheiro, NULL);
        kill(pidCliente, SIGUSR2);
        perror("erro sha1sum");
        _exit(1);
    } 
    else { // PAI
        close(fd[1]);
        res = read(fd[0], digest, 1024);
        if(res == -1) {
            perror("read sha1sum");
            _exit(1);
        }
        digest = strtok(digest, " ");
    }
    return digest;
}



int eliminaFicheiro(pid_t pidCliente, char* ficheiro) {
    pid_t pid;

    pid = fork();
    if(pid == 0) {
        // remover o ficheiro com a flag -f para não pedir confirmação
        execlp("rm", "rm", "-f", ficheiro, NULL);
        kill(pidCliente, SIGUSR2);
        perror("rm");
        _exit(1);
    }
    else if(pid == -1) {
        perror("fork do eliminaFicheiro");
        kill(pidCliente, SIGUSR2);
        return 1;
    }
    else { // PAI
        wait(NULL);
        return 0;
    }
 }












//*********************************************************************************
//*********************************************************************************
//*********************************************************************************
//
//
//                                      RESTORE
//
//
//*********************************************************************************
//*********************************************************************************
//*********************************************************************************



void executaRestore(int pidCliente, char* ficheiroCaminhoAbsoluto, char* caminhoAbsoluto) {
    char ficheiroMetadata [PATH_MAX + NAME_MAX];
    ficheiroMetadata[0] = '\0'; // marcar como uma string vazia
    strcat(ficheiroMetadata, "metadata/");
    strcat(ficheiroMetadata, ficheiroCaminhoAbsoluto);

    
    if(access(ficheiroMetadata, F_OK ) != -1 ) { // se o ficheiro existe

        char ficheiroDigest[PATH_MAX + NAME_MAX];
        // função readlink devolve o ficheiro para onde um link simbólico (atalho) aponta
        // neste caso devolve o ficheiro com o nome do digest na pasta data
        ssize_t len = readlink(ficheiroMetadata, ficheiroDigest, sizeof(ficheiroDigest)-1);
        ficheiroDigest[len] = '\0';

        descomprimirEEnvia(pidCliente, ficheiroDigest, caminhoAbsoluto);
        kill(pidCliente, SIGUSR1);
    }
    else { // não existe
        kill(pidCliente, SIGUSR2);
        _exit(1);
    }
}



void descomprimirEEnvia(pid_t pidCliente, char* ficheiroDigest, char* caminhoAbsoluto) {
    int fdLer, fdEscrever;
    char pipeDados[PATH_MAX + NAME_MAX];
    pid_t pid;

    pid = fork();
    if(pid == 0) {
        // abrir o ficheiro backup
        if((fdLer = open(ficheiroDigest, O_RDONLY)) == -1) {
            kill(pidCliente, SIGUSR2);
            perror("erro a ler o ficheiro do backup");
            _exit(1);
        }

        // abrir o pipe de dados do cliente
        sprintf(pipeDados, "%s/%d", caminhoAbsoluto, pidCliente);
        if((fdEscrever = open(pipeDados, O_WRONLY)) == -1) {
            kill(pidCliente, SIGUSR2);
            perror("erro a abrir pipe de dados");
            _exit(1);
        }

        dup2(fdLer, 0);
        dup2(fdEscrever, 1);
        close(fdLer);
        close(fdEscrever);

        execlp("gunzip", "gunzip", NULL);
        kill(pidCliente, SIGUSR2);
        perror("gunzip");
        _exit(1);
    } else if(pid == -1) {
        perror("fork");
        kill(pidCliente, SIGUSR2);
        _exit(1);
    } else {
        wait(NULL);
    }
} 



//*********************************************************************************
//*********************************************************************************
//*********************************************************************************
//
//
//                                      DELETE
//
//
//*********************************************************************************
//*********************************************************************************
//*********************************************************************************




void executaDelete(int pidCliente, char* ficheiroCaminhoAbsoluto) {
    char ficheiroMetadata [PATH_MAX + NAME_MAX];
    ficheiroMetadata[0] = '\0'; // marcar como uma string vazia
    strcat(ficheiroMetadata, "metadata/");
    strcat(ficheiroMetadata, ficheiroCaminhoAbsoluto);

    if(access(ficheiroMetadata, F_OK ) != -1 ) { // se o ficheiro existe, elimina
        eliminaFicheiro(pidCliente, ficheiroMetadata);
    }
    kill(pidCliente, SIGUSR1);
}




//*********************************************************************************
//*********************************************************************************
//*********************************************************************************
//
//
//                                      GC
//
//
//*********************************************************************************
//*********************************************************************************
//*********************************************************************************




void executaGc(int pidCliente) {
    pid_t pid;
    int fd[2];
    int i, lido, res;
    char buffer[NAME_MAX];
    int nFicheirosDados = 0;
    char ficheirosDados[MAX_FILE_DATA][NAME_MAX];

    DIR* d;
    struct dirent *dir;

    // recolher os nomes dos ficheiros na pasta data
    // http://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    d = opendir("data");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                sprintf(buffer, "%s/.Backup/data/%s", getenv("HOME"), dir->d_name);
                strcpy(ficheirosDados[nFicheirosDados], buffer);
                nFicheirosDados++;
            }
        }
        closedir(d);
    }

    // verificar qual dos ficheiros na pasta data não tem atalho associado
    for(i=0; i < nFicheirosDados; i++) {
        res = pipe(fd);
        if(res == -1) {
            perror("pipe GC");
            _exit(1);
        }
        pid = fork();
        if(pid == 0) { // Filho
            close(fd[0]);
            dup2(fd[1], 1);
            // http://askubuntu.com/questions/429247/how-to-find-and-list-all-the-symbolic-links-created-for-a-particular-file/429248#429248
            execlp("find", "find", "metadata", "-lname", ficheirosDados[i], NULL);
            perror("SERVER GC find");
            kill(pidCliente, SIGUSR2);
            _exit(1);
        } 
        else { // PAI
            close(fd[1]);
            lido = read(fd[0], buffer, sizeof(buffer));
            if(lido == 0) { // se o find não encontrou links simbólicos, eliminar ficheiro
                eliminaFicheiro(pidCliente, ficheirosDados[i]);
            }
        }
    }
    kill(pidCliente, SIGUSR1);
}
