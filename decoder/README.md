GCIF Reader
===========

Reader for GCIF version 1.0

See https://github.com/gameclosure/gcif for more information.

Instructions for use are in GCIFReader.h

Example integration:

~~~
#include "gcif-reader/GCIFReader.h"

unsigned char *load_image_from_memory(unsigned char *bits, long bits_length, int *width, int *height, int *channels) {
	// If is GCIF,
	if (!gcif_sig_cmp(bits, bits_length)) {
		GCIFImage image;
		int err;

		// Read it
		if ((err = gcif_read_memory(bits, bits_length, &image))) {
			LOG("{tex} Unable to read GCIF image: %s", gcif_read_errstr(err));
		}

		*width = image.width;
		*height = image.height;
		*channels = 4;
		return image.rgba;
	}

	...
}

...

gcif_free_image(rgba);
~~~

