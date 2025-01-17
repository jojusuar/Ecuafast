#include "heap.h"

Heap *newHeap(bool isMax)
{   
    char *heapfile = isMax ? MAXHEAP_STRUCT : MINHEAP_STRUCT;
    bool cleanstruct = access(heapfile, F_OK) != 0;
    int structfd = open(heapfile, O_RDWR | O_CREAT, 0666);
    if (ftruncate(structfd, sizeof(Heap)) == -1) {
        perror("Failed to set heap struct file size");
        close(structfd);
        return NULL;
    }
    Heap *heap = mmap(NULL, sizeof(Heap), PROT_READ | PROT_WRITE, MAP_SHARED, structfd, 0);
    close(structfd); 
    if(cleanstruct){
        heap->size = 0;
        heap->max_capacity = INITIAL_SIZE;
        heap->isMax = isMax;
    }
    char *datafile = isMax ? MAXHEAP_DATA : MINHEAP_DATA;
    int datafd = open(datafile, O_RDWR | O_CREAT, 0666);
    if (ftruncate(datafd, heap->max_capacity * sizeof(double)) == -1) {
        perror("Failed to set heap data file size");
        close(datafd);
        return NULL;
    }
    heap->data = mmap(NULL, heap->max_capacity * sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, datafd, 0);
    return heap;
}

void closeHeap(Heap *heap)
{
    if (heap == NULL) {
        return;
    }
    if (heap->data != NULL) {
        munmap(heap->data, heap->max_capacity * sizeof(double));
    }
    munmap(heap, sizeof(Heap));
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
   if (heap == NULL || heap->data == NULL) {
        fprintf(stderr, "Invalid heap or data pointer.\n");
        return;
    }
    heap->max_capacity *= 2;
    const char *datafile = heap->isMax ? MAXHEAP_DATA : MINHEAP_DATA;
    int datafd = open(datafile, O_RDWR);
    if (datafd == -1) {
        perror("Failed to open data file for resizing");
        return;
    }
    if (ftruncate(datafd, heap->max_capacity * sizeof(double)) == -1) {
        perror("Failed to resize data file");
        close(datafd);
        return;
    }
    heap->data = mremap(heap->data, (heap->max_capacity / 2) * sizeof(double), 
                        heap->max_capacity * sizeof(double), MREMAP_MAYMOVE);
    if (heap->data == MAP_FAILED) {
        perror("Failed to remap data");
        close(datafd);
        return;
    }
    close(datafd);
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
    while (index > 0 && ((!heap->isMax && ((heap->data[index] - heap->data[parent]) < EPSILON)) || (heap->isMax && (EPSILON > (heap->data[parent] - heap->data[index])))))
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
            if (rightChild < heap->size && ((heap->data[rightChild] - heap->data[smallest]) < EPSILON))
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