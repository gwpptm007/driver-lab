CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: send_stage11_frame recv_stage11_frame

send_stage11_frame: send_stage11_frame.c
	$(CC) $(CFLAGS) -o $@ $<

recv_stage11_frame: recv_stage11_frame.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f send_stage11_frame recv_stage11_frame