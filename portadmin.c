#include "boat.h"
#include "common.h"
#include "list.h"
#include <ctype.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_STR_SIZE 8192
#define MAX_BOAT_ENTRY_SIZE 200

typedef struct {
    List *lowPriorityQueue;
    List *highPriorityQueue;
    char *queue_str;
    int str_capacity;
    sem_t mutex;
    sem_t full;
    int next_id;
} DockingQueue;

void *workerThread(void *);
void *unloaderThread(void *);
void enqueue_boat(Boat *);
void remove_boat(int);
void sigint_handler(int);
void serialize_queue(char *);
Boat *popBoat();
void broadcast_update(const char *);
void sendToNodes(const char *, List *);

DockingQueue *queue;
int docks_number;

int main(int argc, char *argv[]) {
    char *nvalue = NULL;
    bool nflag = false;
    int index;
    int c;
     printf("***********PORT ADMINISTRATION***********\n\n");
    opterr = 0;
    while ((c = getopt(argc, argv, "n:")) != -1) {
        switch (c) {
        case 'n':
            nvalue = optarg;
            nflag = true;
            break;
        case 'h':
            printf("Usage: %s [-n] <number of concurrently unloaded boats>\n",
                   argv[0]);
            printf("    -h:             Shows this message.\n");
            return 0;
        case '?':
            if (optopt == 'n')
                fprintf(stderr, "-%c requires an argument.\n", optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        }
    }
    if (nflag) {
        docks_number = atoi(nvalue);
    } else {
        docks_number = 5;
        printf("Starting port service with default 5 concurrent unloaders.\n");
    }

    signal(SIGPIPE, SIG_IGN);

    queue = (DockingQueue *)malloc(sizeof(DockingQueue));
    queue->lowPriorityQueue = newList();
    queue->highPriorityQueue = newList();
    queue->str_capacity = INITIAL_STR_SIZE;
    queue->queue_str = (char *)malloc(queue->str_capacity * sizeof(char));
    queue->queue_str[0] = '\0';
    queue->next_id = 0;
    sem_init(&(queue->mutex), 0, 1);
    sem_init(&queue->full, 0, 0);

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
    char *port = "8083";
    listenfd = open_listenfd(port);

    if (listenfd < 0) {
        connection_error(listenfd);
    }
    printf("Server listening on port %s.\n  Press Ctrl+C to quit safely.\n",
           port);
    pthread_t unloader_tid;
    for (int i = 0; i < docks_number; i++) {
        pthread_create(&unloader_tid, NULL, unloaderThread, NULL);
    }
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
    int connfd = *(int *)arg;
    free(arg);
    Boat *currentBoat = (Boat *)malloc(sizeof(Boat));
    currentBoat->toCheck = false;
    currentBoat->unloading_time = 0;
    currentBoat->connfd = connfd;
    size_t dest_length;
    sem_wait(&(queue->mutex));
    currentBoat->id = queue->next_id;
    queue->next_id++;
    sem_post(&(queue->mutex));

    if (write(connfd, &currentBoat->id, sizeof(int)) == -1) {
        printf("A boat broke before establishing connection.\n");
        free(currentBoat);
        pthread_exit(NULL);
    }
    if (read(connfd, &(currentBoat->type), sizeof(BoatType)) == 0) {
        printf("A boat broke before establishing connection.\n");
        free(currentBoat);
        pthread_exit(NULL);
    }
    if (read(connfd, &(currentBoat->avg_weight), sizeof(double)) == 0) {
        printf("A boat broke before establishing connection.\n");
        free(currentBoat);
        pthread_exit(NULL);
    }
    if (read(connfd, &dest_length, sizeof(size_t)) == 0) {
        printf("A boat broke before establishing connection.\n");
        free(currentBoat);
        pthread_exit(NULL);
    }
    currentBoat->destination = (char *)malloc(dest_length + 1);
    if (read(connfd, currentBoat->destination, dest_length) == -1) {
        printf("A boat broke before establishing connection.\n");
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }

    currentBoat->destination[dest_length] = '\0';
    printf("\n*******************************************************\n");
    printf("waiting for check\n");
    if (read(connfd, &(currentBoat->toCheck), sizeof(bool)) == 0) {
        printf("A boat broke before establishing connection.\n");
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
    if (read(connfd, &(currentBoat->unloading_time), sizeof(double)) == 0) {
        printf("A boat broke before establishing connection.\n");
        free(currentBoat->destination);
        free(currentBoat);
        pthread_exit(NULL);
    }
    enqueue_boat(currentBoat);
    printf("\n. Boat ID: %d, class: %d, avg weight: %.2f, destination: "
           "%s, to "
           "check: %d, cargo unloading time: %.2f hours\n",
           currentBoat->id, currentBoat->type, currentBoat->avg_weight,
           currentBoat->destination, currentBoat->toCheck,
           currentBoat->unloading_time);

    char *message;
    size_t msg_length;
    if (read(connfd, &msg_length, sizeof(size_t)) <= 0) {
        printf("Boat with ID: %d has lost connection! removing from list...\n",
               currentBoat->id);
        remove_boat(currentBoat->id);
        pthread_exit(NULL);
    }
    message = (char *)malloc((msg_length + 1) * sizeof(char));
    if (read(connfd, message, msg_length) <= 0) {
        printf("Boat with ID: %d has lost connection! removing from list...\n",
               currentBoat->id);
        free(message);
        remove_boat(currentBoat->id);
        pthread_exit(NULL);
    }
    message[msg_length] = '\0';
    if (strcmp(message, "DMG") == 0) {
        printf("Boat with ID: %d has broken it's hull! removing from "
               "list...\n",
               currentBoat->id);
        free(message);
        remove_boat(currentBoat->id);
        pthread_exit(NULL);
    } else if (strcmp(message, "BYE") == 0) {
        printf("Gracefully ended connection with ID: %d\n", currentBoat->id);
        free(message);
        close(currentBoat->connfd);
        pthread_exit(NULL);
    }
}

void enqueue_boat(Boat *currentBoat) {
    sem_wait(&(queue->mutex));
    if (currentBoat->toCheck &&
        strcmp(currentBoat->destination, "ecuador") != 0) {
        tailInsert(queue->highPriorityQueue, (void *)currentBoat);
    } else {
        tailInsert(queue->lowPriorityQueue, (void *)currentBoat);
    }
    if (MAX_BOAT_ENTRY_SIZE * (queue->highPriorityQueue->length +
                               queue->lowPriorityQueue->length) >=
        queue->str_capacity) {
        size_t new_capacity = queue->str_capacity * 2;
        char *new_queue_str = (char *)realloc(queue->queue_str, new_capacity);
        if (new_queue_str == NULL) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            deleteList(queue->highPriorityQueue);
            deleteList(queue->lowPriorityQueue);
            free(queue->queue_str);
            sem_destroy(&queue->full);
            sem_destroy(&queue->mutex);
            free(queue);
            exit(1);
        }
        queue->queue_str = new_queue_str;
        queue->str_capacity = new_capacity;
    }
    serialize_queue(queue->queue_str);
    broadcast_update(queue->queue_str);
    sem_post(&(queue->mutex));
    sem_post(&queue->full);
}

void remove_boat(int id) {
    sem_wait(&queue->full);
    sem_wait(&(queue->mutex));
    if (!deleteBoat(queue->highPriorityQueue, id)) {
        deleteBoat(queue->lowPriorityQueue, id);
    }
    serialize_queue(queue->queue_str);
    broadcast_update(queue->queue_str);
    sem_post(&(queue->mutex));
}

void sigint_handler(int signum) {
    sem_wait(&(queue->mutex));
    printf("Gracefully closing connections and freeing resources...\n");
    deleteList(queue->highPriorityQueue);
    deleteList(queue->lowPriorityQueue);
    free(queue->queue_str);
    sem_destroy(&queue->full);
    sem_destroy(&queue->mutex);
    free(queue);
    exit(0);
}

void serialize_queue(char *target) {
    target[0] = '\0';
    if (queue->highPriorityQueue->head == NULL &&
        queue->lowPriorityQueue->head == NULL) {
        strcat(target, "The queue is clear!\n");
        return;
    }
    strcat(target, "PORT DOCKING QUEUE");
    char formatted[MAX_BOAT_ENTRY_SIZE];
    int turn = 1;
    Node *current = queue->highPriorityQueue->head;
    Boat *currentBoat;
    while (current != NULL) {
        currentBoat = (Boat *)current->n;
        sprintf(formatted,
                "\n%d. Boat ID: %d, class: %d, avg weight: %.2f, destination: "
                "%s, to "
                "check: %d, cargo unloading time: %.2f hours\n",
                turn, currentBoat->id, currentBoat->type,
                currentBoat->avg_weight, currentBoat->destination,
                currentBoat->toCheck, currentBoat->unloading_time);
        strcat(target, formatted);
        turn++;
        current = current->next;
    }
    current = queue->lowPriorityQueue->head;
    while (current != NULL) {
        currentBoat = (Boat *)current->n;
        sprintf(formatted,
                "\n%d. Boat ID: %d, class: %d, avg weight: %.2f, destination: "
                "%s, to "
                "check: %d, cargo unloading time: %.2f hours\n",
                turn, currentBoat->id, currentBoat->type,
                currentBoat->avg_weight, currentBoat->destination,
                currentBoat->toCheck, currentBoat->unloading_time);
        strcat(target, formatted);
        turn++;
        current = current->next;
    }
}

void *unloaderThread(void *arg) {
    Boat *currentBoat = NULL;
    char message[7] = "DOCKED";
    int current_id;
    size_t msg_length = strlen(message);
    while (true) {
        sem_wait(&queue->full);
        sem_wait(&(queue->mutex));
        currentBoat = popBoat();
        current_id = currentBoat->id;
        write(currentBoat->connfd, &msg_length, sizeof(size_t));
        write(currentBoat->connfd, message, msg_length);
        serialize_queue(queue->queue_str);
        broadcast_update(queue->queue_str);
        sem_post(&(queue->mutex));
        printf("Unloading boat with ID: %d... Seconds remaining: %.2f\n",
               currentBoat->id, currentBoat->unloading_time);
        sleep(currentBoat->unloading_time);
        printf(
            "Boat with ID: %d has finished unloading and has left the port.\n",
            current_id);
        free(currentBoat->destination);
        free(currentBoat);
    }
}

Boat *popBoat() {
    Boat *boat = NULL;
    if (queue->highPriorityQueue->head != NULL) {
        boat = (Boat *)pop(queue->highPriorityQueue, 0);
    } else if (queue->lowPriorityQueue->head != NULL) {
        boat = (Boat *)pop(queue->lowPriorityQueue, 0);
    }
    return boat;
}

void broadcast_update(const char *queue_str) {
    sendToNodes(queue_str, queue->highPriorityQueue);
    sendToNodes(queue_str, queue->lowPriorityQueue);
}

void sendToNodes(const char *str, List *queue) {
    Node *previous = NULL;
    Node *current;
    Boat *currentBoat;
    size_t msg_length = strlen(str);
    current = queue->head;
    while (current != NULL) {
        currentBoat = (Boat *)current->n;
        Node *nextNode = current->next;
        if (write(currentBoat->connfd, &msg_length, sizeof(size_t)) == -1 ||
            write(currentBoat->connfd, str, msg_length) == -1) {
            if (previous != NULL) {
                previous->next = nextNode;
            } else {
                queue->head = nextNode;
            }
            if (nextNode == NULL) {
                queue->tail = previous;
            }
            queue->length--;
            free(currentBoat->destination);
            free(currentBoat);
            free(current);
        } else {
            previous = current;
        }
        current = nextNode;
    }
}