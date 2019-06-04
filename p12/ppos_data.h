// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.1 -- Julho de 2016

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto
#include "queue.h"		// biblioteca de filas genéricas

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
   struct task_t *prev, *next ;		// ponteiros para usar em filas
   int id ;				// identificador da tarefa
   ucontext_t context ;			// contexto armazenado da tarefa
   void *stack ;			// aponta para a pilha da tarefa
   int prio;
   int dynamic_prio;
   int is_user_task;
   unsigned long int total_ticks;
   int ticks;
   int creation_time;
   int activations;
   int status; //-1 = morta, 0 = suspensa, 1 = running
   int exit_code;
   struct task_t *waited_task; //se suspensa, qual tarefa está esperando
   unsigned int slept_time; //momento que foi dormir
   unsigned int nap_time; //tempo que deve dormir
   // ... (outros campos serão adicionados mais tarde)
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  int id;
  int counter;
  task_t **suspended_tasks;
  int task_counter;
  // preencher quando necessário
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  int id;
  int max_msgs;
  int msg_size;
  int current_itens;
  void* current_item;
  void* next_position;
  void* buffer;
  // preencher quando necessário
} mqueue_t ;

#endif

