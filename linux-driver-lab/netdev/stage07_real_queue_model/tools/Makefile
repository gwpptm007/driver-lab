CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: send_stage07_frame recv_stage07_frame

send_stage07_frame: send_stage07_frame.c
	$(CC) $(CFLAGS) -o $@ $<

recv_stage07_frame: recv_stage07_frame.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f send_stage07_frame recv_stage07_frame
