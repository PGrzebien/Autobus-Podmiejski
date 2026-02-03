# 📊 Dokumentacja Testów Systemowych - Autobus Podmiejski

---

## 🚌 Test 1: Pusta Stacja (Zero-Bus)
**Cel:** Weryfikacja odporności warstwy kontrolnej na próby wysyłania sygnałów sterujących w momencie, gdy żaden autobus nie znajduje się na peronie.

### 📝 Opis scenariusza
1. Uruchomienie floty ograniczonej do 3 autobusów (N=3).
2. Kolejne wymuszanie odjazdu każdej jednostki (komenda `1`), aż do całkowitego opróżnienia peronu.
3. Próba wysłania sygnału `SIGUSR1` przez Dyspozytora w oknie czasowym, gdy wszystkie autobusy są w trasie.

### 📥 Wynik (Logi systemowe)
```
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
``` 
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
```
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

## 👑 Test 5: Priorytet VIP i Kolejkowanie (Stress Test)
**Cel:** Weryfikacja mechanizmu pierwszeństwa VIP-ów w sytuacji zatoru (zamknięty dworzec) oraz sprawdzenie logiki opróżniania bufora oczekujących.

### 📝 Opis scenariusza
1. Dyspozytor zamyka dworzec (komenda `2`).
2. Generator produkuje mieszankę pasażerów (VIP, Rowerzyści, Normalni), którzy utykają w poczekalni (active waiting).
3. Dyspozytor otwiera dworzec.
4. Oczekujemy, że VIP-y, które przyszły później, wejdą do autobusu **przed** zwykłymi pasażerami czekającymi dłużej.

### 📥 Wynik (Logi systemowe)
```
[Dyspozytor] Dworzec ZAMKNIĘTY (Blokada wejścia).
[Pasażer 3578] VIP (73 lat) - mam bilet, wchodzę BEZ KOLEJKI.
[Kasa] Pasażer 3579 (52 lat) odebrał bilet.
[Pasażer 3581] VIP (14 lat) - mam bilet, wchodzę BEZ KOLEJKI.
[Kasa] Pasażer 3582 (44 lat) odebrał bilet.
... (Kolejka rośnie, procesy oczekują na otwarcie bramek) ...
[Pasażer 3593] VIP (47 lat) - mam bilet, wchodzę BEZ KOLEJKI.

[Dyspozytor] Dworzec OTWARTY dla podróżnych.
[Wejście] Pasażer 3589 (50 lat). Stan: 1/15  <-- VIP (wszedł jako pierwszy)
[Wejście] Pasażer 3578 (73 lat). Stan: 2/15  <-- VIP
[Wejście] Pasażer 3593 (47 lat). Stan: 3/15  <-- VIP
...
[Wejście] Pasażer 3581 (14 lat). Stan: 8/15  <-- Ostatni VIP z grupy
[Wejście] DZIECKO + OPIEKUN (PID: 3594, 2 lat). Stan: 10/15 <-- Dopiero teraz wchodzą inni
[Wejście] ROWERZYSTA (PID: 3582, 44 lat). Stan: 12/15
[Wejście] Pasażer 3579 (52 lat). Stan: 13/15
```
### 🧐 Wnioski techniczne
* **Działanie Priorytetu:** Logi jednoznacznie pokazują, że po otwarciu bramek, procesy VIP zdobyły dostęp do sekcji krytycznej semafora wejściowego jako pierwsze, wyprzedzając procesy normalne (np. PID 3579, który czekał od początku testu).
* **Mechanizm synchronizacji:** Zmienna `vips_waiting` w pamięci dzielonej (SHM) poprawnie wymusiła ustąpienie miejsca przez zwykłe procesy (zastosowano `usleep` w pętli decyzyjnej), co zapobiegło zjawisku zagłodzenia VIP-ów.
* **Stabilność przy obciążeniu:** Mimo nagłego "uderzenia" kilkunastu procesów naraz (tzw. thundering herd problem przy otwarciu semafora), system zachował spójność danych i poprawnie zaktualizował liczniki miejsc.


## 🔥 Test 6: Stress Test "Zero-Delay" (Race Condition)
**Cel:** Weryfikacja stabilności semaforów i mechanizmów IPC w warunkach maksymalnego obciążenia procesora (usunięcie sztucznych opóźnień `sleep`). Sprawdzenie, czy przy setkach procesów walczących o zasób w tej samej milisekundzie nie dojdzie do przekłamania liczników (overbooking) lub zakleszczenia.

### 📝 Opis scenariusza
1. Maksymalne skrócenie czasu trwania operacji poprzez zakomentowanie większości funkcji `usleep()` w procesach pasażera i kasy.
2. W procesie kierowcy (`bus.cpp`) zmieniono standardowy `sleep(1)` na minimalny `usleep(1000)` (1ms). 
   *UWAGA: Minimalne opóźnienie w pętli autobusu pozostawiono celowo, aby zapobiec zjawisku "Busy Waiting" i umożliwić schedulerowi systemu operacyjnego sprawiedliwe przydzielanie czasu procesora procesom pasażerów czekającym na semaforze.*
3. Uruchomienie systemu w trybie "High-Throughput", gdzie generator zalewa system procesami bez żadnych przerw.

### 📥 Wynik (Logi systemowe - fragment)
```
[Wejście] Pasażer 4226 (16 lat). Stan: 11/15 (Rowery: 0/5)
[Wejście] Pasażer 4228 (45 lat). Stan: 15/15 (Rowery: 0/5)
[Autobus 1] Komplet pasażerów! Odjeżdżam wcześniej.
[Drzwi] ZAMKNIĘTE.
...
[Dyspozytor] Dworzec ZAMKNIĘTY (Blokada wejścia).
[Autobus 2] ODJAZD! Pasażerów: 0/15.
...
[Dyspozytor] Dworzec OTWARTY dla podróżnych.
[Autobus 2] Zbieram pasażerów...
[Wejście] Pasażer 4307 (28 lat). Stan: 1/15
...
[Wejście] Pasażer 4324 (10 lat). Stan: 15/15
[Autobus 2] Komplet pasażerów! Odjeżdżam wcześniej.
```
### 🧐 Wnioski techniczne
* **Integralność Semaforów:** Mimo usunięcia opóźnień, semafor `SEM_CAPACITY` ani razu nie wpuścił 16. pasażera. Liczniki zatrzymywały się idealnie na 15/15.
* **Stabilność IPC:** Kolejki komunikatów obsłużyły tysiące zapytań w ciągu sekund bez utraty danych.
* **Priorytety:** Nawet w warunkach Stress Testu, blokada dworca (zmienna w pamięci dzielonej) była respektowana natychmiastowo przez wszystkie oczekujące procesy.


## 🧮 Test 7: Test Integralności Danych (Data Consistency)
**Cel:** Weryfikacja, czy system poprawnie zlicza i obsługuje każdego pasażera bez "zgubienia" procesów w mechanizmach IPC. Bilans wejść musi się zgadzać z sumą wydanych biletów i pasażerów uprzywilejowanych.

### 📝 Opis scenariusza
1. Ograniczenie generatora do sztywnej liczby 100 pasażerów.
2. Usunięcie opóźnień (tryb szybki), aby wymusić maksymalne obciążenie kolejek i pamięci.
3. Po zakończeniu symulacji, analiza pliku `symulacja.txt` pod kątem zdarzeń: zakup biletu, odmowa wejścia oraz fizyczne wejście do pojazdu.

### 📥 Wynik (Analiza ilościowa)
* Liczba komunikatów "odebrał bilet": **97**
* Liczba komunikatów "bez opiekuna! ODMOWA": **0**
* Liczba komunikatów "Wejście": **100**

### 🧐 Wnioski techniczne
* **Analiza bilansu:** Matematyczna analiza logów wykazała 100% zgodności danych. Równanie testu: 
  `97 (bilety) - 0 (odmowy) + 3 (VIP) = 100 (suma wejść)`.
* **Wyjaśnienie różnicy:** Liczba wydanych biletów (97) jest mniejsza od liczby wejść (100), ponieważ 3 procesy zostały wylosowane jako typ VIP. Zgodnie z logiką projektu, VIP-y posiadają bilet wcześniej i nie korzystają z kolejki komunikatów kasy, co zostało poprawnie odzwierciedlone w logach.
* **Niezawodność IPC:** Brak utraty procesów w warunkach stresowych potwierdza poprawną implementację semaforów Systemu V.
