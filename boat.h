typedef enum BoatType{
    CONVENTIONAL,
    PANAMAX
}BoatType;

typedef struct Boat{
    BoatType type;
    double avg_weight;
    char *destination;
}Boat;