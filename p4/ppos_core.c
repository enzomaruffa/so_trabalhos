#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 32768
#define MIN_PRIORITY -20
#define MAX_PRIORITY 20

int last_task_id = 0;

task_t main_task;
task_t dispatcher_task;
task_t *current_task;
task_t *previous_task = NULL;
task_t *to_be_next_task = NULL; //used when task exits

task_t **dispatcher_active_tasks;
int userTasks = 0;

// ========================== P4 ==============================

void task_setprio (task_t *task, int prio) {
    if (prio > MAX_PRIORITY || prio < MIN_PRIORITY) {
        #ifdef DEBUG
            printf("Erro: prioridade fora do intervalo.");
        #endif
        return;
    }
    if (task != NULL) {
        task->prio = task->dynamic_prio = prio;
        return;
    }
    current_task->prio = current_task->dynamic_prio = prio;
    return;
}

int task_getprio (task_t *task) {
    if(task != NULL) {
        return task->prio;
    }
    return current_task->prio;
}



// ========================== Dispatcher ==============================

task_t *scheduler() 
{
    task_t *runner = (*dispatcher_active_tasks)->next;
    task_t *priority_task = (*dispatcher_active_tasks);

    while (runner != (*dispatcher_active_tasks)) {
        if (runner->dynamic_prio < priority_task->dynamic_prio) {
            priority_task->dynamic_prio--;
            priority_task = runner;
        } else {
            runner->dynamic_prio--;
        }
        runner = runner->next;
    }

    task_setprio(priority_task, priority_task->prio);

    return priority_task;
}

// corpo do dispatcher
void dispatcher_body () // dispatcher é uma tarefa
{
    task_t *next = NULL;
    while ( userTasks > 0 )
    {
        next = NULL;
        next = scheduler() ;  // scheduler é uma função

        if (to_be_next_task) 
            to_be_next_task = NULL;

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
    task_create(&dispatcher_task, dispatcher_body, NULL);

    dispatcher_active_tasks = malloc(sizeof(task_t *));
    (*dispatcher_active_tasks) = NULL;
    userTasks = 0;
}

// ========================== Core ==============================

// Inicializa o sistema
void ppos_init () 
{
    /* cria a main task*/
    task_create(&main_task, NULL, NULL);
    current_task = &main_task;

    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;
    create_dispatcher() ;
}

void task_stack_destroy(task_t *task) {
    free(task->stack);
}

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task,			// descritor da nova tarefa
                 void (*start_func)(void *),	// funcao corpo da tarefa
                 void *arg) // argumentos para a tarefa
{   
    getcontext (&task->context) ;

    task->id = last_task_id;
    last_task_id++;
    task->stack = malloc(STACKSIZE) ;
    task_setprio(task, 0);
    
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

    if (dispatcher_active_tasks) {
        userTasks++;
        queue_append((queue_t**) dispatcher_active_tasks, (queue_t*) task);
    }

    return task->id;   
}			

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode) 
{
    if (userTasks > 0 ) {
        to_be_next_task = current_task->next; //salva a proxima tarefa

        queue_remove((queue_t**) dispatcher_active_tasks, (queue_t*) current_task); //remove da lista de tarefas
        userTasks -= 1; 
        task_stack_destroy(current_task);
        current_task = NULL; // seta tarefa atual como nula, assim não tenta atribuir contexto pra task que não existe

        task_yield(); //cede pro dispatcher
    } else {
        task_stack_destroy(&dispatcher_task);
        free(dispatcher_active_tasks);
        
        current_task = NULL;

        task_switch(&main_task);
    }
}

// Tarefa solta o processador
void task_yield () 
{
    task_switch(&dispatcher_task);
}

// alterna a execução para a tarefa indicada
int task_switch (task_t *task) 
{
    previous_task = current_task;
    current_task = task;

    if (previous_task)
        swapcontext (&(previous_task->context), &(task->context));
    else
        setcontext(&(task->context));

    return 0;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id () 
{
    return current_task->id;
}
