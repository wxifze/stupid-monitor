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
#include <ctype.h>
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

void alloc_area(struct Area* area, size_t width, size_t height){
	if (!(area->buff = malloc(height * sizeof(bool*))))
		errx(1, "failed to allocate area");

	for (size_t y = 0; y < height; y++)
		if (!(area->buff[y] = malloc(width * sizeof(bool))))
			errx(1, "failed to allocate area");

	area->width = width;
	area->height = height;
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
double get_time() {
	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now))
		err(1, "clock_gettime failed");
	return now.tv_sec + now.tv_nsec / 1e9;
}

// TODO use clock_nanosleep?
void sleep_until(double target) {
	target -= get_time();
	if (target <= 0)
		return;

	struct timespec until = {
		.tv_sec = target,
		.tv_nsec = fmod(target, 1) * 1e9,
	};
	struct timespec left;

	for (;;) {
		int code = nanosleep(&until, &left);
		if (code == 0)
			break;
		if (code == -1 && errno == EINTR)
			until = left;
		else
			err(1, "nanosleep failed");
	}
}

struct Value get_rand() {
	return (struct Value) { 
		.timestamp = get_time(),
		.value = rand() % 10000
	};
}

struct Bitmap {
	const bool** buff;
	size_t width;
	size_t height;
};

struct Bitmap* digits;
struct Bitmap uptime_icon;
struct Bitmap uptime_text;
struct Bitmap fan_icon;
struct Bitmap rtt_icon;

int isspace_nonl(int c) {
	return isspace(c) && c != '\n';
}

// TODO refactor this mess
bool parse_pbm_header(const char* header, size_t* width, size_t* height) {
	// i'm to lazy to parse comments

	if (strncmp(header, "P1", 2))
		return false;

	if (!isspace_nonl(*(header + 2)))
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

	while (isspace_nonl(*end))
		end++;

	return *end == '\n';
}

struct Bitmap load_pbm(const char* path, size_t exp_width, size_t exp_height) {
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
					buff[y][x] = true;
					break;
				} else if (c == '1') {
					buff[y][x] = false;
					break;
				} else if (!isspace(c)) {
					errx(1, "garbage in pixel data in `%s`", path);
				} else if (c == EOF)
					errx(1, "not enough pixel data in `%s`", path);
			}

	return (struct Bitmap) {
		.buff = (const bool**)buff,
		.width = width,
		.height = height,
	};
}

void render_bitmap(struct Area* area, struct Bitmap* bitmap) {
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

	size_t maxn = (area->width + 1) / 4;

	clear_area(area);
	for (size_t n = maxn - 1; n < -1 /* don't ask */ && value; n--) {
		struct Area digit_area;
		get_subarea(area, &digit_area, n * 4, 0, 3, 4);
		if (value)
			render_bitmap(&digit_area, digits + (value % 10));
		value /= 10;
	}

	if (value)
		fill_area(area);
}

struct Ring {
	double* buff;
	size_t capacity;
	size_t begin;
	size_t length;
};

void alloc_ring(struct Ring* ring, size_t capacity) {
	if (!(ring->buff = malloc(capacity * sizeof(double))))
		errx(1, "failed to allocate ring");
	ring->capacity = capacity;
	ring->begin = 0;
	ring->length = 0;
}

/*
void set_ring(struct Ring* ring, size_t index, double value) {
	ring->buff[(ring->begin + index) % ring->capacity] = value;
}
*/

double get_ring(const struct Ring* ring, size_t index) {
	return ring->buff[(ring->begin + index) % ring->length];
}

void push_ring(struct Ring* ring, double value) {
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

	clear_area(area);
	for (size_t i = 0; i < ring->length; i++) {
		char value =  get_ring(ring, ring->length - 1 - i);
		assert(0 <= value && value <= area->height);
		if (value == area->height)
			value--;
		
		set_area(
			area, 
			area->width - 1 - i, 
			area->height - 1 - value,
			true
		);
	}
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
	uptime_icon = load_pbm("bitmaps/uptime_icon.pbm", 13, 11);
	uptime_text = load_pbm("bitmaps/uptime_text.pbm", 5, 14);
	fan_icon = load_pbm("bitmaps/fan_icon.pbm", 11, 11);
	rtt_icon = load_pbm("bitmaps/rtt_icon.pbm", 9, 32);
}

void open_or_die(const char* path, FILE** file) {
	if (!*file) {
		 *file = fopen(path, "r");
		if (!*file)
			err(1, "failed to open `%s`", path);
	}
	else {
		fflush(*file);
		rewind(*file);
	}
}

// first read returns garbage (average cpu load since boot)
double get_cpu() {
	const char* path = "/proc/stat";
	static FILE* stat;
	open_or_die(path, &stat);

	// linux/fs/proc/stat.c uses u64, so long long must be enough
	unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
	if (fscanf(
		stat, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", 
		&user, &nice, &system, &idle, &iowait,
		&irq, &softirq, &steal, &guest, &guest_nice
	) != 10)
		errx(1, "failed to parse `%s`", path);

	static unsigned long long old_busy, old_total;
	unsigned long long  busy, total, delta_busy, delta_total;

	busy = user + nice + system + irq + softirq + steal + guest + guest_nice;
	total = busy + idle + iowait;

	delta_busy = busy - old_busy;
	delta_total = total - old_total;

	old_busy = busy;
	old_total = total;

	printf("%f\n", (double) delta_busy / delta_total);
	return (double) delta_busy / delta_total;
}

double get_ram() {
	const char* path = "/proc/meminfo";
	static FILE* meminfo;
	open_or_die(path, &meminfo);

	// linux/fs/proc/meminfo.c uses long, but just to be sure
	unsigned long long total, available;

	if (fscanf(
		meminfo, 
		"MemTotal: %llu kB MemFree: %*llu kB MemAvailable: %llu kB", 
		&total, &available
	) != 2)
		errx(1, "failed to parse `%s`", path);

	printf("%f\n", (double) (total - available) / total);
	return (double) (total - available) / total;
}

double get_tccd1() {
	const char* path = "/sys/class/hwmon/hwmon0/temp3_input";
	static FILE* temp3_input;
	open_or_die(path, &temp3_input);

	unsigned long temp;
	if (fscanf(temp3_input, "%lu", &temp) != 1)
		errx(1, "failed to parse `%s`", path);

	printf("%f\n", (double) temp / 1000.0);
	return temp / 1000.0;
}

double get_fan1() {
	const char* path = "/sys/class/hwmon/hwmon2/fan1_input";
	static FILE* fan1_input;
	open_or_die(path, &fan1_input);

	unsigned int speed;
	if (fscanf(fan1_input, "%u", &speed) != 1)
		errx(1, "failed to parse `%s`", path);

	return speed;
}

double get_fan2() {
	const char* path = "/sys/class/hwmon/hwmon2/fan2_input";
	static FILE* fan2_input;
	open_or_die(path, &fan2_input);

	unsigned int speed;
	if (fscanf(fan2_input, "%u", &speed) != 1)
		errx(1, "failed to parse `%s`", path);

	return speed;
}

double get_fan3() {
	const char* path = "/sys/class/hwmon/hwmon2/fan3_input";
	static FILE* fan3_input;
	open_or_die(path, &fan3_input);

	unsigned int speed;
	if (fscanf(fan3_input, "%u", &speed) != 1)
		errx(1, "failed to parse `%s`", path);

	return speed;
}

double get_enp4s0_rx() {
	const char* path = "/sys/class/net/enp4s0/statistics/rx_bytes";
	static FILE* rx_bytes;
	open_or_die(path, &rx_bytes);

	unsigned long long bytes;
	if (fscanf(rx_bytes, "%llu", &bytes) != 1)
		errx(1, "failed to parse `%s`", path);

	static unsigned long long old_bytes;
	unsigned long long delta = bytes - old_bytes;
	old_bytes = bytes;

	return delta;
}

double get_enp4s0_tx() {
	const char* path = "/sys/class/net/enp4s0/statistics/tx_bytes";
	static FILE* tx_bytes;
	open_or_die(path, &tx_bytes);

	unsigned long long bytes;
	if (fscanf(tx_bytes, "%llu", &bytes) != 1)
		errx(1, "failed to parse `%s`", path);

	static unsigned long long old_bytes;
	unsigned long long delta = bytes - old_bytes;
	old_bytes = bytes;

	return delta;
}


struct Stats {
	double cpu;
	double ram;
	double temp;
	double fan1;
	double fan2;
	double fan3;
	double net_rx;
	double net_tx;
	double drive_read;
	double drive_write;
};

struct Stats get_stats() {
	struct Stats stats = {
		.cpu = get_cpu(),
		.ram = get_ram(),
		.temp = get_tccd1(),
		.fan1 = get_fan1(),
		.fan2 = get_fan2(),
		.fan3 = get_fan3(),
		/* TODO normalize 
		.net_rx = get_enp4s0_rx(),
		.net_tx = get_enp4s0_tx(),
		*/
	};
	return stats;
}

int main() {
	init_bitmaps();

	struct Area area;
	alloc_area(&area, 57, 32);

	struct Area cpu_plot_area;
	get_subarea(&area, &cpu_plot_area, 0, 0, 30, 10);

	struct Area tmp_plot_area;
	get_subarea(&area, &tmp_plot_area, 0, 11, 30, 10);

	struct Area ram_plot_area;
	get_subarea(&area, &ram_plot_area, 0, 22, 30, 10);

	struct Ring cpu_ring;
	alloc_ring(&cpu_ring, 30);

	struct Ring tmp_ring;
	alloc_ring(&tmp_ring, 30);

	struct Ring ram_ring;
	alloc_ring(&ram_ring, 30);

	/*
	struct Ring net_tx_ring;
	alloc_ring(&net_tx_ring, 30);

	struct Ring net_rx_ring;
	alloc_ring(&net_rx_ring, 30);
	*/

	
	double next_update = get_time();
	double x = 0;
	for (;;) {
		sleep_until(next_update);
		next_update += 1.0 / 1;

		push_ring(&cpu_ring, get_cpu() * 10);
		push_ring(&ram_ring, get_ram() * 10);
		push_ring(&tmp_ring, get_tccd1() / 10);
		//push_ring(&net_tx_ring, get_enp4s0_tx());
		//push_ring(&net_rx_ring, get_enp4s0_rx());

		render_plot(&cpu_plot_area, &cpu_ring);
		render_plot(&ram_plot_area, &ram_ring);
		render_plot(&tmp_plot_area, &tmp_ring);

		//render_scalar(&sv_area, (int)rv.value);
		//render_plot(&pl_area, &ring);

		draw_area(&area);
	}

	// no need to free memory, the program will either run forever or die horribly
}
