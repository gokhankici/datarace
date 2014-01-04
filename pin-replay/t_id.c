#include <stdio.h>
#include <pthread.h>

int main(void)
{
	pthread_t tid= pthread_self();
	printf("tid=%lu\n",tid);
	return 0;
}
