#include "common.h"
#include "boat.h"
#include <pthread.h>
#include <semaphore.h>
#include "heap.h"
#include <math.h>

#define EPSILON 1e-9

typedef struct{
    Heap *minHeap;
    Heap *maxHeap;
    int totalSize;
    sem_t mutex;
}SENAEBuffer;

SENAEBuffer *buffer;

void *workerThread(void *arg);
double get3rdQuartile(SENAEBuffer *buffer);
void rebalanceHeaps(SENAEBuffer *buffer);

int main(int argc, char* argv[]){
    buffer = (SENAEBuffer *)malloc(sizeof(SENAEBuffer));
    buffer->totalSize = 0;
    buffer->minHeap = newHeap(false);
    buffer->maxHeap = newHeap(true);
    sem_init(&(buffer->mutex), 0, 1);

    int listenfd;
    unsigned int clientlen;
    struct sockaddr_in clientaddr;
    char *port = "8081";
    listenfd = open_listenfd(port);

    if (listenfd < 0){
		connection_error(listenfd);
    }
    printf("Server listening on port %s.\n", port);
    pthread_t tid;
    while(true){
        clientlen = sizeof(clientaddr);
        int *connfd = (int *)malloc(sizeof(int));
		*connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, workerThread, (void *)connfd);
    }
    destroyHeap(buffer->minHeap);
    destroyHeap(buffer->maxHeap);
    sem_destroy(&(buffer->mutex));
    free(buffer);
    return 0;
}

void *workerThread(void *arg){
    pthread_detach(pthread_self());
    int connfd = *(int *)arg;
    free(arg);
    Boat *currentBoat = (Boat *)malloc(sizeof(Boat));
    int dest_length;
    read(connfd, &(currentBoat->type), sizeof(BoatType));
    read(connfd, &(currentBoat->avg_weight), sizeof(float));
    read(connfd, &dest_length, sizeof(int));
    currentBoat->destination = (char *)malloc((dest_length + 1)*sizeof(char));
    read(connfd, currentBoat->destination, dest_length);
    currentBoat->destination[dest_length] = '\0';

    bool checkBoat = false;

    sem_wait(&(buffer->mutex));
    double thirdQ = get3rdQuartile(buffer);
    printf("[");
    printHeap(buffer->minHeap);
    printHeap(buffer->maxHeap);
    printf("]\n");
    printf("Q3: %f\n", thirdQ);
    bool overweight  = (currentBoat->avg_weight - thirdQ) > EPSILON;
    bool isPanamax = currentBoat->type == PANAMAX;
    bool toTarget = strcmp(currentBoat->destination, "usa") == 0 || strcmp(currentBoat->destination, "europa") == 0 || strcmp(currentBoat->destination, "europe") == 0;
    checkBoat = overweight && isPanamax && toTarget;
    if (!isEmpty(buffer->minHeap) && currentBoat->avg_weight >= peek(buffer->minHeap)) {
        insert(buffer->minHeap, currentBoat->avg_weight);
    } else {
        insert(buffer->maxHeap, currentBoat->avg_weight);
    }
    rebalanceHeaps(buffer);
    buffer->totalSize++;
    sem_post(&(buffer->mutex));

    if(checkBoat){
        write(connfd, "CHECK", 5);
    }
    else{
        write(connfd, "PASS", 4);
    }
    printf("Type: %d, avg weight: %f, destination: %s, flag: %d\n", currentBoat->type, currentBoat->avg_weight, currentBoat->destination, checkBoat);
    
    close(connfd);
    free(currentBoat->destination);
    free(currentBoat);
    return NULL;
}


double get3rdQuartile(SENAEBuffer *buffer){
    if(buffer->totalSize < 3){
        return -1;
    }
    double ponderation = 0.75 * (buffer->totalSize + 1);
    if(ponderation >= (buffer->maxHeap->size + 1)){
        return peek(buffer->minHeap);
    }
    double decimal = ponderation - (int)ponderation;
    return peek(buffer->maxHeap) + decimal * (peek(buffer->minHeap) - peek(buffer->maxHeap));
}


void rebalanceHeaps(SENAEBuffer *buffer){
    while (buffer->maxHeap->size > ((buffer->totalSize + 1) * 3 / 4)) {
        int root = pop(buffer->maxHeap);
        insert(buffer->minHeap, root);
    }
    while (buffer->minHeap->size > ((buffer->totalSize + 1) / 4)) {
        int root = pop(buffer->minHeap);
        insert(buffer->maxHeap, root);
    }
}