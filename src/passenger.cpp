#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sched.h>  // <--- WAŻNE: Potrzebne do sched_yield()
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
    semid = semget(SEM_KEY, 5, 0666);
    check_error(semid, "[Pasażer] Błąd semget");

    msgid = msgget(MSG_KEY, 0666);
    if (msgid == -1) {
        perror("[Pasażer] Błąd msgget");
        exit(1);
    }
}

void* child_thread_behavior(void* arg) {
    return NULL; 
}

void board_bus(PassengerType type, pid_t pid, int age) {
    if (age < 8) {
        if (rand() % 100 < 10) {
            log_action(C_BOLD C_RED "[Kontrola] Dziecko (%d lat, PID: %d) bez opiekuna! ODMOWA." C_RESET, age, pid);
            return;
        }
    }

    if (type == VIP) {
        log_action(C_BOLD C_MAGENTA "[Pasażer %d] VIP (%d lat) - mam bilet, wchodzę BEZ KOLEJKI." C_RESET, pid, age);
    } else {
        TicketMsg msg;
        msg.mtype = MSG_TYPE_REQ;
        msg.passenger_pid = pid;
        msg.age = age;

        if (msgsnd(msgid, &msg, MSG_SIZE, 0) == -1) return;
        if (msgrcv(msgid, &msg, MSG_SIZE, pid, 0) == -1) return;
        
        log_action("[Kasa] Pasażer %d (%d lat) odebrał bilet.", pid, age);
    }
    
    if (type == VIP) {
        semaphore_p(semid, SEM_MUTEX);
        bus->vips_waiting++;
        semaphore_v(semid, SEM_MUTEX);
    }

    bool boarded = false;

    while (!boarded) {
        // 1. CZEKAMY NA DRZWI (Blokada systemowa - proces śpi)
        int door_sem = (type == BIKER) ? SEM_DOOR_2 : SEM_DOOR_1;
        struct sembuf sb = {(unsigned short)door_sem, -1, 0};
        
        if (semop(semid, &sb, 1) == -1) {
            if (errno == EINTR) continue;
            return;
        }
         
        usleep(1);

        // 2. MUTEX
        semaphore_p(semid, SEM_MUTEX);
        
        // A. Sprawdzamy czy autobus nie odjechał / dworzec otwarty
        if (bus->is_at_station == 0 || !bus->is_station_open) {
            semaphore_v(semid, SEM_MUTEX);
            // Jak dworzec zamknięty, to mały oddech żeby nie spalić CPU
            if (!bus->is_station_open) usleep(100000); 
            continue; 
        }

        if (type != VIP && bus->vips_waiting > 0) {
            semaphore_v(semid, SEM_MUTEX);
            struct sembuf v_door = {(unsigned short)door_sem, 1, 0};
            semop(semid, &v_door, 1);
            continue;
        }

        bool can_enter = true;
        int seats_needed = (age < 8) ? 2 : 1;

        if (bus->current_passengers + seats_needed > P_CAPACITY) can_enter = false;
        else if (type == BIKER && bus->current_bikes >= R_BIKES) can_enter = false;

        if (can_enter) {
            bus->current_passengers += seats_needed;
            if (type == BIKER) bus->current_bikes++;
            
            if (age < 8) {
                pthread_t child_tid;
                if (pthread_create(&child_tid, NULL, child_thread_behavior, NULL) == 0) {
                    pthread_join(child_tid, NULL);
                }
            }
            
            if (type == VIP) bus->vips_waiting--;

            // SZYBKIE WEJŚCIE (Bez sleepa)
            if (age < 8) {
                log_action(C_BOLD C_GREEN "[Wejście] DZIECKO + OPIEKUN (PID: %d). Stan: %d/%d" C_RESET, pid, bus->current_passengers, P_CAPACITY);
            } else {
                log_action(C_BOLD C_GREEN "[Wejście] Pasażer %d (%d lat). Stan: %d/%d" C_RESET, pid, age, bus->current_passengers, P_CAPACITY);
            }
            boarded = true; 
        } 
        else {
            struct sembuf v_door = {(unsigned short)door_sem, 1, 0};
            semop(semid, &v_door, 1);
        }

        semaphore_v(semid, SEM_MUTEX);
    }
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

    semaphore_v(semid, SEM_LIMIT); 

    shmdt(bus);
    return 0;
}
