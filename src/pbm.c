#include <err.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include "pbm.h"

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

