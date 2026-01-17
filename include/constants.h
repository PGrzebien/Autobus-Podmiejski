#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <sys/types.h>

// Klucze IPC
#define SHM_KEY 123456
#define SEM_KEY 654321

// Indeksy semaforów w zestawie
#define SEM_MUTEX 0    // Chroni dostęp do pamięci dzielonej (BusState)
#define SEM_DOOR_1 1   // Symuluje pierwsze wejście
#define SEM_DOOR_2 2   // Symuluje drugie wejście

// Parametry symulacji
#define N_BUSES 3
#define P_CAPACITY 15
#define R_BIKES 5
#define T_WAIT 10

typedef enum {
    REGULAR,
    BIKER,
    VIP,
    CHILD_WITH_GUARDIAN
} PassengerType;

typedef struct {
    pid_t pid;
    PassengerType type;
    int has_ticket;
} Passenger;

typedef struct {
    int current_passengers;
    int current_bikes;
    int is_at_station;
} BusState;

void check_error(int result, const char* msg);

#endif
