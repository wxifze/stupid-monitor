#ifndef PBM_H
#define PBM_H

#include <stddef.h>
#include <stdbool.h>

struct Bitmap {
	const bool** buff;
	size_t width;
	size_t height;
};

struct Bitmap load_pbm(const char* path, size_t exp_width, size_t exp_height);

// since the program will never stop and free it's resources, there is no free_pbm()

#endif
