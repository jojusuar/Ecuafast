#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

void sigint_handler(int);

int main() {
    struct sigaction sigintAction;
    sigintAction.sa_handler = sigint_handler;
    sigintAction.sa_flags = 0; // No special flags
    sigemptyset(&sigintAction.sa_mask);
    if (sigaction(SIGINT, &sigintAction, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    for (int i = 0; i < 100; i++) {
        if (fork() == 0) {
            srand((unsigned int)(time(NULL) ^ getpid()));
            int class = rand() % 2;
            char classstr[2];
            sprintf(classstr, "%d", class);
            double weight = 1 + (rand() % 100);
            char weightstr[12];
            sprintf(weightstr, "%f", weight);
            int destination = 1 + rand() % 3;
            char *dest;

            switch (destination) {
            case 1:
                dest = "ecuador";
                break;
            case 2:
                dest = "usa";
                break;
            case 3:
                dest = "europe";
                break;
            default:
                dest = "unknown";
                break;
            }

            int timeout = 5;
            char timeoutstr[5];
            sprintf(timeoutstr, "%d", timeout);
            printf("Boat class: %s, weight: %s, destination: %s, timeout: %s\n",
                   classstr, weightstr, dest, timeoutstr);
            char *argv[] = {"./ecuafast", "-c", classstr, "-w",       weightstr,
                            "-d",         dest, "-t",     timeoutstr, NULL};
            execvp(argv[0], argv);
            perror("execvp");
            exit(1);
        }

        usleep(5000000);
    }
    while (wait(NULL) > 0);
    return 0;
}

void sigint_handler(int signum) {
    printf("Boat generation stopped.\n");
    exit(0);
}