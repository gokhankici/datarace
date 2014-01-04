#include <pthread.h>
#include <stdio.h>

void*f(void*p){}

int main(int argc, char **argv)
{
pthread_t             tid;
pthread_create(&tid, NULL, f, NULL);
  printf("Main completed\n");
  return 0;
}
