#include <stdbool.h>

typedef enum BoatType { CONVENTIONAL, PANAMAX } BoatType;

typedef struct Boat {
    BoatType type;
    double avg_weight;
    char *destination;
    double unloading_time;
    bool toCheck;
    int id;
} Boat;