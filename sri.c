#define _GNU_SOURCE
#include "common.h"
#include "boat.h"
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>

#define EPSILON 1e-9
#define WEIGHTS_BUFFERSIZE 20

void *workerThread(void *);
void sigpipe_handler(int);
void sigint_handler(int);

typedef struct WeightBuffer{
    double array[WEIGHTS_BUFFERSIZE];
    int index;
    double current_avg;
    int full;
    double item_ponderation;
    sem_t *mutex;
}WeightBuffer;

WeightBuffer *weights;
int min_latency;
int max_latency;

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

    weights = (WeightBuffer *)malloc(sizeof(WeightBuffer));
    weights->full = 0;
    weights->current_avg = -1;
    weights->item_ponderation = 1 / (double) WEIGHTS_BUFFERSIZE;
    weights->mutex = (sem_t *)malloc(sizeof(sem_t));
    sem_init(weights->mutex, 0, 1);

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
    char *port = "8080";
    listenfd = open_listenfd(port);

    if (listenfd < 0){
		connection_error(listenfd);
    }
    printf("Server listening on port %s.\n Press Ctrl+C to quit safely.\n", port);
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
    struct sigaction sa;
    sa.sa_handler = sigpipe_handler;
    sa.sa_flags = 0; // No special flags
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        pthread_exit(NULL);
    }
    Boat *currentBoat = (Boat *)malloc(sizeof(Boat));
    int dest_length;
    char *transaction = (char *)malloc(7*sizeof(char));
    transaction[0] = '\0';

    read(connfd, &(currentBoat->type), sizeof(BoatType));
    read(connfd, &(currentBoat->avg_weight), sizeof(double));
    read(connfd, &dest_length, sizeof(int));
    currentBoat->destination = (char *)malloc((dest_length + 1)*sizeof(char));
    read(connfd, currentBoat->destination, dest_length);
    currentBoat->destination[dest_length] = '\0';
    printf("\n*******************************************************\n");
    printf("Boat just arrived. Type: %d, avg weight: %f, destination: %s\n", currentBoat->type, currentBoat->avg_weight, currentBoat->destination);

    sem_wait(weights->mutex);
    printf("Current avg = %f\n", weights->current_avg);
    bool isConventional = currentBoat->type == CONVENTIONAL;
    bool toEcuador = strcmp(currentBoat->destination, "ecuador") == 0;
    bool exceedsWeight = (currentBoat->avg_weight - weights->current_avg) > EPSILON;
    bool checkBoat = isConventional && toEcuador && exceedsWeight;
    sem_post(weights->mutex);
    int latency = min_latency + rand() % (max_latency - min_latency + 1);
    sleep(latency); //simulate response latency
    if(checkBoat){
        write(connfd, "CHECK", 5);
    }
    else{
        write(connfd, "PASS", 4);
    }
    printf("Response sent to boat.\n");
    read(connfd, transaction, 6);
    transaction[7] = '\0';

    if(strcmp(transaction, "COMMIT") != 0){
        printf("Discarding...\n");
        close(connfd);
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
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
    sem_post(weights->mutex);
    printf("New data saved.\n");

    close(connfd);
    free(currentBoat->destination);
    free(currentBoat);
    return NULL;
}

void sigpipe_handler(int signum) {
    printf("Thread received SIGPIPE, exiting...\n");
    pthread_exit(NULL);
}

void sigint_handler(int signum) {
    printf("Closing gracefully...\n");
    sem_destroy(weights->mutex);
    free(weights->mutex);
    free(weights);
    exit(0);
}