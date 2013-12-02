#include <limits.h>
#include <cstdarg>
#include <string.h>
#include <assert.h>

#include <iostream>
#include <algorithm>

#include "Bloom.h"
#include "MurmurHash2.h"

#define SETBIT(filter, n) (filter[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define UNSETBIT(filter, n) (filter[n/CHAR_BIT] &= ~(1<<(n%CHAR_BIT)))
#define GETBIT(filter, n) (filter[n/CHAR_BIT] & (1<<(n%CHAR_BIT)))

static unsigned int defaultHashFunction(const unsigned char * key)
{
	return MurmurHash2(key, ADDR_SIZE, 0);
}

Bloom::Bloom()
{
	int size = DEFAULT_BLOOM_FILTER_SIZE;
	int nfuncs = 1;

	filter = (unsigned char *) calloc((size + CHAR_BIT - 1) / CHAR_BIT,
			sizeof(char));
	funcs = (hashfunc_t*) malloc(nfuncs * sizeof(hashfunc_t));

	funcs[0] = defaultHashFunction;

	this->nfuncs = nfuncs;
	this->filterSize = size;
	elementCount = 0;
}

Bloom::Bloom(int size, int nfuncs, ...)
{
	va_list l;

	filter = (unsigned char *) calloc((size + CHAR_BIT - 1) / CHAR_BIT,
			sizeof(char));
	funcs = (hashfunc_t*) malloc(nfuncs * sizeof(hashfunc_t));

	va_start(l, nfuncs);
	for (int n = 0; n < nfuncs; ++n)
	{
		funcs[n] = va_arg(l, hashfunc_t);
	}
	va_end(l);

	this->nfuncs = nfuncs;
	this->filterSize = size;
	elementCount = 0;
}

Bloom::~Bloom()
{
	free(filter);
	free(funcs);
}

void Bloom::add(const unsigned char *s)
{
#ifndef SET_OVERRIDE
	for(int n=0; n < nfuncs; ++n)
	{
		SETBIT(filter, funcs[n](s)%filterSize);
	}
#else
	locations.insert(*((ADDRINT*) s));
#endif

	elementCount++;
}

void Bloom::remove(ADDRINT removedAddress)
{
#ifndef SET_OVERRIDE
	for(int n=0; n < nfuncs; ++n)
	{
		UNSETBIT(filter, funcs[n](s)%filterSize);
	}
#else
	locations.insert(removedAddress);
#endif

	// TODO: multiple elements could be removed from the filter but doesn't matter
	elementCount--;
}

const Bloom& Bloom::operator=(const Bloom& bloom)
{
	nfuncs = bloom.nfuncs;
	filterSize = bloom.filterSize;
	elementCount = bloom.elementCount;

	filter = (unsigned char *) calloc((filterSize + CHAR_BIT - 1) / CHAR_BIT,
			sizeof(char));
	funcs = (hashfunc_t*) malloc(nfuncs * sizeof(hashfunc_t));

	memcpy(filter, bloom.filter, (filterSize + CHAR_BIT - 1) / CHAR_BIT);
	memcpy(funcs, bloom.funcs, nfuncs * sizeof(hashfunc_t));

#ifdef SET_OVERRIDE
	locations = bloom.locations;
#endif

	return *this;
}

Bloom::Bloom(const Bloom& bloom)
{
	(*this) = bloom;
}

bool Bloom::hasInCommon(const Bloom& bloom)
{
#ifndef SET_OVERRIDE
	assert(filterSize == bloom.filterSize);
	int byteCount = getFilterSizeInBytes();
	int same = 0;

	for(int n = 0; n < byteCount; ++n)
	{
		if(filter[n] & bloom.filter[n])
		{
#ifdef DEBUG_MODE
			printf("%02X / %02X (%d) === Bits: %d | Bytes: %d | Byte#: %d\n", filter[n], bloom.filter[n], same, filterSize, byteCount, n);
#endif
			return true;
		}
		same++;
	}

	return false;
#else
	std::vector<ADDRINT> v_intersection;
	std::set_intersection(locations.begin(), locations.end(),
			bloom.locations.begin(), bloom.locations.end(),
			std::back_inserter(v_intersection));

	if (v_intersection.size())
	{
		cout << "Filter 1" << endl;
		for (std::set<ADDRINT>::iterator it = locations.begin();
				it != locations.end(); ++it)
			printf("%lX, ", *it);
		printf("\n");

		cout << "Filter 2" << endl;
		for (std::set<ADDRINT>::iterator it = bloom.locations.begin();
				it != bloom.locations.end(); ++it)
			printf("%lX, ", *it);
		printf("\n");

		printf("Same elements: \n");
		for (unsigned int i = 0; i < v_intersection.size(); i++)
		{
			printf("%lX, ", v_intersection[i]);
		}
		printf("\n");
		fflush(stdout);

		return true;
	}
	return false;
#endif
}

bool Bloom::check(const unsigned char *s)
{
#ifndef SET_OVERRIDE
	for(int n=0; n < nfuncs; ++n)
	{
		if(!(GETBIT(filter, funcs[n](s)%filterSize)))
		{
			return false;
		}
	}
	return true;
#else
	return locations.find(*((ADDRINT*) s)) != locations.end();
#endif
}

void Bloom::clear()
{
	memset(filter, 0, getFilterSizeInBytes());
	elementCount = 0;

#ifdef SET_OVERRIDE
	locations.clear();
#endif
}

bool Bloom::isEmpty()
{
	return elementCount == 0;
}

/**
 * Clear the filter between these positions
 */
void Bloom::clear(ADDRINT startAddress, ADDRINT endAddress)
{
	ADDRINT address = startAddress;
	for (; address < endAddress; address++)
	{
		remove(address);
	}
}
