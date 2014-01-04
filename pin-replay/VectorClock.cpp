#include "VectorClock.h"




VectorClock::VectorClock()
{
	threadId=-1;
	v=new size_t[MAX_THREAD_COUNT];
}


/*
 * Create vector clock for each thread
 * */
VectorClock::VectorClock(uint32_t threadIdIn)
{
	threadId=threadIdIn;
	v=new size_t[MAX_THREAD_COUNT];
	v[threadId]=1;
}


VectorClock::VectorClock(VectorClock* in, uint32_t threadIdIn)
{
	threadId=threadIdIn;
	v=new size_t[MAX_THREAD_COUNT];
	size_t * src=in->getValues();
	copy(src,src+MAX_THREAD_COUNT,v);
}

VectorClock::~VectorClock()
{
	delete[] v;
}

size_t * VectorClock::getValues() const
{
	return v;
}

void VectorClock::tick()
{
	++v[threadId];
}

void VectorClock::sendEvent()
{
	tick();
}

void VectorClock::receiveAction(VectorClock* vectorClockReceived)
{
	size_t *vOfReceivedClock = vectorClockReceived->getValues();
	for (int i = 0; i <MAX_THREAD_COUNT; ++i)
		v[i] = ( v[i] > vOfReceivedClock[i] ) ? v[i] : vOfReceivedClock[i];

}


bool VectorClock::happensBefore(VectorClock* input)
{
		return  (*this < *input);
}

bool VectorClock::areConcurrent(VectorClock* input, uint32_t threadIdIn)
{
	if  (!this->happensBefore(input) &&  !input->happensBefore(this))
		return true;

	return false;
}

VectorClock& VectorClock::operator++() //++v;
{
	++v[threadId];
	return *this;
}
VectorClock VectorClock::operator++(int) //v++;
{
	VectorClock tmp = *this;
	v[threadId]++;
	return tmp;
}


bool VectorClock::operator!=(const VectorClock& vRight)
{
	return !(operator==(vRight));
}

bool VectorClock::operator==(const VectorClock& vRight)
{
	size_t* vRightValues=vRight.getValues();
	for (int i = 0; i <MAX_THREAD_COUNT; ++i)
		if (v[i] != vRightValues[i])
			return false;
	return true;
}


bool VectorClock::operator<=(const VectorClock& vRight)
{
	if (operator<(vRight)  || operator==(vRight) )
		return true;
	return false;
}

bool VectorClock::operator<(const VectorClock& vRight)
{
	bool strictlySmaller = false;
	size_t* vRightValues = vRight.getValues();
	for (int i = 0; i < MAX_THREAD_COUNT; ++i)
	{
		if ( v[i] < vRightValues[i] )		//at least ONE value is stricly smaller
			strictlySmaller = true;
		else if (v[i] > vRightValues[i])  	//if any value of v[i] is greater, than no way v<vRight
			 return false;
	}
	return strictlySmaller;
}

ostream& operator<<(ostream& os, const VectorClock& vRight)
{
	os << "Vector Clock Of " << vRight.threadId<< ":" << endl;
	size_t *values = vRight.getValues();
	for (int i = 0; i < MAX_THREAD_COUNT; ++i)
	{

		os << setw(33) << values[i]; //
		if ( (i +1) % 4 == 0)
			os << endl;
	}

	return os;
}

