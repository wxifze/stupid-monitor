#ifndef STATS_H
#define STATS_H

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
struct Stats get_stats();

#endif
