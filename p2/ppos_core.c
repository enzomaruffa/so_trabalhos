#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 32768

int last_task_id = 0;
task_t *main_task;
task_t *current_task;

int has_switched_task = 0;

// Inicializa o sistema
void ppos_init () 
{
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;
}

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task,			// descritor da nova tarefa
                 void (*start_func)(void *),	// funcao corpo da tarefa
                 void *arg) // argumentos para a tarefa
{   
    getcontext (&task->context) ;

    last_task_id++;
    task->id = last_task_id;
    task->stack = malloc(STACKSIZE) ;

    if (task->stack)
    {
        task->context.uc_stack.ss_sp = task->stack ;
        task->context.uc_stack.ss_size = STACKSIZE ;
        task->context.uc_stack.ss_flags = 0 ;
        task->context.uc_link = 0 ;
    }
    else
    {
        perror ("Erro na criação da pilha: ") ;
        exit (1) ;
    }

    if (start_func) {
        makecontext (&task->context, (void*)(*start_func), 1, arg) ;
    }

    return task->id;   
}			

void task_create_main (task_t *task)			// descritor da nova tarefa
{   
    getcontext (&task->context) ;

    task->id = 0;
    task->stack = malloc(STACKSIZE) ;
    if (task->stack)
    {
        task->context.uc_stack.ss_sp = task->stack ;
        task->context.uc_stack.ss_size = STACKSIZE ;
        task->context.uc_stack.ss_flags = 0 ;
        task->context.uc_link = 0 ;
    }
    else
    {
        perror ("Erro na criação da pilha: ") ;
        exit (1) ;
    }

}			

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode) 
{
    task_switch(main_task);
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task) 
{
    if (!has_switched_task) { // first task switch
        main_task = (task_t *) malloc(sizeof(task_t)) ;
        task_create_main(main_task);
        current_task = main_task;
        has_switched_task = 1;
    }

    task_t * previous_task = current_task;
    current_task = task;

    swapcontext (&(previous_task->context), &(task->context));

    return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id () 
{
    return current_task->id;
}