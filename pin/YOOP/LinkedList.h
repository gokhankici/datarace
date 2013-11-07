// This object is defined using YOOP Specification 1

#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include "UnaryNode.h"

// Required by YOOP1
#define THIS ((LinkedList *)_this)

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
    void (*delete)();
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
        int i;
        for(i = 0; i < index; i++)
            current = current->next;

        return current->object;
    }
    // If target is closer to "end"
    else if (index < THIS->size)
    {
        UnaryNode * current = THIS->end;
        int i;
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

void removeLinkedListObject(unsigned int index) 
{
    // If it is the first element
    if (index == 0)
    {
        UnaryNode * current = THIS->begin;
        THIS->begin = current->next;
        THIS->size--;
        OOP_CALL(current, delete());
    }
    // If the element is closer to "begin"
    else if (index < (THIS->size >> 1))
    {
        UnaryNode * current = THIS->begin;

        // Find the node
        int i;
        for(i = 0; i < index; i++)
            current = current->next;

        // Remove
        current->prev->next = current->next;
        current->next->prev = current->prev;
        THIS->size--;
        OOP_CALL(current, delete());
    }
    // If the element is closer to "end"
    else if (index < (THIS->size - 2))
    {
        UnaryNode * current = THIS->end;

        // Find the node
        int i;
        for (i = THIS->size - 1; i > index; i--)
            current = current->prev;

        // Remove
        current->prev->next = current->next;
        current->next->prev = current->prev;
        THIS->size--;
        OOP_CALL(current, delete());
    }
    // If it is the last element
    else if (index == (THIS->size - 1))
    {
        UnaryNode * current = THIS->end;
        THIS->end = current->prev;
        THIS->size--;
        OOP_CALL(current, delete());
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
        int i;
        for(i = 0; i < index; i++)
            current = current->next;

        current->object = object;
        current->delete = delFunc;
    }
    // If target is closer to "end"
    else if (index < THIS->size)
    {
        UnaryNode * current = THIS->end;
        int i;
        for(i = THIS->size - 1; i > index; i--)
            current = current->prev;

        current->object = object;
        current->delete = delFunc;
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
        int i;
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
        int i;
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

// Removes the LinkedList and frees the memory
void defaultDeleteLinkedList() 
{
    LinkedList * this = THIS;

    if (this->size > 0)
    {
        UnaryNode * current;
        for (current = this->begin->next;
             current != NULL;
             current = current->next)
        {
            OOP_CALL(current->prev, delete());
        }
        OOP_CALL(this->end, delete());
    }

    free(this);
}

// Initializes and returns a new LinkedList object
LinkedList * createLinkedList() 
{
    LinkedList * this = NEW(LinkedList);

    this->size = 0;
    this->get = getLinkedListObject;
    this->set = setLinkedListObject;
    this->add = addLinkedListObject;
    this->push = pushLinkedListObject;
    this->pop = popLinkedListObject;
    this->remove = removeLinkedListObject;
    this->insert = insertLinkedListObject;
    this->delete = defaultDeleteLinkedList;

    return this;
}

// Required by YOOP1
#undef THIS

#endif

