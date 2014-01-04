#include <pthread.h>
#include <unistd.h>

/*
Create 4 level thread tree
*/

void*dummy(void*p){}

void *  l4(void * param)
{
  pthread_t  tid[2];
  usleep(rand()%2000);
  pthread_create(&tid[0], NULL, dummy,NULL);
  pthread_create(&tid[1], NULL, dummy,NULL);
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  return NULL;
}

void *  l3(void * param)
{
  pthread_t  tid[2];
  usleep(rand()%2000);
  pthread_create(&tid[0], NULL, l4,NULL);
  pthread_create(&tid[1], NULL, l4,NULL);
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  return NULL;
}

void *  l2(void * param)
{
  pthread_t  tid[2];
  usleep(rand()%2000);
  pthread_create(&tid[0], NULL, l3,NULL);
  pthread_create(&tid[1], NULL, l3,NULL);
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  return NULL;
}

void *  l1(void * param)
{
  pthread_t  tid[2];
  usleep(rand()%2000);
  pthread_create(&tid[0], NULL, l2,NULL);
  pthread_create(&tid[1], NULL, l2,NULL);
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  return NULL;
}

int main(int argc, char **argv)
{
  pthread_t tid;
  pthread_create(&tid, NULL, l1, NULL);
  pthread_join(tid,  NULL);
  return 0;
}




