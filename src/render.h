#ifndef RENDER_H
#define RENDER_H

#include "pbm.h"
#include "area.h"
#include "ring.h"

// path must point to a folder with:
// 10 images named "0.pbm", "1.pbm", ..., "9.pbm" of size 3 by 4
// and images named "bs.pbm", "kibs.pbm", "mibs.pbm" and "gibs.pbm" of sizes
// 9 by 4, 15 by 4, 17 by 4 and 15 by 4 respectively
void init_render(const char* path);

// since the program will never stop and free it's resources, there is no free_render()

void render_bitmap(struct Area* area, struct Bitmap* bitmap);

// area must be 4 by 4*n-1
void render_scalar(struct Area* area, unsigned int value);

// area must be 4 by 33
// value souldn't be greater or equal to 1024^4
void render_scalar_prefixed(struct Area* area, unsigned long long value);

// values in the ring must be normalized to [0:1]
void render_plot(
		struct Area* area, 
		const struct Ring* ring
);

void render_plot_norm(
		struct Area* area, 
		const struct Ring* ring
);

void render_plot_fluct(
		struct Area* area, 
		const struct Ring* ring
);

#endif
