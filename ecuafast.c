#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "common.c"
#include "boat.h"

#define SRI_HOSTNAME "127.0.0.1"
#define SRI_PORT "8080"
#define SENAE_HOSTNAME "127.0.0.1"
#define SENAE_PORT "8081"
#define SUPERCIA_HOSTNAME "127.0.0.1"
#define SUPERCIA_PORT "8082"

void *askSRI();
void *askSENAE();
void *askSUPERCIA();

Boat *myBoat;


int main(int argc, char *argv[])
{   
    myBoat = (Boat *)malloc(sizeof(Boat));
    myBoat->type = PANAMAX;
    myBoat->avg_weight = 4.33;
    myBoat->destination = "USA";

    pthread_t sri_tid, senae_tid, supercia_tid;
    pthread_create(&sri_tid, NULL, askSRI, NULL);
    // pthread_create(&senae_tid, NULL, askSENAE, NULL);
    // pthread_create(&supercia_tid, NULL, askSUPERCIA, NULL);
    void *result;
    pthread_join(sri_tid, (void **)&result);
    free(result);
    free(myBoat);
    return 0;
}

void *askSRI()
{
    int connfd = open_clientfd(SRI_HOSTNAME, SRI_PORT);
    if (connfd < 0)
        connection_error(connfd);
    write(connfd, &(myBoat->type), sizeof(BoatType));
    write(connfd, &(myBoat->avg_weight), sizeof(float));
    int dest_length = strlen(myBoat->destination);
    write(connfd, &(dest_length), sizeof(int));
    write(connfd, myBoat->destination, dest_length);
    bool *result = (bool *)malloc(sizeof(bool));
    *result = true;
    return (void *)result;
}

void *askSENAE()
{
    int connfd = open_clientfd(SRI_HOSTNAME, SRI_PORT);
    if (connfd < 0)
        connection_error(connfd);
}

void *askSUPERCIA()
{
    int connfd = open_clientfd(SRI_HOSTNAME, SRI_PORT);
    if (connfd < 0)
        connection_error(connfd);
}