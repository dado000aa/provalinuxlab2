CC = gcc
CFLAGS = -Wall -Wextra -O3 -pthread

msf.out: msf.o funzioni.o
	$(CC) $(CFLAGS) -o msf.out msf.o funzioni.o -pthread

msf.o: msf.c msf.h
	$(CC) $(CFLAGS) -c msf.c

funzioni.o: funzioni.c msf.h
	$(CC) $(CFLAGS) -c funzioni.c

Msf.class: Msf.java
	javac Msf.java

clean:
	rm -f *.o msf.out *.class