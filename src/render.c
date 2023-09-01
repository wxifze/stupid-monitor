#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>

#include "render.h"

struct Bitmap digits[10];
struct Bitmap prefixes[4];

void render_bitmap(struct Area* area, struct Bitmap* bitmap) {
	assert(bitmap);
	assert(bitmap->buff);
	assert(area->width == bitmap->width);
	assert(area->height == bitmap->height);
	for (size_t y = 0; y < area->height; y++)
		for (size_t x = 0; x < area->width; x++)
			set_area(area, x, y, bitmap->buff[y][x]);
}

// area must be 4 by 4*n-1
void render_scalar(struct Area* area, unsigned int value) {
	assert((area->width + 1) % 4 == 0);
	assert(area->height == 4);
	assert(digits);

	size_t maxn = (area->width + 1) / 4;
	assert(value / (int)pow(10, maxn) == 0);

	clear_area(area);

	if (value == 0) {
		struct Area digit_area;
		subarea(area, &digit_area, area->width - 3, 0, 3, 4);
		render_bitmap(&digit_area, digits);
		return;
	}

	for (ssize_t n = maxn - 1; n >= 0; n--) {
		struct Area digit_area;
		subarea(area, &digit_area, n * 4, 0, 3, 4);
		if (value)
			render_bitmap(&digit_area, digits + (value % 10));
		value /= 10;
	}
}

// area must be 4 by 33
// value souldn't be greater or equal to 1024^4
void render_scalar_prefixed(struct Area* area, unsigned long long value) {
	assert(area->width == 33);

	clear_area(area);
	ssize_t p = 0;
	while (value > 1024) {
		value /= 1024;
		p++;
	}

	assert(p <= 3);

	struct Area scalar_area;
	subarea(area, &scalar_area, 0, 0, 15, 4);
	render_scalar(&scalar_area, value);

	struct Bitmap* prefix = prefixes + p;
	struct Area prefix_area;
	subarea(area, &prefix_area, 16, 0, prefix->width, 4);
	render_bitmap(&prefix_area, prefix);
}

// values in the ring must be normalized to [0:1]
void render_plot(
		struct Area* area, 
		const struct Ring* ring
) {
	assert(ring->capacity == area->width);

	clear_area(area);
	for (size_t i = 0; i < ring->length; i++) {
		double value = get_ring(ring, ring->length - 1 - i);
		assert(0 <= value && value <= 1);

		/*
		area->height == 5
		[4:5] -> 4
		[3:4) -> 3
		[2:3) -> 2
		[1:2) -> 1
		[0:1) -> 0
		*/
		value = value * area->height;
		if (value == area->height)
			value--;

		for (ssize_t h = value; h >= 0; h--) 
			set_area(
				area, 
				area->width - 1 - i, 
				area->height - 1 - h,
				true
			);
	}
}

void render_plot_norm(
		struct Area* area, 
		const struct Ring* ring
) {
	double tmp[ring->capacity];
	struct Ring norm_ring = {
		.buff = tmp,
		.capacity = ring->capacity,
		.begin = 0, 
		.length = 0,
	};

	double max = 0;
	for (size_t i = 0; i < ring->length; i++) {
		double value = get_ring(ring, i);
		assert(0 <= value);
		if (value > max)
			max = value;
	}

	for (size_t i = 0; i < ring->length; i++) {
		double value = max ? get_ring(ring, i) / max : 0;
		push_ring(&norm_ring, value);
	}

	render_plot(area, &norm_ring);
}

void render_plot_fluct(
		struct Area* area, 
		const struct Ring* ring
) {
	double tmp[ring->capacity];
	struct Ring fluct_ring = {
		.buff = tmp,
		.capacity = ring->capacity,
		.begin = 0, 
		.length = 0,
	};

	double min = INFINITY, max = 0;
	for (size_t i = 0; i < ring->length; i++) {
		double value = get_ring(ring, i);
		assert(0 <= value);
		if (value > max)
			max = value;
		if (value < min)
			min = value;
	}
	assert(min != INFINITY);
	max -= min;

	for (size_t i = 0; i < ring->length; i++) {
		double value;
		if (min == max || max == 0)
			value = 0;
		else
			value = (get_ring(ring, i) - min) / max;
		push_ring(&fluct_ring, value);
	}

	render_plot(area, &fluct_ring);
}

struct Bitmap init_bitmap(const char* path, const char* name, size_t width, size_t height) {
	size_t path_len = strlen(path);
	size_t name_len = strlen(name);
	char full_path[path_len + 1 + name_len + 1];

	memcpy(full_path, path, path_len);
	full_path[path_len] = '/';
	memcpy(full_path + path_len + 1, name, name_len + 1);

	return load_exp_pbm(full_path, width, height);
}

// path must point to a folder with:
// 10 images named "0.pbm", "1.pbm", ..., "9.pbm" of size 3 by 4
// and images named "bs.pbm", "kibs.pbm", "mibs.pbm" and "gibs.pbm" of sizes
// 9 by 4, 15 by 4, 17 by 4 and 15 by 4 respectively
void init_render(const char* path) {
	char digit_name[] = " .pbm";
	for (size_t i = 0; i < 10; i++) {
		digit_name[0] = '0' + i;
		digits[i] = init_bitmap(path, digit_name, 3, 4);
	}

	prefixes[0] = init_bitmap(path, "bs.pbm", 9, 4);
	prefixes[1] = init_bitmap(path, "kibs.pbm", 15, 4);
	prefixes[2] = init_bitmap(path, "mibs.pbm", 17, 4);
	prefixes[3] = init_bitmap(path, "gibs.pbm", 15, 4);
}

