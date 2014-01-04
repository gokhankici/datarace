#include <pthread.h>
#include <stdio.h>

int                 global = 0;
pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;

#define NTHREADS    16

void *threadfunc(void *parm)
{
  pthread_mutex_lock(&mutex);
  pthread_cond_wait(&cond, &mutex);
  pthread_mutex_unlock(&mutex);
  return NULL;
}

int main(int argc, char **argv)
{
  int                   i;
  pthread_t             threadid[NTHREADS];

  for(i=0; i<NTHREADS; ++i) 
    pthread_create(&threadid[i], NULL, threadfunc, NULL);

  sleep(8);  

  pthread_cond_broadcast(&cond);

  for (i=0; i<NTHREADS; ++i) 
    pthread_join(threadid[i],  NULL);
  
  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&mutex);

  return 0;
}
