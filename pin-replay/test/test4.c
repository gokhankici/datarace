#include <pthread.h>
#include <unistd.h>

/*
Simple test for testing thread id determinism.
*/

void*dummy(void*p){}

void *  create(void * param)
{
  pthread_t  tid[2];
  usleep(rand()%2000);
  pthread_create(&tid[0], NULL, dummy,NULL);
  pthread_create(&tid[1], NULL, dummy,NULL);
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  return NULL;
}

int main(int argc, char **argv)
{
  pthread_t tid[2];
  pthread_create(&tid[0], NULL, create, NULL);
  pthread_create(&tid[1], NULL, create, NULL);
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  return 0;
}
