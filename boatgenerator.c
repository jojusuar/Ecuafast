#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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
    printf("Hi! 100 boats will be generated, then the 101th will be opened in "
           "a new window\n for you to see it's progress in the queue.\n\n");
    for (int i = 0; i < 100; i++) {
        printf("Creating boat nbr. %d...\n", i + 1);
        if (fork() == 0) {
            srand((unsigned int)(time(NULL) ^ getpid()));
            int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null == -1) {
                perror("open");
                return 1;
            }

            // Redirect stdout to /dev/null
            if (dup2(dev_null, STDOUT_FILENO) == -1) {
                perror("dup2");
                return 1;
            }
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

        usleep(50000);
    }
    printf("Boats generated. Launching a new window for the last \n one (it "
           "may make the queue faster than other boats).\n\nPress Ctrl+C to "
           "terminate all but the last one.\n");

    char command[256];
    snprintf(command, sizeof(command),
             "gnome-terminal -- bash -c './ecuafast -c 1 -w 22.76 -d ecuador "
             "-t 10; exec bash'");
    system(command);
    while (wait(NULL) > 0)
        ;
    printf("All boats have finished their transactions.\n");
    return 0;
}

void sigint_handler(int signum) {
    printf("Boat generation stopped.\n");
    exit(0);
}