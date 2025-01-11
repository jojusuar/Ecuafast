#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <getopt.h>
#include <ctype.h>
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
    char *tvalue = NULL;
    char *wvalue = NULL;
    char *dvalue = NULL;
    bool tflag = false;
    bool wflag = false;
    bool dflag = false;
    int index;
    int c;

    opterr = 0;
    while ((c = getopt (argc, argv, "t:w:d:")) != -1){
        switch (c)
        {
        case 't':
            tvalue = optarg;
            tflag = true;
            break;
        case 'w':
            wvalue = optarg;
            wflag = true;
            break;
        case 'd':
            dvalue = optarg;
            dflag = true;
            break;
        case 'h':
            printf("Usage: %s [-t] <load type> [-w] <avg. weight> [-d] <destination>\n", argv[0]);
            printf("    -h:             Shows this message.\n");
            return 0;
        case '?':
            if (optopt == 't')
            fprintf (stderr, "-%c requires an argument.\n", optopt);
            if (optopt == 'w')
            fprintf (stderr, "-%c requires an argument.\n", optopt);
            if (optopt == 'd')
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

    myBoat = (Boat *)malloc(sizeof(Boat));
    if(tflag){
        switch(atoi(tvalue)){
            case 1:
                myBoat->type = CONVENTIONAL;
                break;
            case 2:
                myBoat->type = PANAMAX;
                break;
            default:
                free(myBoat);
                fprintf(stderr, "Invalid boat type.\n");
                return 1;
        }
    }
    if(wflag){
        myBoat->avg_weight = atof(wvalue);
    }
    if(dflag){
        for (int i = 0; dvalue[i]; i++) {
            dvalue[i] = tolower((unsigned char)dvalue[i]);
        }
        myBoat->destination = dvalue;
    }

    pthread_t sri_tid, senae_tid, supercia_tid;
    pthread_create(&sri_tid, NULL, askSRI, NULL);
    // pthread_create(&senae_tid, NULL, askSENAE, NULL);
    // pthread_create(&supercia_tid, NULL, askSUPERCIA, NULL);
    char *result;
    pthread_join(sri_tid, (void **)&result);
    printf("SRI response: %s\n", result);
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
    char *response = (char *)malloc(5*sizeof(char));
    ssize_t bytes_read = read(connfd, response, 5);
    response[bytes_read] = '\0';
    close(connfd);
    return (void *)response;
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