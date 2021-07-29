#if defined(__APPLE__)
#  define COMMON_DIGEST_FOR_OPENSSL
#  include <CommonCrypto/CommonDigest.h>
#  define SHA1 CC_SHA1
#else
#  include <openssl/md5.h>
#endif

#include "mrt_ds.h"

// generates a md5 hash for data validating
char * get_md5(const char *str) {
    int n, length;
    MD5_CTX c;
    unsigned char digest[16];
    char * md5_hash = (char *)malloc(33);

    length = strlen(str);

    MD5_Init(&c);

    while (length > 0) {
        if (length > 512) MD5_Update(&c, str, 512);
        else MD5_Update(&c, str, length);
        length -= 512;
        str += 512;
    }

    MD5_Final(digest, &c);

    for (n = 0; n < 16; ++n) 
        snprintf(&(md5_hash[2 * n]), 32, "%02x", (unsigned int)digest[n]);

    return md5_hash;
}

void print_mh(mrt_header * mh) {
  printf("seq: %d\n", mh->seq);
  printf("chksum: %s\n", mh->chksum);
  printf("rcvwnd: %d\n", mh->rcvwnd);
  if (mh->flags&FLAG_SYN) printf("SYN ");
  if (mh->flags&FLAG_ACK) printf("ACK ");
  if (mh->flags&FLAG_FIN) printf("FIN");
  if (mh->flags) printf("\n");
}

void print_pkt(mrt_packet * pkt) {
  printf("<<<<<<<<<<<<<<<<<<<<<\n");
  print_mh(&pkt->header);
  printf("%lu byte(s): %s\n", strlen(pkt->data), pkt->data);
  printf(">>>>>>>>>>>>>>>>>>>>>\n");
}

void fill_addr(struct sockaddr_in *sa, char * addr, int port) {
  memset(sa, 0, sizeof(struct sockaddr_in));
  sa->sin_family = AF_INET;
  sa->sin_addr.s_addr = INADDR_ANY;
  sa->sin_port = htons(port);
}

// util funcs for rcvbuf - implement priority queue if efficiency is of concern
// rcvbuf init and free
void rb_init(rcvbuf * rb) {
  rb->size = MAX_BUFFERED_PACKETS_NUM + 1;
  rb->head = 0;
  rb->tail = 0;
  memset(rb->buf, 0, sizeof(rb->buf));
}
void rb_free(rcvbuf * rb) {
  free(rb->buf);
}
int rb_full(rcvbuf * rb) {
  return (rb->tail + 1) % rb->size == rb->head;
}
int rb_empty(rcvbuf * rb) {
  return rb->head == rb->tail;
}
void rb_push(rcvbuf * rb, int seq, char * msg, int size) {
  int i;

  if (rb_full(rb)) {
    printf("rb_push failed: Full buffer\n");
    exit(1);
  }
    
  rb->buf[rb->tail].seq = seq;
  memcpy(rb->buf[rb->tail].data, msg, size);
  rb->tail = (rb->tail + 1) % rb->size;
}
int rb_pop(rcvbuf * rb, char * buf) {
  int i;

  if (rb_empty(rb)) return 1;
  
  memcpy(buf, rb->buf[rb->head].data, MSS>MAXLINE?MAXLINE:MSS);
  rb->head = (rb->head + 1) % rb->size;

  return 0;
}
// spots available
unsigned rb_size(rcvbuf * rb) {
  return (rb->tail >= rb->head? MAX_BUFFERED_PACKETS_NUM - (rb->tail - rb->head):rb->head - rb->tail - 1);
}

// clear out $size number of connection starting from $conns
void conn_init(mrt_connection * conns, int size) {
    int i;
    for (i = 0; i < size; i++) {
        memset(&conns[i], 0, sizeof(mrt_connection));
        rb_init(&conns[i].rb);
    }
}
int addr_is_equal(struct sockaddr_in addr1, struct sockaddr_in addr2) {
  return ((addr1.sin_addr.s_addr == addr2.sin_addr.s_addr) && (addr1.sin_port == addr2.sin_port));
}

void send_ctrl_pkt(int sockfd, const struct sockaddr_in * addr, int flags, int seq, int rcvwnd) {
  mrt_packet * pkt = (mrt_packet *)malloc(sizeof(mrt_packet));

  memset(pkt, 0, sizeof(mrt_packet));
  pkt->header.seq = seq;
  pkt->header.rcvwnd = rcvwnd;
  pkt->header.flags = flags;

  sendto(sockfd, (const char *)pkt, sizeof(mrt_header),
     0, (const struct sockaddr *)addr, sizeof(struct sockaddr_in)); 

  free(pkt);
  return ;
}
// send size bytes of data starting from buf
// all data packets have to be zero-terminated
// make sure that size <= MSS
void send_data_pkt(int sockfd, const struct sockaddr_in * addr, int seq, int rcvwnd, char * buf, int size) {
  mrt_packet * pkt = (mrt_packet *)malloc(sizeof(mrt_packet));

  memset(pkt, 0, sizeof(mrt_packet));
  memcpy(&pkt->data, buf, size);

  strcpy(pkt->header.chksum, get_md5(pkt->data));
  pkt->header.seq = seq;
  pkt->header.rcvwnd = rcvwnd;

  sendto(sockfd, (const char *)pkt, sizeof(mrt_header) + strlen(pkt->data) + 1,
              0, (const struct sockaddr *)addr, sizeof(struct sockaddr_in)); 

  free(pkt);
  return ;
}


// close the connection
void conn_close(mrt_connection * conn) {
  int wstatus;

  kill(conn->pid, SIGKILL);
  close(conn->fd);

  memset(conn, 0, sizeof(mrt_connection));
  printf("The connection is closed.\n");
}

void timer_set(timer * t) {
    clock_gettime(CLOCK_REALTIME, &t->start);
}

long timer_diff(timer * t) {
    clock_gettime(CLOCK_REALTIME, &t->curr);
    return (long)t->curr.tv_sec - (long)t->start.tv_sec; 
}