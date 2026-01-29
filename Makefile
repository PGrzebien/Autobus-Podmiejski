CC = g++
CFLAGS = -Wall -O2 -Iinclude

all: system autobus pasazer kasa

system: src/main.cpp src/utils.cpp
	$(CC) $(CFLAGS) src/main.cpp src/utils.cpp -o system

autobus: src/bus.cpp src/utils.cpp
	$(CC) $(CFLAGS) src/bus.cpp src/utils.cpp -o autobus

pasazer: src/passenger.cpp src/utils.cpp
	$(CC) $(CFLAGS) src/passenger.cpp src/utils.cpp -o pasazer

kasa: src/kasa.cpp src/utils.cpp
	$(CC) $(CFLAGS) src/kasa.cpp src/utils.cpp -o kasa

clean:
	rm -f system autobus pasazer kasa symulacja.txt
