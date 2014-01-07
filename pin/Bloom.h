#ifndef __BLOOM_H__
#define __BLOOM_H__

#include "pin.H"
#include <stdlib.h>

#include <vector>
#include <set>
#include "MyFlags.h"

#define DEFAULT_BLOOM_FILTER_SIZE 2048
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
	void remove(ADDRINT removedAddress);
	void clear(ADDRINT startAddress, ADDRINT endAddress);
	bool isEmpty();
	void print(FILE* out);

	friend ostream& operator<<(ostream& os, const Bloom &v);

	const unsigned char* getFilter()
	{
		return filter;
	}

	int getFilterSize()
	{
		return filterSize;
	}

	int getFilterSizeInBytes() const
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
