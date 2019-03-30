#ifndef _COMANDOS
#define _COMANDOS

#include <limits.h>

typedef enum {
    BACKUP,
    RESTORE,
    DELETE,
    GC,
    ERRO,
    QUIT
} Operacao;


typedef struct sComando {
    pid_t cliente;
    Operacao op;
    char ficheiroCaminhoAbsoluto [PATH_MAX + NAME_MAX];
    char caminhoAbsoluto [PATH_MAX];
} Comando;

#endif 