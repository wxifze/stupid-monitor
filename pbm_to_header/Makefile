CC = gcc
CFLAGS = -std=c99 -I../lib -lm -Werror -Wall -Wextra -O3 -march=native

PTHSRC = ../lib/pbm.c
PTHDEPS = $(PTHSRC) ../lib/pbm.h
pbm_to_header: $(PTHDEPS) main.c
	$(CC) $(CFLAGS) $(PTHDEPS) main.c -o pbm_to_header

clean:
	rm -f pbm_to_header
