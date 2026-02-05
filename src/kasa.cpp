#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include "../include/common.h"
#include "../include/utils.h"

// --- KOLORY ---
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_YELLOW  "\033[33m"

int msgid;

int main() {
    // 1. Podłączenie do kolejki (Kasa ją tworzy)
    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("Błąd msgget w Kasie");
        exit(1);
    }

    log_action(C_BOLD C_YELLOW "[Kasa] Otwieram okienko. Czekam na klientów..." C_RESET);

    TicketMsg msg;
    while (true) {
        // 2. Odbierz żądanie (czekaj na typ 1)
        // msgrcv jest blokujące - kasa śpi jak nie ma klientów
        if (msgrcv(msgid, &msg, MSG_SIZE, MSG_TYPE_REQ, 0) == -1) {
            perror("Błąd msgrcv");
            break; 
        }

        // 3. Obsługa klienta (Symulacja czasu pracy)
        // Kasa obsługuje szybko (np. 10ms), ale przy 5000 osób zrobi się zator!
        usleep(10000);

        // Logowanie (opcjonalne, żeby nie zamulić terminala przy 5000)
        // log_action("[Kasa] Sprzedano bilet dla PID: %d", msg.passenger_pid);

        // 4. Odeślij potwierdzenie
        // Ustawiamy mtype na PID pasażera, żeby tylko on to odebrał!
        msg.mtype = msg.passenger_pid;
        
        if (msgsnd(msgid, &msg, MSG_SIZE, 0) == -1) {
            perror("Błąd msgsnd (odpowiedź)");
        }
    }

    return 0;
}
