// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.2 -- Julho de 2017

// interface do gerente de disco rígido (block device driver)

#ifndef __DISK_MGR__
#define __DISK_MGR__

// estruturas de dados e rotinas de inicializacao e acesso
// a um dispositivo de entrada/saida orientado a blocos,
// tipicamente um disco rigido.

// estrutura que representa um disco no sistema operacional
typedef struct
{
  semaphore_t *disk_semaphore;
  task_t **disk_suspended_tasks;
  int disk_suspended_tasks_count;
  int blocks_size;
  int num_blocks;
  // completar com os campos necessarios
} disk_t ;

enum disk_request_type {read = 1, write = 0}; 

typedef struct
{
    struct disk_request_t *prev, *next ;		// ponteiros para usar em filas
    task_t *task;
    enum disk_request_type type ;
    int block;
    void *buffer;
    // completar com os campos necessarios
} disk_request_t ;

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize) ;

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer) ;

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer) ;

// corpo do gestor do disco
void disk_mgr_body (void * args) ;

#endif
