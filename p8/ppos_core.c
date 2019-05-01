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

#define TASK_RUNNING 1
#define TASK_DEAD -1
#define TASK_SUSPENDED 0
#define DEFAULT_EXIT_CODE 0

int last_task_id = 0;

task_t *main_task;
task_t *dispatcher_task;
task_t *current_task;
task_t *previous_task = NULL;
task_t *to_be_next_task = NULL; //used when task exits

task_t **dispatcher_active_tasks;
task_t **dispatcher_suspended_tasks;
int active_tasks = 0;
int suspended_tasks = 0;

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer;

// ========================== P8 ==============================

void wake_up_task(task_t *task) {
    #ifdef DEBUG
        printf("[Wake Up Task] acordando a tarefa de id %d\n", task->id);
    #endif

    //remover da lista de suspensas
    queue_remove((queue_t**) dispatcher_suspended_tasks, (queue_t*) task); //remove da lista de tarefas
    suspended_tasks -= 1; 

    //adicionar na lista de suspensas
    queue_append((queue_t**) dispatcher_active_tasks, (queue_t*) task);
    active_tasks += 1;

    task->status = TASK_RUNNING;
    task->waited_task = NULL;
}

void check_suspended_tasks() {
    #ifdef DEBUG
        printf("[Check Suspended Task] verificando as %d tarefas suspensas\n", suspended_tasks);
    #endif
    if (suspended_tasks > 0) {
        task_t *first_task = (*dispatcher_suspended_tasks);
        #ifdef DEBUG
            printf("[Check Suspended Task] primeira tarefa considerada: (id %d)\n", first_task->id);
        #endif

        // caso a primeira ja seja liberada, chama recursivamente para verificar as outras
        if (first_task->waited_task->status == TASK_DEAD) {
            #ifdef DEBUG
                printf("[Check Suspended Task] acordando a primeira tarefa da lista de suspensas (id %d)\n", first_task->id);
            #endif
            wake_up_task(first_task);
            check_suspended_tasks();
        } else {
            task_t *next_task = first_task->next;

            while(next_task != first_task){
                if (next_task->waited_task->status == TASK_DEAD) {
                    next_task = next_task->next; //faz a mudança antes para evitar acessar elemento que saiu da lista
                    wake_up_task(next_task->prev); 
                } else {
                    next_task = next_task->next;
                }
            }

        }
    }
}

int task_join (task_t *task) {
    task_t *self = current_task;

    //remover da lista de ativas
    #ifdef DEBUG
        printf("[Task Join] removendo a tarefa de id %d da lista de tarefas ativas\n", self->id);
    #endif
    queue_remove((queue_t**) dispatcher_active_tasks, (queue_t*) self); //remove da lista de tarefas
    active_tasks -= 1; 

    //adicionar na lista de suspensas
    #ifdef DEBUG
        printf("[Task Join] adicionando a tarefa de id %d na lista de tarefas suspensas\n", self->id);
    #endif
    queue_append((queue_t**) dispatcher_suspended_tasks, (queue_t*) self);
    suspended_tasks += 1;

    self->status = TASK_SUSPENDED;
    self->waited_task = task;

    //yield
    task_yield();

    //voltou
    #ifdef DEBUG
        printf("[Task Join] a tarefa de id %d voltou a ser processada\n", self->id);
    #endif
    int exit_code = task->exit_code;
    return exit_code;
}

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
            printf("Erro: prioridade fora do intervalo.\n");
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
    while ( active_tasks > 0 )
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
            
            #ifdef DEBUG
                printf("[Dispatcher] Dispatcher trocando para a tarefa %d\n", next->id);
            #endif

            task_switch (next) ; // transfere controle para a tarefa "next"
            //... // ações após retornar da tarefa "next", se houverem
        }
    }

    #ifdef DEBUG
        printf("[Dispatcher] Fim do dispatcher\n");
    #endif
    task_exit(0) ; // encerra a tarefa dispatcher
}

// cria a tarefa dispatcher
void create_dispatcher() 
{
    #ifdef DEBUG
        printf("[Create Dispatcher] Criando tarefa do dispatcher\n");
    #endif
    dispatcher_task = malloc(sizeof(task_t));
    task_create(dispatcher_task, dispatcher_body, NULL);
    dispatcher_task->is_user_task = 0;

    #ifdef DEBUG
        printf("[Create Dispatcher] Criando a lista de tarefas ativas do dispatcher\n");
    #endif
    dispatcher_active_tasks = (task_t **)malloc(sizeof(task_t *));
    (*dispatcher_active_tasks) = NULL;
    active_tasks = 0;

    #ifdef DEBUG
        printf("[Create Dispatcher] Criando a lista de tarefas suspensas do dispatcher\n");
    #endif
    dispatcher_suspended_tasks = (task_t **)malloc(sizeof(task_t *));
    (*dispatcher_suspended_tasks) = NULL;
    suspended_tasks = 0;
}

// ========================== Core ==============================

// Inicializa o sistema
void ppos_init () 
{
    current_time = 0;

    /* cria o dispatcher*/
    create_dispatcher() ;

    /* cria a main task*/
    #ifdef DEBUG
        printf("[PPOS INIT] Criando a main_task\n");
    #endif
    main_task = malloc(sizeof(task_t));
    task_create(main_task, NULL, NULL);
    current_task = main_task;

    dispatcher_task->id = 1;
    main_task->id = 0;

    /* desativa o buffer da saida padrao (stdout), usado pela função printf */
    setvbuf (stdout, 0, _IONBF, 0) ;

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

    #ifdef DEBUG
        printf("[PPOS INIT] Trocando o fluxo para dispatcher\n");
    #endif
    task_switch(dispatcher_task);
}

void task_destroy(task_t *task) {
    free(task->stack);
}

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task,			// descritor da nova tarefa
                 void (*start_func)(void *),	// funcao corpo da tarefa
                 void *arg) // argumentos para a tarefa
    {
    #ifdef DEBUG
        printf("[Task Create] Criando a tarefa %d\n", last_task_id);
    #endif
      
    getcontext (&task->context) ;

    task->id = last_task_id;
    last_task_id++;
    task->stack = malloc(STACKSIZE) ;
    task_setprio(task, 0);
    task->is_user_task = 1;
    task->status = TASK_RUNNING;
    task->exit_code = DEFAULT_EXIT_CODE;
    task->waited_task = NULL;

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
        #ifdef DEBUG
            printf("[Task Create] Criando o contexto da tarefa %d\n", last_task_id-1);
        #endif
        makecontext (&task->context, (void*)(*start_func), 1, arg) ;
    }

    if (dispatcher_active_tasks) {
        #ifdef DEBUG
            printf("[Task Create] Adicionando a tarefa %d na fila de tarefas ativas\n", last_task_id);
        #endif
        queue_append((queue_t**) dispatcher_active_tasks, (queue_t*) task);
        active_tasks++;
    }

    return task->id;   
}			

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode) 
{
    print_current_task_runtime();

    task_t *self = current_task;
    self->status = TASK_DEAD;
    self->exit_code = exitCode;

    if (active_tasks > 0 ) {
        to_be_next_task = self->next; //salva a proxima tarefa

        queue_remove((queue_t**) dispatcher_active_tasks, (queue_t*) self); //remove da lista de tarefas
        active_tasks -= 1; 
        //task_destroy(self);

        #ifdef DEBUG
            printf("[Task Exit] A tarefa %d já está fora da fila de tarefas\n", self->id);
        #endif

        //Todo: acordar as outras tarefas
        check_suspended_tasks(self);

        current_task = NULL; // seta tarefa atual como nula, assim não tenta atribuir contexto pra task que não existe

        task_yield(); //cede pro dispatcher
    } else {
        //free dispatcher
        #ifdef DEBUG
            printf("[Task Exit] Liberando a memória do dispatcher\n");
        #endif
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
