#define _GNU_SOURCE
#include "common.h"
#include "boat.h"
#include <pthread.h>
#include <semaphore.h>
#include "heap.h"
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <ctype.h>

#define EPSILON 1e-9

typedef struct{
    Heap *minHeap;
    Heap *maxHeap;
    int totalSize;
    sem_t mutex;
}SENAEBuffer;

SENAEBuffer *buffer;
int min_latency;
int max_latency;

void *workerThread(void *arg);
double get3rdQuartile(SENAEBuffer *buffer);
void rebalanceHeaps(SENAEBuffer *buffer);
void sigpipe_handler(int);
void sigint_handler(int);

int main(int argc, char* argv[]){
    char *fvalue = NULL;
    char *tvalue = NULL;
    bool fflag = false;
    bool tflag = false;
    int index;
    int c;

    opterr = 0;
    while ((c = getopt (argc, argv, "f:t:")) != -1){
        switch (c)
        {
        case 'f':
            fvalue = optarg;
            fflag = true;
            break;
        case 't':
            tvalue = optarg;
            tflag = true;
            break;
        case 'h':
            printf("Usage: %s -f <response latency floor> -t <response latency top>\n", argv[0]);
            printf("    -h:             Shows this message.\n");
            return 0;
        case '?':
            if (optopt == 'f')
            fprintf (stderr, "-%c requires an argument.\n", optopt);
            if (optopt == 't')
            fprintf (stderr, "-%c requires an argument.\n", optopt);
            else if (isprint (optopt))
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
            fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
            return 1;
        }
    }
    if(fflag){
        min_latency = atoi(fvalue);
    }
    else{
        fprintf(stderr, "Usage: %s -f <response latency floor (seconds)> -t <response latency top (seconds)>\n", argv[0]);
        return 1;
    }
    if(tflag){
        max_latency = atoi(tvalue);
    }
    else{
        fprintf(stderr, "Usage: %s -f <response latency floor (seconds)> -t <response latency top (seconds)>\n", argv[0]);
        return 1;
    }

    buffer = (SENAEBuffer *)malloc(sizeof(SENAEBuffer));
    buffer->totalSize = 0;
    buffer->minHeap = newHeap(false);
    buffer->maxHeap = newHeap(true);
    sem_init(&(buffer->mutex), 0, 1);

    struct sigaction sigintAction;
    sigintAction.sa_handler = sigint_handler;
    sigintAction.sa_flags = 0; // No special flags
    sigemptyset(&sigintAction.sa_mask);
    if (sigaction(SIGINT, &sigintAction, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

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
    struct sigaction sa;
    sa.sa_handler = sigpipe_handler;
    sa.sa_flags = 0; // No special flags
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        pthread_exit(NULL);
    }
    int latency = min_latency + rand() % (max_latency - min_latency + 1);
    Boat *currentBoat = (Boat *)malloc(sizeof(Boat));
    int dest_length;
    bool checkBoat = false;
    char *transaction = (char *)malloc(7*sizeof(char));
    transaction[0] = '\0';
    
    read(connfd, &(currentBoat->type), sizeof(BoatType));
    read(connfd, &(currentBoat->avg_weight), sizeof(float));
    read(connfd, &dest_length, sizeof(int));
    currentBoat->destination = (char *)malloc((dest_length + 1)*sizeof(char));
    read(connfd, currentBoat->destination, dest_length);
    currentBoat->destination[dest_length] = '\0';
    printf("Type: %d, avg weight: %f, destination: %s, flag: %d\n", currentBoat->type, currentBoat->avg_weight, currentBoat->destination, checkBoat);
    
    sem_wait(&(buffer->mutex));
    double thirdQ = get3rdQuartile(buffer);
    printf("[");
    printHeap(buffer->minHeap);
    printHeap(buffer->maxHeap);
    printf("]\n");
    printf("Q3: %f\n", thirdQ);
    bool overweight  = (currentBoat->avg_weight - thirdQ) > EPSILON;
    sem_post(&(buffer->mutex));
    bool isPanamax = currentBoat->type == PANAMAX;
    bool toTarget = strcmp(currentBoat->destination, "usa") == 0 || strcmp(currentBoat->destination, "europa") == 0 || strcmp(currentBoat->destination, "europe") == 0;
    checkBoat = overweight && isPanamax && toTarget;
    sleep(latency); //simulate response latency
    if(checkBoat){
        write(connfd, "CHECK", 5);
    }
    else{
        write(connfd, "PASS", 4);
    }
    printf("Sent response to boat.\n");
    read(connfd, transaction, 6);
    transaction[7] = '\0';
    
    if(strcmp(transaction, "COMMIT") != 0){
        printf("Discarding...\n");
        close(connfd);
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
    sem_wait(&(buffer->mutex));
    if (!isEmpty(buffer->minHeap) && currentBoat->avg_weight >= peek(buffer->minHeap)) {
        insert(buffer->minHeap, currentBoat->avg_weight);
    } else {
        insert(buffer->maxHeap, currentBoat->avg_weight);
    }
    rebalanceHeaps(buffer);
    buffer->totalSize++;
    sem_post(&(buffer->mutex));
    printf("New data buffered.\n");

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

void sigpipe_handler(int signum) {
    printf("Thread received SIGPIPE, exiting...\n");
    pthread_exit(NULL);
}

void sigint_handler(int signum) {
    printf("Closing gracefully...\n");
    destroyHeap(buffer->minHeap);
    destroyHeap(buffer->maxHeap);
    sem_destroy(&(buffer->mutex));
    free(buffer);
    exit(0);
}