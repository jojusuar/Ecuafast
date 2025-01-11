#include "common.h"
#include "boat.h"
#include <pthread.h>
#include <semaphore.h>

#define WEIGHTS_BUFFERSIZE 20

void *workerThread(void *arg);

typedef struct WeightBuffer{
    double array[WEIGHTS_BUFFERSIZE];
    int index;
    double current_avg;
    int full;
    double item_ponderation;
    sem_t *mutex;
}WeightBuffer;

WeightBuffer *weights;

int main(int argc, char* argv[]){

    weights = (WeightBuffer *)malloc(sizeof(WeightBuffer));
    weights->full = 0;
    weights->current_avg = 0;
    weights->item_ponderation = 1 / (double) WEIGHTS_BUFFERSIZE;
    weights->mutex = (sem_t *)malloc(sizeof(sem_t));
    sem_init(weights->mutex, 0, 1);

    int listenfd;
    unsigned int clientlen;
    struct sockaddr_in clientaddr;
    char *port = "8080";
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
    sem_destroy(weights->mutex);
    free(weights->mutex);
    free(weights);
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

    sem_wait(weights->mutex);
    if(weights->full == 0){
        weights->current_avg = currentBoat->avg_weight;
        weights->full++;
    }
    else if(weights->full < WEIGHTS_BUFFERSIZE){
        weights->current_avg = (weights->current_avg * weights->full + currentBoat->avg_weight)/(weights->full + 1);
        weights->full++;
    }
    else{ //fast as hell recalculation
        weights->current_avg = weights->current_avg + weights->item_ponderation * (currentBoat->avg_weight - weights->array[weights->index]);
    }
    weights->array[weights->index] = currentBoat->avg_weight;
    weights->index = (weights->index + 1) % WEIGHTS_BUFFERSIZE;
    printf("Current avg weight: %f\n", weights->current_avg);
    sem_post(weights->mutex);

    printf("Type: %d, avg weight: %f, destination: %s\n", currentBoat->type, currentBoat->avg_weight, currentBoat->destination);

    free(currentBoat->destination);
    free(currentBoat);
    return NULL;
}