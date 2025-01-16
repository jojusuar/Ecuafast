#include "heap.h"

Heap *newHeap(bool isMax)
{
    Heap *heap = (Heap *)malloc(sizeof(Heap));
    heap->size = 0;
    heap->max_capacity = INITIAL_SIZE;
    heap->data = (double *)malloc(INITIAL_SIZE * sizeof(double));
    heap->isMax = isMax;
    return heap;
}

void destroyHeap(Heap *heap)
{
    free(heap->data);
    free(heap);
}

bool isFull(Heap *heap)
{
    return heap->size == heap->max_capacity;
}

bool isEmpty(Heap *heap)
{
    return heap->size == 0;
}

void addCapacity(Heap *heap)
{
    heap->max_capacity *= 2;
    heap->data = (double *)realloc(heap->data, heap->max_capacity * sizeof(double));
}


void swap(double *a, double *b)
{
    double temp = *a;
    *a = *b;
    *b = temp;
}


void bubbleUp(Heap *heap, int index)
{
    int parent = (index - 1) / 2;
    while (index > 0 && ((!heap->isMax && ((heap->data[index] - heap->data[parent]) < EPSILON)) || (heap->isMax && ((heap->data[index] - heap->data[parent]) > EPSILON))))
    {
        swap(&heap->data[index], &heap->data[parent]);
        index = parent;
        parent = (index - 1) / 2;
    }
}


void insert(Heap *heap, double value)
{
    if (isFull(heap))
    {
        addCapacity(heap);
    }
    heap->data[heap->size] = value; 
    bubbleUp(heap, heap->size);     
    heap->size++;
}


void bubbleDown(Heap *heap, int index)
{
    int leftChild, rightChild, smallest, biggest, temp;
    while (1)
    {
        leftChild = 2 * index + 1;
        rightChild = 2 * index + 2;
        if (heap->isMax)
        {
            biggest = index;
            if(leftChild < heap->size && (EPSILON > (heap->data[biggest] - heap->data[leftChild])))
            {
                biggest = leftChild;
            }
            if(rightChild < heap->size && (EPSILON > (heap->data[biggest] - heap->data[rightChild])))
            {
                biggest = rightChild;
            }
            if(biggest != index)
            {
                swap(&heap->data[index], &heap->data[biggest]);
                index = biggest;
            }
            else
            {
                break;
            }
        }
        else
        {
            smallest = index;
            if (leftChild < heap->size && ((heap->data[leftChild] - heap->data[smallest]) < EPSILON))
            {
                smallest = leftChild;
            }
            if (rightChild < heap->size && ((heap->data[rightChild] -heap->data[smallest]) < EPSILON))
            {
                smallest = rightChild;
            }
            if (smallest != index)
            {
                swap(&heap->data[index], &heap->data[smallest]);
                index = smallest;
            }
            else
            {
                break;
            }
        }
    }
}


double pop(Heap *heap)
{
    if (heap->size == 0)
    {
        printf("Heap is empty!\n");
        return -1;
    }
    double root = heap->data[0];
    heap->data[0] = heap->data[heap->size - 1];
    heap->size--;
    bubbleDown(heap, 0);
    return root;
}

double peek(Heap *heap)
{
    if (heap->size == 0)
    {
        printf("Heap is empty!\n");
        return -1;
    }
    return heap->data[0];
}


void printHeap(Heap *heap)
{
    for (int i = 0; i < heap->size; i++)
    {
        printf("%.2f ", heap->data[i]);
    }
    printf("\n");
}