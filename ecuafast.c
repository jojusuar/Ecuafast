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
    "127.0.0.1" // these TCP/IP sockets could be changed to the actual IPs and
                // ports of each institution's server
#define SRI_PORT "8080"
#define SENAE_HOSTNAME "127.0.0.1"
#define SENAE_PORT "8081"
#define SUPERCIA_HOSTNAME "127.0.0.1"
#define SUPERCIA_PORT "8082"
#define PORTADMIN_HOSTNAME "127.0.0.1"
#define PORTADMIN_PORT "8083"

void askAgency(int, char *);
void *askSRI();
void *askSENAE();
void *askSUPERCIA();
void *connectToPortAdmin(void *);
void *startTimeout();
void breachHull();
void finish_client();
void rollback();
void commit();
void handle_sigint_on_agency_check(int);
void handle_sigint_on_admin_connection(int);

typedef struct {
    int srifd;
    int senaefd;
    int superciafd;
    int adminfd;
} Connections;

Boat *myBoat;
int timeout;
int responseCounter;
sem_t counterMutex;
sem_t commitMutex;
sem_t greenlightMutex;
sem_t damageMutex;
Connections *connections;
pthread_t request_tid, admin_tid;
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
    myBoat->toCheck = false;
    myBoat->unloading_time = 10;

    sem_init(&counterMutex, 0, 1);
    sem_init(&commitMutex, 0, -2);
    sem_wait(&counterMutex);
    responseCounter = 0;
    sem_post(&counterMutex);
    connections = (Connections *)malloc(sizeof(Connections));
    connections->srifd = -1;
    connections->senaefd = -1;
    connections->superciafd = -1;
    connections->adminfd = -1;

    sem_init(&greenlightMutex, 0, 1);
    sem_wait(&greenlightMutex);
    sem_init(&damageMutex, 0, 0);

    struct sigaction sa;
    sa.sa_handler = handle_sigint_on_agency_check;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    pthread_create(&request_tid, NULL, startTimeout, NULL);
    pthread_create(&admin_tid, NULL, connectToPortAdmin, NULL);
    sem_wait(&commitMutex);
    printf("SRI says: %s\n", sriResult);
    printf("SENAE says: %s\n", senaeResult);
    printf("SUPERCIA says: %s\n", superciaResult);
    commit();
    sa.sa_handler = handle_sigint_on_admin_connection;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    sem_post(&greenlightMutex);
    sem_destroy(&counterMutex);
    printf("supposedly gave signal...\n");
    sem_wait(&damageMutex);
    srand((unsigned int)time(NULL));
    int random_int;
    while (1) {
        sleep(10);
        random_int = rand() % 100;
        if (random_int < 5) {
            breachHull();
        }
    }
    pthread_join(admin_tid, NULL);
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

void *connectToPortAdmin(void *arg) {
    connections->adminfd = open_clientfd(PORTADMIN_HOSTNAME, PORTADMIN_PORT);
    printf("Initiated communication with portuary administration.\n");
    if (read(connections->adminfd, &(myBoat->id), sizeof(int)) < 0) {
        printf("Port administration is unreachable.\n");
        free(myBoat);
        sem_destroy(&counterMutex);
        free(connections);
        exit(0);
    }
    printf("The portuary admin has assigned us ID: %d\n", myBoat->id);
    write(connections->adminfd, &(myBoat->type), sizeof(BoatType));
    write(connections->adminfd, &(myBoat->avg_weight), sizeof(double));
    size_t dest_length = strlen(myBoat->destination);
    write(connections->adminfd, &(dest_length), sizeof(size_t));
    write(connections->adminfd, myBoat->destination, dest_length);
    printf("The port is waiting for agencies' decision before greenlighting "
           "us.\n");
    sem_wait(&greenlightMutex);
    int check_counter = 0;
    check_counter =
        strcmp(sriResult, "CHECK") == 0 ? check_counter + 1 : check_counter;
    check_counter =
        strcmp(senaeResult, "CHECK") == 0 ? check_counter + 1 : check_counter;
    check_counter = strcmp(superciaResult, "CHECK") == 0 ? check_counter + 1
                                                         : check_counter;
    myBoat->toCheck = (check_counter >= 2);
    if (myBoat->toCheck) {
        myBoat->unloading_time *= 2;
    }
    if (strcmp(myBoat->destination, "ecuador") != 0) {
        myBoat->unloading_time *= 0.5;
    }
    printf("going to write!!\n");
    write(connections->adminfd, &(myBoat->toCheck), sizeof(bool));
    printf("supposedly written..\n");
    write(connections->adminfd, &(myBoat->unloading_time), sizeof(double));
    sem_post(&damageMutex);
    char *server_message;
    size_t msg_length;
    while (true) {
        read(connections->adminfd, &msg_length, sizeof(size_t));
        server_message = (char *)malloc(msg_length * sizeof(char) + 1);
        read(connections->adminfd, server_message, msg_length);
        server_message[msg_length] = '\0';
        if (strcmp(server_message, "DOCKED") == 0) {
            finish_client();
        }
        printf("\n%s", server_message);
        free(server_message);
    }
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

void handle_sigint_on_agency_check(int sig) {
    printf("Aborting gracefully...\n");
    pthread_cancel(sri_tid);
    pthread_cancel(senae_tid);
    pthread_cancel(supercia_tid);
    rollback();
    free(myBoat);
    sem_destroy(&counterMutex);
    sem_destroy(&commitMutex);
    sem_destroy(&greenlightMutex);
    sem_destroy(&damageMutex);
    free(connections);
    exit(0);
}

void handle_sigint_on_admin_connection(int sig) { breachHull(); }

void breachHull() {
    printf(
        "The hull has been breached! Reporting to portuary administrator...\n");
    char message[4] = "DMG";
    write(connections->adminfd, message, 3);
    close(connections->adminfd);
    sem_destroy(&commitMutex);
    sem_destroy(&greenlightMutex);
    sem_destroy(&damageMutex);
    free(myBoat);
    free(connections);
    exit(0);
}

void finish_client() {
    printf("We have reached the port! Closing client...\n");
    char message[4] = "BYE";
    write(connections->adminfd, message, 3);
    close(connections->adminfd);
    sem_destroy(&commitMutex);
    sem_destroy(&greenlightMutex);
    sem_destroy(&damageMutex);
    free(myBoat);
    free(connections);
    exit(0);
}
