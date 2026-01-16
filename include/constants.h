#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <sys/types.h>

// Typy pasażerów
typedef enum {
    REGULAR,
    BIKER,
    VIP,
    CHILD_WITH_GUARDIAN
} PassengerType;

// Struktura opisująca pasażera w systemie
typedef struct {
    pid_t pid;
    PassengerType type;
    int has_ticket;
} Passenger;

// Struktura stanu autobusu w pamięci dzielonej (IPC)
typedef struct {
    int current_passengers;
    int current_bikes;
    int is_at_station; // 1 jeśli stoi na dworcu, 0 jeśli w trasie
} BusState;

#endif
