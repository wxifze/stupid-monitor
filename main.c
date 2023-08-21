#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <err.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <limits.h>

/*
struct Frame {
	size_t width;
	size_t height;
	bool** buff;
};
*/

struct Area {
	bool** buff;
	size_t width;
	size_t height;
	size_t x_offset;
	size_t y_offset;
};

bool alloc_area(struct Area* area, size_t width, size_t height){
	if (!(area->buff = malloc(height * sizeof(bool*))))
		return false;

	for (size_t y = 0; y < height; y++)
		if (!(area->buff[y] = malloc(width*sizeof(bool))))
		{
			while (--y >= 0)
				free(area->buff[y]);
			return false;
		}

	area->width = width;
	area->height = height;
	area->x_offset = 0;
	area->y_offset = 0;
	return true;
}

void free_area(struct Area* area) {
	for (size_t y = 0; y < area->height; y++)
		free(area->buff[y]);
	free(area->buff);
	area->buff = NULL;
	area->width = 0;
	area->height = 0;
	area->x_offset = 0;
	area->y_offset = 0;
}

void draw_area(const struct Area* area) {
	for (size_t i = 0; i < area->width; i++)
		printf("--");
	printf("\n");
	for (size_t y = 0; y < area->height; y++) {
		for (size_t x = 0; x < area->width; x++)
			if (area->buff[y][x])
				printf("##");
			else
				printf("  ");
		printf("\n");
	}
}

void set_area(struct Area* area, size_t x, size_t y, bool value) {
	assert(x < area->width);
	assert(y < area->height);
	area->buff[area->y_offset + y][area->x_offset + x] = value;
}

void get_subarea(
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

void fill_area(struct Area* area) {
	for (size_t y = 0; y < area->height; y++)
		for (size_t x = 0; x < area->width; x++)
			set_area(area, x, y, true);
}

struct Value {
	double timestamp;
	double value;
};


// TODO use CLOCK_MONOLITHIC?
double gettime() {
	struct timespec now;
	assert(!clock_gettime(CLOCK_REALTIME, &now));
	return now.tv_sec + now.tv_nsec / 1e9;
}

// TODO use clock_nanosleep?
void sleep_until(double target) {
	target -= gettime();
	if (target <= 0)
		return;

	struct timespec untill = {
		.tv_sec = target,
		.tv_nsec = fmod(target, 1) * 1e9,
	};
	struct timespec left;

	for (;;) {
		int code = nanosleep(&untill, &left);
		if (code == 0)
			break;
		if (code == EINTR)
			untill = left;
		else
			abort();
	}
}

struct Value get_rand() {
	return (struct Value) { 
		.timestamp = gettime(),
		.value = rand() % 10000
	};
}

bool digits[10][4][3] = {
	{ // 0
		{ 1,1,1 },
		{ 1,0,1 },
		{ 1,0,1 },
		{ 1,1,1 },
	},
	{ // 1
		{ 0,1,0 },
		{ 1,1,0 },
		{ 0,1,0 },
		{ 1,1,1 },
	},
	{ // 2
		{ 1,1,1 },
		{ 0,0,1 },
		{ 1,1,0 },
		{ 1,1,1 },
	},
	{ // 3
		{ 1,1,1 },
		{ 0,1,1 },
		{ 0,0,1 },
		{ 1,1,1 },
	},
	{ // 4
		{ 1,0,1 },
		{ 1,0,1 },
		{ 1,1,1 },
		{ 0,0,1 },
	},
	{ // 5
		{ 1,1,1 },
		{ 1,0,0 },
		{ 0,1,1 },
		{ 1,1,1 },
	},
	{ // 6
		{ 1,1,1 },
		{ 1,0,0 },
		{ 1,1,1 },
		{ 1,1,1 },
	},
	{ // 7
		{ 1,1,1 },
		{ 0,0,1 },
		{ 0,1,0 },
		{ 0,1,0 },
	},
	{ // 8
		{ 0,1,1 },
		{ 0,1,1 },
		{ 1,0,1 },
		{ 1,1,1 },
	},
	{ // 9
		{ 1,1,1 },
		{ 1,1,1 },
		{ 0,0,1 },
		{ 1,1,1 },
	},
};

void render_digit(struct Area* area, int digit) {
	assert(area->width == 3);
	assert(area->height == 4);
	assert(digit / 10 == 0);
	for (size_t y = 0; y < area->height; y++)
		for (size_t x = 0; x < area->width; x++)
			set_area(area, x, y, digits[digit][y][x]);
}

// area must be 4 by 4*n-1
void render_scalar(struct Area* area, unsigned int value) {
	printf("rendering scalar %u\n", value);
	assert((area->width + 1) % 4 == 0);
	assert(area->height == 4);

	size_t maxn = (area->width + 1) / 4;

	clear_area(area);
	for (size_t n = maxn - 1; n < -1 /* don't ask */ && value; n--) {
		struct Area digit_area;
		get_subarea(area, &digit_area, n * 4, 0, 3, 4);
		if (value)
			render_digit(&digit_area, value % 10);
		value /= 10;
	}

	if (value)
		fill_area(area);
}

int main() {
	struct Area area;
	if (!alloc_area(&area, 20, 5))
		errx(1, "failed to allocate area");

	struct Area sv_area;
	get_subarea(&area, &sv_area, 1, 1, 15, 4);
	
	double next_update = gettime();
	for (;;) {
		sleep_until(next_update);
		next_update += 0.5;

		struct Value rv = get_rand();

		//set_area(&area, (int)rv.value % 20, (int)rv.timestamp % 5, true);

		render_scalar(&sv_area, (int)rv.value);

		draw_area(&area);
		
	}

	/*
	draw_area(&area);

	fill_area(&area);
	draw_area(&area);
	
	struct Area subarea;
	get_subarea(&area, &subarea, 1, 1, 18, 3);
	clear_area(&subarea);
	draw_area(&area);
	*/

	free_area(&area);
}
