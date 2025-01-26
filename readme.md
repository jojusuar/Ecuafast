# Ecuafast
Operating Systems project by José Julio Suárez

## Compilation
Execute `$ make` on a Bash terminal.

## Simple execution example
1. Execute `$ ./servicelauncher` on a Bash terminal.
2. Execute `$ ./boatgenerator` on a Bash terminal.

## Advanced execution
1. Execute `$ ./sri [-f] <response latency floor> [-t] <response latency ceiling>`.
2. Execute `$ ./senae [-f] <response latency floor> [-t] <response latency ceiling>`.
3. Execute `$ ./supercia [-f] <response latency floor> [-t] <response latency ceiling>`.
4. Execute `$ ./portadmin [-n] <number of concurrently unloaded boats>`.
5. Execute `$ ./ecuafast [-c] <boat class> [-w] <avg. weight> [-d] <destination> [-t] <response timeout (seconds)> [-u] <cargo unloading time (seconds)>`.

## Usage
When the services are up and `./ecuafast` is executed, the Ecuafast client
will display on the terminal the status of its requests to the various agencies' servers
and the port administratior server. Shortly after, the docking queue and all
updates to it will be displayed. When the boat reaches the front of the queue, the client 
will inform it through the terminal and terminate itself.

Likewise, the terminal window of each server (agency or port) will display
status updates. Press Ctrl+C to gracefully close them at any time.
