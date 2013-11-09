#ifndef __BLOOM_H__
#define __BLOOM_H__

#include <stdlib.h>

#define ADDR_SIZE 8

typedef unsigned int (*hashfunc_t)(const char *);

class Bloom 
{
	public:
		Bloom(int size, int nfuncs, ...);
		Bloom();
		~Bloom();
		void add(const char *s);
		bool check(const char *s);
		void clear();

		const unsigned char* getFilter() { return filter; }
		int getFilterSize(){ return filterSize; }

	private:
		int filterSize;
		unsigned char *filter;
		int nfuncs;
		hashfunc_t *funcs;
};

#endif
