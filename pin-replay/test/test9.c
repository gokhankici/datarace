#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

/*
4 thread 2 waits,2 signals on same cv.
*/

int global=0;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;

void*wait1(void*p){
	//usleep(rand()%3000);
	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&cond, &mutex);
	global=1;
	pthread_mutex_unlock(&mutex);
	
}

void*wait2(void*p){
	//usleep(rand()%3000);
	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&cond, &mutex);
	global=2;
	pthread_mutex_unlock(&mutex);
	
}

void*sig1(void*p){
	//usleep(rand()%3000);	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);
	global=3;
	pthread_mutex_unlock(&mutex);
	
}

void*sig2(void*p){
	//usleep(rand()%3000);	
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);
	global=4;
	pthread_mutex_unlock(&mutex);
	
}


int main(int argc, char **argv)
{
  pthread_t tid[4];
  pthread_create(&tid[0], NULL, wait1, NULL);
  pthread_create(&tid[1], NULL, wait2, NULL);
  pthread_create(&tid[2], NULL, sig1, NULL);
  pthread_create(&tid[3], NULL, sig2, NULL);	
  pthread_join(tid[0],  NULL);
  pthread_join(tid[1],  NULL);
  pthread_join(tid[2],  NULL);
  pthread_join(tid[3],  NULL);
  printf("GLOBAL:%d\n",global);
  return 0;
}
