	#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <time.h>
#include "../include/common.h"
#include "../include/utils.h"

// --- PALETA KOLORÓW (UX/UI) ---
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"

// Unia dla semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Zmienne globalne
int shmid, semid, msgid;
pid_t bus_pids[N_BUSES]; // Tablica dla całej floty
pid_t gen_pid = 0;
pid_t kasa_pid = 0;

// Funkcja sprzątająca (SIGINT)
void cleanup(int sig) {
    printf("\n" C_BOLD C_RED "=========================================" C_RESET "\n");
    printf(C_BOLD C_RED "[System] OTRZYMANO SYGNAŁ ZAMKNIĘCIA!" C_RESET "\n");
    
    // Zabijanie całej floty autobusów
    for (int i = 0; i < N_BUSES; i++) {
        if (bus_pids[i] > 0) kill(bus_pids[i], SIGTERM);
    }
    
    if (gen_pid > 0) kill(gen_pid, SIGTERM);
    if (kasa_pid > 0) kill(kasa_pid, SIGTERM);

    if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("Błąd usuwania SHM");
    if (semctl(semid, 0, IPC_RMID) == -1) perror("Błąd usuwania SEM");
    if (msgctl(msgid, IPC_RMID, NULL) == -1) perror("Błąd usuwania MSG");
    
    printf(C_RED "[System] Zasoby wyczyszczone. Do widzenia." C_RESET "\n");
    printf(C_BOLD C_RED "=========================================" C_RESET "\n");
    exit(0);
}

void validate_parameters() {
    if (R_BIKES > P_CAPACITY) {
        fprintf(stderr, C_BOLD C_RED "BŁĄD KRYTYCZNY: Liczba rowerów (%d) > Miejsc (%d)!\n" C_RESET, R_BIKES, P_CAPACITY);
        exit(1);
    }
}

// Funkcja Generatora (Proces Potomny)
void run_generator() {
    srand(time(NULL) ^ getpid());
    log_action(C_CYAN "[Generator] Start fabryki pasażerów (PID: %d)" C_RESET, getpid());
    signal(SIGCHLD, SIG_IGN); 

    int i = 0;
    while (i < 500) {
        usleep((rand() % 1000 + 500) * 1000); 

        int r = rand() % 100;
        int type_id;
        if (r < 1) type_id = 2;       // VIP (1%)
        else if (r < 15) type_id = 3; // DZIECKO (ok. 14%)
        else if (r < 40) type_id = 1; // ROWERZYSTA (25%)
        else type_id = 0;             // NORMALNY (60%)

        char type_str[10];
        sprintf(type_str, "%d", type_id);

        semaphore_p(semid, SEM_LIMIT);

        pid_t p_pid = fork();
        if (p_pid == 0) {
            execl("./pasazer", "pasazer", type_str, NULL);
            perror("Błąd execl pasażera");
            exit(1);
        }
        i++;
    }
    exit(0);
}

// Menu Dyspozytora
void run_dispatcher() {
    printf(C_BOLD C_BLUE "\n############################################\n");
    printf("#      PANEL STEROWANIA DYSPOZYTORA         #\n");
    printf("############################################\n" C_RESET);
    printf(C_BOLD C_YELLOW " [1]" C_RESET " -> Wymuś odjazd autobusu (SIGUSR1)\n");
    printf(C_BOLD C_YELLOW " [2]" C_RESET " -> Otwórz/Zamknij dworzec (Blokada)\n");
    printf(C_BOLD C_RED    " [q]" C_RESET " -> Zakończ symulację\n");
    printf(C_BLUE "--------------------------------------------\n" C_RESET);

    BusState* bus = (BusState*)shmat(shmid, NULL, 0);
    char cmd;
    
    while (std::cin >> cmd) {
        if (cmd == '1') {
    log_action(C_BOLD C_MAGENTA "[Dyspozytor] >>> WYMUSZONO ODJAZD! <<<" C_RESET);
    if (bus->is_at_station && bus->bus_at_station_pid > 0) {
        kill(bus->bus_at_station_pid, SIGUSR1);
    } else {
        log_action(C_RED "[System] Brak autobusu na peronie!" C_RESET);
    }
}

        else if (cmd == '2') {
            bus->is_station_open = !bus->is_station_open;
            if (bus->is_station_open) 
                log_action(C_BOLD C_GREEN "[Dyspozytor] Dworzec OTWARTY dla podróżnych." C_RESET);
            else 
                log_action(C_BOLD C_RED "[Dyspozytor] Dworzec ZAMKNIĘTY (Blokada wejścia)." C_RESET);
        }
        else if (cmd == 'q') break;
    }
    shmdt(bus);
}

int main() {
    signal(SIGINT, cleanup);
    validate_parameters();

    // 1. IPC
    shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget init");
    
    BusState* bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "shmat init");
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_station_open = 1;
    bus->total_travels = 0;
    shmdt(bus);

    semid = semget(SEM_KEY, 5, IPC_CREAT | 0666);
    check_error(semid, "semget init");

    union semun arg;
    arg.val = 1; semctl(semid, SEM_MUTEX, SETVAL, arg);
    arg.val = 1; semctl(semid, SEM_PLATFORM, SETVAL, arg); 
    arg.val = 0; semctl(semid, SEM_DOOR_1, SETVAL, arg); 
                 semctl(semid, SEM_DOOR_2, SETVAL, arg);     
    arg.val = 200; // Limit: max 200 pasażerów w systemie naraz
    semctl(semid, SEM_LIMIT, SETVAL, arg);

    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    check_error(msgid, "msgget init");

    log_action(C_BOLD C_GREEN "[System] INICJALIZACJA ZAKOŃCZONA. Uruchamiam procesy..." C_RESET);

    // 2. Procesy
    kasa_pid = fork();
    if (kasa_pid == 0) {
        execl("./kasa", "kasa", NULL);
        perror("Błąd execl kasa");
        exit(1);
    }

    // --- URUCHAMIAMY FLOTĘ N_BUSES ---
    for (int i = 0; i < N_BUSES; i++) {
        char bus_nr[10];
        sprintf(bus_nr, "%d", i + 1);
        bus_pids[i] = fork();
        if (bus_pids[i] == 0) {
            execl("./autobus", "autobus", bus_nr, NULL);
            perror("Błąd execl autobus");
            exit(1);
        }
        usleep(50000); // Mały odstęp przy starcie
    }

    gen_pid = fork();
    if (gen_pid == 0) {
        run_generator();
        exit(0);
    }

    // 3. Menu
    run_dispatcher();

    cleanup(0);
    return 0;
}
