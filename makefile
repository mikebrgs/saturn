all: reqserv service

reqserv: src/reqserv.c
	gcc -g src/reqserv.c -o build/reqserv

service: src/service.c
	gcc -g src/service.c -o build/service