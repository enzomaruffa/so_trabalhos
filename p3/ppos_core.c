#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 32768

int last_task_id = 0;

task_t *main_task;
int has_switched_task = 0;

task_t *dispatcher_task;
task_t *current_task;
task_t *previous_task = NULL;

task_t **dispatcher_active_tasks;
int userTasks = 0;


// ========================== Dispatcher ==============================

task_t *scheduler() 
{
    if (previous_task && previous_task != main_task) {
        return previous_task->next;
    }
    else
    {
        return *(dispatcher_active_tasks);
    }
}

// corpo do dispatcher
void dispatcher_body () // dispatcher é uma tarefa
{
    task_t *next = NULL;
    while ( userTasks > 0 )
    {
        next = scheduler() ;  // scheduler é uma função
        if (next)
        {
            //... // ações antes de lançar a tarefa "next", se houverem
            task_switch (next) ; // transfere controle para a tarefa "next"
            //... // ações após retornar da tarefa "next", se houverem
        }
    }
    task_exit(0) ; // encerra a tarefa dispatcher
}

// cria a tarefa dispatcher
void create_dispatcher() 
{
    dispatcher_task = malloc(sizeof(task_t));
    task_create(dispatcher_task, dispatcher_body, NULL);

    dispatcher_active_tasks = (task_t **)malloc(sizeof(task_t *));
    (*dispatcher_active_tasks) = NULL;
    userTasks = 0;

}

// ========================== Core ==============================

// Inicializa o sistema
void ppos_init () 
{
    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;
    create_dispatcher() ;
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

    if (dispatcher_active_tasks)
        userTasks++;
        queue_append((queue_t**) dispatcher_active_tasks, (queue_t*) task);

    return task->id;   
}			

void task_create_main ()			// descritor da nova tarefa
{   
    main_task = (task_t *) malloc(sizeof(task_t)) ;
    getcontext (&main_task->context) ;

    main_task->id = 0;
    main_task->stack = malloc(STACKSIZE) ;
    if (main_task->stack)
    {
        main_task->context.uc_stack.ss_sp = main_task->stack ;
        main_task->context.uc_stack.ss_size = STACKSIZE ;
        main_task->context.uc_stack.ss_flags = 0 ;
        main_task->context.uc_link = 0 ;
    }
    else
    {
        perror ("Erro na criação da pilha: ") ;
        exit (1) ;
    }

    current_task = main_task;
    has_switched_task = 1;
}	

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode) 
{
    queue_remove((queue_t**) dispatcher_active_tasks, (queue_t*) current_task);
    userTasks -= 1;
    task_switch(main_task);
}

// Tarefa solta o processador
void task_yield () 
{
    task_switch(dispatcher_task);
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task) 
{
    if (!has_switched_task) { // first task switch
        task_create_main();
    }

    previous_task = current_task;
    current_task = task;

    swapcontext (&(previous_task->context), &(task->context));

    return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id () 
{
    return current_task->id;
}
