#include "boat.h"
#include "common.h"
#include <ctype.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

void *workerThread(void *arg);
void sigpipe_handler(int);
void sigint_handler(int);

int min_latency;
int max_latency;

int main(int argc, char *argv[]) {
    char *fvalue = NULL;
    char *tvalue = NULL;
    bool fflag = false;
    bool tflag = false;
    int index;
    int c;
    printf("***********SUPERCIA***********\n\n");
    opterr = 0;
    while ((c = getopt(argc, argv, "f:t:")) != -1) {
        switch (c) {
        case 'f':
            fvalue = optarg;
            fflag = true;
            break;
        case 't':
            tvalue = optarg;
            tflag = true;
            break;
        case 'h':
            printf("Usage: %s [-f] <response latency floor> [-t] <response latency "
                   "top>\n",
                   argv[0]);
            printf("    -h:             Shows this message.\n");
            return 0;
        case '?':
            if (optopt == 'f')
                break;
            if (optopt == 't')
                break;
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        }
    }
    if (fflag) {
        min_latency = atoi(fvalue);
    } else {
        min_latency = 0;
        printf("Starting up service with default minimum response latency = 0 seconds.\n");
    }
    if (tflag) {
        max_latency = atoi(tvalue);
    } else {
        max_latency = 6;
        printf("Starting up service with default maximum response latency = 6 seconds.\n");
    }

    srand((unsigned int)time(NULL));

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
    char *port = "8082";
    listenfd = open_listenfd(port);

    if (listenfd < 0) {
        connection_error(listenfd);
    }
    printf("Server listening on port %s.\n  Press Ctrl+C to quit safely.\n",
           port);
    pthread_t tid;
    while (true) {
        clientlen = sizeof(clientaddr);
        int *connfd = (int *)malloc(sizeof(int));
        *connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, workerThread, (void *)connfd);
    }
    return 0;
}

void *workerThread(void *arg) {
    pthread_detach(pthread_self());
    int connfd = *(int *)arg;
    free(arg);
    srand((unsigned int)time(NULL) ^ (unsigned int)pthread_self());

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
    char *transaction = (char *)malloc(7 * sizeof(char));
    transaction[0] = '\0';

    read(connfd, &(currentBoat->type), sizeof(BoatType));
    read(connfd, &(currentBoat->avg_weight), sizeof(double));
    read(connfd, &dest_length, sizeof(int));
    currentBoat->destination = (char *)malloc((dest_length + 1) * sizeof(char));
    read(connfd, currentBoat->destination, dest_length);
    currentBoat->destination[dest_length] = '\0';
    printf("\n*******************************************************\n");
    printf("Boat just arrived. Type: %d, avg weight: %f, destination: %s\n",
           currentBoat->type, currentBoat->avg_weight,
           currentBoat->destination);

    int random_int = rand() % 100;
    bool checkBoat = (currentBoat->type == PANAMAX && random_int < 50) ||
                     (currentBoat->type == CONVENTIONAL && random_int < 30);
    int latency = min_latency + rand() % (max_latency - min_latency + 1);
    sleep(latency); // simulate response latency
    if (checkBoat) {
        write(connfd, "CHECK", 5);
    } else {
        write(connfd, "PASS", 4);
    }
    printf("Sent response to boat.\n");
    read(connfd, transaction, 6);
    transaction[7] = '\0';

    if (strcmp(transaction, "COMMIT") != 0) {
        printf("Discarding...\n");
        close(connfd);
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }

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
    exit(0);
}