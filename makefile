all: reqserv service

reqserv: src/reqserv.c
	gcc src/reqserv.c -o build/reqserv

service: src/service.c
	gcc src/service.c -o build/service