#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <sys/types.h>

// Klucze IPC
#define SHM_KEY 123456

// Parametry symulacji
#define N_BUSES 3         // Liczba autobusów (N)
#define P_CAPACITY 15     // Ogólna pojemność autobusu (P)
#define R_BIKES 5         // Limit miejsc na rowery (R)
#define T_WAIT 10         // Czas oczekiwania na przystanku (T)

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

// Deklaracja funkcji narzędziowej
void check_error(int result, const char* msg);

#endif
