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
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm/termbits.h>

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

/*
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
*/

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

struct Bitmap digits[10];
struct Bitmap prefixes[4];
/*
struct Bitmap uptime_icon;
struct Bitmap uptime_text;
struct Bitmap fan_icon;
struct Bitmap rtt_icon;
*/

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

	size_t maxn = (area->width + 1) / 4;
	assert(value / (int)pow(10, maxn) == 0);

	clear_area(area);

	if (value == 0) {
		struct Area digit_area;
		get_subarea(area, &digit_area, area->width - 3, 0, 3, 4);
		render_bitmap(&digit_area, digits);
		return;
	}

	for (size_t n = maxn - 1; n < -1 /* don't ask */ && value; n--) {
		struct Area digit_area;
		get_subarea(area, &digit_area, n * 4, 0, 3, 4);
		if (value)
			render_bitmap(&digit_area, digits + (value % 10));
		value /= 10;
	}
}

void render_scalar_prefixed(struct Area* area, unsigned long long value) {
	assert(area->width == 33);

	clear_area(area);
	ssize_t p = 0;
	while (value > 1024) {
		value /= 1024;
		p++;
	}

	if (p > 3) 
		abort();

	struct Area scalar_area;
	get_subarea(area, &scalar_area, 0, 0, 15, 4);
	render_scalar(&scalar_area, value);

	struct Bitmap* prefix = prefixes + p;
	struct Area prefix_area;
	get_subarea(area, &prefix_area, 16, 0, prefix->width, 4);
	render_bitmap(&prefix_area, prefix);
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

void render_plot(
		struct Area* area, 
		const struct Ring* ring
) {
	assert(ring->capacity <= area->width);

	clear_area(area);
	for (size_t i = 0; i < ring->length; i++) {
		int value = get_ring(ring, ring->length - 1 - i);
		assert(0 <= value && value <= area->height);
		if (value == area->height)
			value--;
		
		while (value >= 0) {
			set_area(
				area, 
				area->width - 1 - i, 
				area->height - 1 - value,
				true
			);
			value--;
		}
	}
}

void render_plot_normalized(
		struct Area* area, 
		const struct Ring* ring
) {
	assert(ring->capacity <= area->width);

	double max = 0;
	for (size_t i = 0; i < ring->length; i++)
	{
		double value = get_ring(ring, i);
		assert(0 <= value);
		if (value > max)
			max = value;
	}
	max /= area->height;

	clear_area(area);
	for (size_t i = 0; i < ring->length; i++) {
		int value = max == 0 ? 0 : get_ring(ring, ring->length - 1 - i) / max;
		if (value == area->height)
			value--;
		
		while (value >= 0) {
			set_area(
				area, 
				area->width - 1 - i, 
				area->height - 1 - value,
				true
			);
			value--;
		}
	}
}


void init_digits() {
	const char* paths[10] = {
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

	for (size_t i = 0; i < 10; i++)
		digits[i] = load_pbm(paths[i], 3, 4);
}

struct Bitmap template;

void init_bitmaps() {
	init_digits();
	/*
	uptime_icon = load_pbm("bitmaps/uptime_icon.pbm", 13, 11);
	uptime_text = load_pbm("bitmaps/uptime_text.pbm", 5, 14);
	fan_icon = load_pbm("bitmaps/fan_icon.pbm", 11, 11);
	rtt_icon = load_pbm("bitmaps/rtt_icon.pbm", 9, 32);
	*/

	template = load_pbm("bitmaps/template.pbm", 128, 64);
	prefixes[0] = load_pbm("bitmaps/bs.pbm", 9, 4);
	prefixes[1] = load_pbm("bitmaps/kibs.pbm", 15, 4);
	prefixes[2] = load_pbm("bitmaps/mibs.pbm", 17, 4);
	prefixes[3] = load_pbm("bitmaps/gibs.pbm", 15, 4);
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
// TODO remove garbage on the first run
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

	return (double) (total - available) / total;
}

double get_tccd1() {
	const char* path = "/sys/class/hwmon/hwmon0/temp3_input";
	static FILE* temp3_input;
	open_or_die(path, &temp3_input);

	unsigned long temp;
	if (fscanf(temp3_input, "%lu", &temp) != 1)
		errx(1, "failed to parse `%s`", path);

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

double get_uptime() {
	const char* path = "/proc/uptime";
	static FILE* uptime ;
	open_or_die(path, &uptime);

	double time;
	if (fscanf(uptime, "%lf", &time) != 1)
		errx(1, "failed to parse `%s`", path);

	return time;
}

void get_drive(double* read, double* written) {
	static const char* paths[] = {
		"/sys/block/sda/stat",
		"/sys/block/sdb/stat",
	};

	static double old_read_total, old_written_total;
	double read_total, written_total;
	static FILE* drives[sizeof(paths) / sizeof(*paths)];
	for (size_t i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
		open_or_die(paths[i], drives + i);

		unsigned long long read, written;
		if (fscanf(drives[i], "%*llu %*llu %llu %*llu %*llu %*llu %llu", &read, &written) != 2)
			errx(1, "failed to parse `%s`", paths[i]);

		read_total += read;
		written_total += written;
	}

	// sector size is independent of the actual drive
	*read = (read_total - old_read_total) * 512;
	*written = (written_total - old_written_total) * 512;
	old_read_total = read_total;
	old_written_total = written_total;
}


struct Stats {
	double cpu;
	double ram;
	double temp;
	double minutes;
	double hours;
	double days;
	double fan1;
	double fan2;
	double fan3;
	double net_rx;
	double net_tx;
	double drive_r;
	double drive_w;
};

struct Stats get_stats() {
	static double old_time;
	double time = get_time();
	double delta = time - old_time;
	old_time = time;

	struct Stats stats = {
		.cpu = get_cpu(),
		.ram = get_ram(),
		.temp = get_tccd1(),
		.fan1 = get_fan1(),
		.fan2 = get_fan2(),
		.fan3 = get_fan3(),
		.net_rx = get_enp4s0_rx() / delta,
		.net_tx = get_enp4s0_tx() / delta,
	};

	get_drive(&stats.drive_r, &stats.drive_w);
	stats.drive_r /= delta;
	stats.drive_w /= delta;

	double uptime = get_uptime();
	stats.minutes = fmod(uptime / 60, 60);
	stats.hours = fmod(uptime / (60 * 60), 24);
	stats.days = uptime / (60 * 60 * 24);

	return stats;
}

int display;
void init_display(const char* path, speed_t baud) {
	display = open(path, O_RDWR);
	if (display == -1)
		err(1, "failed to open `%s`", path);

	struct termios2 config;
	if (ioctl(display, TCGETS2, &config))
		err(1, "failed to get terminos2 on `%s`", path);

	config.c_cflag &= ~PARENB;
	config.c_cflag &= ~CSTOPB;
	config.c_cflag &= ~CSIZE;
	config.c_cflag |= CS8;
	config.c_cflag &= ~CRTSCTS;
	config.c_cflag |= CLOCAL | CREAD;

	config.c_lflag &= ~ICANON;
	config.c_lflag &= ~ECHO;
	config.c_lflag &= ~ISIG;

	config.c_iflag &= ~(IXON|IXOFF|IXANY);
	config.c_iflag &= ~(IXON|IXOFF|IXANY);
	config.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);

	config.c_oflag &= ~OPOST;
	config.c_oflag &= ~ONLCR;

	config.c_cc[VMIN] = 0;
	config.c_cc[VTIME] = 0;

	config.c_ispeed = baud;
	config.c_ospeed = baud;

	config.c_cflag &= ~CBAUD;
	config.c_cflag |= BOTHER;
	if (ioctl(display, TCSETS2, &config))
		err(1, "failed to set terminos2 on `%s`", path);

	// arduino bootloader waits for 1.6 seconds before executing code
	sleep_until(get_time() + 2);
}

void draw_display(const struct Area* area) {
	assert(area->width == 128);
	assert(area->height == 64);

	unsigned char buff[1024] = {};
	for (size_t i = 0; i < sizeof(buff); i++) {
		for (int j = 0; j < 8; j++)
			buff[i] |= area->buff[i / 128 * 8 + j][i % 128] << j;
	}

	for (ssize_t written = 0;;) {
		ssize_t w = write(display, buff + written, sizeof(buff) - written);
		if (w == -1)
			err(1, "failed to write to the display");
		else 
			written += w;

		if (written == sizeof(buff))
			break;
	}

	//TODO add error handling
}


int main() {
	init_bitmaps();
	init_display("/dev/ttyUSB0", 666666);


	struct Area area;
	alloc_area(&area, 128, 64);
	render_bitmap(&area, &template);


	struct Area cpu_plot_area;
	get_subarea(&area, &cpu_plot_area, 0, 0, 30, 10);

	struct Area temp_plot_area;
	get_subarea(&area, &temp_plot_area, 0, 11, 30, 10);

	struct Area ram_plot_area;
	get_subarea(&area, &ram_plot_area, 0, 22, 30, 10);

	struct Area net_rx_area;
	get_subarea(&area, &net_rx_area, 61, 0, 30, 10);

	struct Area net_tx_area;
	get_subarea(&area, &net_tx_area, 61, 11, 30, 10);

	struct Area drive_r_area;
	get_subarea(&area, &drive_r_area, 61, 22, 30, 10);

	struct Area drive_w_area;
	get_subarea(&area, &drive_w_area, 61, 33, 30, 10);


	struct Area cpu_scalar_area;
	get_subarea(&area, &cpu_scalar_area, 41, 3, 11, 4);

	struct Area temp_scalar_area;
	get_subarea(&area, &temp_scalar_area, 45, 14, 7, 4);

	struct Area ram_scalar_area;
	get_subarea(&area, &ram_scalar_area, 41, 25, 11, 4);

	// TODO check order
	struct Area net_tx_scalar_area;
	get_subarea(&area, &net_tx_scalar_area, 95, 1, 33, 4);

	struct Area net_rx_scalar_area;
	get_subarea(&area, &net_rx_scalar_area, 95, 16, 33, 4);

	struct Area drive_r_scalar_area;
	get_subarea(&area, &drive_r_scalar_area, 95, 23, 33, 4);

	struct Area drive_w_scalar_area;
	get_subarea(&area, &drive_w_scalar_area, 95, 38, 33, 4);


	struct Area uptime_days_area;
	get_subarea(&area, &uptime_days_area, 0, 50, 11, 4);

	struct Area uptime_hours_area;
	get_subarea(&area, &uptime_hours_area, 4, 55, 7, 4);

	struct Area uptime_minutes_area;
	get_subarea(&area, &uptime_minutes_area, 4, 60, 7, 4);


	struct Area fan1_area;
	get_subarea(&area, &fan1_area, 19, 50, 15, 4);

	struct Area fan2_area;
	get_subarea(&area, &fan2_area, 19, 55, 15, 4);

	struct Area fan3_area;
	get_subarea(&area, &fan3_area, 19, 60, 15, 4);


	struct Ring cpu_ring;
	alloc_ring(&cpu_ring, 30);

	struct Ring tmp_ring;
	alloc_ring(&tmp_ring, 30);

	struct Ring ram_ring;
	alloc_ring(&ram_ring, 30);

	struct Ring net_rx_ring;
	alloc_ring(&net_rx_ring, 30);

	struct Ring net_tx_ring;
	alloc_ring(&net_tx_ring, 30);

	struct Ring drive_r_ring;
	alloc_ring(&drive_r_ring, 30);

	struct Ring drive_w_ring;
	alloc_ring(&drive_w_ring, 30);
	
	double next_update = get_time();
	double x = 0;
	for (;;) {
		sleep_until(next_update);
		next_update += 1.0 / 1;

		struct Stats stats = get_stats();

		push_ring(&cpu_ring, stats.cpu * 10);
		push_ring(&tmp_ring, stats.temp / 10);
		push_ring(&ram_ring, stats.ram * 10);
		push_ring(&net_rx_ring, stats.net_rx);
		push_ring(&net_tx_ring, stats.net_tx);
		push_ring(&drive_r_ring, stats.drive_r);
		push_ring(&drive_w_ring, stats.drive_w);

		render_scalar(&cpu_scalar_area, stats.cpu * 100);
		render_scalar(&temp_scalar_area, stats.temp);
		render_scalar(&ram_scalar_area, stats.ram * 100);

		render_scalar_prefixed(&net_rx_scalar_area, stats.net_rx);
		render_scalar_prefixed(&net_tx_scalar_area, stats.net_tx);
		render_scalar_prefixed(&drive_r_scalar_area, stats.drive_r);
		render_scalar_prefixed(&drive_w_scalar_area, stats.drive_w);

		render_scalar(&uptime_days_area, stats.days);
		render_scalar(&uptime_hours_area, stats.hours);
		render_scalar(&uptime_minutes_area, stats.minutes);

		render_scalar(&fan1_area, stats.fan1);
		render_scalar(&fan2_area, stats.fan2);
		render_scalar(&fan3_area, stats.fan3);

		render_plot(&cpu_plot_area, &cpu_ring);
		render_plot(&ram_plot_area, &ram_ring);
		render_plot(&temp_plot_area, &tmp_ring);
		render_plot_normalized(&net_rx_area, &net_rx_ring);
		render_plot_normalized(&net_tx_area, &net_tx_ring);
		render_plot_normalized(&drive_r_area, &drive_r_ring);
		render_plot_normalized(&drive_w_area, &drive_w_ring);

		draw_display(&area);
	}

	// no need to free memory or close files
	// the program will either run forever or die horribly
}
