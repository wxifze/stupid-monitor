#ifndef DISPLAY_H
#define DISPLAY_H

#include <asm/termbits.h>

#include "area.h"

int init_display(const char* path, speed_t baud);

// since the program will never stop and free it's resources, there is no free_display()

void draw_display(int display, const struct Area* area);

#endif
