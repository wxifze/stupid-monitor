#include <err.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>

#include "timing.h"

double get_time() {
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now))
		err(1, "clock_gettime failed");
	return now.tv_sec + now.tv_nsec / 1e9;
}

bool sleep_until(double target) {
	target -= get_time();
	if (target <= 0)
		return false;

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
	return true;
}
