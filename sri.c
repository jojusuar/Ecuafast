#include "common.h"
#include "boat.h"
#include <pthread.h>

void *workerThread(void *arg);

int main(int argc, char* argv[]){
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

    printf("Type: %d, avg weight: %f, destination: %s\n", currentBoat->type, currentBoat->avg_weight, currentBoat->destination);
    free(currentBoat->destination);
    free(currentBoat);
    return NULL;
}