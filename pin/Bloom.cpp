#include <limits.h>
#include <cstdarg>
#include <string.h>

#include "Bloom.h"
#include "MurmurHash2.h"

#define SETBIT(filter, n) (filter[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GETBIT(filter, n) (filter[n/CHAR_BIT] & (1<<(n%CHAR_BIT)))

static unsigned int defaultHashFunction(const char * key)
{
	return MurmurHash2(key, ADDR_SIZE, 0);
}

Bloom::Bloom()
{
	int size = 256;
	int nfuncs = 1;
	
	filter = (unsigned char *) calloc((size+CHAR_BIT-1)/CHAR_BIT, sizeof(char));
	funcs  = (hashfunc_t*) malloc(nfuncs*sizeof(hashfunc_t));

	funcs[0] = defaultHashFunction;

	this->nfuncs = nfuncs;
	this->filterSize  = size;
}

Bloom::Bloom(int size, int nfuncs, ...)
{
	va_list l;
	
	filter = (unsigned char *) calloc((size+CHAR_BIT-1)/CHAR_BIT, sizeof(char));
	funcs  = (hashfunc_t*) malloc(nfuncs*sizeof(hashfunc_t));

	va_start(l, nfuncs);
	for(int n=0; n < nfuncs; ++n) 
	{
		funcs[n]=va_arg(l, hashfunc_t);
	}
	va_end(l);

	this->nfuncs = nfuncs;
	this->filterSize  = size;
}

Bloom::~Bloom()
{
	free(filter);
	free(funcs);
}

void Bloom::add(const char *s)
{
	for(int n=0; n < nfuncs; ++n) 
	{
		SETBIT(filter, funcs[n](s)%filterSize);
	}
}

bool Bloom::check(const char *s)
{
	for(int n=0; n < nfuncs; ++n) 
	{
		if(!(GETBIT(filter, funcs[n](s)%filterSize)))
		{
			return false;
		}
	}

	return true;
}

void Bloom::clear()
{
	memset(filter, 0, filterSize);
}
