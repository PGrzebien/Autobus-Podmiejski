# Temat 12: Autobus podmiejski

**Autor:** Patryk Grzebień  
**Nr albumu:** 139922  
**Link do repozytorium:** https://github.com/PGrzebien/Autobus-Podmiejski

## 1. Opis zadania
Projekt realizuje symulację węzła komunikacyjnego w architekturze wieloprocesowej na systemie Linux. System jest w pełni zdecentralizowany – każdy autobus, pasażer oraz kasa to autonomiczne procesy. Głównym celem była synchronizacja dostępu do ograniczonego zasobu (pojedynczy peron) oraz zapewnienie stabilności przy dużym obciążeniu (Stress Test).

## 2. Mechanizmy Systemowe (IPC)
System wykorzystuje mechanizmy IPC Systemu V oraz standard POSIX:

* **Procesy (fork/exec):** Flota autobusów, Generator oraz Kasa działają jako niezależne jednostki.
* **Wątki (pthread):** Zastosowano model hybrydowy. Pasażer podróżujący z dzieckiem tworzy wątek pomocniczy, co gwarantuje spójność grupy ("wchodzimy razem albo wcale").
* **Pamięć Współdzielona (shm):** Przechowuje stan globalny stacji (np. flaga blokady wejścia `is_station_open`, licznik `active_passengers`) oraz PID autobusu na peronie dla celowanej sygnalizacji.
* **Semafory (System V):**
    * `SEM_PLATFORM`: Mutex peronu (tylko 1 autobus naraz).
    * `SEM_MUTEX`: Ochrona sekcji krytycznej pamięci przed wyścigami (Race Conditions).
    * `SEM_DOOR_1/2`: Liczniki przepustowości drzwi.
    * `SEM_LIMIT`: Zabezpieczenie systemu przed przeciążeniem tablicy procesów (limit globalny).
* **Kolejki Komunikatów (msg):** Obsługa zakupu biletów. Pasażerowie wysyłają żądanie i czekają na dedykowaną odpowiedź (filtrowanie po PID).
* **Sygnały:**
    * `SIGINT`: Bezpieczne zatrzymanie systemu (Cleanup).
    * `SIGUSR1`: Wymuszenie odjazdu autobusu przez Dyspozytora.
    * `SIGTERM`: Wewnętrzny sygnał do zamykania procesów potomnych.

## 3. Kluczowe algorytmy i rozwiązania
W projekcie zaimplementowano zaawansowane mechanizmy sterowania:

1. **Graceful Shutdown & Zombie Reaper:** Program główny nie zabija procesów brutalnie (`SIGKILL`). Wysyła sygnał `SIGTERM`, a następnie w pętli `wait()` oczekuje na ich poprawne zakończenie. Zapobiega to powstawaniu procesów "Zombie" i uszkodzeniom pamięci.
2. **Auto-Shutdown (Watchdog):** Dedykowany proces monitorujący w tle sprawdza, czy generator zakończył pracę i czy wszyscy pasażerowie zostali obsłużeni. Po wykryciu końca pracy system samoczynnie się zamyka.
3. **Priorytet VIP (Anti-Starvation):** Zastosowano mechanizm sprawiedliwości. Jeśli VIP czeka na wejście (zmienna `vips_waiting`), zwykli pasażerowie ustępują mu pierwszeństwa na poziomie semafora.

## 4. Scenariusze Testowe (Weryfikacja)
System przeszedł weryfikację w 7 rygorystycznych scenariuszach (pełny opis w pliku `TESTY.md`):

1. **Pusta Stacja:** Weryfikacja odporności warstwy kontrolnej. Próba wysłania sygnału odjazdu, gdy peron jest pusty, kończy się komunikatem błędu, a nie awarią systemu (walidacja PID w pamięci dzielonej).
2. **Blokada Dworca:** Sprawdzenie reakcji na zamknięcie wejścia przez Dyspozytora. Pasażerowie poprawnie gromadzą się w poczekalni (Active Waiting) i nie wchodzą do strefy krytycznej do momentu otwarcia.
3. **Zator na peronie:** Test ochrony sekcji krytycznej. Wymuszenie odjazdu w trakcie wsiadania pasażera nie przerywa transakcji – autobus czeka na zwolnienie Mutexu.
4. **Atomowość Grupy:** Test dla pasażera z dzieckiem. Przy tylko 1 wolnym miejscu system odmawia wejścia całej parze, zapobiegając rozdzieleniu rodzica od dziecka.
5. **Priorytet VIP:** Weryfikacja mechanizmu anty-zagłodzeniowego. W sytuacji tłoku VIP-y są obsługiwane poza kolejnością, wyprzedzając pasażerów oczekujących dłużej.
6. **Stress Test (Zero-Delay):** Test przeciążeniowy przy usuniętych opóźnieniach czasowych. Potwierdzono szczelność semaforów – autobus nigdy nie zabiera więcej osób niż wynosi limit (brak overbookingu).
7. **Integralność Danych:** Audyt końcowy wykazał 100% zgodności między liczbą sprzedanych biletów a liczbą pasażerów, którzy fizycznie wsiedli do autobusów.

## 5. Obsługa Błędów
Wdrożono rygorystyczną obsługę błędów:
* Każde wywołanie `fork()` jest sprawdzane (ochrona przed limitem procesów).
* Operacje na semaforach są odporne na przerwania sygnałami (`EINTR`) dzięki pętlom ponawiającym.
* Funkcja `cleanup` ignoruje `SIGTERM` w procesie głównym, aby uniknąć przypadkowego "samobójstwa" podczas zamykania grupy procesów.
