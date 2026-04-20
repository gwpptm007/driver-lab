CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: send_stage12_frame recv_stage12_frame

send_stage12_frame: send_stage12_frame.c
	$(CC) $(CFLAGS) -o $@ $<

recv_stage12_frame: recv_stage12_frame.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f send_stage12_frame recv_stage12_frame