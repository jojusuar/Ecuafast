#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define INITIAL_SIZE 100

typedef struct
{
    int size;
    int max_capacity;
    int *data;
    bool isMax;
} Heap;

Heap *newHeap(bool isMax); 

void destroyHeap(Heap *heap);

bool isFull(Heap *heap);

bool isEmpty(Heap *heap);

void addCapacity(Heap *heap);

void swap(int *a, int *b);

void bubbleUp(Heap *heap, int index);

void insert(Heap *heap, int value);

void bubbleDown(Heap *heap, int index);

int pop(Heap *heap);

int peek(Heap *heap);

void printHeap(Heap *heap);