all : server

server : srv.c
	 gcc -g srv.c -o proxy_cache -lcrypto -lpthread

