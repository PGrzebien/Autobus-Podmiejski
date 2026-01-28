#ifndef UTILS_H
#define UTILS_H

// Funkcja logująca (ekran + plik)
void log_action(const char* format, ...);

// Obsługa błędów (perror + exit)
void check_error(int result, const char* msg);

// Operacje na semaforach (P - opuść, V - podnieś)
void semaphore_p(int semid, int sem_num);
void semaphore_v(int semid, int sem_num);

#endif
