#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

/*
Simple test for testing lock/unlock.
Program prints 3 or 5 for global.
*/

int global=0;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;

void*set3(void*p){
	usleep(rand()%3000);
	pthread_mutex_lock(&mutex);
	if(global==0) global=3;
	pthread_mutex_unlock(&mutex);
}

void*set5(void*p){
	usleep(rand()%3000);
	pthread_mutex_lock(&mutex);
	if(global==0) global=5;
	pthread_mutex_unlock(&mutex);
}


int main(int argc, char **argv)
{
  pthread_t tid[2];
  pthread_create(&tid[0], NULL, set5, NULL);
  pthread_create(&tid[1], NULL, set3, NULL);
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  printf("GLOBAL:%d\n",global);
  return 0;
}
