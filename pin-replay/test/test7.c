#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

/*
Simple test for testing lock/unlock.
Program prints 3 or 5 for global.
*/

int global=0;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;

void*set3(void*p){
	usleep(rand()%3000);
	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&cond, &mutex);
	global=3;
	pthread_mutex_unlock(&mutex);
	
}

void*set5(void*p){
	usleep(rand()%3000);	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);
	global=5;
	pthread_mutex_unlock(&mutex);
	
}

void*set7(void*p){
	usleep(rand()%3000);	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);
	global=7;
	pthread_mutex_unlock(&mutex);
	
}


int main(int argc, char **argv)
{
  pthread_t tid[3];
  pthread_create(&tid[0], NULL, set5, NULL);
  pthread_create(&tid[1], NULL, set3, NULL);
  pthread_create(&tid[2], NULL, set7, NULL);	
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  pthread_join(tid[2],  NULL);
  printf("GLOBAL:%d\n",global);
  return 0;
}
