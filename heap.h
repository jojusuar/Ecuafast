#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define INITIAL_SIZE 100
#define EPSILON 1e-9

typedef struct
{
    int size;
    int max_capacity;
    double *data;
    bool isMax;
} Heap;

Heap *newHeap(bool isMax); 

void destroyHeap(Heap *heap);

bool isFull(Heap *heap);

bool isEmpty(Heap *heap);

void addCapacity(Heap *heap);

void swap(double *a, double *b);

void bubbleUp(Heap *heap, int index);

void insert(Heap *heap, double value);

void bubbleDown(Heap *heap, int index);

double pop(Heap *heap);

double peek(Heap *heap);

void printHeap(Heap *heap);