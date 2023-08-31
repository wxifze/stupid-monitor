#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <err.h>

#include "area.h"

void alloc_area(struct Area* area, size_t width, size_t height){
	if (!(area->buff = calloc(height, sizeof(bool*))))
		err(1, "failed to allocate memory for area");

	for (size_t y = 0; y < height; y++)
		if (!(area->buff[y] = calloc(width, sizeof(bool))))
			err(1, "failed to allocate memory for area");

	area->width = width;
	area->height = height;
	area->x_offset = 0;
	area->y_offset = 0;
}

void set_area(struct Area* area, size_t x, size_t y, bool value) {
	assert(x < area->width - area->x_offset);
	assert(y < area->height - area->y_offset);
	area->buff[area->y_offset + y][area->x_offset + x] = value;
}

bool get_area(const struct Area* area, size_t x, size_t y) {
	assert(x < area->width - area->x_offset);
	assert(y < area->height - area->y_offset);
	return area->buff[area->y_offset + y][area->x_offset + x];
}

void subarea(
		const struct Area*  area, 
		struct Area* subarea,
		size_t x_offset,
		size_t y_offset,
		size_t width,
		size_t height
) {
	assert(x_offset + width <= area->width);
	assert(y_offset + height <= area->height);

	subarea->buff = area->buff;
	subarea->x_offset = area->x_offset + x_offset;
	subarea->y_offset = area->y_offset + y_offset;
	subarea->width = width;
	subarea->height = height;
}

void clear_area(struct Area* area) {
	for (size_t y = 0; y < area->height; y++)
		for (size_t x = 0; x < area->width; x++)
			set_area(area, x, y, false);
}
