/*
 * VectorClock.h
 *
 *  Created on: Apr 19, 2013
 *      Author: onder
 */

#ifndef VECTORCLOCK_H_
#define VECTORCLOCK_H_

#include "pin.H"

#define NON_THREAD_VECTOR_CLOCK -1

class VectorClock
{
private:
	UINT32* vc;
public:
	int threadId;
	static int totalProcessCount;
	static int totalDeletedLockCount;

	// constructors & destructor
	VectorClock();
	VectorClock(const VectorClock& copyVC);
	VectorClock(int processId);
	VectorClock(VectorClock& inClockPtr, int processId);
	~VectorClock();

	// actions
	void advance();
	void receiveAction(VectorClock& vectorClockReceived);
	void receiveActionFromSpecialPoint(VectorClock& vectorClockReceived,
			UINT32 specialPoint);
	void sendEvent();
	void set(int index, UINT32 value);
	UINT32 get();
	void clear();

	// happens-before functions
	bool happensBefore(const VectorClock& input); //OK
	bool happensBeforeSpecial(const VectorClock* input, UINT32 processId);
	bool areConcurrent(VectorClock& vectorClockReceived, ADDRINT processId);
	bool isUniqueValue(int processIdIn);

	// operators
	VectorClock& operator++(); //prefix increment ++vclock
	VectorClock operator++(int x); //postfix increment
	const VectorClock& operator=(const VectorClock& vcRight);
	bool operator==(const VectorClock &vRight);
	bool operator!=(const VectorClock &vRight);
	bool operator<(const VectorClock& vRight);
	bool operator<=(const VectorClock& vRight);
	friend ostream& operator<<(ostream& os, const VectorClock &v);

	// utilities
	bool isEmpty();
	void printVector(FILE* out);
	void toString();
};

#endif /* VECTORCLOCK_H_ */
