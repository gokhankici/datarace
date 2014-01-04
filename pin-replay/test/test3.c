#include <pthread.h>

#include <stdio.h>

/* For safe condition variable usage, must use a boolean predicate and   */
/* a mutex with the condition.                                           */
int                 global = 0;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;

#define NTHREADS    5

void *threadfunc(void *parm)
{
  pthread_mutex_lock(&mutex);
  global++;
  pthread_mutex_unlock(&mutex);
  return NULL;
}

int main(int argc, char **argv)
{
  int                   i;
  pthread_t             threadid[NTHREADS];

  printf("Create %d threads\n", NTHREADS);
  for(i=0; i<NTHREADS; ++i) 
    pthread_create(&threadid[i], NULL, threadfunc, NULL);

  printf("Wait for threads and cleanup\n");
  for (i=0; i<NTHREADS; ++i) 
    pthread_join(threadid[i],  NULL);

  pthread_mutex_destroy(&mutex);

  printf("Main completed\n");
  return 0;
}
