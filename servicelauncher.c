#include <stdio.h>
#include <stdlib.h>

int main() {
    const char *commands[] = {
        "./portadmin",
        "./senae",
        "./sri",
        "./supercia"
    };

    for (int i = 0; i < 4; i++) {
        char command[256];

        snprintf(command, sizeof(command), "gnome-terminal -- bash -c '%s; exec bash'", commands[i]);

        int result = system(command);
        if (result != 0) {
            fprintf(stderr, "Failed to open terminal for: %s\n", commands[i]);
        }
    }

    return 0;
}
