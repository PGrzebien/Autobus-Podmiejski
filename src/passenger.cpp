#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include "../include/common.h"
#include "../include/utils.h"

// --- PALETA KOLORÓW ---
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"

int shmid, semid, msgid;
BusState* bus = nullptr;

void init_resources() {
    shmid = shmget(SHM_KEY, sizeof(BusState), 0666);
    check_error(shmid, "[Pasażer] Błąd shmget");
    bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "[Pasażer] Błąd shmat");
    semid = semget(SEM_KEY, 3, 0666);
    check_error(semid, "[Pasażer] Błąd semget");

    msgid = msgget(MSG_KEY, 0666);
    if (msgid == -1) {
        perror("[Pasażer] Błąd msgget - Kasa nie działa!");
        exit(1);
    }
}

void board_bus(PassengerType type, pid_t pid, int age) {
    // 1. Walidacja wieku (Dziecko < 8 lat)
    if (age < 8) {
        if (rand() % 100 < 10) {
            log_action(C_BOLD C_RED "[Kontrola] Dziecko (%d lat, PID: %d) bez opiekuna! ODMOWA." C_RESET, age, pid);
            return;
        }
    }

    // 2. Bilet
    if (type == VIP) {
        log_action(C_BOLD C_MAGENTA "[Pasażer %d] VIP (%d lat) - mam bilet, wchodzę BEZ KOLEJKI." C_RESET, pid, age);
    } else {
        TicketMsg msg;
        msg.mtype = MSG_TYPE_REQ;
        msg.passenger_pid = pid;
        msg.age = age;

        // Wyślij zapytanie do Kasy
        if (msgsnd(msgid, &msg, MSG_SIZE, 0) == -1) {
            perror("Błąd msgsnd (zakup)");
            return;
        }

        // Czekaj na odpowiedź (Blokada procesu)
        if (msgrcv(msgid, &msg, MSG_SIZE, pid, 0) == -1) {
            perror("Błąd msgrcv (odbiór biletu)");
            return;
        }

        log_action("[Kasa] Pasażer %d (%d lat) odebrał bilet.", pid, age);
    }

    // 3. Dworzec
    semaphore_p(semid, SEM_MUTEX);
    if (!bus->is_station_open) {
        log_action(C_RED "[Pasażer %d] Dworzec ZAMKNIĘTY. Rezygnuję." C_RESET, pid);
        semaphore_v(semid, SEM_MUTEX);
        exit(0);
    }
    semaphore_v(semid, SEM_MUTEX);

    // 4. Drzwi
    int door_sem = (type == BIKER) ? SEM_DOOR_2 : SEM_DOOR_1;
    semaphore_p(semid, door_sem);

    // 5. Wsiadanie
    semaphore_p(semid, SEM_MUTEX);
    
    bool can_enter = true;
    const char* reason = "";
    int seats_needed = 1;

    // --- LOGIKA MIEJSC ---
    if (age < 8) {
        seats_needed = 2; // Dziecko + Opiekun
    }

    // Sprawdzanie limitów
    if (bus->current_passengers + seats_needed > P_CAPACITY) {
        can_enter = false; 
        reason = "Autobus PEŁNY (Brak miejsc dla grupy)";
    } 
    else if (type == BIKER && bus->current_bikes >= R_BIKES) {
        can_enter = false;
        reason = "Brak miejsca na ROWER";
    }

    if (can_enter) {
        bus->current_passengers += seats_needed;
        if (type == BIKER) bus->current_bikes++;
        
        // LOGOWANIE ZALEŻNE OD TYPU
        if (age < 8) {
            // Dziecko (Cyjan)
            log_action(C_BOLD C_GREEN "[Wejście] " C_CYAN "DZIECKO + OPIEKUN (PID: %d, %d lat)" C_GREEN ". Stan: %d/%d" C_RESET, 
                   pid, age, bus->current_passengers, P_CAPACITY);
        } 
        else if (type == BIKER) {
            // Rowerzysta (Niebieski)
            log_action(C_BOLD C_GREEN "[Wejście] " C_BLUE "ROWERZYSTA (PID: %d, %d lat)" C_GREEN ". Stan: %d/%d (Rowery: %d/%d)" C_RESET, 
                   pid, age, bus->current_passengers, P_CAPACITY, bus->current_bikes, R_BIKES);
        }
        else {
            // Normalny/VIP (Zielony)
            log_action(C_BOLD C_GREEN "[Wejście] Pasażer %d (%d lat). Stan: %d/%d (Rowery: %d/%d)" C_RESET, 
                   pid, age, bus->current_passengers, P_CAPACITY, bus->current_bikes, R_BIKES);
        }
    } else {
        log_action(C_BOLD C_RED "[ODMOWA] PID: %d (%d lat) -> %s! Rezygnuję." C_RESET, pid, age, reason);
    }

    semaphore_v(semid, SEM_MUTEX);
    if (!can_enter) exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) exit(EXIT_FAILURE);
    int type_id = atoi(argv[1]);
    PassengerType type = static_cast<PassengerType>(type_id);
    pid_t my_pid = getpid();
    
    srand(my_pid * time(NULL));
    init_resources();

    int age;
    if (type == CHILD_WITH_GUARDIAN) {
        age = rand() % 8; 
    } else {
        age = 8 + (rand() % 75); 
    }

    board_bus(type, my_pid, age);
    shmdt(bus);
    return 0;
}
