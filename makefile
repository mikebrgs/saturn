all: reqserv service

reqserv: src/reqserv.c
	gcc -g -Wall src/reqserv.c -o build/reqserv

service: src/service.c
	gcc -g -Wall src/service.c -o build/service
