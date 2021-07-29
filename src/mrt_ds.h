// the data structure for packet headers
#ifndef _MRT_DS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define _MRT_DS_H
// enable MRT_TEST to display packet trace
#define MRT_TEST 0
// #define MRT_TEST 1

#define FLAG_SYN 4
#define FLAG_FIN 2
#define FLAG_ACK 1
// maximum number of characters perline
#define MAXLINE 1024
// maximum size of segment
#define MSS 1450
#define MAX_BUFFERED_PACKETS_NUM 100

// in this implementation the headers for sender and receiver 
// could have been tailored (less flag bits, unused fields by sender)
// to lower the cost, but is preserved this way in case a full 
// duplex version is needed in the future

// msgs and acks are not piggybacked - one way transmission

typedef struct {
  // seq # (not byte # in the stream)- 4 bytes
  int32_t seq:32;
  // chksum - 33 bytes
  char chksum[33];
  // receive window for flow control - 2 bytes
  unsigned rcvwnd:16;
  // flags for handshakes - only 4 bits used
  unsigned flags:4;
} mrt_header;

typedef struct {
  mrt_header header;
  char data[MSS + 1];
} mrt_packet;

// by default ISN would be a random number greater than 0
typedef struct {
    int seq;
    char data[MSS + 1];
} mrt_seg;

typedef struct {
    int size;
    int head;
    int tail;
    mrt_seg buf[MAX_BUFFERED_PACKETS_NUM + 1];
} rcvbuf;

// connection - static structure, no need to free
typedef struct {
    // the addr:port - for sending acks
    struct sockaddr_in addr;
    // initialized - got the syn but not acked / used for disconnection in client
    int initialized;
    // active - acked
    int active;
    // offset - ISN from receiver
    int32_t offset;
    // base
        // sender - sent and acked seq
        // receiver - seq of packet to be consumed
    int32_t base;
    // nextseq
        // sender - unused
        // receiver - expected seq of next packet
    unsigned nextseq;
    // receive buffer - N * sizeof(mrt_seg)
    rcvbuf rb;
    //receive window
    unsigned rcvwnd;
    // for client
    int pid;
    int fd;
    // * for sender, check if nextseq - base - 1 < rcvwnd before sending next seq
        // upon ack, update base and rcvwnd field
        // sleep if it does not hold
    // * for receiver, it is guaranteed that:
        // packet[base] to packet[nextseq - 1] are in the ht
} mrt_connection;

typedef struct {
    struct timespec start, curr;
} timer;

// an integrated for easy termination on server end
typedef struct {
    // the associated socket
    int fd;
    // the listener pid
    int pid;
    // the # of conns the server is waiting for
    int size;
    // the connections for incoming senders
    mrt_connection * conns;
} mrt_serv_conn;

timer t;

char * get_md5(const char *);
void print_mh(mrt_header *);
void print_pkt(mrt_packet *);
void rb_init(rcvbuf *);
void rb_free(rcvbuf *);
int rb_full(rcvbuf *);
int rb_empty(rcvbuf *);
void rb_push(rcvbuf * rb, int seq, char * msg, int size);
int rb_pop(rcvbuf * rb, char * buf);
unsigned rb_size(rcvbuf * rb);
void conn_init(mrt_connection *, int);
void fill_addr(struct sockaddr_in *sa, char * addr, int port);
int addr_is_equal(struct sockaddr_in addr1, struct sockaddr_in addr2);
void send_ctrl_pkt(int sockfd, const struct sockaddr_in * addr, int flags, int seq, int rcvwnd);
void send_data_pkt(int sockfd, const struct sockaddr_in * addr, int seq, int rcvwnd, char * buf, int size);
void conn_close(mrt_connection * conn);
void timer_set(timer * t);
long timer_diff(timer * t);
#endif