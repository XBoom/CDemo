all: httpd client
LIBS = -lpthread #-lsocket
httpd: httpd.c
	gcc -g -O0 -W -Wall $(LIBS) -o $@ $<

client: simpleclient.c
	gcc -W -Wall -o $@ $<
clean:
	rm httpd
