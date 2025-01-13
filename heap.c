#include "heap.h"

Heap *newHeap(bool isMax)
{
    Heap *heap = (Heap *)malloc(sizeof(Heap));
    heap->size = 0;
    heap->max_capacity = INITIAL_SIZE;
    heap->data = (int *)malloc(INITIAL_SIZE * sizeof(int));
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
    heap->data = (int *)realloc(heap->data, heap->max_capacity * sizeof(int));
}


void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}


void bubbleUp(Heap *heap, int index)
{
    int parent = (index - 1) / 2;
    while (index > 0 && ((!heap->isMax && heap->data[index] < heap->data[parent]) || (heap->isMax && heap->data[index] > heap->data[parent])))
    {
        swap(&heap->data[index], &heap->data[parent]);
        index = parent;
        parent = (index - 1) / 2;
    }
}


void insert(Heap *heap, int value)
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
            if(leftChild < heap->size && heap->data[leftChild] > heap->data[biggest])
            {
                biggest = leftChild;
            }
            if(rightChild < heap->size && heap->data[rightChild] > heap->data[biggest])
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
            if (leftChild < heap->size && heap->data[leftChild] < heap->data[smallest])
            {
                smallest = leftChild;
            }
            if (rightChild < heap->size && heap->data[rightChild] < heap->data[smallest])
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


int pop(Heap *heap)
{
    if (heap->size == 0)
    {
        printf("Heap is empty!\n");
        return -1;
    }
    int root = heap->data[0];
    heap->data[0] = heap->data[heap->size - 1];
    heap->size--;
    bubbleDown(heap, 0);
    return root;
}

int peek(Heap *heap)
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
        printf("%d ", heap->data[i]);
    }
    printf("\n");
}