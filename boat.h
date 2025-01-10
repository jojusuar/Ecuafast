typedef enum BoatType{
    CONVENTIONAL,
    PANAMAX
}BoatType;

typedef struct Boat{
    BoatType type;
    float avg_weight;
    char *destination;
}Boat;