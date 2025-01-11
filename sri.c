#include "common.h"
#include "boat.h"
#include <pthread.h>
#include <semaphore.h>

#define WEIGHTS_BUFFERSIZE 20

void *workerThread(void *arg);

typedef struct WeightBuffer{
    float array[WEIGHTS_BUFFERSIZE];
    bool full;
    int index;
    float last_avg;
    float item_ponderation;
    sem_t *mutex;
}WeightBuffer;

WeightBuffer *weights;

int main(int argc, char* argv[]){

    weights = (WeightBuffer *)malloc(sizeof(WeightBuffer));
    weights->full = false;
    weights->index = 0;
    float last_avg = -1;
    float item_ponderation = 1 / WEIGHTS_BUFFERSIZE;
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
    weights->array[weights->index] = currentBoat->avg_weight;
    if(weights->full){ //fast recalculation
        weights->last_avg = weights->last_avg -( weights->array[weights->index] * weights->item_ponderation) + (currentBoat->avg_weight * weights->item_ponderation);
    }
    else{
        if(weights->index == WEIGHTS_BUFFERSIZE-1){
            weights->full = true;
        }
        float sum = 0;
        for(int i = 0; i <= weights->index; i++){ //slow initial calculation
            sum += weights->array[i];
        }
        weights->last_avg = sum / (weights->index + 1);
    }
    weights->index = (weights->index + 1) % WEIGHTS_BUFFERSIZE;
    printf("Current avg weight: %f\n", weights->last_avg);
    sem_post(weights->mutex);

    printf("Type: %d, avg weight: %f, destination: %s\n", currentBoat->type, currentBoat->avg_weight, currentBoat->destination);

    free(currentBoat->destination);
    free(currentBoat);
    return NULL;
}