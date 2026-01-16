#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <sys/types.h>

// Klucz dla pamięci dzielonej (musi być unikalny w systemie)
#define SHM_KEY 123456

// Typy pasażerów
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

// Deklaracja funkcji narzędziowej (użyjemy jej w main)
void check_error(int result, const char* msg);

#endif
