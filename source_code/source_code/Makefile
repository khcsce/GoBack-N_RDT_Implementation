# NAME: Khoa Quach
# EMAIL: khoaquachschool@gmail.com
# ID: 105123806
CC = gcc
CFLAGS = -Wall -g
UID=105123806

default: build

build: server.c client.c
	gcc -Wall -Wextra -o server server.c
	gcc -Wall -Wextra -o client client.c

clean: 
	rm -f *.tar.gz *.o *~ client server *.file

dist: clean
	tar -cvzf $(UID).tar.gz server.c client.c README Makefile 
