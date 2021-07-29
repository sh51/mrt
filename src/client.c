#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#include "mrt_ds.h"
#include "mrt.h"

#define DST_PORT 8177

mrt_connection * conn;

void intHandler(int sig) {
	mrt_disconnect(conn);
	printf("\nConnection closed.\n");
	exit(0);
}

void chldHandler(int sig) {
	mrt_disconnect(conn);
	printf("Connection closed.\n");
	exit(0);
}

int main (int argc, char const * argv[]) { 
	int i;
	char str[MAXLINE];
	signal(SIGINT, intHandler);
	signal(SIGCHLD, intHandler);

	// connect to server
	if (!(conn = mrt_connect(0, DST_PORT))) {
		printf("Failed to connect.\n");
		exit(1);
	}
	
	if (conn->active) printf("Connected to the server!\n");

	while(conn->active && fgets(str, MAXLINE, stdin))
		mrt_send(conn, str, strlen(str));
	
	if (mrt_disconnect(conn)) printf("Connection closed, but the server did not acknowledge.\n");
	else printf("Connection closed.\n");

	return 0; 
} 
