#ifndef __BLOOM_H__
#define __BLOOM_H__

#include "pin.H"
#include <stdlib.h>

#include <vector>
#include <set>

#define DEFAULT_BLOOM_FILTER_SIZE 10240000
#define ADDR_SIZE 8
#define BLOOM_ADDR(address) ((const unsigned char*) &address)
#define SET_OVERRIDE

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
	void remove(ADDRINT removedAddress);
	void clear(ADDRINT startAddress, ADDRINT endAddress);
	bool isEmpty();

	const unsigned char* getFilter()
	{
		return filter;
	}
	int getFilterSize()
	{
		return filterSize;
	}
	int getFilterSizeInBytes()
	{
		return (filterSize + 7) / 8;
	}

private:
	int elementCount;
	int filterSize;
	unsigned char *filter;
	int nfuncs;
	hashfunc_t *funcs;

#ifdef SET_OVERRIDE
	std::set<ADDRINT> locations;
#endif
};

#endif
