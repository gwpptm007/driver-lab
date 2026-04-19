CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: send_stage10_frame recv_stage10_frame

send_stage10_frame: send_stage10_frame.c
	$(CC) $(CFLAGS) -o $@ $<

recv_stage10_frame: recv_stage10_frame.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f send_stage10_frame recv_stage10_frame