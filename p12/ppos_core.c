#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <strings.h>
#include "ppos.h"
#include "queue.h"

#define STACKSIZE 32768
#define _XOPEN_SOURCE 600	/* para compilar no MacOS */

#define MIN_PRIORITY -20
#define MAX_PRIORITY 20
#define QUANTUM 20

#define TASK_RUNNING 1
#define TASK_DEAD -1
#define TASK_SUSPENDED 0
#define TASK_SLEEPING 2
#define DEFAULT_EXIT_CODE 0

int last_task_id = 0;
int last_semaphore_id = -1;
int last_mqueue_id = -1;


int can_preempt = 1;

task_t *main_task;
task_t *dispatcher_task;
task_t *current_task;
task_t *previous_task = NULL;
task_t *to_be_next_task = NULL; //used when task exits

task_t **dispatcher_active_tasks;
task_t **dispatcher_suspended_tasks;
task_t **dispatcher_sleeping_tasks;

int active_tasks = 0;
int suspended_tasks = 0;
int sleeping_tasks = 0;

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer;


// ========================== P12 ============================== 



// cria uma fila para até max mensagens de size bytes cada
int mqueue_create (mqueue_t *queue, int max, int size) {

    #ifdef DEBUG
        printf("[Mqueue Create] Criando fila de mensagens com ID %d, %d espaços de %d bytes\n", last_mqueue_id+1, max, size);
    #endif

    if (!(queue)) {
        return 1;
    }
    
    last_mqueue_id += 1;
    queue->id = last_mqueue_id;
    
    queue->buffer = malloc(max * size);
    queue->max_msgs = max;
    queue->msg_size = size;
    queue->current_item = queue->buffer;
    queue->next_position = queue->buffer;

    queue->s_empty_lots = malloc(sizeof(semaphore_t));
    queue->s_buffer = malloc(sizeof(semaphore_t));
    queue->s_items = malloc(sizeof(semaphore_t));
    sem_create(queue->s_empty_lots, max);
    sem_create(queue->s_buffer, 1);
    sem_create(queue->s_items, 0);

    return 0;
}

// envia uma mensagem para a fila
int mqueue_send (mqueue_t *queue, void *msg) {
    #ifdef DEBUG
        printf("[Mqueue Send] Enviando mensagem para a fila de mensagens com ID %d com %d espaços\n", queue->id, queue->max_msgs);
    #endif

    #ifdef DEBUG
        printf("[Mqueue Send] Down no semáforo de vagas\n");
    #endif
    if (sem_down(queue->s_buffer) == -1) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe mais!\n");
        #endif
        return -1;
    }

    #ifdef DEBUG
        printf("[Mqueue Send] Down no semáforo do buffer\n");
    #endif
    if (sem_up(queue->s_buffer) == -1) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe mais!\n");
        #endif
        return -1;
    }

    if (queue->buffer) {
        bcopy(msg, queue->next_position, queue->msg_size);

        if (queue->next_position + queue->msg_size == (queue->buffer + queue->max_msgs * queue->msg_size)) { //ou seja, estouraria o buffer
            queue->next_position = queue->buffer;
        } else {
            queue->next_position += queue->msg_size;
        }
    }

    #ifdef DEBUG
        printf("[Mqueue Send] Up no semáforo do buffer\n");
    #endif
    if (sem_up(queue->s_buffer) == -1) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe mais!\n");
        #endif
        return -1;
    }

    #ifdef DEBUG
        printf("[Mqueue Send] Up no semáforo de itens\n");
    #endif
    if (sem_up(queue->s_items) == -1) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe mais!\n");
        #endif
        return -1;
    }

    if (queue->s_empty_lots) //Queue is still alive
        return 0;
    return -1;
}

// recebe uma mensagem da fila
int mqueue_recv (mqueue_t *queue, void *msg) {
    #ifdef DEBUG
        printf("[Mqueue Recv] Consumindo mensagem para a fila de mensagens com ID %d com %d espaços\n", queue->id, queue->max_msgs);
    #endif

    #ifdef DEBUG
        printf("[Mqueue Recv] Down no semáforo de itens\n");
    #endif
    if (sem_down(queue->s_items) == -1) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe mais!\n");
        #endif
        return -1;
    }

    #ifdef DEBUG
        printf("[Mqueue Recv] Down no semáforo do buffer\n");
    #endif
    if (sem_down(queue->s_buffer) == -1) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe mais!\n");
        #endif
        return -1;
    }

    if (queue->buffer) {
        bcopy(queue->current_item, msg, queue->msg_size);

        if (queue->current_item + queue->msg_size == (queue->buffer + queue->max_msgs * queue->msg_size)) { //ou seja, estouraria o buffer
            queue->current_item = queue->buffer;
        } else {
            queue->current_item += queue->msg_size;
        }
    }

    #ifdef DEBUG
        printf("[Mqueue Recv] Up no semáforo do buffer\n");
    #endif
    if (sem_up(queue->s_buffer) == -1) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe mais!\n");
        #endif
        return -1;
    }

    #ifdef DEBUG
        printf("[Mqueue Recv] Up no semáforo de vagas\n");
    #endif
    if (sem_up(queue->s_empty_lots) == -1) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe mais!\n");
        #endif
        return -1;
    }

    if (queue->s_empty_lots) //Queue is still alive
        return 0;
    return -1;

}

// destroi a fila, liberando as tarefas bloqueadas
int mqueue_destroy (mqueue_t *queue) {

    #ifdef DEBUG
        printf("[Mqueue Destroy] Destruindo fila de mensagens\n");
    #endif

    can_preempt = 0;

    #ifdef DEBUG
        printf("[Mqueue Destroy] Destruindo semáfotos\n");
    #endif
    sem_destroy(queue->s_buffer);
    sem_destroy(queue->s_empty_lots);
    sem_destroy(queue->s_items);

    queue->s_buffer = NULL;
    queue->s_empty_lots = NULL;
    queue->s_items = NULL;

    #ifdef DEBUG
        printf("[Mqueue Destroy] Desalocando buffer\n");
    #endif
    free(queue->buffer);
    queue->buffer = NULL;

    can_preempt = 1;

    return 0;
}

// informa o número de mensagens atualmente na fila
int mqueue_msgs (mqueue_t *queue) {
    return queue->s_items->counter;
}

// ========================== P10 ============================== 

// cria um semaforo
int sem_create (semaphore_t *s, int value) {
    last_semaphore_id++;
    s->id = last_semaphore_id;

    #ifdef DEBUG 
        printf("[Semaphore Create] Criando semáforo %d com valor %d\n", s-> id, value);
    #endif

    s->task_counter = 0;
    s->counter = value;
    s->suspended_tasks = (task_t **)malloc(sizeof(task_t *));
    *(s->suspended_tasks) = NULL;
    return 0;
}

// requisita o semáforo
int sem_down (semaphore_t *s) {
    if (current_task->ticks < 0) {
        task_yield();
    }

    if (!(s)) {
        #ifdef DEBUG
            perror("[ERRO] O semáforo não existe!\n");
        #endif
        return -1;
    }

    #ifdef DEBUG
        printf("[Semaphore Down] desabilitando preempção em %d\n", current_task->id);
    #endif
    can_preempt = 0;
    #ifdef DEBUG
        printf("[Semaphore Down] tarefa %d iniciando no semáforo %d\n", current_task->id, s->id);
        printf("[Semaphore Down] semaforo tem %d vagas e %d pessoas na fila\n", s->counter, s->task_counter);
    #endif

    s->counter -= 1;

    if (s->counter < 0) {
        #ifdef DEBUG
            printf("[Semaphore Down] removendo a tarefa de id %d na lista de tarefas ativas\n", current_task->id);
        #endif
        queue_remove((queue_t**) dispatcher_active_tasks, (queue_t*) current_task); //remove da lista de tarefas
        active_tasks -= 1; 

        #ifdef DEBUG
            printf("[Semaphore Down] adicionando a tarefa de id %d na lista de tarefas do semáforo\n", current_task->id);
        #endif
        
        s->task_counter += 1;
        queue_append((queue_t**) s->suspended_tasks, (queue_t*) current_task);
        current_task->status = TASK_SUSPENDED;

        #ifdef DEBUG
            printf("[Semaphore Down] Reabilitando preempção em %d\n", current_task->id);
        #endif
        can_preempt = 1;
        task_yield();
    }

    #ifdef DEBUG
        printf("[Semaphore Down] Reabilitando preempção em %d\n", current_task->id);
    #endif
    can_preempt = 1;

    if (s->suspended_tasks) //se existe, o semáforo ainda é válido
        return 0;
    return 1;
}

void sem_wake_up_first(semaphore_t *s) {
    task_t * task = *(s->suspended_tasks);

    #ifdef DEBUG
        printf("[Semaphore Wake Up First] removendo a tarefa de id %d das lista de tarefas do semaforo\n", task->id);
    #endif
    queue_remove((queue_t**) s->suspended_tasks, (queue_t*) task); //remove da lista de tarefas

    //adicionar na lista de ativas
    #ifdef DEBUG
        printf("[Semaphore Wake Up First] adicionando a tarefa de id %d na lista de tarefas ativas\n", task->id);
    #endif
    queue_append((queue_t**) dispatcher_active_tasks, (queue_t*) task);
    active_tasks += 1;

    task->status = TASK_RUNNING;
}

// libera o semáforo
int sem_up (semaphore_t *s) {

    if (!(s)) {
        #ifdef DEBUG
            perror("[ERRO] O semáforo não existe!\n");
        #endif
        return -1;
    }

    can_preempt = 0;
    s->counter += 1;

    #ifdef DEBUG
        printf("[Semaphore Up] tarefa %d iniciando no semáforo %d\n", current_task->id, s->id);
        printf("[Semaphore Up] semaforo tem %d vagas e %d pessoas na fila\n", s->counter, s->task_counter);
    #endif

    if (s->task_counter > 0) {
        #ifdef DEBUG
            printf("[Semaphore Up] acordando primeira tarefa\n");
        #endif
        s->task_counter -= 1;
        sem_wake_up_first(s);
    }
    can_preempt = 1;
    return 0;
}


// destroi o semáforo, liberando as tarefas bloqueadas
int sem_destroy (semaphore_t *s) {
    int old_can_preempt = can_preempt; //preserva o estado da preempção
    can_preempt = 0;

    if ((s->task_counter) > 0) {
        for (int i=0; i < s->task_counter; i++)
            sem_wake_up_first(s);
    }

    free(s->suspended_tasks);
    s->suspended_tasks = NULL;

    can_preempt = old_can_preempt;

    return 0;
}


// ========================== P9 ============================== 

// suspende a tarefa corrente por t milissegundos
void task_sleep (int t) {
    task_t *self = current_task;

    //remover da lista de ativas
    #ifdef DEBUG
        printf("[Task Sleep] removendo a tarefa de id %d da lista de tarefas ativas\n", self->id);
    #endif
    queue_remove((queue_t**) dispatcher_active_tasks, (queue_t*) self); //remove da lista de tarefas
    active_tasks -= 1; 

    //adicionar na lista de suspensas
    #ifdef DEBUG
        printf("[Task Sleep] adicionando a tarefa de id %d na lista de tarefas dormentes\n", self->id);
    #endif
    queue_append((queue_t**) dispatcher_sleeping_tasks, (queue_t*) self);
    sleeping_tasks += 1;

    self->status = TASK_SLEEPING;
    self->slept_time = systime();
    self->nap_time = t;

    #ifdef DEBUG
        printf("[Task Sleep] a tarefa de id %d está indo dormir as %d com nap_time de %d\n", self->id, self->slept_time, self->nap_time);
    #endif

    //yield
    task_yield();
}

void task_finish_sleep(task_t *task) { 
    #ifdef DEBUG
        printf("[Task Sleep] a tarefa de id %d dormiu das %d até as %d. Seu nap_time é de %d\n", task->id, task->slept_time, systime(), task->nap_time);
    #endif

    //remover da lista de ativas
    #ifdef DEBUG
        printf("[Task Sleep] removendo a tarefa de id %d da lista de tarefas dormentes\n", task->id);
    #endif
    queue_remove((queue_t**) dispatcher_sleeping_tasks, (queue_t*) task); //remove da lista de tarefas
    sleeping_tasks -= 1;

    //adicionar na lista de suspensas
    #ifdef DEBUG
        printf("[Task Sleep] adicionando a tarefa de id %d na lista de tarefas ativas\n", task->id);
    #endif
    queue_append((queue_t**) dispatcher_active_tasks, (queue_t*) task);
    active_tasks += 1; 

    task->status = TASK_RUNNING;
    task->slept_time = -1;
    task->nap_time = -1;
}

void check_sleeping_tasks() {
    if (sleeping_tasks > 0) {
        task_t *first_task = (*dispatcher_sleeping_tasks);
        #ifdef DEBUG
            //printf("[Check Sleeping Tasks] primeira tarefa considerada: (id %d)\n", first_task->id);
        #endif

        // caso a primeira ja seja liberada, chama recursivamente para verificar as outras        
        #ifdef DEBUG
            //printf("Check Sleeping Tasks] a tarefa de id %d dormiu das %d até as %d. Seu nap_time é de %d\n", first_task->id, first_task->slept_time, systime(), first_task->nap_time);
        #endif
        if (first_task->slept_time + first_task->nap_time <= systime()) {
            #ifdef DEBUG
                printf("[Check Sleeping Tasks] acordando a primeira tarefa da lista de dormentes (id %d)\n", first_task->id);
            #endif
            task_finish_sleep(first_task);
            check_sleeping_tasks();
        } else {
            task_t *next_task = first_task->next;

            while(next_task != first_task){
                #ifdef DEBUG
                    //printf("Check Sleeping Tasks] a tarefa de id %d dormiu das %d até as %d. Seu nap_time é de %d\n", first_task->id, first_task->slept_time, systime(), first_task->nap_time);
                #endif
                if (next_task->slept_time + next_task->nap_time <= systime()) {
                    next_task = next_task->next; //faz a mudança antes para evitar acessar elemento que saiu da lista
                    task_finish_sleep(next_task->prev); 
                } else {
                    next_task = next_task->next;
                }
            }

        }
    }
}

// ========================== P8 ==============================

void wake_up_task(task_t *task) {
    #ifdef DEBUG
        printf("[Wake Up Task] acordando a tarefa de id %d\n", task->id);
    #endif

    //remover da lista de suspensas
    #ifdef DEBUG
        printf("[Task Join] removendo a tarefa de id %d da lista de tarefas suspensas\n", task->id);
    #endif
    queue_remove((queue_t**) dispatcher_suspended_tasks, (queue_t*) task); //remove da lista de tarefas
    suspended_tasks -= 1; 

    //adicionar na lista de suspensas
    #ifdef DEBUG
        printf("[Task Join] adicionando a tarefa de id %d na lista de tarefas ativas\n", task->id);
    #endif
    queue_append((queue_t**) dispatcher_active_tasks, (queue_t*) task);
    active_tasks += 1;

    task->status = TASK_RUNNING;
    task->waited_task = NULL;
}

void check_suspended_tasks(task_t** suspendend_tasks) {
    #ifdef DEBUG
        printf("[Check Suspended Tasks] verificando as %d tarefas suspensas\n", suspended_tasks);
    #endif
    if (suspended_tasks > 0) {
        task_t *first_task = (*dispatcher_suspended_tasks);
        #ifdef DEBUG
            printf("[Check Suspended Tasks] primeira tarefa considerada: (id %d)\n", first_task->id);
        #endif

        // caso a primeira ja seja liberada, chama recursivamente para verificar as outras
        if (first_task->waited_task->status == TASK_DEAD) {
            #ifdef DEBUG
                printf("[Check Suspended Tasks] acordando a primeira tarefa da lista de suspensas (id %d)\n", first_task->id);
            #endif
            wake_up_task(first_task);
            check_suspended_tasks(suspendend_tasks);
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

    if (task == NULL || task->status == TASK_DEAD) {
        #ifdef DEBUG
            printf("[Task Join] a tarefa passada não existe. Retornando imediatamente.\n");
        #endif
        return -1;
    }

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
    unsigned long int processor_time = current_task->total_ticks;

    // se a tarefa for o dispatcher, seta o tempo de processador como 0

    processor_time = current_task == dispatcher_task ? 0 : processor_time;
    printf("Task %d exit: execution time %u ms, processor time %lu ms, %d activations\n", current_task->id, execution_time, processor_time, current_task->activations);
}

// ========================== P5 ==============================
void tick_handler() {
    current_time += 1;
    if (!current_task->is_user_task) {
        return;
    }
    current_task->ticks--;
    current_task->total_ticks += 1;

    #ifdef DEBUG
        printf("[Tick Handler] can_preempt = %d, current_task->ticks = %d\n", can_preempt, current_task->ticks);
    #endif
    if (can_preempt) {
        #ifdef DEBUG
            printf("[Tick Handler] possibly preempting! \n");
        #endif
        if (current_task->ticks <= 0) {
            #ifdef DEBUG
                printf("[Tick Handler] yielding! \n");
            #endif
            task_yield();
        }
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
    while ( active_tasks > 0 || sleeping_tasks > 0)
    {
        check_sleeping_tasks();

        if (active_tasks > 0) {  // pode ter tarefas dormentes
            next = NULL;
            next = scheduler() ;  // scheduler é uma função 
        
            if (to_be_next_task) 
                to_be_next_task = NULL;

            if (next)
            {
                //... // ações antes de lançar a tarefa "next", se houverem
                next->ticks = QUANTUM;
                next->activations += 1; 
                next->total_ticks += 1; //consideramos pelo menos 1ms por ativação

                #ifdef DEBUG
                    printf("[Dispatcher] Tarefa %d está com %d ativacões\n", next->id, next->activations);
                #endif
                
                #ifdef DEBUG
                    printf("[Dispatcher] Dispatcher trocando para a tarefa %d\n", next->id);
                #endif

                task_switch (next) ; // transfere controle para a tarefa "next"

                dispatcher_task->activations += 1 ;
                //... // ações após retornar da tarefa "next", se houverem
            }
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

    #ifdef DEBUG
        printf("[Create Dispatcher] Criando a lista de tarefas dormentes do dispatcher\n");
    #endif
    dispatcher_sleeping_tasks = (task_t **)malloc(sizeof(task_t *));
    (*dispatcher_sleeping_tasks) = NULL;
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

    
    #ifdef DEBUG
        printf("[PPOS INIT] Main_task com %d ativacôes\n", main_task->activations);
    #endif

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
    task->total_ticks = 0;
    task->status = TASK_RUNNING;
    task->exit_code = DEFAULT_EXIT_CODE;
    task->waited_task = NULL;
    task->slept_time = -1;
    task->nap_time = -1;

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
        check_suspended_tasks(dispatcher_suspended_tasks);

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
