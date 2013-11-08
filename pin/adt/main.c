#include <stdio.h>
#include "hashmap.h"

int main(int argc, const char *argv[])
{
	hashmap* map = hashmapCreate(0);
	char* myName = "gokhan";
	char* sisterName = "nergis";

	hashmapInsert(map, myName, 1);
	hashmapInsert(map, sisterName, 2);

	printf("My name : %s\n", (char*) hashmapGet(map, 1));
	printf("Sister name : %s\n", (char*) hashmapGet(map, 2));

	return 0;
}
