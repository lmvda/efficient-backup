#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "comando.h"

Operacao operacao;
char ficheiro[NAME_MAX];
pid_t pidFilhoRestore;

void trata_SIGKILL(int signal);
void trata_SIGUSR1(int signal);
void trata_SIGUSR2(int signal);
int main (int argc, char* argv[]);
Operacao getOperacao(char *operacao);
void executaOperacao(int fifoComandos);
void enviaDadosBackup(char* pipeDados, char *nomeFicheiroAbsoluto);
void recebeDadosRestore(char* pipeDados, char *nomeFicheiroAbsoluto);
char* expandeNomeFicheiro(char * argumento);


void trata_SIGKILL(int signal) {
    execlp("rm", "rm", "-f", ficheiro, NULL);
    perror("rm restore");
    _exit(1);
}

void trata_SIGUSR1(int signal) { //TODO não podemos fazer print's (só write e read)
    if (operacao == BACKUP) {
        int pid = getpid();
        char pipeDados[100];
        sprintf(pipeDados, "%d", pid);
        unlink(pipeDados);

        printf("%s: copiado\n", ficheiro);
    }
    else if (operacao == RESTORE) {
        int pid = getpid();
        char pipeDados[100];
        sprintf(pipeDados, "%d", pid);
        unlink(pipeDados);

        printf("%s: restaurado\n", ficheiro);
    }
    else if (operacao == DELETE) {
        printf("%s: apagado\n", ficheiro);
    }
    else if (operacao == GC) {
        printf("gc: efectuado\n");
    }
} 

void trata_SIGUSR2(int signal) { //TODO não podemos fazer print's nem sprintf's (só write e read)
    if (operacao == BACKUP) {
        int pid = getpid();
        char pipeDados[100];

        sprintf(pipeDados, "%d", pid);
        unlink(pipeDados);

        printf("%s: não copiado\n", ficheiro);
    }
    else if (operacao == RESTORE) {
        int pid = getpid();
        char pipeDados[100];
        sprintf(pipeDados, "%d", pid);
        unlink(pipeDados);
        kill(pidFilhoRestore, SIGKILL);
        printf("%s: não restaurado\n", ficheiro);
    }
    else if (operacao == DELETE) {
        printf("%s: não apagado\n", ficheiro);
    }
    else if (operacao == GC) {
        printf("gc: não efectuado\n");
    }
}

Operacao getOperacao(char *operacao) {
    if (strcmp(operacao, "backup") == 0) {
        return BACKUP;
    }
    else if (strcmp(operacao, "restore") == 0) {
        return RESTORE;
    }
    else if (strcmp(operacao, "delete") == 0){
        return DELETE;
    }
    else if (strcmp(operacao, "gc") == 0){
        return GC;
    }
    else if (strcmp(operacao, "quit") == 0){
        return QUIT;
    }
    else {
        return ERRO;
    }
}



int main (int argc, char* argv[]) {

    int fifoComandos; //pipe de comandos! nome? pipeComandos?
    pid_t fpid;
    int op;
    int fd;
    char caminhoFifoComandos[PATH_MAX];


    // redirecionar os erros para o .Backup/erros.txt
    if((fd = open("erros.txt", O_CREAT | O_WRONLY, 0644)) == -1) {
        perror("erro a criar o ficheiro erros.txt");
        _exit(1);
    }
    dup2(fd, 2);
    close(fd);

    operacao = getOperacao(argv[1]); //devolve -1 se operação inválida

    if(operacao == ERRO) { //verficar se é uma operação válida (backup, restore, delete, gc, quit) 
        printf("Operação inválida!\n");
        _exit(1);
    }

    sprintf(caminhoFifoComandos, "%s/.Backup/fifoComandos", getenv("HOME"));
    fifoComandos = open(caminhoFifoComandos, O_WRONLY); //pipe que só recebe comandos

    if(fifoComandos == -1){
        perror("Cliente: erro no fifo do .Backup");
        _exit(1);
    }
   
    if(operacao == GC || operacao == QUIT){ // Se a operação é GC ou QUIT, incrementar o argc para generalizar o ciclo for
        argc++;
    }

    // criar um filho por cada operação
    for(op = 2; op < argc; op++){
        fpid = fork();
        if(fpid == 0){ // filho
            if (operacao != GC && operacao != QUIT) { //se não GC ou EXIT, tem que se guardar o nome do ficheiro
                strcpy(ficheiro, argv[op]);
            }

            executaOperacao(fifoComandos);
            _exit(0);
        }
    }

    // esperar pelos filhos (operações)
    for(op = 2; op < argc; op++){
        wait(NULL);
    }

    return 0;
}


void executaOperacao(int fifoComandos) {
    char pipeDados[100];
    int fPipe, res;
    Comando cmd;
    char * nomeFicheiroAbsoluto;

    signal (SIGUSR1, trata_SIGUSR1);
    signal (SIGUSR2, trata_SIGUSR2);

    cmd.cliente = getpid();
    cmd.op = operacao;

    // preencher o resto da struct Comando
    if (operacao == BACKUP || operacao == RESTORE || operacao == DELETE) {
        nomeFicheiroAbsoluto = expandeNomeFicheiro(ficheiro);
        strcpy(cmd.ficheiroCaminhoAbsoluto, nomeFicheiroAbsoluto);
        free(nomeFicheiroAbsoluto);

        if (getcwd(cmd.caminhoAbsoluto, sizeof(cmd.caminhoAbsoluto)) == NULL) { //get current working directory
            perror("getcwd() error");
            _exit(1);
        }
    }

    // criar o pipe com o nome do pipe para enviar/receber o ficheiro
    if (operacao == BACKUP || operacao == RESTORE) {
        sprintf(pipeDados, "%d", getpid());
        fPipe = mkfifo(pipeDados, 0644);
        if(fPipe == -1) {
            perror("erro criar FIFO dados");
            _exit(1);
        }
    }

    // enviar o comando para o servidor pelo pipe dos comandos
    res = write(fifoComandos, &cmd, sizeof(Comando));
    if(res == -1) {
        perror("write executaOperacao");
        _exit(1);
    }
    close(fifoComandos);

    if (operacao == BACKUP){
        enviaDadosBackup(pipeDados, cmd.ficheiroCaminhoAbsoluto);
    }
    else if (operacao == RESTORE){
        pidFilhoRestore = fork();
        if(pidFilhoRestore == 0) { // filho vai receber o ficheiro caso exista, senão o pai mata-o com SIGKILL
            signal(SIGKILL, trata_SIGKILL);
            recebeDadosRestore(pipeDados, cmd.ficheiroCaminhoAbsoluto);
            _exit(1);
        }
    }
    else if (operacao == QUIT) {
        _exit(0);
    }

    pause(); //bloqueia até receber um sinal
}


char* expandeNomeFicheiro(char * argumento) {
    char cwd[PATH_MAX];
    char * novoNome;
   
    if(argumento[0] != '/') {
        if (getcwd(cwd, sizeof(cwd)) == NULL) { //get current working directory
            perror("getcwd() error");
            _exit(1);
        }
        novoNome = (char*) malloc(sizeof(char) * PATH_MAX + NAME_MAX);
        sprintf(novoNome, "%s/%s", cwd, argumento);
    }
    else {
        novoNome = argumento;
    }
    return novoNome;
}

//se a operação for backup temos que enviar os dados/conteudo do ficheiro
void enviaDadosBackup(char* pipeDados, char *nomeFicheiroAbsoluto) {
    char buffer[4096];
    int fPipe, fDados;
    int n, res;

    fPipe = open(pipeDados, O_WRONLY);
    if (fPipe == -1 && errno == ENOENT) {
        mkfifo(pipeDados, 0644);
        fPipe = open(pipeDados, O_WRONLY);
    }

    fDados = open(nomeFicheiroAbsoluto, O_RDONLY);
    if (fDados == -1) {
        perror("Backup -> Problema a abrir o ficheiro");
        exit(1);
    }

    while( (n = read(fDados, buffer, 4096)) > 0 ) {
        res = write(fPipe, buffer, n);
        if(res == -1) {
            perror("write enviaDadosBackup");
            _exit(1);
        }
    }

    close(fPipe);
    close(fDados);
}




void recebeDadosRestore(char* pipeDados, char *nomeFicheiroAbsoluto) {
    char buffer[4096];
    int fPipe, fDados;
    int n, res;

    fPipe = open(pipeDados, O_RDONLY);
    if (fPipe == -1 && errno == ENOENT) {
        mkfifo(pipeDados, 0644);
        fPipe = open(pipeDados, O_RDONLY);
    }

    fDados = open(nomeFicheiroAbsoluto, O_CREAT | O_WRONLY, 0644);
    if (fDados == -1) {
        perror("Restore -> Problema a abrir o ficheiro");
        exit(1);
    }

    while( (n = read(fPipe, buffer, 4096)) > 0 ) {
        res = write(fDados, buffer, n);
        if(res == -1) {
            perror("write recebeDadosRestore");
            _exit(1);
        }
    }

    close(fPipe);
    close(fDados);
}




