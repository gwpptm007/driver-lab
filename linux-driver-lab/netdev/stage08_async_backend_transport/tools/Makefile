CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: send_stage08_frame recv_stage08_frame

send_stage08_frame: send_stage08_frame.c
	$(CC) $(CFLAGS) -o $@ $<

recv_stage08_frame: recv_stage08_frame.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f send_stage08_frame recv_stage08_frame
