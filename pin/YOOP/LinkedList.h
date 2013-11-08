// This object is defined using YOOP Specification 1

#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdlib.h>
#include "Yoop.h"
#include "UnaryNode.h"
#include "LinkedList.h"
#include "Boolean.h"

// Required by YOOP1
#define THIS ((LinkedList *)___this)

// ******************
// *   CLASS BODY   *
// ******************

typedef struct LinkedListBody 
{
    // Members
    UnaryNode * begin;
    UnaryNode * end;
    unsigned int size;

    // Methods
    void * (*get)(unsigned int);
    void (*remove)(unsigned int);
    void (*add)(void *, void (*delFunc)());
    void (*push)(void *);
    void * (*pop)();
    void (*set)(unsigned int, void *, void (*delFunc)());
    void (*insert)(unsigned int, void *, void (*delFunc)());
    void (*myDelete)();
	Boolean (*contains)(int (*cmp)(void*, void*), void * data);
} LinkedList;

// ******************
// * IMPLEMENTATION *
// ******************

void * getLinkedListObject(unsigned int index) 
{
    // If target is closer to "begin"
    if (0 <= index && index <= (THIS->size >> 1))
    {
        UnaryNode * current = THIS->begin;
        unsigned int i;
        for(i = 0; i < index; i++)
            current = current->next;

        return current->object;
    }
    // If target is closer to "end"
    else if (index < THIS->size)
    {
        UnaryNode * current = THIS->end;
        unsigned int i;
        for(i = THIS->size - 1; i > index; i--)
            current = current->prev;

        return current->object;
    }
    // If target is out of bounds
    else
    {
        return NULL;
    }
}

Boolean containsLinkedListObject(int (*cmp)(void*, void*), void * data) 
{
	UnaryNode * itr;
	for (itr = THIS->begin; itr != NULL; itr = itr->next)
	{
		if(! cmp(data, itr))
		{
			return TRUE;
		}
	}
	return FALSE;
}

void removeLinkedListObject(unsigned int index) 
{
    // If it is the first element
    if (index == 0)
    {
        UnaryNode * current = THIS->begin;
        THIS->begin = current->next;
        THIS->size--;
        OOP_CALL(current, myDelete());
    }
    // If the element is closer to "begin"
    else if (index < (THIS->size >> 1))
    {
        UnaryNode * current = THIS->begin;

        // Find the node
        unsigned int i;
        for(i = 0; i < index; i++)
            current = current->next;

        // Remove
        current->prev->next = current->next;
        current->next->prev = current->prev;
        THIS->size--;
        OOP_CALL(current, myDelete());
    }
    // If the element is closer to "end"
    else if (index < (THIS->size - 2))
    {
        UnaryNode * current = THIS->end;

        // Find the node
        unsigned int i;
        for (i = THIS->size - 1; i > index; i--)
            current = current->prev;

        // Remove
        current->prev->next = current->next;
        current->next->prev = current->prev;
        THIS->size--;
        OOP_CALL(current, myDelete());
    }
    // If it is the last element
    else if (index == (THIS->size - 1))
    {
        UnaryNode * current = THIS->end;
        THIS->end = current->prev;
        THIS->size--;
        OOP_CALL(current, myDelete());
    }
    // If index is out of bounds
    else
    {
        // DO NOTHING
    }
}

void addLinkedListObject(void * object, void (*delFunc)()) 
{
    if (THIS->size == 0)
    {
        THIS->begin = THIS->end = createUnaryNode(NULL, object, NULL, delFunc);
    }
    else
    {
        createUnaryNode(THIS->end, object, NULL, delFunc);
        THIS->end = THIS->end->next;
    }
    THIS->size++;
}

void pushLinkedListObject(void * object) 
{
    if (THIS->size == 0)
    {
        THIS->begin = THIS->end
            = createUnaryNode(NULL, object, NULL, defaultDeleteUnaryNode);
    }
    else
    {
        createUnaryNode(THIS->end, object, NULL, defaultDeleteUnaryNode);
        THIS->end = THIS->end->next;
    }
    THIS->size++;
}

void * popLinkedListObject()
{
    if (THIS->size == 0)
    {
        return NULL;
    }
    else
    {
        void * object = THIS->get(THIS->size - 1);
        THIS->remove(THIS->size - 1);
        return object;
    }
}

void setLinkedListObject(unsigned int index, void * object, void (*delFunc)()) 
{
    // If target is closer to "begin"
    if (0 <= index && index <= (THIS->size >> 1))
    {
        UnaryNode * current = THIS->begin;
        unsigned int i;
        for(i = 0; i < index; i++)
            current = current->next;

        current->object = object;
        current->myDelete = delFunc;
    }
    // If target is closer to "end"
    else if (index < THIS->size)
    {
        UnaryNode * current = THIS->end;
        unsigned int i;
        for(i = THIS->size - 1; i > index; i--)
            current = current->prev;

        current->object = object;
        current->myDelete = delFunc;
    }
    // If target is out of bounds
    else
    {
        // DO NOTHING
    }
}

void insertLinkedListObject(unsigned int index, void * object, void (*delFunc)()) 
{
    // If target is the first element
    if (index == 0 && index < THIS->size)
    {
        createUnaryNode(NULL, object, THIS->begin, delFunc);
        THIS->begin = THIS->begin->prev;
        THIS->size++;
    }
    // If target is closer to "begin"
    else if (0 < index && index <= (THIS->size >> 1))
    {
        UnaryNode * current = THIS->begin;
        unsigned int i;
        for(i = 0; i < index; i++)
            current = current->next;

        UnaryNode * node = createUnaryNode(current->prev, object, current, delFunc);
        current->prev->next = node;
        current->prev = node;
        THIS->size++;
    }
    // If target is closer to "end"
    else if (index < THIS->size)
    {
        UnaryNode * current = THIS->end;
        unsigned int i;
        for(i = THIS->size - 1; i > index; i--)
            current = current->prev;

        current->object = object;

        UnaryNode * node = createUnaryNode(current->prev, object, current, delFunc);
        current->prev->next = node;
        current->prev = node;
        THIS->size++;
    }
    // If target is out of bounds
    else
    {
        THIS->add(object, delFunc);
    }
}

static void deleteNode(UnaryNode* node)
{
    if(node->prev != NULL)
        node->prev->next = node->next;

    if(node->next != NULL)
        node->next->prev = node->prev;

	node->myDelete();
}

void defaultDeleteLinkedListObject(int (*cmp)(void*, void*), void* data)
{
	UnaryNode * itr;
	for (itr = THIS->begin; itr != NULL; itr = itr->next)
	{
		if(! cmp(data, itr))
		{
			deleteNode(itr);
		}
	}
}

// Removes the LinkedList and frees the memory
void defaultDeleteLinkedList() 
{
    LinkedList * __this = THIS;

    if (__this->size > 0)
    {
        UnaryNode * current;
        for (current = __this->begin->next;
             current != NULL;
             current = current->next)
        {
            OOP_CALL(current->prev, myDelete());
        }
        OOP_CALL(__this->end, myDelete());
    }

    free(__this);
}

// Initializes and returns a new LinkedList object
LinkedList * createLinkedList() 
{
    LinkedList * __this = NEW(LinkedList);

    __this->size = 0;
    __this->get = getLinkedListObject;
    __this->set = setLinkedListObject;
    __this->add = addLinkedListObject;
    __this->push = pushLinkedListObject;
    __this->pop = popLinkedListObject;
    __this->remove = removeLinkedListObject;
    __this->insert = insertLinkedListObject;
    __this->myDelete = defaultDeleteLinkedList;
	__this->contains = containsLinkedListObject;

    return __this;
}

// Required by YOOP1
#undef THIS

#endif

