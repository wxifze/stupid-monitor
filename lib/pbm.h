#ifndef PBM_H
#define PBM_H

#include <stddef.h>
#include <stdbool.h>

struct Bitmap {
	const bool** buff;
	size_t width;
	size_t height;
};

struct Bitmap load_pbm(const char* path);

struct Bitmap load_exp_pbm(const char* path, size_t exp_width, size_t exp_height);

void free_pbm(struct Bitmap* bitmap);

#endif
