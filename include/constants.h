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
#define LOG_FILE "symulacja.txt"
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
    int total_travels;      // Licznik wykonanych kursów
    int is_station_open;   //(1=otwarte, 0=zamknięte)
} BusState;

void check_error(int result, const char* msg);
void semaphore_p(int semid, int sem_num);
void semaphore_v(int semid, int sem_num);
void log_action(const char* format, ...);
#endif
