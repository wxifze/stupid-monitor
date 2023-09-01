#include <stdio.h>
#include <err.h>
#include <math.h>

#include "timing.h"

// hwmon names aren't persistent
// most of this should probably be reimplemented with libsensors

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
	double read_total = 0, written_total = 0;
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

// some stats are calcuated for the time perid between successive calls
// therefore it returns garbage on the first run
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
