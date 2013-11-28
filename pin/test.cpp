#include <iostream>
#include <string.h>
#include "Bloom.h"

using namespace std;

unsigned int hash_1(const char* data)
{
	return (unsigned int) strlen(data);
}

unsigned int hash_2(const char* data)
{
	unsigned int result = 0;
	int len = strlen(data);

	for (int i = 0; i < len; i++)
	{
		result += data[i];
	}

	return (unsigned int) result;
}

int main(int argc, const char *argv[])
{
	Bloom bloom(2048, 2, hash_1, hash_2);

	int size = 6;
	const unsigned char* list[] =
	{ "ali", "veli", "deli", "49", "50", "94" };

	for (int i = 0; i < size - 1; i++)
	{
		bloom.add(list[i]);
	}

	for (int i = 0; i < size; i++)
	{
		cout << "Contains " << list[i] << " : " << bloom.check(list[i]) << endl;
	}

	return 0;
}
