#include "boat.h"
#include "common.h"
#include "heap.h"
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#define EPSILON 1e-9
#define BUFFER_FILE "data/senae/buffer.bin"

typedef struct {
    Heap *minHeap;
    Heap *maxHeap;
    int totalSize;
} SENAEBuffer;

SENAEBuffer *buffer;
sem_t mutex;
int min_latency;
int max_latency;

void *workerThread(void *arg);
double get3rdQuartile(SENAEBuffer *buffer);
void rebalanceHeaps(SENAEBuffer *buffer);
void sigpipe_handler(int);
void sigint_handler(int);

int main(int argc, char *argv[]) {
    char *fvalue = NULL;
    char *tvalue = NULL;
    bool fflag = false;
    bool tflag = false;
    int index;
    int c;

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

    bool cleanbuffer = access(BUFFER_FILE, F_OK) != 0;
    int bufferfd = open(BUFFER_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ftruncate(bufferfd, sizeof(SENAEBuffer));
    buffer =
        (SENAEBuffer *)mmap(NULL, sizeof(SENAEBuffer), PROT_READ | PROT_WRITE,
                            MAP_SHARED, bufferfd, 0);
    if (cleanbuffer) {
        buffer->totalSize = 0;
    }
    close(bufferfd);

    buffer->minHeap = newHeap(false);
    buffer->maxHeap = newHeap(true);
    sem_init(&mutex, 0, 1);

    struct sigaction sigintAction;
    sigintAction.sa_handler = sigint_handler;
    sigintAction.sa_flags = 0;
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
    closeHeap(buffer->minHeap);
    closeHeap(buffer->maxHeap);
    sem_destroy(&mutex);
    munmap(buffer, sizeof(SENAEBuffer));
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
    printf("Boat just arrived. Type: %d, avg weight: %.2f, destination: %s\n",
           currentBoat->type, currentBoat->avg_weight,
           currentBoat->destination);

    sem_wait(&mutex);
    double thirdQ = get3rdQuartile(buffer);
    printf("minHeap (top 25%%): ");
    printHeap(buffer->minHeap);
    printf("maxHeap (bottom 75%%): ");
    printHeap(buffer->maxHeap);
    printf("Q3: %f\n", thirdQ);
    bool overweight = (currentBoat->avg_weight - thirdQ) > EPSILON;
    sem_post(&mutex);
    bool isPanamax = currentBoat->type == PANAMAX;
    bool toTarget = strcmp(currentBoat->destination, "usa") == 0 ||
                    strcmp(currentBoat->destination, "europa") == 0 ||
                    strcmp(currentBoat->destination, "europe") == 0;
    bool checkBoat = overweight && isPanamax && toTarget;
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
    sem_wait(&mutex);
    if (!isEmpty(buffer->minHeap) &&
        (EPSILON >= (peek(buffer->minHeap) - currentBoat->avg_weight))) {
        insert(buffer->minHeap, currentBoat->avg_weight);
    } else {
        insert(buffer->maxHeap, currentBoat->avg_weight);
    }
    rebalanceHeaps(buffer);
    buffer->totalSize++;
    sem_post(&mutex);
    printf("New data saved.\n");

    close(connfd);
    free(currentBoat->destination);
    free(currentBoat);
    return NULL;
}

double get3rdQuartile(SENAEBuffer *buffer) {
    if (buffer->totalSize < 3) {
        return -1;
    }
    double position = 0.75 * (buffer->totalSize + 1) - 1;
    printf("maxHeap size = %d, minHeap size = %d\n ", buffer->maxHeap->size,
           buffer->minHeap->size);
    printf("Q3 position = %.2f\n", position);
    if (position >= buffer->maxHeap->size) {
        return peek(buffer->minHeap);
    }
    double decimal = position - (int)position;
    return peek(buffer->maxHeap) +
           decimal * (peek(buffer->minHeap) - peek(buffer->maxHeap));
}

void rebalanceHeaps(SENAEBuffer *buffer) {
    while (buffer->maxHeap->size > ((buffer->totalSize + 1) * 3 / 4)) {
        double root = pop(buffer->maxHeap);
        insert(buffer->minHeap, root);
    }
    while (buffer->minHeap->size > ((buffer->totalSize + 1) / 4)) {
        double root = pop(buffer->minHeap);
        insert(buffer->maxHeap, root);
    }
}

void sigpipe_handler(int signum) {
    printf("Thread received SIGPIPE, exiting...\n");
    sem_post(&mutex);
    pthread_exit(NULL);
}

void sigint_handler(int signum) {
    printf("Closing gracefully...\n");
    closeHeap(buffer->minHeap);
    closeHeap(buffer->maxHeap);
    sem_destroy(&mutex);
    msync(buffer, sizeof(SENAEBuffer), MS_SYNC);
    munmap(buffer, sizeof(SENAEBuffer));
    exit(0);
}