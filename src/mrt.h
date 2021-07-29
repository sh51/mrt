#ifndef _MRT_H
#define _MRT_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "mrt_ds.h"

#define SYN_TIMEOUT 1024
#define SEND_TIMEOUT 1// ideally just about RTT
#define FIN_TIMEOUT 3

int global_offset;

mrt_serv_conn * mrt_open(int ip_addr, int PORT_NUM, int size);
int mrt_accept_all(mrt_serv_conn * sc, mrt_connection ** res, int size);
void mrt_accept1(mrt_serv_conn * sc);
void mrt_receive1(mrt_connection * conn, char * buf, int size);
int mrt_probe(mrt_serv_conn * sc, mrt_connection ** res, int size);
int mrt_close(mrt_serv_conn * sc);
int mrt_disconnect(mrt_connection * conn);
int mrt_send(mrt_connection * conn, char * buf, int size);
mrt_connection * mrt_connect(int ip_addr, int PORT_NUM);


#endif