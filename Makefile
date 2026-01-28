CC = g++
CFLAGS = -Wall -O2 -Iinclude

all: system autobus pasazer

system: src/main.cpp src/utils.cpp
	$(CC) $(CFLAGS) src/main.cpp src/utils.cpp -o system

autobus: src/bus.cpp src/utils.cpp
	$(CC) $(CFLAGS) src/bus.cpp src/utils.cpp -o autobus

pasazer: src/passenger.cpp src/utils.cpp
	$(CC) $(CFLAGS) src/passenger.cpp src/utils.cpp -o pasazer

clean:
	rm -f system autobus pasazer symulacja.txt
