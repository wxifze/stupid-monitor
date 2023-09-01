CC = gcc
CFLAGS = -std=gnu99 -lm -Werror -Wall -Wextra

release: CFLAGS += -O3 -march=native
release: monitor

run: release
	./monitor

debug: CFLAGS += -ggdb3 -O0
debug: monitor_debug

run_debug: debug
	gdb monitor_debug


MONSRC = src/area.c src/display.c src/display.h src/pbm.c src/render.c src/ring.c src/stats.c src/timing.c 
MONDEPS = $(MONSRC) src/area.h src/display.h src/pbm.h src/render.h src/ring.h src/stats.h src/timing.h

monitor: $(MONDEPS) src/main.c
	$(CC) $(CFLAGS) $(MONSRC) src/main.c -o monitor

monitor_debug: $(MONDEPS) src/main.c
	$(CC) $(CFLAGS) $(MONSRC) src/main.c -o monitor_debug

clean:
	rm -f monitor monitor_debug
