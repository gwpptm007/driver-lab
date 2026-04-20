CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: send_stage13_frame recv_stage13_frame

send_stage13_frame: send_stage13_frame.c
	$(CC) $(CFLAGS) -o $@ $<

recv_stage13_frame: recv_stage13_frame.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f send_stage13_frame recv_stage13_frame
