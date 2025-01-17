#include "boat.h"
#include "common.c"
#include <ctype.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#define SRI_HOSTNAME                                                           \
    "127.0.0.1" // these TCP/IP addresses could be changed to the actual IPs of
                // each institution's server
#define SRI_PORT "8080"
#define SENAE_HOSTNAME "127.0.0.1"
#define SENAE_PORT "8081"
#define SUPERCIA_HOSTNAME "127.0.0.1"
#define SUPERCIA_PORT "8082"

void askAgency(int, char *);
void *askSRI();
void *askSENAE();
void *askSUPERCIA();
void *startTimeout();
void rollback();
void commit();
void handle_sigint(int);

typedef struct {
    int srifd;
    int senaefd;
    int superciafd;
} Connections;

Boat *myBoat;
int timeout;
int responseCounter;
sem_t counterMutex;
sem_t commitMutex;
Connections *connections;
pthread_t request_tid;
pthread_t sri_tid, senae_tid, supercia_tid;
char sriResult[6], senaeResult[6], superciaResult[6];

int main(int argc, char *argv[]) {
    printf("Starting...\n");
    char *cvalue = NULL;
    char *wvalue = NULL;
    char *dvalue = NULL;
    char *tvalue = NULL;
    bool cflag = false;
    bool wflag = false;
    bool dflag = false;
    bool tflag = false;
    int index;
    int c;

    opterr = 0;
    while ((c = getopt(argc, argv, "c:w:d:t:")) != -1) {
        switch (c) {
        case 'c':
            cvalue = optarg;
            cflag = true;
            break;
        case 'w':
            wvalue = optarg;
            wflag = true;
            break;
        case 'd':
            dvalue = optarg;
            dflag = true;
            break;
        case 't':
            tvalue = optarg;
            tflag = true;
            break;
        case 'h':
            printf("Usage: %s -c <boat class> -w <avg. weight> -d "
                   "<destination> -t "
                   "<response timeout (seconds)>\n",
                   argv[0]);
            printf("    -h:             Shows this message.\n");
            return 0;
        case '?':
            if (optopt == 't')
                fprintf(stderr, "-%c requires an argument.\n", optopt);
            if (optopt == 'c')
                fprintf(stderr, "-%c requires an argument.\n", optopt);
            if (optopt == 'w')
                fprintf(stderr, "-%c requires an argument.\n", optopt);
            if (optopt == 'd')
                fprintf(stderr, "-%c requires an argument.\n", optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        }
    }

    myBoat = (Boat *)malloc(sizeof(Boat));
    if (cflag) {
        switch (atoi(cvalue)) {
        case 0:
            myBoat->type = CONVENTIONAL;
            break;
        case 1:
            myBoat->type = PANAMAX;
            break;
        default:
            free(myBoat);
            fprintf(stderr, "Invalid boat type.\n");
            return 1;
        }
    } else {
        fprintf(
            stderr,
            "Usage: %s -c <boat class> -w <avg. weight> -d <destination> -t "
            "<response timeout (seconds)>\n",
            argv[0]);
        exit(1);
    }
    if (wflag) {
        myBoat->avg_weight = atof(wvalue);
    } else {
        fprintf(
            stderr,
            "Usage: %s -c <boat class> -w <avg. weight> -d <destination> -t "
            "<response timeout (seconds)>\n",
            argv[0]);
        exit(1);
    }
    if (dflag) {
        for (int i = 0; dvalue[i]; i++) {
            dvalue[i] = tolower((unsigned char)dvalue[i]);
        }
        myBoat->destination = dvalue;
    } else {
        fprintf(
            stderr,
            "Usage: %s -c <boat class> -w <avg. weight> -d <destination> -t "
            "<response timeout (seconds)>\n",
            argv[0]);
        exit(1);
    }
    if (tflag) {
        timeout = atoi(tvalue);
    } else {
        fprintf(
            stderr,
            "Usage: %s -c <boat class> -w <avg. weight> -d <destination> -t "
            "<response timeout (seconds)>\n",
            argv[0]);
        exit(1);
    }

    sem_init(&counterMutex, 0, 1);
    sem_init(&commitMutex, 0, -2);
    sem_wait(&counterMutex);
    responseCounter = 0;
    sem_post(&counterMutex);
    connections = (Connections *)malloc(sizeof(Connections));
    connections->srifd = -1;
    connections->senaefd = -1;
    connections->superciafd = -1;

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    pthread_create(&request_tid, NULL, startTimeout, NULL);
    sem_wait(&commitMutex);
    commit();

    printf("SRI says: %s\n", sriResult);
    printf("SENAE says: %s\n", senaeResult);
    printf("SUPERCIA says: %s\n", superciaResult);

    free(myBoat);
    sem_destroy(&counterMutex);
    free(connections);
    return 0;
}

void *askSRI() {
    connections->srifd = open_clientfd(SRI_HOSTNAME, SRI_PORT);
    if (connections->srifd != -1) {
        askAgency(connections->srifd, sriResult);
        responseCounter++;
        printf("SRI responded\n");
        if (responseCounter == 3) {
            sem_post(&commitMutex);
        }
        sem_post(&counterMutex);
    } else {
        printf("SRI is unreachable.\n");
    }
}

void *askSENAE() {
    connections->senaefd = open_clientfd(SENAE_HOSTNAME, SENAE_PORT);
    if (connections->senaefd != -1) {
        askAgency(connections->senaefd, senaeResult);
        responseCounter++;
        printf("SENAE responded\n");
        if (responseCounter == 3) {
            sem_post(&commitMutex);
        }
        sem_post(&counterMutex);
    } else {
        printf("SENAE is unreachable.\n");
    }
}

void *askSUPERCIA() {
    connections->superciafd = open_clientfd(SUPERCIA_HOSTNAME, SUPERCIA_PORT);
    if (connections->superciafd != -1) {
        askAgency(connections->superciafd, superciaResult);
        responseCounter++;
        printf("SUPERCIA responded\n");
        if (responseCounter == 3) {
            sem_post(&commitMutex);
        }
        sem_post(&counterMutex);
    } else {
        printf("SUPERCIA is unreachable.\n");
    }
}

void *startTimeout() {
    pthread_detach(pthread_self());
    bool attended = false;
    do {
        pthread_create(&sri_tid, NULL, askSRI, NULL);
        pthread_create(&senae_tid, NULL, askSENAE, NULL);
        pthread_create(&supercia_tid, NULL, askSUPERCIA, NULL);
        sleep(timeout);
        sem_wait(&counterMutex);
        if (responseCounter < 3) {
            printf("An agency failed to respond, retrying...\n");
            pthread_cancel(sri_tid);
            pthread_cancel(senae_tid);
            pthread_cancel(supercia_tid);
            rollback();
            responseCounter = 0;
        } else {
            attended = true;
            pthread_join(sri_tid, NULL);
            pthread_join(senae_tid, NULL);
            pthread_join(supercia_tid, NULL);
        }
        sem_post(&counterMutex);
    } while (!attended);
    return NULL;
}

void askAgency(int connfd, char *response) {
    printf("Started a request\n");
    write(connfd, &(myBoat->type), sizeof(BoatType));
    write(connfd, &(myBoat->avg_weight), sizeof(double));
    int dest_length = strlen(myBoat->destination);
    write(connfd, &(dest_length), sizeof(int));
    write(connfd, myBoat->destination, dest_length);
    ssize_t bytes_read = read(connfd, response, 5);
    sem_wait(&counterMutex);
    response[bytes_read] = '\0';
}

void rollback() {
    if (connections->srifd > -1) {
        write(connections->srifd, "ROLLBK", 6);
        close(connections->srifd);
    }
    if (connections->senaefd > -1) {
        write(connections->senaefd, "ROLLBK", 6);
        close(connections->senaefd);
    }
    if (connections->superciafd > -1) {
        write(connections->superciafd, "ROLLBK", 6);
        close(connections->superciafd);
    }
}

void commit() {
    if (connections->srifd > -1) {
        write(connections->srifd, "COMMIT", 6);
        close(connections->srifd);
    }
    if (connections->senaefd > -1) {
        write(connections->senaefd, "COMMIT", 6);
        close(connections->senaefd);
    }
    if (connections->superciafd > -1) {
        write(connections->superciafd, "COMMIT", 6);
        close(connections->superciafd);
    }
}

void handle_sigint(int sig) {
    printf("Aborting gracefully...\n");
    pthread_cancel(sri_tid);
    pthread_cancel(senae_tid);
    pthread_cancel(supercia_tid);
    rollback();
    free(myBoat);
    sem_destroy(&counterMutex);
    free(connections);
    exit(0);
}