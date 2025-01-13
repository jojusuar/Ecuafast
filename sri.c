#include "common.h"
#include "boat.h"
#include <pthread.h>
#include <semaphore.h>
#include <math.h>

#define EPSILON 1e-9
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
    weights->current_avg = INFINITY;
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

    bool checkBoat = false;
    sem_wait(weights->mutex);
    printf("Current avg: %f\n", weights->current_avg);
    bool isConventional = currentBoat->type == CONVENTIONAL;
    bool toEcuador = strcmp(currentBoat->destination, "ecuador") == 0;
    bool exceedsWeight = (currentBoat->avg_weight - weights->current_avg) > EPSILON;
    checkBoat = isConventional && toEcuador && exceedsWeight;
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
    sem_post(weights->mutex);

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