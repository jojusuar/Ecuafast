#include "boat.h"
#include "common.h"
#include "list.h"
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_STR_SIZE 8192
#define MAX_BOAT_ENTRY_SIZE 144

typedef struct {
    Boat **docks;
    sem_t mutex;
    sem_t full;
    sem_t empty;
} Port;

typedef struct {
    List *lowPriorityQueue;
    List *highPriorityQueue;
    char *queue_str;
    int str_capacity;
    sem_t rw_mutex;
    sem_t mutex;
    int read_count;
    int next_id;
    int version;
    pthread_cond_t cond;
    pthread_mutex_t cond_mutex;
} DockingOrder;

typedef struct {
    int connfd;
    Boat *myBoat;
} ListenerData;

void *workerThread(void *);
void *damageListener(void *);
void sigint_handler(int);
void serialize_queue(char *);

Port *port;
DockingOrder *order;
int docks_number = 10;

int main() {
    signal(SIGPIPE, SIG_IGN);
    port = (Port *)malloc(sizeof(Port));
    port->docks = (Boat **)malloc(docks_number * sizeof(Boat *));
    sem_init(&(port->empty), 0, 1);
    sem_init(&(port->full), 0, 1);

    order = (DockingOrder *)malloc(sizeof(DockingOrder));
    order->lowPriorityQueue = newList();
    order->highPriorityQueue = newList();
    order->str_capacity = INITIAL_STR_SIZE;
    order->queue_str = (char *)malloc(order->str_capacity * sizeof(char));
    order->queue_str[0] = '\0';
    order->read_count = 0;
    order->version = 0;
    sem_init(&(order->mutex), 0, 1);
    sem_init(&(order->rw_mutex), 0, 1);
    order->version = 0;
    pthread_cond_init(&order->cond, NULL);
    pthread_mutex_init(&order->cond_mutex, NULL);

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
    int dest_length;
    sem_wait(&(order->rw_mutex));
    currentBoat->id = order->next_id;
    order->next_id++;
    sem_post(&(order->rw_mutex));

    write(connfd, &currentBoat->id, sizeof(int));
    read(connfd, &(currentBoat->type), sizeof(BoatType));
    read(connfd, &(currentBoat->avg_weight), sizeof(double));
    read(connfd, &dest_length, sizeof(int));
    currentBoat->destination = (char *)malloc((dest_length + 1) * sizeof(char));
    read(connfd, currentBoat->destination, dest_length);
    currentBoat->destination[dest_length] = '\0';
    printf("\n*******************************************************\n");
    printf("waiting for check\n");
    read(connfd, &(currentBoat->toCheck), sizeof(bool));
    printf("check flag: %d", currentBoat->toCheck);
    read(connfd, &(currentBoat->unloading_time), sizeof(double));
    printf("\n. Boat ID: %d, class: %d, avg weight: %.2f, destination: "
           "%s, to "
           "check: %d, cargo unloading time: %.2f hours\n",
           currentBoat->id, currentBoat->type, currentBoat->avg_weight,
           currentBoat->destination, currentBoat->toCheck,
           currentBoat->unloading_time);
    /// WRITER START
    sem_wait(&(order->rw_mutex));
    if (currentBoat->toCheck &&
        strcmp(currentBoat->destination, "ecuador") != 0) {
        tailInsert(order->highPriorityQueue, (void *)currentBoat);
    } else {
        tailInsert(order->lowPriorityQueue, (void *)currentBoat);
    }
    if (MAX_BOAT_ENTRY_SIZE * (order->highPriorityQueue->length +
                               order->lowPriorityQueue->length) >
        order->str_capacity) {
        order->str_capacity *= 2;
        order->queue_str = (char *)realloc(
            order->queue_str, order->str_capacity * sizeof(order->queue_str));
    }
    serialize_queue(order->queue_str);
    printf("serialized. entering pthread mutex\n");
    pthread_mutex_lock(&order->cond_mutex);
    order->version++;
    pthread_cond_broadcast(&order->cond);
    pthread_mutex_unlock(&order->cond_mutex);
    sem_post(&(order->rw_mutex));
    /// WRITER END

    int mytid = pthread_self();
    ListenerData *listenerdata = (ListenerData *)malloc(sizeof(ListenerData));
    listenerdata->connfd = connfd;
    listenerdata->myBoat = currentBoat;
    pthread_t listener_tid;
    pthread_create(&listener_tid, NULL, damageListener, (void *)listenerdata);

    // TODO: create a sigpipe handler

    int last_version = -1;
    bool shouldStop;
    while (1) {
        pthread_mutex_lock(&order->cond_mutex);
        while (order->version == last_version) {
            pthread_cond_wait(&order->cond, &order->cond_mutex);
        }
        last_version = order->version;
        pthread_mutex_unlock(&order->cond_mutex);

        // READER START
        sem_wait(&order->mutex);
        order->read_count++;
        if (order->read_count == 1) {
            sem_wait(&order->rw_mutex);
        }
        sem_post(&order->mutex);
        size_t list_length = strlen(order->queue_str);
        if (write(connfd, &list_length, sizeof(size_t)) == -1) {
            printf("Instructed to terminate\n");
            sem_wait(&order->mutex);
            order->read_count--;
            if (order->read_count == 0) {
                sem_post(&order->rw_mutex);
            }
            sem_post(&order->mutex);
            pthread_join(listener_tid, NULL);
            free(currentBoat->destination);
            free(currentBoat);
            pthread_exit(NULL);
        }
        if (write(connfd, order->queue_str, strlen(order->queue_str)) == -1) {
            printf("Instructed to terminate\n");
            sem_wait(&order->mutex);
            order->read_count--;
            if (order->read_count == 0) {
                sem_post(&order->rw_mutex);
            }
            sem_post(&order->mutex);
            pthread_join(listener_tid, NULL);
            free(currentBoat->destination);
            free(currentBoat);
            pthread_exit(NULL);
        }
        sem_wait(&order->mutex);
        order->read_count--;
        if (order->read_count == 0) {
            sem_post(&order->rw_mutex);
        }
        sem_post(&order->mutex);
    }
    // READER END
}

void sigint_handler(int signum) {
    // TOO: SET A CLEANUP FLAG INSTEAD OF ALL THIS SHIT BELOW
    //  printf("Closing gracefully...\n");
    //  free(order->lowPriorityQueue);
    //  free(order->highPriorityQueue);
    //  sem_destroy(&order->mutex);
    //  sem_destroy(&order->rw_mutex);
    //  pthread_cond_destroy(&order->cond);
    //  pthread_mutex_destroy(&order->cond_mutex);
    //  free(order);
    //  free(port->docks);
    //  sem_destroy(&(port->full));
    //  sem_destroy(&(port->empty));
    //  free(port);
    exit(0);
}

void serialize_queue(char *target) {
    target[0] = '\0';
    if (order->highPriorityQueue->head == NULL &&
        order->lowPriorityQueue->head == NULL) {
        strcat(target, "Empty queue!");
        return;
    }
    strcat(target, "PORT DOCKING QUEUE");
    char formatted[MAX_BOAT_ENTRY_SIZE];
    int turn = 1;
    Node *current = order->highPriorityQueue->head;
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
    current = order->lowPriorityQueue->head;
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

void *damageListener(void *arg) {
    ListenerData *data = (ListenerData *)arg;
    char message[4];
    read(data->connfd, message, 3);
    message[4] = '\0';
    if (strcmp(message, "DMG") == 0) {
        printf("Boat with ID: %d has broken! removing from list...\n", data->myBoat->id);
        close(data->connfd);
        sem_wait(&(order->rw_mutex));
        bool stop = false;
        Node *previous = NULL;
        Node *current = order->highPriorityQueue->head;
        Boat *currentBoat;
        while (current != NULL) {
            currentBoat = (Boat *)current->n;
            if (currentBoat->id == data->myBoat->id) {
                if (previous != NULL) {
                    previous->next = current->next;
                } else {
                    order->highPriorityQueue->head = current->next;
                }
                if (current->next == NULL && previous != NULL) {
                    previous->next = NULL;
                    order->highPriorityQueue->tail = previous;
                }
                order->highPriorityQueue->length--;
                stop = true;
                break;
            }
            previous = current;
            current = current->next;
        }
        if (!stop) {
            previous = NULL;
            current = order->lowPriorityQueue->head;
            while (current != NULL) {
                currentBoat = (Boat *)current->n;
                if (currentBoat->id == data->myBoat->id) {
                    if (previous != NULL) {
                        previous->next = current->next;
                    } else {
                        order->lowPriorityQueue->head = current->next;
                    }
                    if (current->next == NULL && previous != NULL) {
                        previous->next = NULL;
                        order->lowPriorityQueue->tail = previous;
                    }
                    order->lowPriorityQueue->length--;
                    break;
                }
                previous = current;
                current = current->next;
            }
        }
        serialize_queue(order->queue_str);
        pthread_mutex_lock(&order->cond_mutex);
        order->version++;
        pthread_cond_broadcast(&order->cond);
        pthread_mutex_unlock(&order->cond_mutex);
        sem_post(&(order->rw_mutex));
        free(data);
        pthread_exit(NULL);
    }
    return NULL;
}