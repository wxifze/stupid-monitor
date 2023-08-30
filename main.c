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

struct Area {
	bool** buff;
	size_t width;
	size_t height;
	size_t x_offset;
	size_t y_offset;
};

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
	assert(x < area->width);
	assert(y < area->height);
	area->buff[area->y_offset + y][area->x_offset + x] = value;
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

double get_time() {
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now))
		err(1, "clock_gettime failed");
	return now.tv_sec + now.tv_nsec / 1e9;
}

// NOTE aborts if target is overdue
void sleep_until(double target) {
	target -= get_time();
	if (target <= 0)
		abort();

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

struct Bitmap {
	const bool** buff;
	size_t width;
	size_t height;
};

struct Bitmap digits[10];
struct Bitmap prefixes[4];

int isspace_nonl(int c) {
	return isspace(c) && c != '\n';
}


// https://netpbm.sourceforge.net/doc/pbm.html

char* load_text(const char* path, size_t* size) {
	int file = open(path, O_RDONLY);
	if (file == -1)
		err(1, "failed to open `%s`", path);

	ssize_t file_size = lseek(file, 0, SEEK_END);
	if (file_size == -1)
		err(1, "failed to lseek to the end of `%s`", path);
	if (lseek(file, 0, SEEK_SET) == -1)
		err(1, "failed to lseek to the beginning of `%s`", path);

	char* buffer = malloc(file_size + 1);
	if (!buffer)
		err(1, "failed to allocate buffer for `%s`", path);
	ssize_t buffer_len = 0;

	while (buffer_len < file_size) {
		ssize_t r = read(file, buffer + buffer_len, file_size - buffer_len);
		if (r == -1)
			err(1, "failed to read `%s`", path);
		if (r == 0)
			errx(1, "file `%s` ended unexpectedly", path);
		buffer_len += r;
	}
	
	if (close(file) == -1)
		err(1, "failed to close `%s`", path);

	*size = file_size + 1;
	buffer[file_size] = '\0';
	return buffer;
}

// comments are allowed only in the header
// but it won't hurt if we remove them from the whole file
void clear_pbm(char* file, size_t* file_size) {
	size_t clean = 0;
	for (;;) {
		size_t start;
		for (start = clean; start < *file_size; start++)
			if (file[start] == '#')
				break;
		if (start == *file_size)
			return;

		size_t stop;
		for (stop = start; stop < *file_size; stop++)
			if (file[stop] == '\r' || file[stop] == '\n')
				break;
		// if invalid comment is after the end of the data section
		// it's fine, otherwise it will fail in the further parsing
		if (start == *file_size)
			return;

		size_t offset = stop - start;
		memmove(file + start, file + start + offset, *file_size - start - offset);

		clean = start;
	}
}

bool parse_pbm_header(char* file, size_t* file_size, size_t* width, size_t* height) {
	clear_pbm(file, file_size);

	if (*file_size < 2 || strncmp(file, "P1", 2))
		return false;

	char* start = file + 2;
	char* end;
	size_t* vars[] = { width, height };
	for (size_t i = 0; i < sizeof(vars) / sizeof(*vars); i++) {
		errno = 0;
		uintmax_t val = strtoumax(start, &end, 10);
		if (start == end || val == 0 ||  errno != 0) 
			return false;
		
		*vars[i] = val;
		start = end;
	}

	if (!*end && !isspace(*end))
		return false;

	size_t offset = end + 1 - file;
	memmove(file, file + offset, *file_size - offset);
	return true;
}

// NOTE leaks memory on fault by design
 const bool** parse_pbm_data(const char* file, size_t width, size_t height) {
	bool** buffer = calloc(height, sizeof(bool*));
	if (!buffer)
		return NULL;

	file--;
	for (size_t y = 0; y < height; y++) {
		buffer[y] = calloc(width, sizeof(bool));
		if (!buffer[y])
			return NULL;

		for (size_t x = 0; x < width; x++) {
			while (isspace(*++file));

			if (*file == '0')
				buffer[y][x] = true;
			else if (*file == '1')
				buffer[y][x] = false;
			else
				return NULL;
		}
	}

	return (const bool**)buffer;
}

struct Bitmap load_pbm(const char* path, size_t exp_width, size_t exp_height) {
	size_t file_size;
	char* file = load_text(path, &file_size);

	size_t width, height;
	if (!parse_pbm_header(file, &file_size, &width, &height))
		errx(1, "failed to parse header od `%s`", path);

	if (width != exp_width)
		errx(1, " width of image `%s` is %zu, while expecting %zu", path, width, exp_width);

	if (height != exp_height)
		errx(1, "height of image `%s` is %zu, while expecting %zu", path, height, exp_height);

	const bool** buffer = parse_pbm_data(file, width, height);
	if (!buffer)
		errx(1, "failed to parse data of `%s`", path);

	free(file);
	return (struct Bitmap) {
		.buff = buffer,
		.height = height,
		.width = width
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
		subarea(area, &digit_area, area->width - 3, 0, 3, 4);
		render_bitmap(&digit_area, digits);
		return;
	}

	for (size_t n = maxn - 1; n < -1 /* don't ask */ && value; n--) {
		struct Area digit_area;
		subarea(area, &digit_area, n * 4, 0, 3, 4);
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
	subarea(area, &scalar_area, 0, 0, 15, 4);
	render_scalar(&scalar_area, value);

	struct Bitmap* prefix = prefixes + p;
	struct Area prefix_area;
	subarea(area, &prefix_area, 16, 0, prefix->width, 4);
	render_bitmap(&prefix_area, prefix);
}

struct Ring {
	double* buff;
	size_t capacity;
	size_t begin;
	size_t length;
};

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

// values in ring must be normalized to [0:1]
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

		for (size_t h = value; h < -1; h--) 
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
		if (fflush(*file) == EOF)
			err(1, "failed to flush `%s`", path);
		rewind(*file);
	}
}

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
		"MemTotal: %llu kB MemFree: %*u kB MemAvailable: %llu kB", 
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

double get_jc42() {
	const char* path = "/sys/class/hwmon/hwmon1/temp1_input";
	static FILE* temp1_input;
	open_or_die(path, &temp1_input);

	unsigned long temp;
	if (fscanf(temp1_input, "%lu", &temp) != 1)
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

void get_disk(double* read, double* written) {
	static const char* paths[] = {
		"/sys/block/sda/stat",
		"/sys/block/sdb/stat",
	};

	static double old_read_total, old_written_total;
	double read_total, written_total;
	static FILE* disks[sizeof(paths) / sizeof(*paths)];
	for (size_t i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
		open_or_die(paths[i], disks + i);

		unsigned long long read, written;
		if (fscanf(disks[i], "%*u %*u %llu %*u %*u %*u %llu", &read, &written) != 2)
			errx(1, "failed to parse `%s`", paths[i]);

		read_total += read;
		written_total += written;
	}

	// sector size is independent of the actual disk
	*read = (read_total - old_read_total) * 512;
	*written = (written_total - old_written_total) * 512;
	old_read_total = read_total;
	old_written_total = written_total;
}


struct Stats {
	double cpu;
	double ram;
	double cpu_tmp;
	double ram_tmp;
	double minutes;
	double hours;
	double days;
	double fan1;
	double fan2;
	double fan3;
	double net_rx;
	double net_tx;
	double disk_r;
	double disk_w;
};

// many functions return garbage on the first run
struct Stats get_stats() {
	static double old_time;
	double time = get_time();
	double delta = time - old_time;
	old_time = time;

	struct Stats stats = {
		.cpu = get_cpu(),
		.ram = get_ram(),
		.cpu_tmp = get_tccd1(),
		.ram_tmp = get_jc42(),
		.fan1 = get_fan1(),
		.fan2 = get_fan2(),
		.fan3 = get_fan3(),
		.net_rx = get_enp4s0_rx() / delta,
		.net_tx = get_enp4s0_tx() / delta,
	};

	get_disk(&stats.disk_r, &stats.disk_w);
	stats.disk_r /= delta;
	stats.disk_w /= delta;

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

void check_display() {
	char error[128];
	ssize_t error_len = 0;
	for (int i = 0; i < 10; i++) {
		ssize_t r = read(display, error + error_len, sizeof(error) - error_len);
		if (r == -1)
			err(1, "failed to read from the display");

		if (r == 0 && error_len == 0)
			return;
		error_len += r;

		if (i < 9)
			sleep_until(get_time() + 0.1);
	}

	ssize_t start_crlf = -1, stop_crlf = -1;
	for (ssize_t i = 0; i < error_len; i++)
		if (error[i] == '\n') {
			start_crlf = stop_crlf + 1;
			stop_crlf = i;
		}

	if (start_crlf == -1) {
		for (ssize_t i = 0; i < error_len; i++)
			if (!isprint(error[i]))
				error[i] = '?';

		if (error_len < sizeof(error))
			errx(
				1, "display failed and timed out with mesage `%.*s`", 
				(int)error_len, error
			);

		errx(
			1, "display failed with invalid mesage `%.*s`",
			(int)error_len, error
		);
	}

	errx(
		1, "display failed with the mesage `%.*s`", 
		(int)(stop_crlf - start_crlf), error + start_crlf
	);
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

	check_display();
}

#define PLOT_WIDTH 38
#define PLOT_HEIGHT 10
#define UPDATES_PER_S 1.0

int main() {
	init_bitmaps();
	init_display("/dev/ttyUSB0", 666666);


	struct Area area;
	alloc_area(&area, 128, 64);
	render_bitmap(&area, &template);

	struct Area cpu_plot_area;
	subarea(&area, &cpu_plot_area, 0, 0, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area cpu_tmp_plot_area;
	subarea(&area, &cpu_tmp_plot_area, 0, 12, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area ram_tmp_plot_area;
	subarea(&area, &ram_tmp_plot_area, 0, 42, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area ram_plot_area;
	subarea(&area, &ram_plot_area, 0, 54, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area net_tx_area;
	subarea(&area, &net_tx_area, 90, 0, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area net_rx_area;
	subarea(&area, &net_rx_area, 90, 12, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area disk_r_area;
	subarea(&area, &disk_r_area, 90, 42, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area disk_w_area;
	subarea(&area, &disk_w_area, 90, 54, PLOT_WIDTH, PLOT_HEIGHT);


	struct Area cpu_scalar_area;
	subarea(&area, &cpu_scalar_area, 49, 6, 11, 4);

	struct Area cpu_tmp_scalar_area;
	subarea(&area, &cpu_tmp_scalar_area, 49, 12, 11, 4);

	struct Area ram_tmp_scalar_area;
	subarea(&area, &ram_tmp_scalar_area, 49, 48, 11, 4);

	struct Area ram_scalar_area;
	subarea(&area, &ram_scalar_area, 49, 54, 11, 4);

	struct Area net_tx_scalar_area;
	subarea(&area, &net_tx_scalar_area, 53, 0, 33, 4);

	struct Area net_rx_scalar_area;
	subarea(&area, &net_rx_scalar_area, 53, 18, 33, 4);

	struct Area disk_r_scalar_area;
	subarea(&area, &disk_r_scalar_area, 53, 42, 33, 4);

	struct Area disk_w_scalar_area;
	subarea(&area, &disk_w_scalar_area, 53, 60, 33, 4);


	struct Area uptime_days_area;
	subarea(&area, &uptime_days_area, 100, 25, 11, 4);

	struct Area uptime_hours_area;
	subarea(&area, &uptime_hours_area, 104, 30, 7, 4);

	struct Area uptime_minutes_area;
	subarea(&area, &uptime_minutes_area, 104, 35, 7, 4);


	struct Area fan1_area;
	subarea(&area, &fan1_area, 0, 25, 15, 4);

	struct Area fan2_area;
	subarea(&area, &fan2_area, 0, 30, 15, 4);

	struct Area fan3_area;
	subarea(&area, &fan3_area, 0, 35, 15, 4);


	struct Ring cpu_ring;
	alloc_ring(&cpu_ring, PLOT_WIDTH);

	struct Ring cpu_tmp_ring;
	alloc_ring(&cpu_tmp_ring, PLOT_WIDTH);

	struct Ring ram_tmp_ring;
	alloc_ring(&ram_tmp_ring, PLOT_WIDTH);

	struct Ring ram_ring;
	alloc_ring(&ram_ring, PLOT_WIDTH);

	struct Ring net_rx_ring;
	alloc_ring(&net_rx_ring, PLOT_WIDTH);

	struct Ring net_tx_ring;
	alloc_ring(&net_tx_ring, PLOT_WIDTH);

	struct Ring disk_r_ring;
	alloc_ring(&disk_r_ring, PLOT_WIDTH);

	struct Ring disk_w_ring;
	alloc_ring(&disk_w_ring, PLOT_WIDTH);

	
	double next_update = get_time() + 1.0 / UPDATES_PER_S;
	get_stats(); // removes first run garbage

	for (;;) {
		sleep_until(next_update);
		next_update += 1.0 / UPDATES_PER_S;

		struct Stats stats = get_stats();

		push_ring(&cpu_ring, stats.cpu);
		push_ring(&cpu_tmp_ring, stats.cpu_tmp);
		push_ring(&ram_tmp_ring, stats.ram_tmp);
		push_ring(&ram_ring, stats.ram);
		push_ring(&net_rx_ring, stats.net_rx);
		push_ring(&net_tx_ring, stats.net_tx);
		push_ring(&disk_r_ring, stats.disk_r);
		push_ring(&disk_w_ring, stats.disk_w);

		render_scalar(&cpu_scalar_area, stats.cpu * 100);
		render_scalar(&cpu_tmp_scalar_area, stats.cpu_tmp);
		render_scalar(&ram_tmp_scalar_area, stats.ram_tmp);
		render_scalar(&ram_scalar_area, stats.ram * 100);

		render_scalar_prefixed(&net_rx_scalar_area, stats.net_rx);
		render_scalar_prefixed(&net_tx_scalar_area, stats.net_tx);
		render_scalar_prefixed(&disk_r_scalar_area, stats.disk_r);
		render_scalar_prefixed(&disk_w_scalar_area, stats.disk_w);

		render_scalar(&uptime_days_area, stats.days);
		render_scalar(&uptime_hours_area, stats.hours);
		render_scalar(&uptime_minutes_area, stats.minutes);

		render_scalar(&fan1_area, stats.fan1);
		render_scalar(&fan2_area, stats.fan2);
		render_scalar(&fan3_area, stats.fan3);

		render_plot(&cpu_plot_area, &cpu_ring);
		render_plot_fluct(&cpu_tmp_plot_area, &cpu_tmp_ring);
		render_plot_fluct(&ram_tmp_plot_area, &ram_tmp_ring);
		render_plot(&ram_plot_area, &ram_ring);
		render_plot_norm(&net_rx_area, &net_rx_ring);
		render_plot_norm(&net_tx_area, &net_tx_ring);
		render_plot_norm(&disk_r_area, &disk_r_ring);
		render_plot_norm(&disk_w_area, &disk_w_ring);

		draw_display(&area);
	}

	// no need to free memory or close files
	// the program will either run forever or die horribly
}
