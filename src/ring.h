#ifndef RING_H
#define RING_H

#include <stddef.h>

struct Ring {
	double* buff;
	size_t capacity;
	size_t begin;
	size_t length;
};

void alloc_ring(struct Ring* ring, size_t capacity);

// since the program will never stop and free it's resources, there is no free_ring()

double get_ring(const struct Ring* ring, size_t index);

void push_ring(struct Ring* ring, double value);

#endif
