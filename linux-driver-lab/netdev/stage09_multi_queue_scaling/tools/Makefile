CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: send_stage09_frame recv_stage09_frame

send_stage09_frame: send_stage09_frame.c
	$(CC) $(CFLAGS) -o $@ $<

recv_stage09_frame: recv_stage09_frame.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f send_stage09_frame recv_stage09_frame
