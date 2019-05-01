#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 32768
#define MIN_PRIORITY -20
#define MAX_PRIORITY 20
#define QUANTUM 20

int last_task_id = 0;

task_t *main_task;
task_t *dispatcher_task;
task_t *current_task;
task_t *previous_task = NULL;
task_t *to_be_next_task = NULL; //used when task exits

task_t **dispatcher_active_tasks;
int userTasks = 0;

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer;

// ========================== P6 ==============================

//variavel que contem os milisegundos desde o inicio da execução
unsigned int current_time = -1;

unsigned int systime () {
    return current_time;
}

void print_current_task_runtime() {
    unsigned int execution_time = systime() - current_task->creation_time;

    //subtrai 1 de activations pois a activation só é completa se todos os ticks forem zerados.
    // um exemplo disso é uma tarefa que executa antes da primeira ativação acabar. se não excluirmos 
    // esse primeiro caso, ela acabaria com QUANTUM + tempo de fato utilizado, sendo irreal
    unsigned int processor_time = (current_task->activations-1)*QUANTUM + (QUANTUM - current_task->ticks);

    // se a tarefa for o dispatcher, seta o tempo de processador como 0

    processor_time = current_task == dispatcher_task ? 0 : processor_time;
    printf("Task %d exit: execution time %u ms, processor time %u ms, %d activations\n", current_task->id, execution_time, processor_time, current_task->activations);
}

// ========================== P5 ==============================
void tick_handler() {
    current_time += 1;
    if (!current_task->is_user_task) {
        return;
    }
    current_task->ticks--;
    if (current_task->ticks == 0) {
        task_yield();
    }
    return;
}

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
        dispatcher_task->activations += 1;
        next = NULL;
        next = scheduler() ;  // scheduler é uma função

        if (to_be_next_task) 
            to_be_next_task = NULL;

        if (next)
        {
            //... // ações antes de lançar a tarefa "next", se houverem
            next->ticks = QUANTUM;
            next->activations += 1;
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
    dispatcher_task->is_user_task = 0;

    dispatcher_active_tasks = (task_t **)malloc(sizeof(task_t *));
    (*dispatcher_active_tasks) = NULL;
    userTasks = 0;
}

// ========================== Core ==============================

// Inicializa o sistema
void ppos_init () 
{
    current_time = 0;

    /* cria a main task*/
    main_task = malloc(sizeof(task_t));
    task_create(main_task, NULL, NULL);
    current_task = main_task;

    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;
    create_dispatcher() ;

    action.sa_handler = tick_handler;
    sigemptyset (&action.sa_mask) ;
    action.sa_flags = 0 ;
    if (sigaction (SIGALRM, &action, 0) < 0)
    {
        perror ("Erro em sigaction: ") ;
        exit (1) ;
    }

    // ajusta valores do temporizador
    timer.it_value.tv_usec = 1000 ;      // primeiro disparo, em micro-segundos
    timer.it_value.tv_sec  = 0 ;      // primeiro disparo, em segundos
    timer.it_interval.tv_usec = 1000 ;   // disparos subsequentes, em micro-segundos
    timer.it_interval.tv_sec  = 0 ;   // disparos subsequentes, em segundos



    // arma o temporizador
    if (setitimer (ITIMER_REAL, &timer, 0) < 0)
    {
        perror ("Erro em setitimer: ") ;
        exit (1) ;
    }


}

void task_destroy(task_t *task) {
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
    task->is_user_task = 1;

    task->creation_time = systime(); //cria com a data atual
    task->activations = 0; //numero de vezes que foi ativa

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
    print_current_task_runtime();

    if (userTasks > 0 ) {
        to_be_next_task = current_task->next; //salva a proxima tarefa

        queue_remove((queue_t**) dispatcher_active_tasks, (queue_t*) current_task); //remove da lista de tarefas
        userTasks -= 1; 
        //task_destroy(current_task);
        current_task = NULL; // seta tarefa atual como nula, assim não tenta atribuir contexto pra task que não existe

        task_yield(); //cede pro dispatcher
    } else {
        //free dispatcher
        task_switch(main_task);
    }
}

// Tarefa solta o processador
void task_yield () 
{
    task_switch(dispatcher_task);
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
