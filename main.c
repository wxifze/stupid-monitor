#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <err.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <inttypes.h>

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
		if (!(area->buff[y] = malloc(width * sizeof(bool))))
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
				printf("[]");
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

bool*** digits;

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

struct Ring {
	char* buff;
	size_t capacity;
	size_t begin;
	size_t length;
};

bool alloc_ring(struct Ring* ring, size_t capacity) {
	if (!(ring->buff = malloc(capacity * sizeof(char))))
		return false;
	ring->capacity = capacity;
	ring->begin = 0;
	ring->length = 0;
	return true;
}

void free_ring(struct Ring* ring) {
	free(ring->buff);
	ring->capacity = 0;
	ring->begin = 0;
	ring->length = 0;
}

/*
void set_ring(struct Ring* ring, size_t index, double value) {
	ring->buff[(ring->begin + index) % ring->capacity] = value;
}
*/

char get_ring(const struct Ring* ring, size_t index) {
	return ring->buff[(ring->begin + index) % ring->length];
}

void push_ring(struct Ring* ring, char value) {
	ring->buff[(ring->begin + ring->length) % ring->capacity] = value;
	if (ring->length < ring->capacity)
		ring->length++;
	else
		ring->begin = (ring->begin + 1) % ring->capacity;
}

void render_plot(
		struct Area* area, 
		const struct Ring* ring
) {
	assert(ring->capacity <= area->width);
	assert(area->height == 10);

	clear_area(area);
	for (size_t i = 0; i < ring->length; i++) {
		char value =  get_ring(ring, ring->length - 1 - i);
		assert(0 <= value && value < 10);
		set_area(
			area, 
			area->width - 1 - i, 
			area->height - 1 - value,
			true
		);
	}
}

bool parse_pbm_header(const char* header, size_t* width, size_t* height) {
	// i'm to lazy to parse comments

	if (strncmp(header, "P1 ", 3))
		return false;

	size_t* vars[] = { width, height };
	const char* end = header + 3;
	for (size_t i = 0; i < sizeof(vars) / sizeof(*vars); i++) {
		header = end;
		errno = 0;

		// i don't know why i try to be so portable...
		uintmax_t var = strtoumax(header, (char**)&end, 10);
		if (errno || var == 0 || var > SIZE_MAX)
			return false;

		*vars[i] = var;
	}

	while (*end == ' ') 
		end++;

	return *end == '\n';
}

bool** load_pbm(const char* path, size_t exp_width, size_t exp_height) {
	FILE* file = fopen(path, "r");
	if (!file)
		err(1, "failed to open `%s`", path);

	char* header = NULL;
	size_t header_size;
	ssize_t header_len = getline(&header, &header_size, file);

	if (header_len == -1)
		err(1, "failed to read PBM header in `%s`", path);

	size_t width, height;
	if (!parse_pbm_header(header, &width, &height))
		errx(1, "invalid PBM header in `%s`", path);

	if (width != exp_width)
		errx(1, "expected `%s` width to be %zu, got %zu", path, exp_width, width);
	if (height != exp_height)
		errx(1, "expected `%s` height to be %zu, got %zu", path, exp_height, height);

	free(header);


	bool** buff = malloc(height * sizeof(bool*));
	if (!buff)
		errx(1, "failed to allocate buffer for `%s`", path);
	for (size_t i = 0; i < height; i++)
		if (!(buff[i] = malloc(width * sizeof(bool))))
			errx(1, "failed to allocate buffer for `%s`", path);

	for (size_t y = 0; y < height; y++)
		for (size_t x = 0; x < width; x++)
			for (;;) {
				int c = getc(file);
				if (c == '0') {
					buff[y][x] = false;
					break;
				} else if (c == '1') {
					buff[y][x] = true;
					break;
				} else if (c != ' ' && c != '\n') {
					errx(1, "garbage in pixel data in `%s`", path);
				} else if (c == EOF)
					errx(1, "not enough pixel data in `%s`", path);
			}

	return buff;
}

void init_digits() {
	const char* paths[] = {
		"bitmaps/0.pbm",
		"bitmaps/1.pbm",
		"bitmaps/2.pbm",
		"bitmaps/3.pbm",
		"bitmaps/4.pbm",
		"bitmaps/5.pbm",
		"bitmaps/6.pbm",
		"bitmaps/7.pbm",
		"bitmaps/8.pbm",
		"bitmaps/9.pbm",
	};
	const size_t paths_len = sizeof(paths) / sizeof(*paths);

	if (!(digits = malloc(paths_len)))
		errx(1, "failed to allocate for digits");

	for (size_t i = 0; i < paths_len; i++)
		digits[i] = load_pbm(paths[i], 3, 4);
}

void init_bitmaps() {
	init_digits();
}

int main() {
	init_bitmaps();

	struct Area area;
	if (!alloc_area(&area, 20, 16))
		errx(1, "failed to allocate area");

	struct Area sv_area;
	get_subarea(&area, &sv_area, 1, 1, 15, 4);

	struct Ring ring;
	if (!(alloc_ring(&ring, 20)))
		errx(1, "failed to allocate ring");

	struct Area pl_area;
	get_subarea(&area, &pl_area, 0, 6, 20, 10);
	
	double next_update = gettime();
	double x = 0;
	for (;;) {
		sleep_until(next_update);
		next_update += 1.0 / 30;

		struct Value rv = get_rand();

		push_ring(&ring, sin(x += 0.4) * 5 + 5);

		render_scalar(&sv_area, (int)rv.value);
		render_plot(&pl_area, &ring);


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

	free_ring(&ring);
	free_area(&area);
}
