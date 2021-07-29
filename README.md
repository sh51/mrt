# mini-reliable-tranport-layer

This is the code for a mini reliable tranport layer based on UDP protocol.

## Supported Functions
The following functions are defined to support reliable data transmission:
- `mrt_open` - takes an address, a port number and the number of senders expected as arguments and returns a pointer to structure `mrt_serv_conn`, which is encapsulated connections.
- `mrt_accept1` - given a set of connections (a `mrt_serv_conn`), returns a connection. This is a blocking call.
- `mrt_accept_all` - accept all pending connections, and store the results in the provided connection array. This function is non-blocking.
- `mrt_probe` - returns an active connection that has stored data in its receive buffer.
- `mrt_close` - close all active connections and terminate the listening process.
- `mrt_connect` - given a port number, returns an active connection. By default this call would timeout after a period of time. This can be changed by setting the `SYN_TIMEOUT` macro in `src/mrt.h` as needed.
- `mrt_disconnect` - disconnects from the receiver. Returns 0 when the disconnection is acknowledged by the receiver.
- `mrt_send` - provided an active connection sends data to the receiver.

## Source file structure
All Source code could be found under the directory `src`. There are 4 source files:
- `mrt.c` - This file contains the required functions.
- `mrt_ds.c` - The data structure and utility functions.
- `server.c` - The receiver program.
- `client.c` - The sender program.
- `generate.c` - A utility program for testing. It generates a same string over and over.
- `accept.c` - A utility program for testing. It prints the line count of standard input.

Run `make` to compile all four programs, or run `make server`, `make client`, `make generate`, `make accept` to compile the ones needed.

`make kill`, `make clean` are also provided for termination and cleanup.

## Mrt: protocol description

Mostly can be view as a simplified version of tcp, the implemented protocol requires the data transmission to start with a two-way handshake - a SYN packet from the sender followed by an SYN ACK packet from the receiver, confirming the connection has been made.

Once the connection is acknowledged, the sender would proceed to dispatch data packets. Depending on the size of the receive window, the sender might send off all packets with seq number equal to/higher than the acked one, or hold off and wait for an receive window opening.

As the packet reaches the receiver, it get stored in the receive buffer if the sequence number happens to be the same as the number the receiver was expecting, otherwise it would be discarded. In both cases, the new (or old) expected sequence value will be packed in an ACK segment and sent to the sender, along with the updated receive window size.

Upon disconnecting, the sender sends out a FIN packet to inform the receiver. The receiver will then acknowledge with a FIN ACK packet before releasing the allocated resources. After this point, the sender is disconnected from the receiver.

In another scenario, the receiver might close the connections/terminates before the senders do. A FIN packet will be sent to each active sender before the receiver shuts down.

## Testing
To test the programs, `generate` and `accept` can be used as data generator and recorder. For the most part the implementation was tested by tweaking the predefined macros (timeout limits, size of receive buffer, maximum packet size). Uncomment the line `#define MRT_TEST 1` in `src/mrt_ds.h` to enable the debug logs.

### server
This is the equivalent of receiver. The first and only argument to this program will be the number of senders it should expect. For instance, `./receiver 2` will wait for two connections before printing out the received data (in the order of their arrival). The example in the requirements, can be tested with basically exact same commands (and order of execution). The designated port can be changed by setting the macro `PORT` in `src/server.c`.

### client
The equivalent of sender. It takes no argument and will try to connect to the local server (wait for at most 1024 seconds before timeout if the server is not on yet). It grabs input from stdin and send it to the server.

### Protection against packet loss
To provide protection against packet loss, packet sending is timed and resends will be triggered when a timeout occurs. This was tested when the size of the receive window was set to 4 (4 packets max can be buffered at the same time) and the transferred data was broken into more than 4 packets. The packets were sent to the receiver before the sender got the ACK saying rcvwnd = 0 (all packets with sequence number higher than 3 were therefore lost). When the receive window reopened, lost packets were sent again, which could be observed in wireshark.

### Protection against data corruption
Upon arrival, an md5 hash of the data field of the packet will be calculated for each packet. It will be then compared with the checksum carried in the header to determine if the packet has corrupted.

### Protection against out-of-order delivery
Same as GBN, this mini reliable transport layer uses cumulative acks. When packets arrive in the wrong order, only one packet will be acked, which is the expected one. The sender will then resend all other unacked packets.

### High-latency delivery
With receiver's rcvwnd value attached to all their acks, the sender will send as much continuous packets as possible, while trying to make sure the number of sent yet unacked packets is not greater than the rcvwnd. This can be checked by setting the MSS (maximum size of each segment/packet) low, and see in wireshark that a series of packets are sent at once, instead of one segment - one ack.

### Sending small amounts of data
This can be tested by piping it with a data generator or running:
```bash
$ ./server 1
```
in one terminal, and
```bash
$ ./client
This is a short message.
This is another short message.
...
```
in another terminal.

Examine the packet captures to see each line is put in one single packet.

### Sending large amounts of data
Use a data generator or set the MSS to a small value. The data would be broken into a string of packets before being sent out.

### Flow-control
This can be tested using the provide example. Run the server first before running the two clients. Send data with the second sender first. After sending some packets (depending on the receive window) the sender would stop sending. After the first sender sends their data and disconnect, the transmission of second sender's data would continue from where it was left.