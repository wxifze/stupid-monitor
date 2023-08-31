#include <stdlib.h>
#include <assert.h>
#include <err.h>

#include "ring.h"

void alloc_ring(struct Ring* ring, size_t capacity) {
	if (!(ring->buff = calloc(capacity, sizeof(double))))
		err(1, "failed to allocate memory for ring");
	ring->capacity = capacity;
	ring->begin = 0;
	ring->length = 0;
}

double get_ring(const struct Ring* ring, size_t index) {
	assert(index < ring->length);
	return ring->buff[(ring->begin + index) % ring->length];
}

void push_ring(struct Ring* ring, double value) {
	ring->buff[(ring->begin + ring->length) % ring->capacity] = value;
	if (ring->length < ring->capacity)
		ring->length++;
	else
		ring->begin = (ring->begin + 1) % ring->capacity;
}
