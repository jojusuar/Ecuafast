#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <getopt.h>
#include <ctype.h>
#include "common.c"
#include "boat.h"

#define SRI_HOSTNAME "127.0.0.1" // these TCP/IP addresses could be changed to the actual IPs of each institution's server
#define SRI_PORT "8080"
#define SENAE_HOSTNAME "127.0.0.1"
#define SENAE_PORT "8081"
#define SUPERCIA_HOSTNAME "127.0.0.1"
#define SUPERCIA_PORT "8082"

char *askAgency(char *hostname, char *port);
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
    pthread_create(&senae_tid, NULL, askSENAE, NULL);
    pthread_create(&supercia_tid, NULL, askSUPERCIA, NULL);
    char *sriResult, *senaeResult, *superciaResult;
    pthread_join(sri_tid, (void **)&sriResult);
    pthread_join(senae_tid, (void **)&senaeResult);
    pthread_join(supercia_tid, (void **)&superciaResult);
    printf("SRI says: %s\n", sriResult);
    printf("SENAE says: %s\n", senaeResult);
    printf("SUPERCIA says: %s\n", superciaResult);
    free(sriResult);
    free(senaeResult);
    free(superciaResult);
    free(myBoat);
    return 0;
}

void *askSRI()
{
    return (void *)askAgency(SRI_HOSTNAME, SRI_PORT);
}

void *askSENAE()
{
    return (void *)askAgency(SENAE_HOSTNAME, SENAE_PORT);
}

void *askSUPERCIA()
{
    return (void *)askAgency(SUPERCIA_HOSTNAME, SUPERCIA_PORT);
}

char *askAgency(char *hostname, char *port){
    int connfd = open_clientfd(hostname, port);
    if (connfd < 0)
        connection_error(connfd);
    write(connfd, &(myBoat->type), sizeof(BoatType));
    write(connfd, &(myBoat->avg_weight), sizeof(float));
    int dest_length = strlen(myBoat->destination);
    write(connfd, &(dest_length), sizeof(int));
    write(connfd, myBoat->destination, dest_length);
    char *response = (char *)malloc(5*sizeof(char));
    ssize_t bytes_read = read(connfd, response, 5);
    // AQUI LANZAR UNA SENAL PARA NOTIFICAR QUE LLEGO LA RESPUESTA
    response[bytes_read] = '\0';
    // HACER QUE LAS AGENCIAS ESPEREN OTRO MENSAJE DE ECUAFAST PARA HACER COMMIT A LA INSERCION DEL NUEVO BARCO EN SUS ESTRUCTURAS,
    // EL COMMIT SE ENVIA SI SE LANZARON LAS 3 SENALES DE RESPUESTA RECIBIDA,
    // CASO CONTRARIO SE ENVIA UN ROLLBACK
    close(connfd);
    return response;
}