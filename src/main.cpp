#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include "../include/common.h"
#include "../include/utils.h"

// Unia dla semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Zmienne globalne (potrzebne do sprzątania)
int shmid, semid;
pid_t bus_pid = 0;
pid_t gen_pid = 0;

// Funkcja sprzątająca (SIGINT - Ctrl+C)
void cleanup(int sig) {
    printf("\n[System] Zamykanie symulacji...\n");
    
    // Zabijamy procesy potomne
    if (bus_pid > 0) kill(bus_pid, SIGTERM);
    if (gen_pid > 0) kill(gen_pid, SIGTERM);

    // Usuwamy pamięć i semafory
    if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("Błąd usuwania SHM");
    if (semctl(semid, 0, IPC_RMID) == -1) perror("Błąd usuwania SEM");
    
    printf("[System] Zasoby wyczyszczone. Koniec.\n");
    exit(0);
}

// Funkcja Generatora (Proces Potomny)
// Zamiast symulować, tworzy nowe procesy pasażerów
void run_generator() {
    srand(time(NULL) ^ getpid());
    log_action("[Generator] Start procesu generowania (PID: %d)", getpid());

    while (true) {
        // Losowy odstęp czasu
        usleep((rand() % 1000 + 500) * 1000); 

        // Losowanie typu pasażera
        int type_id = rand() % 4; // 0-3
        char type_str[2];
        sprintf(type_str, "%d", type_id);

        // FORK + EXEC (Tworzenie pasażera)
        pid_t p_pid = fork();
        if (p_pid == 0) {
            // To jest proces pasażera - podmieniamy kod na plik wykonywalny
            execl("./pasazer", "pasazer", type_str, NULL);
            
            // Jeśli execl zawiedzie:
            perror("Błąd execl pasażera");
            exit(1);
        }
        
        // Proces generatora wraca do pętli, by stworzyć kolejnego
    }
}

int main() {
    // Rejestracja sprzątania
    signal(SIGINT, cleanup);

    // 1. Tworzenie pamięci dzielonej (IPC_CREAT)
    shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget init");
    
    // Inicjalizacja pamięci zerami
    BusState* bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "shmat init");
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_station_open = 1;
    bus->total_travels = 0;
    shmdt(bus);

    // 2. Tworzenie semaforów
    semid = semget(SEM_KEY, 3, IPC_CREAT | 0666);
    check_error(semid, "semget init");

    // Ustawienie wartości początkowych
    union semun arg;
    arg.val = 1; 
    semctl(semid, SEM_MUTEX, SETVAL, arg); // Mutex otwarty
    
    arg.val = 0; 
    semctl(semid, SEM_DOOR_1, SETVAL, arg); // Drzwi 1 zamknięte
    semctl(semid, SEM_DOOR_2, SETVAL, arg); // Drzwi 2 zamknięte

    log_action("[System] Zasoby IPC gotowe.");

    // 3. Uruchomienie Autobusu (FORK + EXEC)
    bus_pid = fork();
    if (bus_pid == 0) {
        execl("./autobus", "autobus", NULL);
        perror("Błąd execl autobus"); // To się wykona tylko przy błędzie
        exit(1);
    }

    // 4. Uruchomienie Generatora (FORK)
    gen_pid = fork();
    if (gen_pid == 0) {
        run_generator();
        exit(0);
    }

    // 5. Pętla oczekiwania
    log_action("[System] Uruchomiono procesy. Czekam na Ctrl+C...");
    while (true) {
        pause(); // Czeka na sygnały (np. zakończenie)
    }

    return 0;
}
