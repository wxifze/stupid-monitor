#include <err.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "display.h"
#include "timing.h"


int init_display(const char* path, speed_t baud) {
	int display = open(path, O_RDWR);
	if (display == -1)
		err(1, "failed to open `%s`", path);

	struct termios2 config;
	if (ioctl(display, TCGETS2, &config))
		err(1, "failed to get terminos2 on `%s`", path);


	// is all of this really necessary?

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

	return display;
}

void check_display(int display) {
	char error[128];
	size_t error_len = 0;
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
	for (size_t i = 0; i < error_len; i++)
		if (error[i] == '\n') {
			start_crlf = stop_crlf + 1;
			stop_crlf = i;
		}

	if (start_crlf == -1) {
		for (size_t i = 0; i < error_len; i++)
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

void draw_display(int display, const struct Area* area) {
	assert(area->width == 128);
	assert(area->height == 64);

	unsigned char buff[1024] = {};
	for (size_t i = 0; i < sizeof(buff); i++) {
		for (int j = 0; j < 8; j++)
			//buff[i] |= area->buff[i / 128 * 8 + j][i % 128] << j;
			buff[i] |= get_area(area, i % 128, i / 128 * 8 + j) << j;
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

	check_display(display);
}
