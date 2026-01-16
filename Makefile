CC = g++
CFLAGS = -Wall -O2 -Iinclude

all: autobus_sim

autobus_sim: src/main.cpp src/utils.cpp
	$(CC) $(CFLAGS) src/main.cpp src/utils.cpp -o autobus_sim

clean:
	rm -f autobus_sim
