#ifndef __BLOOM_H__
#define __BLOOM_H__

#include <stdlib.h>

#define ADDR_SIZE 8
#define BLOOM_ADDR(address) ((const unsigned char*) &address)

typedef unsigned int (*hashfunc_t)(const unsigned char *);

class Bloom 
{
	public:
		Bloom(int size, int nfuncs, ...);
		Bloom();
		const Bloom& operator=(const Bloom& bloom);
		Bloom(const Bloom& bloom);
		~Bloom();

		void add(const unsigned char *s);
		bool check(const unsigned char *s);
		bool hasInCommon(const Bloom& bloom);
		void clear();

		const unsigned char* getFilter() { return filter; }
		int getFilterSize(){ return filterSize; }
		int getFilterSizeInBytes(){ return (filterSize+7)/8; }

	private:
		int filterSize;
		unsigned char *filter;
		int nfuncs;
		hashfunc_t *funcs;
};

#endif
