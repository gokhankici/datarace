#include <pthread.h>
#include <unistd.h>

pthread_mutex_t    mutex = PTHREAD_MUTEX_INITIALIZER,
		   id_mutex=PTHREAD_MUTEX_INITIALIZER;
int                i,id;
pthread_attr_t     pta;

void * incr(void * param)
{
  pthread_mutex_lock(&mutex);
  ++i; ;
  printf("thread::0x%lx run function incr.\n", (unsigned long)pthread_self() );
  pthread_mutex_unlock(&mutex);
  return NULL;
}

void *  create_t(void * param)
{
  pthread_t  tid;
  usleep(rand()%3000);
  pthread_mutex_lock(&id_mutex);
  pthread_create(&tid, &pta, incr,NULL);
  ++id; 
  printf("***thread::0x%lx gets id=%d***\n", (unsigned long)tid,id );
  pthread_mutex_unlock(&id_mutex);
  printf("thread::0x%lx created thread::0x%lx.\n", (unsigned long)pthread_self(),(unsigned long)tid);
  pthread_join(tid, NULL);
  return NULL;
}



int main(int argc, char **argv)
{
 
  pthread_attr_init(&pta);
  pthread_attr_setdetachstate(&pta, PTHREAD_CREATE_JOINABLE);
  pthread_t tid[3]; 
  printf("main::0x%lx\n",pthread_self());
  pthread_mutex_lock(&id_mutex);
  pthread_create(&tid[0], &pta, create_t,NULL);
  ++id; 
  printf("***thread::0x%lx gets id=%d***\n", (unsigned long)tid[0],id );
  pthread_mutex_unlock(&id_mutex);
  pthread_mutex_lock(&id_mutex);
  pthread_create(&tid[1], &pta, create_t,NULL);
  ++id; 
  printf("***thread::0x%lx gets id=%d***\n", (unsigned long)tid[1],id );
  pthread_mutex_unlock(&id_mutex);
  pthread_mutex_lock(&id_mutex);
  pthread_create(&tid[2], &pta, incr,NULL);
  ++id; 
  printf("***thread::0x%lx gets id=%d***\n", (unsigned long)tid[2],id );
  pthread_mutex_unlock(&id_mutex);

  int loop;
  for (loop=0; loop<3; ++loop)
     printf("thread::0x%lx created in main.\n",(unsigned long)tid[loop]);  

  for (loop=0; loop<3; ++loop) 
     pthread_join(tid[loop], NULL);
 
  printf("Cleanup\n");
  pthread_attr_destroy(&pta);
  pthread_mutex_destroy(&mutex);
  
  printf("Main completed\n");
  return 0;
}

