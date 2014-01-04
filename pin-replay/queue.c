#include "queue.h"

#include <pthread.h>
#include <stdlib.h>

#include <stdio.h>
#include <assert.h>

void queue_init(struct queue * que, int size, int prod_threads) {
  pthread_mutex_init(&que->mutex, NULL);
  pthread_cond_init(&que->empty, NULL);
  pthread_cond_init(&que->full, NULL);
  que->head = que->tail = 0;
  que->data = (void **)malloc(sizeof(void*) * size);
  que->size = size;
  que->prod_threads = prod_threads;
  que->end_count = 0;
}

void queue_destroy(struct queue* que)
{
    pthread_mutex_destroy(&que->mutex);
    pthread_cond_destroy(&que->empty);
    pthread_cond_destroy(&que->full);
    free(que->data);
    que->data = NULL;
}

void queue_signal_terminate(struct queue * que) {
  pthread_mutex_lock(&que->mutex);
  que->end_count++;
  pthread_cond_broadcast(&que->empty);
  pthread_mutex_unlock(&que->mutex);
}

int dequeue(struct queue * que, void **to_buf) {
    pthread_mutex_lock(&que->mutex);
    // chceck if queue is empty?
    while (que->tail == que->head && (que->end_count) < que->prod_threads) {
        pthread_cond_wait(&que->empty, &que->mutex);
    }
    // check if queue has been terminated?
    if (que->tail == que->head && (que->end_count) == que->prod_threads) {
        pthread_cond_broadcast(&que->empty);
        pthread_mutex_unlock(&que->mutex);
        return -1;
    }

    *to_buf = que->data[que->tail];
    que->tail ++;
    if (que->tail == que->size)
	que->tail = 0;
    pthread_cond_signal(&que->full);
    pthread_mutex_unlock(&que->mutex);
    return 0;
}

void enqueue(struct queue * que, void *from_buf) {
    pthread_mutex_lock(&que->mutex);
    while (que->head == (que->tail-1+que->size)%que->size)
	pthread_cond_wait(&que->full, &que->mutex);

    que->data[que->head] = from_buf;
    que->head ++;
    if (que->head == que->size)
	que->head = 0;

    pthread_cond_signal(&que->empty);
    pthread_mutex_unlock(&que->mutex);
}


struct queue q;

void* consumer(void* args)
{
	int i, result;
	pthread_t id = pthread_self();
	for (i = 0; i < 3; i++) {
		int* x;
		result = dequeue(&q, (void**) &x);
		assert(result == 0);
		printf("Get %d, thread %lu\n", *x,id);
	}

	pthread_exit(0);
}

void* producer(void* args)
{
	int i;
	pthread_t id = pthread_self();
	for (i = 0; i < 6; i++) {
		int* x = malloc(sizeof(int));
		printf("Put %d, thread %lu\n", i,id);
		*x = i;
		enqueue(&q, (void*) x);
	}

	queue_signal_terminate(&q);
	pthread_exit(0);
}

int main(int argc, const char *argv[])
{
	queue_init(&q, 2, 1);

	pthread_t t1, t2, t3, t4;

	pthread_create(&t1, NULL, consumer, NULL);
	pthread_create(&t2, NULL, consumer, NULL);
//	pthread_create(&t3, NULL, producer, NULL);
	pthread_create(&t4, NULL, producer, NULL);

	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
//	pthread_join(t3, NULL);
	pthread_join(t4, NULL);

	return 0;
}
