CFLAGS=-lcrypto
CC=gcc

all: dht_daemon client

dht_daemon: dht.o dht_daemon.o
	$(CC) $(CFLAGS) dht.o dht_daemon.o -o dht_daemon

client: client.o
	$(CC) $(CFLAGS) client.o -o client

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o dht_daemon client