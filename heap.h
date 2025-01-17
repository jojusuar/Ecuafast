#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#define INITIAL_SIZE 5
#define EPSILON 1e-9
#define MAXHEAP_DATA "data/senae/maxHeapData.bin"
#define MINHEAP_DATA "data/senae/minHeapData.bin"
#define MAXHEAP_STRUCT "data/senae/maxHeapStruct.bin"
#define MINHEAP_STRUCT "data/senae/minHeapStruct.bin"

typedef struct
{
    int size;
    int max_capacity;
    double *data;
    bool isMax;
} Heap;

Heap *newHeap(bool isMax); 

void closeHeap(Heap *heap);

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