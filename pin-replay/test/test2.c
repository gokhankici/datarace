#include <pthread.h>

#include <stdio.h>

/* For safe condition variable usage, must use a boolean predicate and   */
/* a mutex with the condition.                                           */
int                 conditionMet = 0;
pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;

#define NTHREADS    5

void *threadfunc(void *parm)
{
  pthread_mutex_lock(&mutex);
  while (!conditionMet) {
    printf("Thread blocked\n");
    pthread_cond_wait(&cond, &mutex);
  }
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

  sleep(5);  /* Sleep is not a very robust way to serialize threads */
  pthread_mutex_lock(&mutex);

  /* The condition has occured. Set the flag and wake up any waiting threads */
  conditionMet = 1;
  printf("Wake up all waiting threads...\n");
  for (i=0; i<NTHREADS; ++i) 
  	pthread_cond_signal(&cond);

  pthread_mutex_unlock(&mutex);

  printf("Wait for threads and cleanup\n");
  for (i=0; i<NTHREADS; ++i) 
    pthread_join(threadid[i],  NULL);
  
  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&mutex);

  printf("Main completed\n");
  return 0;
}
