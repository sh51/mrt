ALL = server client generate accept

DIR = ./src

SRCS = ./src/server.c ./src/client.c ./src/generate.c ./src/accept.c
OBJS = $(SRCS:.c=.o)
OUTPUTS = $(SRCS:.c=)
INCS = ./src/mrt_ds.c ./src/mrt.c

CC = gcc
CFLAGS = -Wno-deprecated

all: $(ALL)

clean:
	rm -f ./src/*.o *~ $(ALL)

r: clean all

kill:
	killall $(ALL)

$(DIR)/%.o: $(DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

server: $(DIR)/server.o $(INCS)
	$(CC) $(CFLAGS) -o $@ $^

client: $(DIR)/client.o $(INCS)
	$(CC) $(CFLAGS) -o $@ $^

generate: $(DIR)/generate.o $(INCS)
	$(CC) $(CFLAGS) -o $@ $^

accept: $(DIR)/accept.o $(INCS)
	$(CC) $(CFLAGS) -o $@ $^
