#include "mrt.h"

// protection against packets that are dropped on the UDP layer (packets should be re-sent)
  // timeout
// protection against data corruption (packets should be transferred as-is)
  // transport layer chksum
// protection against out-of-order delivery of packets on the UDP layer (the stream should be reconstructed in the right order)
  // seq num
// fast transmission if the latency is high (implement something smarter than send-then-wait)
  // gbn
// transmission of small amounts of data (in more than one segment of several bytes each), and possibly no data (in zero or one packets)
// transmission of data that does not fit in memory (don't keep all the data in memory)
  // do not store data - block the msg instead of storing in memory
// flow control (for congestion at the receiving-end)
  // rcv window
// implemented isn - prevent lingering packets

// mrt_open: start listening on specified port for UDP packets
// ip_addr is a placeholder for ip address
mrt_serv_conn * mrt_open(int ip_addr, int PORT_NUM, int size) {
  int i, n, pid;
  mrt_serv_conn * rc = (mrt_serv_conn *)malloc(sizeof(mrt_serv_conn));
  // mrt_serv_conn * rc = mmap(NULL, sizeof(mrt_serv_conn), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  

  int sockfd;
  socklen_t len;
  struct sockaddr_in servaddr, cliaddr; 
  mrt_packet * pkt; // to store incoming packets
  mrt_connection * conn;

  setbuf(stdout, (char *)0);

  // Creating socket file descriptor 
  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
      perror("socket creation failed"); 
      exit(EXIT_FAILURE); 
  } 
  // setup addresses
  memset(&servaddr, 0, sizeof(servaddr)); 
  memset(&cliaddr, 0, sizeof(cliaddr)); 
  // Filling server information 
  servaddr.sin_family    = AF_INET; // IPv4 
  servaddr.sin_addr.s_addr = INADDR_ANY; 
  servaddr.sin_port = htons(PORT_NUM); 
  // Bind the socket with the server address 
  if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
          sizeof(servaddr)) < 0 ) { 
      perror("bind failed"); 
      exit(EXIT_FAILURE); 
  }
  // initialize rc
  rc->fd = sockfd;
  rc->size = size;
  // rc->conns = (mrt_connection *)malloc(size * sizeof(mrt_connection));
  rc->conns = mmap(NULL, size * sizeof(mrt_connection), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  conn_init(rc->conns, size);

  // cliaddr for sender identifying
  len = sizeof(cliaddr);  //len is value/result 
// fork - return the mrt_serv_conn
  if (!(pid = fork())) {
    //  child handles all the incoming packets
    // initialize pkt first
    pkt = (mrt_packet *)malloc(sizeof(mrt_packet));
    memset(pkt, 0, sizeof(mrt_packet));

    while ((n = recvfrom(sockfd, (char *)pkt, sizeof(mrt_packet),  
              MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
              &len)) != -1) {


      if (MRT_TEST) print_pkt(pkt);
      // check validity first
      if (!pkt->header.flags && strcmp(pkt->header.chksum, get_md5(pkt->data))) {
        printf("corrupted packet: discarded\n");
        continue;
      }
      // packet is valid
      conn = NULL;
      for (i = 0; i < size; i++) 
        // the sender has reached the server before
        if (addr_is_equal(cliaddr, rc->conns[i].addr)) {
          if (MRT_TEST) printf("Got a match!\n");
          conn = &rc->conns[i];
          // if the packet is from an initialized and active connection
          if (conn->active) {
            if (MRT_TEST) printf("Got an active match!\n");
            switch(pkt->header.flags) {
              case FLAG_SYN:
              // this suggests that the sent ack was lost/had not arrived
              // immediately resend the ack
                conn->offset = pkt->header.seq;
                conn->nextseq = 0;
                send_ctrl_pkt(sockfd, &cliaddr, FLAG_SYN | FLAG_ACK, conn->nextseq + conn->offset, rb_size(&conn->rb));
                break;
              case FLAG_FIN:
              // sender disconnecting
              // send FIN ACK
                if (MRT_TEST) printf("Sending FIN ACK...\n");
                send_ctrl_pkt(sockfd, &cliaddr, FLAG_FIN | FLAG_ACK, 0, 0);
              // wipe out conn
                conn_init(conn, 1);
                break;
              case 0:
              // regular data
                if (rb_full(&conn->rb)){
                  send_ctrl_pkt(sockfd, &cliaddr, FLAG_ACK, conn->nextseq + conn->offset, 0);
                } else {
                  // buffer not full
                  if (pkt->header.seq - conn->offset == conn->nextseq) {
                    // if seq == nextseq
                    if (MRT_TEST) printf("One packet saved!\n");
                    conn->nextseq++;
                    rb_push(&conn->rb, pkt->header.seq - conn->offset, pkt->data, MSS);
                    if (MRT_TEST) printf("Current rcvwnd: %d.\n", rb_size(&conn->rb));
                    send_ctrl_pkt(sockfd, &cliaddr, FLAG_ACK, conn->nextseq + conn->offset, rb_size(&conn->rb));
                        // add the packet to the ht - ! sync here
                        // nextseq++
                        // update rcvwnd
                  } else send_ctrl_pkt(sockfd, &cliaddr, FLAG_ACK, conn->nextseq + conn->offset, rb_size(&conn->rb));
                }
              
              // buffer is full
                // discard packet, resend ack with rcvwnd = 0
                break;
            }    
          }
          // the connection is not active, ignore
          break;
        }
      // not record of sender
      if (!conn) {
        if (MRT_TEST) printf("Not connected user!\n");
        switch(pkt->header.flags) {
          case FLAG_FIN:
          // an unrecognized FIN request, possibly due to a lost FIN ACK
          // resend a FIN ACK to the sender
            send_ctrl_pkt(sockfd, &cliaddr, FLAG_FIN | FLAG_ACK, 0, 0);
            break;
          case FLAG_SYN:
          // a SYN request
            // find a spot for them
            for (i = 0; i < size; i++) 
              if (!rc->conns[i].initialized) {
                // an empty spot, set initialized and cliaddr for that connection, no ack sent yet
                if (MRT_TEST) printf("One incoming connection saved.\n");
                conn = &rc->conns[i];
                conn->initialized = 1;
                conn->offset = pkt->header.seq;
                memcpy(&conn->addr, &cliaddr, sizeof(struct sockaddr_in));
                conn->fd = sockfd;

                break;
              }
            // all occupied, reject service
        }
      }
    
      memset(pkt, 0, sizeof(mrt_packet));
    }
    // clean up
    free(pkt);
    exit(0);
  }
  rc->pid = pid;
  return rc;
}

// mrt_accept1: accept an incoming connection (return a connection), guaranteed to return one (will block until there is one)
void mrt_accept1(mrt_serv_conn * sc) {
  int i;
  // mrt_connection * res;

  while (1) {
    for(i = 0; i < sc->size; i++) 
      if (sc->conns[i].initialized && !sc->conns[i].active) {
       // send ack
       send_ctrl_pkt(sc->fd, &sc->conns[i].addr, FLAG_SYN | FLAG_ACK, sc->conns[i].nextseq + sc->conns[i].offset, rb_size(&sc->conns[i].rb));
       sc->conns[i].active = 1;
       return;
      
      }
  }
}
  
    
// Accept all incoming connections (returns a possibly empty set/array of connections),
// guaranteed not to block
// - conns: list of connections, must has been initialized
// - res: memset to 0 before use, stores the resulting conn pts
// - size: the size of the conn pt array res, maximum amount of connections probed,
//          not greater than sc->size
// - rc: # of accepted conns
int mrt_accept_all(mrt_serv_conn * sc, mrt_connection ** res, int size) {
  int i;
  int rc = 0;

  for (i = 0; i < size; i++) 
    if (sc->conns[i].initialized && !sc->conns[i].active) {
      sc->conns[i].active = 1;
      send_ctrl_pkt(sc->fd, &sc->conns[i].addr, FLAG_SYN | FLAG_ACK, 0, 0);
      res[rc++] = &sc->conns[i];
    }
    
      
  
  return rc;
}

// mrt_receive1: wait for at least one byte of data over a given connection, 
// guaranteed to return data except if the connection closes (will block until
//  there is data or the connection closes)
// make sure size of buf >= MSS
void mrt_receive1(mrt_connection * conn, char * buf, int size) {  
  while (conn->active) {
    memset(buf, 0, size);
    if(!rb_empty(&conn->rb)){
      // memset(buf, 0, size);
      rb_pop(&conn->rb, buf);
      send_ctrl_pkt(conn->fd, &conn->addr, FLAG_ACK, conn->nextseq + conn->offset, rb_size(&conn->rb));
      // memcpy(buf, rb_pop(conn->rb)->data, MSS);
      return;
    }
  }
  return;
}



// given a set of connections, returns a connection in which 
// there is currently data to be received (may return no connection if there 
// is no such connection, never blocks and ensures that a mrt_receive1 call to 
// the resulting connection will not block either)
// - conns: connections being probed
// - res: memset to 0 before use, stores the resulting conn pts
// - size: the size of the conn pt array res, maximum amount of connections probed
// - rc: # of valid conns
int mrt_probe(mrt_serv_conn * sc, mrt_connection ** res, int size) {
  int i;
  int rc = 0;
  
  for (i = 0; i < size; i++) 
    if (sc->conns[i].active && !rb_empty(&sc->conns[i].rb))
      res[rc++] = &sc->conns[i];
  
  return rc;
} 

// mrt_close: indicate incoming connections are no-longer accepted
int mrt_close(mrt_serv_conn * sc) {
  int i;
  if (MRT_TEST) printf("Server closing...\n");
  // send FIN to all active connections
  for (i = 0; i < sc->size; i++) 
    if (sc->conns[i].active) 
      send_ctrl_pkt(sc->fd, &sc->conns[i].addr, FLAG_FIN, 0, 0);


  close(sc->fd);
  kill(sc->pid, SIGKILL);
  
  msync(sc->conns, sizeof(mrt_serv_conn), MS_SYNC);
  munmap(sc->conns, sizeof(mrt_serv_conn));
  free(sc);
  return 0;
}

// mrt_connect: connect to a given server (return a connection)
// always return a connection, check if it's an active connection before use
// empty connection on fail
// ip_addr, reserved
mrt_connection * mrt_connect(int ip_addr, int PORT_NUM) {
  int isn, pid, sockfd; 
  int timeout;
  time_t t;
  struct timespec start, curr;
  struct sockaddr_in cliaddr, sendaddr, recvaddr;
  socklen_t len = sizeof(struct sockaddr_in);
  mrt_packet * pkt; // to store incoming packets

  setbuf(stdout, (char *)0);

  srand((unsigned) time(&t));
  isn = rand() % 1024;
  mrt_connection * rc = mmap(NULL, sizeof(mrt_connection), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  memset(rc, 0, sizeof(mrt_connection));

  rc->addr.sin_addr.s_addr = INADDR_ANY;
  rc->addr.sin_port = htons(PORT_NUM);
  rc->addr.sin_family = AF_INET;

  fill_addr(&sendaddr, "", 0);
  fill_addr(&recvaddr, "", 0);
  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
      perror("socket creation failed"); 
      exit(EXIT_FAILURE); 
  }
  if (bind(sockfd, (const struct sockaddr *)&recvaddr, sizeof(struct sockaddr_in)) == -1) {
    perror("bind sockfd");
  }
  rc->fd = sockfd;
  if ((pid = fork())) {
    rc->pid = pid;
    timeout = 0;
    clock_gettime(CLOCK_REALTIME, &start);
    while(!timeout) {
      send_ctrl_pkt(sockfd, &rc->addr, FLAG_SYN, isn, 0);
      if (rc->active) break;
      nanosleep((const struct timespec[]){0, 1e8L}, NULL);
      clock_gettime(CLOCK_REALTIME, &curr);
      if ((long)curr.tv_sec - (long)start.tv_sec >= SYN_TIMEOUT) timeout++;
    }
    if (timeout) {
      if (!rc->pid) printf("Server is down.\n");
      else {
        kill(pid, SIGKILL);
        printf("Server unreachable.\n");
        close(sockfd);
        memset(rc, 0, sizeof(mrt_connection));
        return NULL;
      }
      
    }
  } else {
    pkt = (mrt_packet *)malloc(sizeof(mrt_packet));
    memset(pkt, 0, sizeof(mrt_packet));
    while ((recvfrom(sockfd, (char *)pkt, sizeof(mrt_packet),  
              MSG_WAITALL, ( struct sockaddr *) &cliaddr, 
              &len)) != -1) {
      if (MRT_TEST) print_pkt(pkt);
      // the addr should be checked here too
      if (cliaddr.sin_port == htons(PORT_NUM)) {
        switch (pkt->header.flags) {
          case FLAG_SYN | FLAG_ACK:
            // save the packet
            memcpy(&rc->addr, &cliaddr, sizeof(struct sockaddr_in));
            rc->active = 1;
            rc->base = -1;
            rc->rcvwnd = pkt->header.rcvwnd;
            rc->offset = isn;
            break;
          case FLAG_FIN:
            // close the connection, and stop transmitting data
            close(sockfd);
            memset(rc, 0, sizeof(mrt_connection));
            printf("The connection is terminated by the server.\n");
            exit(0);
            break;
          case FLAG_FIN | FLAG_ACK:
            // server acked the disconnection
            rc->initialized = 1;
            break;
          case FLAG_ACK:
            // data control, adjust base, send ack, seq, rcvwnd
            if ((pkt->header.seq - rc->offset - 1) > rc->base) rc->base = pkt->header.seq - rc->offset - 1;  

            rc->rcvwnd = pkt->header.rcvwnd;

            break;
        }
        
      }
    }

  }
  return rc;
}


// mrt_send: send a chunk of data over a given connection (may temporarily block execution if the receiver is busy/full)
// size - bytes to send
// buf - data pointer
int mrt_send(mrt_connection * conn, char * buf, int size) {
  int i;
  char * entr;
  int tot_pkts = size/MSS + (size%MSS?1:0);
  timer t_send;
  int timeout_send = 0;

  if (!conn->active) return 1;
  entr = buf;

reset:
  while(conn->active && conn->base - global_offset + 1 != tot_pkts) {
      timer_set(&t_send);

      if (MRT_TEST) printf("I am waiting...\n");
      // test cases
      for (i = conn->base + 1 - global_offset; i < tot_pkts; i++) {
        while(conn->active && i - conn->base  + global_offset > conn->rcvwnd) {
          sleep(1);
          if (timer_diff(&t_send) >= SEND_TIMEOUT) goto reset;
        }
        send_data_pkt(conn->fd, &conn->addr, i + global_offset + conn->offset, 0, buf + i * MSS, i<tot_pkts - 1?MSS:(size%MSS?size%MSS:MSS));
      }
      if (timer_diff(&t_send) >= SEND_TIMEOUT) continue;
      nanosleep((const struct timespec[]){0, 1e8L}, NULL);
  }
  global_offset += tot_pkts;
  return 0;
}

// mrt_disconnect: close the connection, 
// return 0 on success
// if the connection is already closed 0 would be returned
int mrt_disconnect(mrt_connection * conn) {
  int timeout = 0;

  if (conn->active) {
    timer_set(&t);
    while (!timeout) {
      send_ctrl_pkt(conn->fd, &conn->addr, FLAG_FIN, 0, 0);
      // this means a FIN ACK was received
      if (conn->initialized) {
        conn_close(conn);
        break;
      }
      sleep(1);  
      if (timer_diff(&t) >= FIN_TIMEOUT) timeout++;
    }
    if (timeout) return 1;
  }
  
  return 0;
}
