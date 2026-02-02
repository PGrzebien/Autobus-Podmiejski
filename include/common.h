#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

// --- KLUCZE IPC ---
#define SHM_KEY 123456
#define SEM_KEY 654321

// --- INDEKSY SEMAFORÓW ---
#define SEM_MUTEX 0    // Chroni pamięć dzieloną
#define SEM_DOOR_1 1   // Drzwi 1 (zwykłe/bagaż)
#define SEM_DOOR_2 2   // Drzwi 2 (rowery/wózki)
#define SEM_PLATFORM 3
#define SEM_LIMIT    4
// --- KONFIGURACJA ---
#define N_BUSES 3
#define P_CAPACITY 15
#define R_BIKES 5
#define T_WAIT 10
#define LOG_FILE "symulacja.txt"

// --- STRUKTURY DANYCH ---

// Typy pasażerów
typedef enum {
    REGULAR,
    BIKER,
    VIP,
    CHILD_WITH_GUARDIAN
} PassengerType;

// Struktura pojedynczego pasażera
typedef struct {
    pid_t pid;
    PassengerType type;
    int has_ticket;
} Passenger;

// Stan Autobusu/Dworca (Pamięć Dzielona)
typedef struct {
    int current_passengers;
    int current_bikes;
    int is_at_station;      // 1 = na stacji, 0 = w trasie
    int total_travels;      // Licznik kursów
    int is_station_open;    // 1 = otwarte, 0 = zamknięte (blokada)
    pid_t bus_at_station_pid;
    int vips_waiting;
} BusState;

// --- KOLEJKI KOMUNIKATÓW ---
#define MSG_KEY 0x1234  // Klucz kolejki
#define MSG_TYPE_REQ 1  // Typ wiadomości: Żądanie biletu

// Struktura wiadomości
struct TicketMsg {
    long mtype;     // Typ: 1 = żądanie, PID = odpowiedź dla konkretnego procesu
    pid_t passenger_pid; // Kto prosi
    int age;        // Wiek
};
// Wielkość danych
#define MSG_SIZE (sizeof(TicketMsg) - sizeof(long))


#endif
