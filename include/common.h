#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

// --- KLUCZE IPC ---
#define SHM_KEY 123456
#define SEM_KEY 654321

// --- INDEKSY SEMAFORÓW ---
#define SEM_MUTEX 0    // Chroni pamięć dzieloną
#define SEM_DOOR_1 1   // Drzwi 1
#define SEM_DOOR_2 2   // Drzwi 2
#define SEM_PLATFORM 3 // Dostęp do peronu (tylko 1 autobus naraz)
#define SEM_LIMIT    4 // Limit procesów w systemie

// --- KONFIGURACJA ---
#define N_BUSES 3
#define P_CAPACITY 500
#define R_BIKES 300
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
    int current_passengers;  // Ile osób w obecnym autobusie
    int current_bikes;       // Ile rowerów w obecnym autobusie
    int is_at_station;       // 1 = stoi, 0 = jedzie
    int total_travels;       // Statystyka kursów
    int is_station_open;     // 1 = otwarte, 0 = zamknięte (blokada)
    pid_t bus_at_station_pid;// PID autobusu przy peronie
    int vips_waiting;        // Liczba VIP-ów w kolejce
    int force_departure_signal; // Flaga sygnału odjazdu (zachowujemy dla SIGUSR1)
    int active_passengers;   // Liczba żywych procesów pasażerów
    int generator_done;      // 1 = koniec tworzenia nowych pasażerów
} BusState;

// --- KOLEJKI KOMUNIKATÓW ---
#define MSG_KEY 0x1234  // Klucz kolejki
#define MSG_TYPE_REQ 1  // Typ wiadomości: Żądanie biletu

// Struktura wiadomości
struct TicketMsg {
    long mtype;      // Typ: 1 = żądanie, PID = odpowiedź
    pid_t passenger_pid; 
    int age;        
};
// Wielkość danych
#define MSG_SIZE (sizeof(TicketMsg) - sizeof(long))

#endif
