/******************************** Autores **************************************/
/******             Lucas Sampaio Franco - GRR20166836                    ******/
/******             Enzo Maruffa Moreira - GRR20171626                    ******/
/*******************************************************************************/
#include <stdio.h>
#include "queue.h"

//------------------------------------------------------------------------------
// Verifica se a fila não existe (se o ponteiro da fila não está apontando para nada)
// Retorna 1 -> caso não exista, 0 -> caso exista
int check_queue_null(queue_t **queue) {
    if (queue == NULL) {
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
// Verifica se a fila (existente) está vazia (se o primeiro elemento não existe)
// Retorna 1 -> caso seja vazia, 0 -> caso não seja
int check_queue_empty(queue_t **queue) {
    if ((*queue) == NULL) {
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
// Verifica se o elemento não existe
// Retorna 1 -> caso elemento não exista, 0 -> caso exista
int check_element_null(queue_t *elem) {
    if (elem == NULL) {
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
// Verifica se um elemento está dentro da fila
// Percorre a fila comparando cada elemento com o elemento dado, 
// Retorna 1 -> caso algum elemento dentro da fila === elemento de comparação, 0 -> caso não encontre 
int check_element_belongs_queue(queue_t *queue_start, queue_t *elem) {
    queue_t *current_element = queue_start;

    // ve se é o primeiro elemento
    if (queue_start == elem) {
        return 1;
    }

    while (current_element->next != queue_start) {
        if (current_element->next == elem)
            return 1;   
        current_element = current_element->next;
    }

    return 0;
}

//------------------------------------------------------------------------------
// Insere um elemento no final da fila.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - o elemento deve existir
// - o elemento nao deve estar em outra fila
void queue_append (queue_t **queue, queue_t *elem) {
    if (check_queue_null(queue)) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe!\n");
        #endif
        return;
    }

    if (check_element_null(elem)) {
        #ifdef DEBUG
            perror("[ERRO] O elemento não existe!\n");
        #endif
        return;
    }

    if (elem->next != NULL || elem->prev != NULL) {
        #ifdef DEBUG
            perror("[ERRO] O elemento pertence a outra fila!\n");
        #endif
        return;
    }

    int size = queue_size((*queue));

    if (size == 0) {
        (*queue) = elem;
        elem->next = elem;
        elem->prev = elem;
    } else { //insere no final da fila
        queue_t *last_element = (*queue)->prev;
        elem->next = (*queue); //o ex-primeiro vira segundo
        elem->prev = last_element; //o primeiro aponta pro ultimo
        (*queue)->prev = elem; //o ex-primeiro aponta pro primeiro
        last_element->next = elem; //o ultimo aponta pro primeiro 
    }
}

//------------------------------------------------------------------------------
// Remove o elemento indicado da fila, sem o destruir.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - a fila nao deve estar vazia
// - o elemento deve existir
// - o elemento deve pertencer a fila indicada
// Retorno: apontador para o elemento removido, ou NULL se erro
queue_t *queue_remove (queue_t **queue, queue_t *elem) {
    if (check_queue_null(queue)) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe!\n");
        #endif
        return NULL;
    }

    if (check_queue_empty(queue)) {
        #ifdef DEBUG
            perror("[ERRO] A fila está vazia!\n");
        #endif
        return NULL;
    }

    if (check_element_null(elem)) {
        #ifdef DEBUG
            perror("[ERRO] O elemento não existe!\n");
        #endif
        return NULL;
    }

    if (!check_element_belongs_queue((*queue), elem)) {
        #ifdef DEBUG
            perror("[ERRO] O elemento pertence a outra fila!\n");
        #endif
        return NULL;
    }

    // se for o primeiro da fila
    if (*queue == elem) { 
        (*queue) = elem->next;
    }

    if (queue_size((*queue)) == 1) {
        (*queue) = NULL;
    }

    (elem->prev)->next = elem->next;
    (elem->next)->prev = elem->prev;

    elem->next = NULL;
    elem->prev = NULL;

    return elem;
}

//------------------------------------------------------------------------------
// Conta o numero de elementos na fila
// Retorno: numero de elementos na fila
int queue_size (queue_t *queue) {
    if (check_queue_empty(&queue)) {
        #ifdef DEBUG
            perror("[ERRO] A fila está vazia!\n");
        #endif
        return 0;
    }

    if (queue->next == queue) {
        return 1;
    }

    queue_t *current_element = queue;
    int count = 1;

    while (current_element->next != queue) {
        count += 1;
        current_element = current_element->next;
    }

    return count;
}


//------------------------------------------------------------------------------
// Percorre a fila e imprime na tela seu conteúdo. A impressão de cada
// elemento é feita por uma função externa, definida pelo programa que
// usa a biblioteca.
//
// Essa função deve ter o seguinte protótipo:
//
// void print_elem (void *ptr) ; // ptr aponta para o elemento a imprimir
void queue_print (char *name, queue_t *queue, void print_elem (void*) )  {
    if (check_queue_null(&queue)) {
        #ifdef DEBUG
            perror("[ERRO] A fila não existe!\n");
        #endif
        return;
    }

    if (check_queue_empty(&queue)) {
        #ifdef DEBUG
            perror("[ERRO] A fila está vazia!\n");
        #endif
        return;
    }

    queue_t *current_element = queue;
    print_elem((void *)current_element);

    while (current_element->next != queue) {
        current_element = current_element->next;
        print_elem((void *)current_element);
    }
}