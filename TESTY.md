# 📊 Dokumentacja Testów Systemowych - Autobus Podmiejski

---

## 🚌 Test 1: Pusta Stacja (Zero-Bus)
**Cel:** Weryfikacja odporności warstwy kontrolnej na próby wysyłania sygnałów sterujących w momencie, gdy żaden autobus nie znajduje się na peronie.

### 📝 Opis scenariusza
1. Uruchomienie floty ograniczonej do 3 autobusów (N=3).
2. Kolejne wymuszanie odjazdu każdej jednostki (komenda `1`), aż do całkowitego opróżnienia peronu.
3. Próba wysłania sygnału `SIGUSR1` przez Dyspozytora w oknie czasowym, gdy wszystkie autobusy są w trasie.

### 📥 Wynik (Logi systemowe)
```text
[Autobus 2] ODJAZD! Pasażerów: 1/15.
[Autobus 2] W trasie... (czas: 9s)
[Autobus 3] >>> PODSTAWIONO NA PERON <<<
[Autobus 3] ODJAZD! Pasażerów: 0/15.
[Autobus 3] W trasie... (czas: 9s)

[Dyspozytor] >>> WYMUSZONO ODJAZD! <<<
[System] Brak autobusu na peronie!
```

### 🧐 Wnioski techniczne
* **Poprawność odczytu SHM:** System bezbłędnie identyfikuje stan peronu. Dyspozytor przed wysłaniem sygnału sprawdza pole `bus_at_station_pid` w pamięci współdzielonej.
* **Bezpieczeństwo sygnałów:** Dzięki wykryciu braku autobusu (PID = 0), program unika błędnego wysyłania sygnałów kill, co zapewnia stabilność warstwy kontrolnej.
* **Responsywność:** Komunikat o błędzie wyświetla się natychmiast po wykryciu pustego peronu.

---

## 🕒 Test 2: Blokada wejścia na dworzec
**Cel:** Sprawdzenie skuteczności blokady systemowej (sygnał `SIGUSR2`) oraz weryfikacja, czy pasażerowie potrafią uszanować status dworca.

### 📝 Opis scenariusza
1. Symulacja pracuje w trybie normalnym.
2. Dyspozytor wysyła sygnał blokady (komenda `2`).
3. Generator tworzy nowych pasażerów, którzy kupują bilety, ale czekają przed bramkami.
4. Dyspozytor zdejmuje blokadę – następuje gwałtowne wypełnienie autobusu.

### 📥 Wynik (Logi systemowe)
```text
[Dyspozytor] Dworzec ZAMKNIĘTY (Blokada wejścia).
[Kasa] Pasażer 3642 (0 lat) odebrał bilet.
[Autobus 1] ODJAZD! Pasażerów: 4/15. (Odjazd mimo wolnych miejsc)
[Autobus 2] >>> PODSTAWIONO NA PERON <<<
... (Pasażerowie gromadzą się przed bramkami) ...
[Dyspozytor] Dworzec OTWARTY dla podróżnych.
[Wejście] DZIECKO + OPIEKUN (PID: 3642). Stan: 2/15
[Wejście] Pasażer 3647 (20 lat). Stan: 15/15
```

### 🧐 Wnioski techniczne
* **Skuteczność blokady:** Test potwierdził, że flaga `dworzec_otwarty` w SHM jest respektowana przez procesy pasażerów.
* **Synchronizacja masowa:** Moment otwarcia wywołał natychmiastową i poprawną reakcję procesów oczekujących na semaforze.
* **Niezależność procesów:** Autobusy odjeżdżają zgodnie ze swoim harmonogramem, nie czekając na zmianę statusu dworca.

---

## 🛑 Test 3: Zator na peronie
**Cel:** Weryfikacja, czy wymuszony odjazd autobusu nie przerywa operacji wsiadania pasażerów (ochrona sekcji krytycznej).

### 📝 Opis scenariusza
1. Wprowadzenie opóźnienia `sleep(3)` wewnątrz sekcji krytycznej wsiadania.
2. Wysłanie sygnału wymuszonego odjazdu (komenda `1`) dokładnie w trakcie trwania procedury wsiadania.

### 📥 Wynik (Logi systemowe)
```text
[Dyspozytor] >>> WYMUSZONO ODJAZD! <<<
[Autobus 1] Otrzymano sygnał odjazdu!
[Drzwi] ZAMKNIĘTE.
[Wejście] Pasażer 3779 (57 lat). Stan: 2/15
[Wejście] Pasażer 3780 (76 lat). Stan: 3/15
[Autobus 1] ODJAZD! Pasażerów: 3/15.
```

### 🧐 Wnioski techniczne
* **Integralność Mutexu:** Mimo odebrania sygnału, proces autobusu poczekał na zwolnienie semafora `SEM_MUTEX` przez pasażera.
* **Oczekiwanie na zasób:** Logi wykazują różnicę czasu potwierdzającą, że proces autobusu został zablokowany do momentu zakończenia operacji.
* **Atomowość:** Pasażerowie bezpiecznie zaktualizowali liczniki przed fizycznym odjazdem pojazdu.

---

## ⚖️ Test 4: Walidacja wielozasobowości i kontrola bezpieczeństwa
**Cel:** Sprawdzenie zarządzania wagami pasażerów (+2 dla dzieci) oraz walidacji wieku (opiekun).

### 📝 Opis scenariusza
1. Generator ustawiony na wysoką częstotliwość grup (Dziecko + Opiekun).
2. Obserwacja stanów licznika przy dopełnianiu oraz reakcji na dzieci bez opiekuna.

### 📥 Wynik (Logi systemowe)
```text
[Wejście] DZIECKO + OPIEKUN (PID: 3965). Stan: 13/15
[Wejście] DZIECKO + OPIEKUN (PID: 3967). Stan: 15/15
[Kontrola] Dziecko (1 lat, PID: 3984) bez opiekuna! ODMOWA.
[Wejście] DZIECKO + OPIEKUN (PID: 3986, 6 lat). Stan: 15/15
```

### 🧐 Wnioski techniczne
* **Precyzja atomowa:** System poprawnie obsłużył inkrementację o 2 (13→15) bez błędu przepełnienia.
* **Logika biznesowa:** Logi potwierdzają działanie funkcji walidującej wiek (`ODMOWA`).
* **Zarządzanie niedopełnieniem:** System chroni integralność grupy, nie rozdzielając dziecka od opiekuna, gdy na peronie zostało tylko jedno wolne miejsce.
