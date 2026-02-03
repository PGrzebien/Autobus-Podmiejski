# 🚌 Symulacja Węzła Komunikacyjnego (Autobus Podmiejski)

**Autor:** Patryk Grzebień (139922)  
**Temat:** Temat 12 - Autobus podmiejski

---

## 📖 Opis Projektu
Projekt realizuje wysokowydajną symulację systemu transportowego w środowisku Linux, opartą na architekturze wieloprocesowej. System zarządza flotą autobusów, strumieniem pasażerów oraz kasą biletową, gdzie każda jednostka jest autonomicznym procesem. 

Głównym celem inżynierskim była implementacja bezpiecznej synchronizacji dostępu do zasobów współdzielonych (peron, miejsca w pojeździe) bez użycia wątków, wykorzystując natywne mechanizmy **IPC Systemu V**.

## 🚀 Kluczowe Technologie i Rozwiązania
Projekt wyróżnia się zastosowaniem zaawansowanych mechanizmów systemowych zgodnych z POSIX:

* **Pełna Decentralizacja (`fork`, `exec`):** Brak centralnego "zarządcy" iterującego po obiektach. Każdy autobus i pasażer to niezależny proces z własną przestrzenią adresową.
* **Semafory Systemu V (`semop` z flagą `SEM_UNDO`):**
    * Zabezpieczenie przed zakleszczeniem (deadlock) w przypadku nagłej awarii procesu (system operacyjny automatycznie cofa operacje na semaforze).
    * Semafory licznikowe do sterowania pojemnością i drzwiami.
* **Pamięć Współdzielona (`shm`):** Przechowywanie globalnego stanu stacji oraz PID autobusu stojącego na peronie, co umożliwia celowane wysyłanie sygnałów.
* **Kolejki Komunikatów (`msg queue`):** Asynchroniczna komunikacja na linii Pasażer-Kasa z wykorzystaniem typowania wiadomości (selektywny odbiór po PID).
* **Obsługa Sygnałów:**
    * `SIGUSR1`: Interwencja Dyspozytora (wymuszony odjazd konkretnego pojazdu).
    * `SIGCHLD`: Automatyczne usuwanie procesów potomnych (zapobieganie procesom Zombie).

## 🛠️ Kompilacja i Uruchomienie

### Wymagania wstępne
* **System operacyjny:** Linux (Debian/Ubuntu zalecane) lub WSL.
* **Kompilator:** GCC/G++ wspierający standard C++17.
* **Biblioteki:** Standardowa biblioteka C (`libc`), biblioteka wątków POSIX (`pthread`).

### Instalacja i Pierwsze Kroki
Aby uruchomić projekt na nowym komputerze, wykonaj poniższe komendy w terminalu:

1. **Pobranie projektu:**
   ```
   git clone https://github.com/PGrzebien/Autobus-Podmiejski.git
   cd Autobus-Podmiejski
```
### Instrukcja Kompilacji (Krok po kroku)

1.  **Kompilacja projektu:**
    ```
    make
    ```
    *(Zostaną wygenerowane 4 pliki wykonywalne: system, autobus, pasazer, kasa)*

2.  **Uruchomienie symulacji:**
    ```
    ./system
    ```

3.  **Czyszczenie (usuwanie plików binarnych i logów):**
    ```
    make clean
    ```

### 🎮 Sterowanie (Panel Dyspozytora)
Po uruchomieniu głównego procesu, masz do dyspozycji interaktywne menu:

* `1` - **Wymuszony odjazd:** Wysyła sygnał `SIGUSR1` do autobusu aktualnie stojącego na peronie.
* `2` - **Blokada stacji:** Zmienia flagę w pamięci współdzielonej, uniemożliwiając pasażerom wejście na dworzec.
* `q` - **Zakończenie:** Bezpiecznie zamyka wszystkie procesy i czyści struktury IPC.


## 🧪 Scenariusze Testowe
System został poddany weryfikacji w oparciu o kluczowe scenariusze brzegowe i synchronizacyjne:

| Nr | Nazwa Testu | Scenariusz i Obserwacje | Wnioski Techniczne |
|:--:|:---|:---|:---|
| **1** | **Pusta Stacja (Zero-Bus)** | Próba wymuszenia odjazdu przez Dyspozytora (`SIGUSR1`), gdy peron jest pusty. | System weryfikuje pamięć współdzieloną (`bus_at_station_pid`) i blokuje wysłanie sygnału "w próżnię", zapobiegając błędom systemowym. |
| **2** | **Graceful Shutdown** | Wysłanie sygnału `SIGINT` (Ctrl+C) podczas pełnego obciążenia. | Handler sygnału przechwytuje przerwanie, wysyła `SIGTERM` do potomków i usuwa zasoby IPC (potwierdzone przez `ipcs`). |
| **3** | **Odporność na EINTR** | Wymuszenie odjazdu w trakcie operacji na semaforze (wsiadanie). | Funkcje systemowe są zabezpieczone pętlą `while`, która wznawia oczekiwanie po przerwaniu sygnałem (brak naruszenia sekcji krytycznej). |
| **4** | **Atomowość Grupy** | Próba wejścia Rodzica z Dzieckiem przy 1 wolnym miejscu. | Zastosowano logikę "wszystko albo nic". System blokuje wejście pary, zapobiegając sytuacji rozdzielenia rodziny (poprawna walidacja wielozasobowości). |
| **5** | **Priorytet VIP** | Weryfikacja przepuszczania pasażerów VIP w kolejce przed bramkami. | Mechanizm `vips_waiting` w SHM skutecznie zapobiega zagłodzeniu pasażerów z wysokim priorytetem. |
| **6** | **Stress Test (Zero-Delay)** | Usunięcie opóźnień `sleep` w celu wywołania wyścigu o zasoby (Race Condition). | Semafory Systemu V zachowały atomowość operacji – brak błędów typu Overbooking przy maksymalnym obciążeniu. |
| **7** | **Integralność Danych** | Bilans ilościowy: Porównanie liczby stworzonych procesów z liczbą wejść do pojazdów. | System wykazał 100% spójności; suma biletów i pasażerów VIP zgadza się z całkowitą liczbą obsłużonych osób. |

## 📂 Struktura Projektu

``` 
.
├── .gitignore              # Plik wykluczający pliki binarne z repozytorium
├── Makefile                # Skrypt automatyzacji kompilacji
├── README.md               # Główna dokumentacja projektu
├── TESTY.md                # Dokumentacja scenariuszy testowych (Testy 1-7)
├── include/                # Pliki nagłówkowe
│   ├── common.h            # Struktury IPC (BusState, Semafory) i stałe
│   └── utils.h             # Deklaracje funkcji pomocniczych
└── src/                    # Kody źródłowe
    ├── bus.cpp             # Logika procesu Autobusu
    ├── kasa.cpp            # Logika procesu Kasy (Message Queue)
    ├── main.cpp            # Punkt wejścia (Generator, Dyspozytor)
    ├── passenger.cpp       # Implementacja logiki Pasażera
    └── utils.cpp           # Implementacja narzędzi i obsługi błędów

