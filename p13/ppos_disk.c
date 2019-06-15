#include "hard_disk.h"
#include "ppos.h"
#include "ppos_disk.h"

#define TASK_SUSPENDED 0 // must be the same as defined in ppos_core.c


disk_request_t **disk_requests;
int disk_requests_count;

extern task_t *disk_task;
extern task_t *current_task;
extern disk_t *disk;

extern int can_preempt;

int create_disk_request(disk_request_t *request, enum disk_request_type type, int block, void *buffer) {
    request->task = current_task;
    request->type = type;
    request->block = block;

    if (type == write) { // se o tipo for igual a write, copia para o buffer
        request->buffer = malloc(disk->blocks_size);
        bcopy(buffer, request->buffer, disk->blocks_size);
    }  else {
        request->buffer = buffer;
    }

    return 0;

}

int destroy_disk_request(disk_request_t *request) {
    if (request->type == write)  // se o tipo for igual a write, copia para o buffer
        free(request->buffer); 

    return 0;
}

int disk_mgr_init (int *numBlocks, int *blockSize) {
    #ifdef DEBUG
        printf("[Disk Manager Init] Inicializando disco...!\n");
    #endif

    int error = 0;

    if (disk_cmd(DISK_CMD_INIT, 0, 0)) {
        #ifdef DEBUG
            perror("[ERRO] Inicialização do disco deu errado!\n");
        #endif
        error = 1;
    }

    int result = disk_cmd (DISK_CMD_DISKSIZE, 0, 0);
    if (result < 0) {
        #ifdef DEBUG
            perror("[ERRO] Erro pegando o tamanho do disco!\n");
        #endif
        error = 1;
    }

    int disk_size = result; 

    int result = disk_cmd (DISK_CMD_DISKSIZE, 0, 0);
    if (result < 0) {
        #ifdef DEBUG
            perror("[ERRO] Erro pegando o tamanho do bloco!\n");
        #endif
        error = 1;
    }

    *blockSize = result;
    *numBlocks = disk_size / result;

    disk->blocks_size = blockSize;
    disk->num_blocks = numBlocks;

    #ifdef DEBUG
        printf("[Disk Manager Init] Criando fila de pedidos\n", numBlocks, blockSize);
    #endif

    disk_requests = (disk_request_t **)malloc(sizeof(disk_request_t *));
    (*disk_requests) = NULL;
    disk_requests = 0;

    #ifdef DEBUG
        printf("[Disk Manager Init] Disco inicializado com %d blocos, sendo que cada bloco tem %d bytes\n", numBlocks, blockSize);
    #endif

    return error;
}

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer) {
    // obtém o semáforo de acesso ao disco
    sem_down(disk->disk_semaphore);
 
    // inclui o pedido na fila_disco
    disk_request_t *request = malloc(sizeof(disk_request_t));
    create_disk_request(request, read, block, buffer);

    disk_requests_count += 1;
    queue_append((queue_t**) disk_requests, (queue_t*) request);

    if (disk_task->status == TASK_SUSPENDED)
    {
        // acorda o gerente de disco (põe ele na fila de prontas)
    }

    // libera semáforo de acesso ao disco
    sem_up(disk->disk_semaphore);

    // suspende a tarefa corrente (retorna ao dispatcher)
    can_preempt = 0;
    suspend_disk_request_task();
    can_preempt = 1;

    task_yield();
}

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer) {

}


void disk_mgr_body (void * args)
{
   while (1) 
   {
      // obtém o semáforo de acesso ao disco
      sem_down(disk->disk_semaphore);
 
      // se foi acordado devido a um sinal do disco
      if ( 0 /* disco gerou um sinal */)
      {
         // acorda a tarefa cujo pedido foi atendido
      }
 
      // se o disco estiver livre e houver pedidos de E/S na fila
      if ( 0/*  disco_livre && (fila_disco != NULL) */)
      {
         // escolhe na fila o pedido a ser atendido, usando FCFS
         // solicita ao disco a operação de E/S, usando disk_cmd()
      }
 
      // libera o semáforo de acesso ao disco
      sem_up(disk->disk_semaphore);
 
      // suspende a tarefa corrente (retorna ao dispatcher)

      // suspender_tarefa
      suspend_disk_task();

      task_yield();
   }
}