CC = gcc
CFLAGS = -Wall
OBJS = fftp fftp_rec fftp_send

all: clean $(OBJS)

fftp: fftp.h fftp.c
	$(CC) $(CFLAGS) -c -o $@ fftp.c

fftp_rec: fftp fftp_rec.c
	$(CC) $(CFLAGS) -o $@ -lpthread -lm fftp_rec.c fftp

fftp_send: fftp fftp_send.c
	$(CC) $(CFLAGS) -o $@ -lpthread -lm fftp_send.c fftp

.PHONY: all clean

clean:
	rm -f $(OBJS)