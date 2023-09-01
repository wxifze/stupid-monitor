#ifndef AREA_H
#define AREA_H

#include <stddef.h>
#include <stdbool.h>

struct Area {
	bool** buff;
	size_t width;
	size_t height;
	size_t x_offset;
	size_t y_offset;
};

void alloc_area(struct Area* area, size_t width, size_t height);

// since the program will never stop and free it's resources, there is no free_area()

void set_area(struct Area* area, size_t x, size_t y, bool value);

bool get_area(const struct Area* area, size_t x, size_t y);

void subarea(
		const struct Area*  area, 
		struct Area* subarea,
		size_t x_offset,
		size_t y_offset,
		size_t width,
		size_t height
);

void clear_area(struct Area* area);

#endif
