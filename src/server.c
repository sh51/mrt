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


// define port here
#define PORT 8177

// input an N - the number of connections expected

mrt_serv_conn * sc;

void intHandler(int sig) {
	mrt_close(sc);
	if (MRT_TEST) printf("\nConnection closed.\n");
	exit(0);
}

int main (int argc, char const * argv[]) {
    int n, i;
    char str[MAXLINE];
    signal(SIGINT, intHandler);
    int send_num = atoi(argv[1]);
    
    // open connection
    sc = mrt_open(0, PORT, send_num);
    // accept sender connections
    for (i = 0; i < send_num; i++) 
        mrt_accept1(sc);

    // cosume each senders' data in order
    for (i = 0; i < send_num; i++) {
        while (sc->conns[i].active) {
            mrt_receive1(&sc->conns[i], str, MAXLINE);
            printf("%s", str);
        }
    }

    // shut down when all senders disconnected
    mrt_close(sc);

    return 0;
}