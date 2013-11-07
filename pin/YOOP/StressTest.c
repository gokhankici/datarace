#include <stdio.h>
#include "Yoop.h"
#include "ArrayList.h"
#include "LinkedList.h"
#include "StopWatch.h"

StopWatch * myTimer = NULL;

void intro() {
    printf("\n");
    printf(" ArrayList vs LinkedList\n");
    printf(" Yavuz Köroğlu, 2013\n\n");
}

void initTimer() {
    myTimer = createStopWatch();
}

void testStopWatch() {
    printf("Testing StopWatch...\n");
    OOP_CALL(myTimer, start());
    sleep(5);
    OOP_CALL(myTimer,stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));
    printf("\n");
}

void testArrayList(unsigned int testSize) {
    printf("Testing ArrayList...\n");
    
    ArrayList * myList = createArrayList(testSize);

    printf("Initialize and Fill : ");
    OOP_CALL(myTimer, start());
    int i;
    for (i = 0; i < testSize; i++) 
    {
        OOP_CALL(myList, add(NEW(int), aggressiveDeleteContainer));
    }
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));
    
    printf("Random Access       : ");
    OOP_CALL(myTimer, start());
    for (i = 0; i < testSize; i = (i%2) ? (i+5) : (i-3)) 
    {
        OOP_CALL(myList, get(i));
    }
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));    

    printf("Random Removal      : ");
    OOP_CALL(myTimer, start());
    for (i = 0; i < testSize; i = (i%2) ? (i+5) : (i-3)) 
    {
        OOP_CALL(myList, remove(i));
    }
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));

    printf("Random Insertion    : ");
    OOP_CALL(myTimer, start());
    for (i = 0; i < testSize; i = (i%2) ? (i+5) : (i-3)) 
    {
        OOP_CALL(myList, insert(i, NEW(int), aggressiveDeleteContainer));
    }
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));
 
    printf("Disposal            : ");
    OOP_CALL(myTimer, start());
    OOP_CALL(myList, delete());
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));
    
    printf("\n");
}

void testLinkedList(unsigned int testSize) {
    printf("Testing LinkedList...\n");
    
    LinkedList * myList = createLinkedList();

    printf("Initialize and Fill : ");
    OOP_CALL(myTimer, start());
    int i;
    for (i = 0; i < testSize; i++) 
    {
        OOP_CALL(myList, add(NEW(int), aggressiveDeleteUnaryNode));
    }
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));
    
    printf("Random Access       : ");
    OOP_CALL(myTimer, start());
    for (i = 0; i < testSize; i = (i%2) ? (i+5) : (i-3)) 
    {
        OOP_CALL(myList, get(i));
    }
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));    

    printf("Random Removal      : ");
    OOP_CALL(myTimer, start());
    for (i = 0; i < testSize; i = (i%2) ? (i+5) : (i-3)) 
    {
        OOP_CALL(myList, remove(i));
    }
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));

    printf("Random Insertion    : ");
    OOP_CALL(myTimer, start());
    for (i = 0; i < testSize; i = (i%2) ? (i+5) : (i-3)) 
    {
        OOP_CALL(myList, insert(i, NEW(int), aggressiveDeleteUnaryNode));
    }
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));
 
    printf("Disposal            : ");
    OOP_CALL(myTimer, start());
    OOP_CALL(myList, delete());
    OOP_CALL(myTimer, stop());
    printf("%.2lf seconds\n", OOP_CALL(myTimer, diff()));
    
    printf("\n");
}

int main(int argc, char * argv[]) {
    int testSize = (argc < 2) ? 12000000 : atoi(argv[1]);
    intro();
    initTimer();
    testStopWatch();
    testArrayList(testSize);
    testLinkedList(testSize);

    return 0;
}
