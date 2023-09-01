#include <err.h>
#include <stdio.h>

#include "pbm.h"

int main(int argc, const char** argv) { 
	if (argc != 4)
		errx(1, "usage: %s pbm_path array_path array_name", argc > 0 ? argv[0] : "pbm_to_array");

	const char* pbm_path = argv[1];
	const char* array_path = argv[2];
	const char* array_name = argv[3];

	struct Bitmap pbm = load_pbm(pbm_path);
	FILE* output = fopen(array_path, "w");
	if (!output)
		err(1, "failed to open `%s`", array_path);

	if (fprintf(output, "const unsigned char %s[] = {\n\t0x40,\n", array_name) < 1)
		err(1, "failed to write to `%s`", array_path);

	size_t i = 0;
	unsigned char byte = 0;
	for (size_t y = 0; y < pbm.height; y++)
		for (size_t x = 0; x < pbm.width; x++) {
			byte |= pbm.buff[i % 8 + i / 8 / pbm.width * 8][i / 8 % pbm.width] << i % 8;

			i++;
			if (i % 8 == 0) {
				if (i % 128 == 0) {
					if (fprintf(output, "0x%02hhx,\n", byte) < 1)
						err(1, "failed to write to `%s`", array_path);
				} else if (i / 8 % 16 == 1) {
					if (fprintf(output, "\t0x%02hhx, ", byte) < 1)
						err(1, "failed to write to `%s`", array_path);
				} else {
					if (fprintf(output, "0x%02hhx, ", byte) < 1)
						err(1, "failed to write to `%s`", array_path);
				}
				byte = 0;
			}
		}

	if (fprintf(output, "};") < 1)
		err(1, "failed to write to `%s`", array_path);


	// it isn't necessary, but just to be sure
	fclose(output);
	free_pbm(&pbm);
}
