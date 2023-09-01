CC = gcc
CFLAGS = -std=gnu99 -I../lib -lm -Werror -Wall -Wextra

release: CFLAGS += -O3 -march=native
release: monitor

run: release
	./monitor

debug: CFLAGS += -ggdb3 -O0
debug: monitor_debug

run_debug: debug
	gdb monitor_debug


MONSRC = area.c display.c display.h  render.c ring.c stats.c timing.c ../lib/pbm.c
MONDEPS = $(MONSRC) area.h display.h  render.h ring.h stats.h timing.h ../lib/pbm.h

monitor: $(MONDEPS) main.c
	$(CC) $(CFLAGS) $(MONSRC) main.c -o monitor

monitor_debug: $(MONDEPS) main.c
	$(CC) $(CFLAGS) $(MONSRC) main.c -o monitor_debug

clean:
	rm -f monitor monitor_debug
