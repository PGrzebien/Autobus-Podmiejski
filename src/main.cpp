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
#include <errno.h> 
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

// Unia dla funkcji semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Globalne identyfikatory zasobów IPC oraz PID procesów
int shmid, semid, msgid;
pid_t bus_pids[N_BUSES]; // Tablica PID procesów autobusów
pid_t gen_pid = 0;
pid_t kasa_pid = 0;
pid_t monitor_pid = 0; 

 //Funkcja obsługi sygnału SIGINT (Ctrl+C) oraz procedura kończąca system.
 //Realizuje bezpieczne zamknięcie wszystkich procesów potomnych i zwolnienie zasobów IPC.
void cleanup(int sig) {
    printf("\n" C_BOLD C_RED "=========================================" C_RESET "\n");
    printf(C_BOLD C_RED "[System] OTRZYMANO SYGNAŁ ZAMKNIĘCIA! Rozpoczynam procedurę wyjścia..." C_RESET "\n");
    
    // Zignorowanie sygnału SIGTERM przez proces macierzysty (Main).
    signal(SIGTERM, SIG_IGN); 

    // 1. Wysłanie sygnału SIGTERM do całej grupy procesów.
    kill(0, SIGTERM);

    // 2. Oczekiwanie na zakończenie wszystkich procesów potomnych (reaping).
    // Pętla wait() zapobiega powstaniu procesów "Zombie" (procesów zakończonych, ale nieodczytanych przez rodzica).
    printf(C_YELLOW "[System] Czekam na zakończenie procesów potomnych..." C_RESET "\n");
    while (wait(NULL) > 0);

    // 3. Zwolnienie zasobów IPC (System V).
    // Wykonywane dopiero po zakończeniu procesów, aby uniknąć błędów dostępu do pamięci.
    if (shmctl(shmid, IPC_RMID, NULL) == -1) perror("Błąd usuwania SHM");
    if (semctl(semid, 0, IPC_RMID) == -1) perror("Błąd usuwania SEM");
    if (msgctl(msgid, IPC_RMID, NULL) == -1) perror("Błąd usuwania MSG");

    printf(C_RED "[System] Zasoby zwolnione. Koniec pracy." C_RESET "\n");
    printf(C_BOLD C_RED "=========================================" C_RESET "\n");

    exit(0);
}

void validate_parameters() {
    if (R_BIKES > P_CAPACITY) {
        fprintf(stderr, C_BOLD C_RED "BŁĄD KRYTYCZNY: Liczba rowerów (%d) > Miejsc (%d)!\n" C_RESET, R_BIKES, P_CAPACITY);
        exit(1);
    }
}


 //Logika procesu Generatora Pasażerów.
 
void run_generator() {
    srand(time(NULL) ^ getpid());
    log_action(C_CYAN "[Generator] Start fabryki pasażerów (PID: %d)" C_RESET, getpid());
    
    // Ignorowanie SIGCHLD zapobiega tworzeniu zombie przez pasażerów, 
    // których generator (jako ich rodzic) nie będzie "zbierał" przez wait().
    signal(SIGCHLD, SIG_IGN); 

    BusState* bus = (BusState*)shmat(shmid, NULL, 0);

    int i = 0;
    // Pętla generująca zadaną liczbę pasażerów
    while (i < 1000) {
        // usleep((rand() % 1000 + 500) * 1000); 

        // Algorytm losowania typu pasażera z wagami prawdopodobieństwa
        int r = rand() % 100;
        int type_id;
        
        if (r < 1) type_id = 2;        // VIP (1%)
        else if (r < 15) type_id = 3;  // DZIECKO Z OPIEKUNEM (ok. 14%)
        else if (r < 40) type_id = 1;  // ROWERZYSTA (25%)
        else type_id = 0;              // PASAŻER STANDARDOWY (60%)

        char type_str[10];
        sprintf(type_str, "%d", type_id);

        // Oczekiwanie na semaforze limitu globalnego (ograniczenie liczby procesów w systemie)
        semaphore_p(semid, SEM_LIMIT);

        // Sekcja krytyczna: Aktualizacja licznika aktywnych pasażerów w pamięci współdzielonej
        semaphore_p(semid, SEM_MUTEX);
        bus->active_passengers++;
        semaphore_v(semid, SEM_MUTEX);

        pid_t p_pid = fork();
        
        // Obsługa błędu funkcji fork() - wymagana dla stabilności systemu
        if (p_pid < 0) {
            perror("Błąd fork pasażera");
            // Wycofanie zmian w liczniku w przypadku niepowodzenia
            semaphore_p(semid, SEM_MUTEX);
            bus->active_passengers--;
            semaphore_v(semid, SEM_MUTEX);
            semaphore_v(semid, SEM_LIMIT); 
        } else if (p_pid == 0) {
            // Proces potomny: Zamiana obrazu procesu na program "pasazer" (exec)
            execl("./pasazer", "pasazer", type_str, NULL);
            perror("Błąd execl pasażera");
            exit(1);
        }
        i++;
    }

    // Sygnalizacja zakończenia generowania (flaga w pamięci współdzielonej)
    semaphore_p(semid, SEM_MUTEX);
    bus->generator_done = 1;
    log_action(C_BOLD C_BLUE "[Generator] Wyprodukowano %d pasażerów. KONIEC ZMIANY." C_RESET, i);
    semaphore_v(semid, SEM_MUTEX);
    
    shmdt(bus);
    exit(0);
}

// Funkcja obsługująca interakcję z użytkownikiem (Dyspozytor)
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
    
    // Pętla odczytu komend ze standardowego wejścia
    while (std::cin >> cmd) {
        if (cmd == '1') {
            log_action(C_BOLD C_MAGENTA "[Dyspozytor] >>> WYMUSZONO ODJAZD! <<<" C_RESET);
            // Sprawdzenie warunków i wysłanie sygnału użytkownika SIGUSR1 do procesu autobusu
            if (bus->is_at_station && bus->bus_at_station_pid > 0) {
                bus->force_departure_signal = 1; 
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
    // Rejestracja obsługi sygnału SIGINT dla bezpiecznego zamknięcia
    signal(SIGINT, cleanup);
    validate_parameters();

    // --- INICJALIZACJA ZASOBÓW IPC (SYSTEM V) ---
    
    // 1. Pamięć Dzielona (Shared Memory)
    shmid = shmget(SHM_KEY, sizeof(BusState), IPC_CREAT | 0666);
    check_error(shmid, "shmget init");
    
    // Dołączenie pamięci do przestrzeni adresowej procesu
    BusState* bus = (BusState*)shmat(shmid, NULL, 0);
    check_error(bus == (void*)-1 ? -1 : 0, "shmat init");
    
    // Inicjalizacja stanu początkowego w pamięci współdzielonej
    bus->current_passengers = 0;
    bus->current_bikes = 0;
    bus->is_station_open = 1;
    bus->total_travels = 0;
    bus->vips_waiting = 0;
    bus->force_departure_signal = 0;
    bus->active_passengers = 0; 
    bus->generator_done = 0;    
    shmdt(bus);

    // 2. Semafory (Semaphores)
    semid = semget(SEM_KEY, 5, IPC_CREAT | 0666);
    check_error(semid, "semget init");

    // Ustawienie wartości początkowych semaforów
    union semun arg;
    arg.val = 1; semctl(semid, SEM_MUTEX, SETVAL, arg);    // Mutex (binarny)
    arg.val = 1; semctl(semid, SEM_PLATFORM, SETVAL, arg); // Dostęp do peronu
    arg.val = 0; semctl(semid, SEM_DOOR_1, SETVAL, arg);   // Drzwi 1 (zamknięte)
                 semctl(semid, SEM_DOOR_2, SETVAL, arg);   // Drzwi 2 (zamknięte)
    arg.val = 6000; 
    semctl(semid, SEM_LIMIT, SETVAL, arg); // Globalny limit procesów

    // 3. Kolejka Komunikatów (Message Queue)
    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    check_error(msgid, "msgget init");

    log_action(C_BOLD C_GREEN "[System] INICJALIZACJA ZAKOŃCZONA. Uruchamiam procesy..." C_RESET);

    // --- URUCHAMIANIE PROCESÓW POTOMNYCH ---

    // Utworzenie procesu Kasy
    kasa_pid = fork();
    if (kasa_pid < 0) {
        perror("Błąd fork kasa");
        cleanup(0);
    } else if (kasa_pid == 0) {
        execl("./kasa", "kasa", NULL);
        perror("Błąd execl kasa");
        exit(1);
    }

    // Utworzenie floty autobusów
    for (int i = 0; i < N_BUSES; i++) {
        char bus_nr[10];
        sprintf(bus_nr, "%d", i + 1);
        bus_pids[i] = fork();
        if (bus_pids[i] < 0) {
            perror("Błąd fork autobus");
            cleanup(0);
        } else if (bus_pids[i] == 0) {
            execl("./autobus", "autobus", bus_nr, NULL);
            perror("Błąd execl autobus");
            exit(1);
        }
        usleep(50000); // Krótkie opóźnienie dla uporządkowania logów startowych
    }

    // Utworzenie procesu Generatora
    gen_pid = fork();
    if (gen_pid < 0) {
        perror("Błąd fork generator");
        cleanup(0);
    } else if (gen_pid == 0) {
        run_generator();
        exit(0);
    }

    // Proces Monitorujący - sprawdza warunki zakończenia symulacji
    monitor_pid = fork();
    if (monitor_pid < 0) {
        perror("Błąd fork monitor");
    } else if (monitor_pid == 0) {
         BusState* b = (BusState*)shmat(shmid, NULL, 0);
         // Pętla sprawdzająca w tle: czy generator skończył I czy wszyscy pasażerowie zniknęli
         while(true) {
             sleep(2);
             semaphore_p(semid, SEM_MUTEX); // Dostęp do pamięci chroniony semaforem
             int active = b->active_passengers;
             int done = b->generator_done;
             semaphore_v(semid, SEM_MUTEX);
             
             if (done == 1 && active == 0) {
                 log_action(C_BOLD C_RED "[AUTO-SHUTDOWN] Wszyscy obsłużeni. Zamykam system za 5 sekund..." C_RESET);
                 sleep(5);
                 kill(getppid(), SIGINT); // Wysłanie sygnału do rodzica (Main), aby rozpoczął procedurę cleanup()
                 exit(0);
             }
         }
    }

    // Uruchomienie interfejsu dyspozytora w procesie głównym
    run_dispatcher();

    cleanup(0);
    return 0;
}
