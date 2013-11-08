//
// YOOP1 - Yavuz's Object Oriented Programming Specification 1
//
// Author: Yavuz Köroğlu
// Last Update Date: 28.09.2013
// Release Date: 28.09.2013

#ifndef YOOP_H
#define YOOP_H

// Required for malloc, free, memmove, memcpy etc.
#include <stdlib.h>
#include <string.h>

// "_this" represents the current object.
void * ___this = NULL;

// Equivalent to "ObjectPtr->Member" in C++.
#define OOP_CALL(ObjectPtr, Member) (___this = ObjectPtr, ObjectPtr->Member)

// Equivalent to "Object.Member" in C++.
#define OOP_CALL2(Object, Member) (___this = &(Object), Object.Member)

// Equivalent to "new Object" in C++, BUT DOES NOT CALL the constructor.
#define NEW(ClassName) (ClassName *)malloc(sizeof(ClassName))

// Equivalent to "new [] Object" in C++, BUT DOES NOT CALL the constructors.
#define NEW_ARRAY(ClassName, ArraySize) \
    (ClassName *)malloc(ArraySize * sizeof(ClassName))
    
#endif
