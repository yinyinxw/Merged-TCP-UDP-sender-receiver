CC=gcc
GFLAGS=-Wall	-g
all:
	gcc	-g	-o receiver	Receiver.c	
clean:
	rm	-f	sender	receiver	stream_file_written_*	dgram_file_written_*
