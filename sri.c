#include "boat.h"
#include "common.h"
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>

#define EPSILON 1e-9
#define WEIGHTS_BUFFERSIZE 20
#define WEIGHTS_FILE "data/sri/weights.bin"
#define MAX_MESSAGE_LENGTH 6

void *workerThread(void *);
void sigpipe_handler(int);
void sigint_handler(int);

typedef struct WeightBuffer {
    double array[WEIGHTS_BUFFERSIZE];
    int index;
    double current_avg;
    int full;
    double item_ponderation;
} WeightBuffer;

WeightBuffer *weights;
sem_t *mutex;
int min_latency;
int max_latency;

int main(int argc, char *argv[]) {
    char *fvalue = NULL;
    char *tvalue = NULL;
    bool fflag = false;
    bool tflag = false;
    int index;
    int c;
     printf("***********SRI***********\n\n");
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
        max_latency = 5;
        printf("Starting up service with default maximum response latency = 5 seconds.\n");
    }

    bool cleanweights = access(WEIGHTS_FILE, F_OK) != 0;
    int weightsfd = open(WEIGHTS_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ftruncate(weightsfd, sizeof(WeightBuffer));
    weights =
        (WeightBuffer *)mmap(NULL, sizeof(WeightBuffer), PROT_READ | PROT_WRITE,
                             MAP_SHARED, weightsfd, 0);
    close(weightsfd);
    if (cleanweights) {
        weights->index = 0;
        weights->full = 0;
        weights->current_avg = -1;
        weights->item_ponderation = 1 / (double)WEIGHTS_BUFFERSIZE;
    }
    mutex = (sem_t *)malloc(sizeof(sem_t));
    sem_init(mutex, 0, 1);

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

    if (listenfd < 0) {
        connection_error(listenfd);
    }
    printf("Server listening on port %s.\n Press Ctrl+C to quit safely.\n",
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

    sem_wait(mutex);
    printf("Current avg = %f\n", weights->current_avg);
    bool isConventional = currentBoat->type == CONVENTIONAL;
    bool toEcuador = strcmp(currentBoat->destination, "ecuador") == 0;
    bool exceedsWeight =
        (currentBoat->avg_weight - weights->current_avg) > EPSILON;
    bool checkBoat = isConventional && toEcuador && exceedsWeight;
    sem_post(mutex);
    int latency = min_latency + rand() % (max_latency - min_latency + 1);
    sleep(latency); // simulate response latency
    if (checkBoat) {
        char message[MAX_MESSAGE_LENGTH + 1] = "CHECK";
        size_t message_length = strlen(message);
        write(connfd, &message_length, sizeof(size_t));
        write(connfd, message, message_length);
    } else {
        char message[MAX_MESSAGE_LENGTH + 1] = "PASS";
        size_t message_length = strlen(message);
        write(connfd, &message_length, sizeof(size_t));
        write(connfd, message, message_length);
    }
    printf("Response sent to boat.\n");
    size_t transaction_length;
    read(connfd, &transaction_length, sizeof(size_t));
    if(transaction_length > MAX_MESSAGE_LENGTH){
        printf("Invalid notification size received.\n");
        close(connfd);
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
    read(connfd, transaction, transaction_length);
    transaction[transaction_length] = '\0';


    if (strcmp(transaction, "COMMIT") != 0) {
        printf("Discarding...\n");
        close(connfd);
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
    sem_wait(mutex);
    if (weights->full == 0) {
        weights->current_avg = currentBoat->avg_weight;
        weights->full++;
    } else if (weights->full < WEIGHTS_BUFFERSIZE) {
        weights->current_avg =
            (weights->current_avg * weights->full + currentBoat->avg_weight) /
            (weights->full + 1);
        weights->full++;
    } else {
        weights->current_avg =
            weights->current_avg +
            weights->item_ponderation *
                (currentBoat->avg_weight - weights->array[weights->index]);
    }
    weights->array[weights->index] = currentBoat->avg_weight;
    weights->index = (weights->index + 1) % WEIGHTS_BUFFERSIZE;
    sem_post(mutex);
    printf("New data saved.\n");

    close(connfd);
    free(currentBoat->destination);
    free(currentBoat);
    return NULL;
}

void sigpipe_handler(int signum) {
    printf("Client disconnected, closing it's worker thread...\n");
    pthread_exit(NULL);
}

void sigint_handler(int signum) {
    printf("Closing gracefully...\n");
    sem_destroy(mutex);
    free(mutex);
    msync(weights, sizeof(WeightBuffer), MS_SYNC);
    munmap(weights, sizeof(WeightBuffer));
    exit(0);
}