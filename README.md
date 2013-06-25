GCIF
====

Game Closure Image Format : 1.1

This is a lossless RGBA image format suited for mobile game sprite-sheets and
other usage cases (such as webpages) where you want to compress tightly once,
and then read it back many times.  For these images the expected size should be
about 1024x1024 pixels or smaller.

It typically produces files that are 66% the size of PNGCRUSH output and about
94% the size of WebP output, while decoding faster than both.

The format is released under the BSD license as forever patent-free, monetarily
free, and open-source software.  Contributions, discussions, and a healthy dose
of criticism are all welcome.

The code is well-written in our opinion, easy to read and adapt, and easy to
incorporate into mobile development.  The decoder is split off into a minimal
set of portable C++ source files that implement the image reader capability.

The image reader is split off into a light-weight repository here:
https://github.com/gameclosure/gcif-reader


Compression performance
=======================

From one of our more challenging game sprite-sheets chosen at random:

~~~
-rw-r--r--  1 cat  staff   3.0M Mar 31 20:40 noalpha.bmp (original)
-rw-r--r--@ 1 cat  staff   1.2M Apr 28 18:48 noalpha.jp2 (lossless)
-rw-r--r--  1 cat  staff   1.1M Apr 28 18:55 noalpha.png (lossless) <- PNGCRUSH
-rw-r--r--  1 cat  staff   912K Apr 28 18:51 noalpha.webp (lossless)
-rw-r--r--  1 cat  staff   883K Apr 29 17:01 noalpha.m6.webp (lossless)
-rw-r--r--  1 cat  staff   877K Apr  2 14:05 noalpha.bcif (lossless)
-rw-r--r--  1 cat  staff   803K Apr 28 18:47 noalpha.gci (lossless) <- GCIF 1.0
-rw-r--r--  1 cat  staff   799K Jun 24 20:14 noalpha.gci (lossless) <- GCIF 1.1
-rw-r--r--@ 1 cat  staff   682K Apr 28 18:46 noalpha.jpg (lossy)
-rw-r--r--@ 1 cat  staff   441K Apr 28 18:46 noalpha.gif (lossy)
~~~

In this case the result is 70% the size of the equivalent PNGCRUSH file output,
and is 90% the size of the WebP file output.

The compression ratio for this speed reaches for the Pareto frontier for
lossless image compression without using any multithreading, though it is a
thread-safe codebase, allowing you to decode several images in parallel.

The following is spritesheet sizes from several mobile games in bytes:

~~~
mmp
PNG: 30047457
PNGCRUSH: 29651792
WebP: 21737180
GCIF: 18695920

critter
PNG: 9998692
PNGCRUSH: 8770312
WebP: 6241754
GCIF: 5865820

pop
PNG: 10755955
PNGCRUSH: 7861910
WebP: 5418332
GCIF: 5059436

monster
PNG: 4005287
PNGCRUSH: 3227017
WebP: 2211330
GCIF: 2123884

chicken
PNG: 455147
PNGCRUSH: 452368
WebP: 314722
GCIF: 354052

fruit
PNG: 2052841
PNGCRUSH: 1911033
WebP: 1149780
GCIF: 1511792

hippo
PNG: 3291540
PNGCRUSH: 3192566
WebP: 2255838
GCIF: 2170476

pudding
PNG: 1401473
PNGCRUSH: 1401247
WebP: 986948
GCIF: 1113880

wyvern
PNG: 7724058
PNGCRUSH: 6463701
WebP: 4305978
GCIF: 4568944

xxx
PNG: 2131310
PNGCRUSH: 1762601
WebP: 1226082
GCIF: 1241352

blob
PNG: 2131310
PNGCRUSH: 1762601
WebP: 1226082
GCIF: 1263548

voyager
PNG: 50979862
PNGCRUSH: 40413850
WebP: 28309198
GCIF: 26950356

Overall
PNG: 124974932
PNGCRUSH: 106870998
WebP: 75383224
GCIF: 70919460

On average,
GCIF is 56.7% the size of PNG.
GCIF is 66.3% the size of PNGCRUSH.
GCIF is 94.1% the size of WebP.
~~~


Credit Where It's Due
=====================

The image format is built upon the work of several amazing software developers:

Stefano Brocchi
+ Image codec design inspiration
+ BCIF: http://www.dsi.unifi.it/DRIIA/RaccoltaTesi/Brocchi.pdf

Yann Collet
+ Fast LZ codec
+ LZ4HC: https://code.google.com/p/lz4/

Charles Bloom
+ JPEG-LS
+ Blog: http://cbloomrants.blogspot.com/2010/08/08-12-10-lost-huffman-paper.html

Rich Geldreich
+ Fast static Huffman codec
+ LZHAM: https://code.google.com/p/lzham/

Thomas Wang
+ Integer hash function
+ Closest URL: http://burtleburtle.net/bob/hash/integer.html

Google
+ Recursive subresolution compression
+ WebP: https://developers.google.com/speed/webp/docs/webp_lossless_bitstream_specification


Specification
=============

Starting from the BCIF spec we decided to make some improvements.

To optimize for low decompression time, we restricted ourselves to considering
only the fastest compression primitives: Filtering, LZ, and static Huffman.

BCIF does filtering and static Huffman coding, but left out LZ, which we feel
is a mistake since LZ is great for representing repeated data patterns, which
can be encoded byte-wise, reducing the number of Huffman symbols to decode.
In our tests, adding an LZ step improves compression ratio and decomp speed.

BCIF is also not designed for an alpha channel, and our games require alpha
for sprite-sheets.  Furthermore, our images tend to have a lot of
fully-transparent pixels that indicate 1-bit alpha is a good approach.
So we developed a monochrome compressor that outperforms PNG just for the alpha
channel.  Pixels that are represented by this monochrome bitmask can be skipped
during the rest of the RGBA decoding, so the overall performance is improved.

We also decided to rewrite BCIF from scratch after understanding how it worked,
since we found that the code could be significantly improved in decompression
speed, and we also disagreed with some of the finer points of the algorithm.


### Step 1. Dominant Color Pixel Encoding (Optional)

The dominant color is first detected.  It is usually black or full-transparent.

Dominant color pixels are combined into a monochrome raster and a filter is
applied to each pixel:

For the first row:

+ If the pixel to the left is "on", then we predict the pixel is "on."

For other rows:

+ If the pixel above it is "on", then we predict the pixel is "on."
+ If the two pixels to the left are "on", then we predict the pixel is "on."

Whenever the filter fails to predict properly, a 1 bit is written.

The distance between these 1 bits is encoded for each row.
We tried delta-encoding the distances in x and y but did not see improvement.

For each scanline: {number of distances recorded} {list of distances...}
This is encoded as a byte stream, which is then LZ compressed with LZ4HC.

Static Huffman entropy encoding is then performed for further compression.

Pixels that are in the bitmask are skipped over during encoding/decoding.

If this encoding does not achieve a certain minimum compression ratio then it
is not used.  A bit in the encoded file indicates whether or not it is used.


### Step 2. 2D LZ (Optional)

A custom 2D LZ77 algorithm is run to find repeated rectangular regions in the
image.  A rolling hash is used to do initial lookups on 3x3 regions, which is
the minimum size allowed for a match.  Hash collisions are checked, and then
matches are expanded.

If the resulting matches are accepted, they exclude further matches from
overlapping with them.  This approach gets RLE for free.  Each rectangular
region takes 10 bytes to represent, which is then compressed with Huffman
encoding and written to the file.  The resulting overhead is close to 5.5 bytes
per zone, with each zone covering at least 64 bytes of original image data.

If this encoding does not achieve a certain minimum compression ratio then it
is not used.  A bit in the encoded file indicates whether or not it is used.


### Step 3. Palette (Optional)

If 256 or fewer colors comprise the image, then it is attempted to be sent as a
paletted image, since this guarantees a compression ratio of at least 4:1.

The palette is sorted based on the luminance of each palette color, with the
hope that the image is somewhat smooth in brightness.  Spatial filtering is
done but color filtering is not (naturally); see the next section for more
information about filtering.  The filter zone size is increased to 16x16 for
paletted images.

If the palette has 16 or fewer entries, then the palette indices are repacked
into bytes: 

#### For 5-16 palette entries:

This is 3-4 bits/pixel, so 2 palette indices are packed into each byte in the
resulting monochrome image.

+ Combine pairs of pixels on the same scanline together.
+ Final odd pixel in each row is encoded in the low bits.

#### For 3-4 palette entries:

This is 2 bits/pixel, so 4 palette indices are packed into each byte in the
resulting monochrome image.

~~~
Combine blocks of 4 pixels together:

0 0 1 1 2 2 3
0 0 1 1 2 2 3 <- example 7x3 image
4 4 5 5 6 6 7

Each 2x2 block is packed like so:
0 1  -->  HI:[ 3 3 2 2 1 1 0 0 ]:LO
2 3
~~~

#### For 2 palette entries:

This is 1 bit/pixel, so 8 palette indices are packed into each byte in the
resulting monochrome image.

~~~
Combine blocks of 8 pixels together:

0 0 0 0 1 1 1 1 2 2 2
0 0 0 0 1 1 1 1 2 2 2 <- example 11x3 image
3 3 3 3 4 4 4 4 5 5 5

Each 4x2 block is packed like so:
0 1 2 3  -->  HI:[ 0 1 2 3 4 5 6 7 ]:LO
4 5 6 7
~~~

#### For 1 palette entry:

Only the palette color is sent, and the encoding aborts early.  The decoder can
recover the image by reading the image size, and the single color.


### Step 4. RGBA Compression

When the image data is not paletted, spatial and color filters are applied to
the input data in 4x4 pixel blocks as in BCIF.  The pair of filters that produce
the lowest entropy estimate are chosen for each block, using an efficient rough
integer approximation to entropy.

The entropy analysis is accelerated by a 24-bit fixed-point approximation that
allows us to try all of the options in an acceptable amount of time.  By being
fast we are able to try more options so compression improves.

After statistics are collected for the whole image, entropy analysis is re-run
on the first 4000-ish selections to choose better filters with knowledge about
the full image.  This further improves compression by tuning all of the filters
equally well across the whole image.

The filter selections are written out interleaved with the pixel data.  This is
done since sometimes filter data does not need to be sent due to the LZ or mask
steps, which make the filtering unnecessary for those pixels.  The decoder will
keep track of whether or not filter selection has been read for each 4x4 block
and will expect to read in the filter selection exactly when the first pixel in
a block is encountered.

Spatial filters are applied before color filters so that the image smoothness
does not get disturbed by the weird value-aliasing of the color filters.

The purpose of the color filter is to decorrelate each of the RGB channels so
that they can be treated as separate monochrome data streams.

The alpha channel is treated as separate monochrome data uncorrelated with the
RGB data and is prefiltered as (255 - A) so that 255 becomes 0, since 255 is
the most common alpha value and 0 is much easier to compress.

#### Color Filtering

The color filters are taken directly from this paper by Tilo Strutz
["ADAPTIVE SELECTION OF COLOUR TRANSFORMATIONS FOR REVERSIBLE IMAGE COMPRESSION" (2012)](http://www.eurasip.org/Proceedings/Eusipco/Eusipco2012/Conference/papers/1569551007.pdf)

YUV899 kills compression performance too much so we are using aliased -but
reversible- YUV888 transforms based on the ones from the paper where possible.

We also incorporated the color filters from BCIF, JPEG2000, and YCgCo-R.

These transforms apparently cover most of the ideal ways to decorrelate RGB
color data into separate streams:

~~~
CF_GB_RG,	// from BCIF
CF_GR_BG,	// from BCIF
CF_YUVr,	// YUVr from JPEG2000
CF_D9,		// from the Strutz paper
CF_D12,		// from the Strutz paper
CF_D8,		// from the Strutz paper
CF_E2_R,	// Derived from E2 and YCgCo-R
CF_BG_RG,	// from BCIF (recommendation from LOCO-I paper)
CF_GR_BR,	// from BCIF
CF_D18,		// from the Strutz paper
CF_B_GR_R,	// A decent default filter
CF_D11,		// from the Strutz paper
CF_D14,		// from the Strutz paper
CF_D10,		// from the Strutz paper
CF_YCgCo_R,	// Malvar's YCgCo-R
CF_GB_RB,	// from BCIF
CF_NONE,	// No modification
~~~

Pseudo-code implementations:

~~~
CFF_R2Y_GB_RG:
	Y = B;
	U = G - B;
	V = G - R;

CFF_R2Y_GR_BG:
	Y = G - B;
	U = G - R;
	V = R;

CFF_R2Y_YUVr:
	U = B - G;
	V = R - G;
	Y = G + (((char)U + (char)V) >> 2);

CFF_R2Y_D9:
	Y = R;
	U = B - ((R + G*3) >> 2);
	V = G - R;

CFF_R2Y_D12:
	Y = B;
	U = G - ((R*3 + B) >> 2);
	V = R - B;

CFF_R2Y_D8:
	Y = R;
	U = B - ((R + G) >> 1);
	V = G - R;

CFF_R2Y_E2_R:
	char Co = R - G;
	int t = G + (Co >> 1);
	char Cg = B - t;

	Y = t + (Cg >> 1);
	U = Cg;
	V = Co;

CFF_R2Y_BG_RG:
	Y = G - B;
	U = G;
	V = G - R;

CFF_R2Y_GR_BR:
	Y = B - R;
	U = G - R;
	V = R;

CFF_R2Y_D18:
	Y = B;
	U = R - ((G*3 + B) >> 2);
	V = G - B;

CFF_R2Y_B_GR_R:
	Y = B;
	U = G - R;
	V = R;

CFF_R2Y_D11:
	Y = B;
	U = G - ((R + B) >> 1);
	V = R - B;

CFF_R2Y_D14:
	Y = R;
	U = G - ((R + B) >> 1);
	V = B - R;

CFF_R2Y_D10:
	Y = B;
	U = G - ((R + B*3) >> 2);
	V = R - B;

CFF_R2Y_YCgCo_R:
	char Co = R - B;
	int t = B + (Co >> 1);
	char Cg = G - t;

	Y = t + (Cg >> 1);
	U = Cg;
	V = Co;

CFF_R2Y_GB_RB:
	Y = B;
	U = G - B;
	V = R - B;
~~~

These functions are all extremely fast to execute and typically excellent at decorrelation.

#### Spatial Filtering

Images are decoded from left to right and from top to bottom.  The spatial filters use
previously decoded image data to predict the next image pixel to decode.  The difference
between the prediction and the actual value is called the residual and is written out to
the file after entropy encoding (see below for more information).

~~~
Filter inputs:
        E
F C B D
  A ?    <-- pixel to predict
~~~

We use spatial filters from BCIF, supplemented with CBloom's and our own contributions:

~~~
	// Simple filters
	SF_A,				// A
	SF_B,				// B
	SF_C,				// C
	SF_D,				// D
	SF_Z,				// 0

	// Dual average filters (round down)
	SF_AVG_AB,			// (A + B) / 2
	SF_AVG_AC,			// (A + C) / 2
	SF_AVG_AD,			// (A + D) / 2
	SF_AVG_BC,			// (B + C) / 2
	SF_AVG_BD,			// (B + D) / 2
	SF_AVG_CD,			// (C + D) / 2

	// Dual average filters (round up)
	SF_AVG_AB1,			// (A + B + 1) / 2
	SF_AVG_AC1,			// (A + C + 1) / 2
	SF_AVG_AD1,			// (A + D + 1) / 2
	SF_AVG_BC1,			// (B + C + 1) / 2
	SF_AVG_BD1,			// (B + D + 1) / 2
	SF_AVG_CD1,			// (C + D + 1) / 2

	// Triple average filters (round down)
	SF_AVG_ABC,			// (A + B + C) / 3
	SF_AVG_ACD,			// (A + C + D) / 3
	SF_AVG_ABD,			// (A + B + D) / 3
	SF_AVG_BCD,			// (B + C + D) / 3

	// Quad average filters (round down)
	SF_AVG_ABCD,		// (A + B + C + D) / 4

	// Quad average filters (round up)
	SF_AVG_ABCD1,		// (A + B + C + D + 2) / 4

	// ABCD Complex filters
	SF_CLAMP_GRAD,		// ClampedGradPredictor (CBloom #12)
	SF_SKEW_GRAD,		// Gradient skewed towards average (CBloom #5)
	SF_ABC_CLAMP,		// A + B - C clamped to [0, 255] (BCIF)
	SF_PAETH,			// Paeth (PNG)
	SF_ABC_PAETH,		// If A <= C <= B, A + B - C, else Paeth filter (BCIF)
	SF_PLO,				// Offset PL (BCIF)
	SF_SELECT,			// Select (WebP)

	// EF Complex filters
	SF_SELECT_F,		// Pick A or C based on which is closer to F (New)
	SF_ED_GRAD,			// Predict gradient continues from E to D to current (New)
~~~

In addition to the static filters defined here (which are fast to evaluate),
there are a number of linear tapped filters based on A,B,C,D.  Usually a few
of these are preferable to the defaults.  And the encoder transmits which
ones are overwritten in the image file so the decoder stays in synch.

We found through testing that a small list of about 80 tapped filters are
ever preferable to one of the default filters, out of all 6544 combinations,
so only those are evaluated and sent.

See the [Filters.cpp](./decoder/Filters.cpp) file for the complete list.


### Step 5. Monochrome Compression

After all the data in the image has been reduced to a set of monochrome images,
including the subresolution spatial filter and color filter selections, each of
these monochrome images is submitted to a monochrome compressor that uses the
same spatial filters as the RGBA compressor.

The monochrome compressor works mechanically the same as the RGBA compressor
described above, except that it only has to worry about one channel of input
so there is no color filtering.

Unlike the RGBA compressor, its tile sizes can vary from 4x4 up to ~32x32 and the
tile size is selected to minimize the size of the output.

It produces as output, a subresolution tiled image describing the spatial
filters that best compress the given image data, and residuals to encode.
The subresolution tiled image is recursively compressed with the same monochrome
compressor until it is better to encode using simple row filters.

When tile-based compression is less beneficial than encoding the input directly,
simple row filters are employed.  These determine if the input can be compressed
better if a "same as left" predictor is run on the data.


### Note: Order-1 Chaos Modeling and Encoding

After colors are decorrelated with the color filter, the data is essentially
monochrome in each color plane.  And any other monochrome data that must be
compressed is modeled using the "chaos" metric from BCIF.  We extended the
idea quite a bit, allowing a variable number of chaos levels beyond 8.

The chaos metric is a rough approximation to order-1 statistics.  The metric
is defined as the sum of the highest set bit index in the left and up
post-filter values for each color plane.  Recall that after spatial and color
filtering, the image data is mostly eliminated and replaced with residuals
near zero.  Smaller values (and zeroes especially) lead to better compression,
so the "chaos" of a location in the image after filtering is exactly how large
the nearby values are.

Comparing this approach to order-1 statistics, that would be calculating the
statistics for seeing a value of "0" after seeing a value of "1", "2", and so
on.  The limitation of this approach is that it requires significantly more
overhead and working memory since we only admit static Huffman codes for speed.
To get some of the order-1 results, we can group statistics together.  The
probability of seeing "1", "2", etc after "0" is exactly what the chaos level 0
statistics are recording!  Exactly also for chaos level 1.

But for chaos level 2 and above it progressively lumps together more and more
of the order-1 statistics.  For level 2, above:2&left:0, above:1&left:1,
above:0&left:2, above:254&left:0, above:255&left:255, above:0&left:254.  And
from there it gets a lot more fuzzy.  Since most of the symbols are close to
zero, this approach is maximizing the usefulness of the order-1 statistics
without transmitting a ton of static tables.

Furthermore, the chaos metric cares about two dimensions, both the vertical and
horizontal chaos.  As a result it is well-suited for 2D images.

Since most of the image data is near zero, areas where high values occur tend
to be stored together in the statistical model, which means it can be more
tightly tuned for that data, further improving compression.

The number of chaos levels used in the encoding is chosen so that the data
encoding is as small as possible.


## Note: Order-1 Entropy Encoding with dual-Huffman zRLE

Throughout the codec, a generic post-filter entropy encoder is used in several
places.  Each chaos level uses a different instance of the entropy encoder.
The entropy encoder has two modes:

(1) Basic Huffman encoder

The basic Huffman encoder mode is what you may expect.  Statistics of up to 256
symbols are collected, and then symbols are encoded bitwise.  This mode is
chosen if the encoder decides it produces shorter output.  This feature saves
about 1 KB on average when it can be used and can only improve decoder speed.

(2) zRLE dual-Huffman encoder.

The zero-run-length-encoded dual-Huffman encoder has two Huffman encoders that
encode parts of the sequence.  One encoder is used for symbols leading up to
runs of zeroes, and the other encoder is used for symbols just after a zero run
finishes.  This is similar to how BCIF does entropy encoding, and is one of
its unspoken but surprisingly essential advantages over other image codecs.

The before-zero encoder typically has 128 extra symbols above 256 to represent
different run lengths of zeroes.  If runs are longer than 127, additional bytes
are emitted to represent the length of the zero run.  The after-zero encoder
has up to 256 normal symbols, since it cannot encode zeroes.  This acts as a
simple order-1 statistical model around zeroes and improves compression as a
result.


Example usage
=============

~~~
$ ./gcif
USAGE: ./gcif [options] [output file path]

Options:
  --[h]elp                             Print usage and exit.
  --[v]erbose                          Verbose console output
  -0                                   Compression level 0 : Faster
  -1                                   Compression level 1 : Better
  -2                                   Compression level 2 : Harder
  -3                                   Compression level 3 : Stronger (default)
  --[s]ilent                           No console output (even on errors)
  --[c]ompress <input PNG file path>   Compress the given .PNG image.
  --[d]ecompress <input GCI file path> Decompress the given .GCI image
  --[b]enchmark <test set path>        Test compression ratio and decompression
                                       speed for a whole directory at once
  --[p]rofile <input GCI file path>    Decode same GCI file 100x to enhance
                                       profiling of decoder

Examples:
  ./gcif -c ./original.png test.gci
  ./gcif -d ./test.gci decoded.png
 $ ll original.png
-rw-r--r--  1 cat  staff   1.3M Mar 26 00:25 original.png
 $ ./gcif -v -c original.png test.gci
[Apr 28 18:39] <main> Reading input PNG image file: original.png
[Apr 28 18:39] <main> Encoding image: test.gci
[Apr 28 18:39] <mask> Writing mask for color (0,0,0,0) ...
[Apr 28 18:39] <stats> (Mask Encoding)      Chosen Color : (0,0,0,0) ...
[Apr 28 18:39] <stats> (Mask Encoding)     Post-RLE Size : 24065 bytes
[Apr 28 18:39] <stats> (Mask Encoding)      Post-LZ Size : 21211 bytes
[Apr 28 18:39] <stats> (Mask Encoding) Post-Huffman Size : 17720 bytes (141758 bits)
[Apr 28 18:39] <stats> (Mask Encoding)        Table Size : 56 bytes (444 bits)
[Apr 28 18:39] <stats> (Mask Encoding)      Filtering : 117 usec (6.67047 %total)
[Apr 28 18:39] <stats> (Mask Encoding)            RLE : 301 usec (17.1608 %total)
[Apr 28 18:39] <stats> (Mask Encoding)             LZ : 1028 usec (58.6089 %total)
[Apr 28 18:39] <stats> (Mask Encoding)      Histogram : 56 usec (3.1927 %total)
[Apr 28 18:39] <stats> (Mask Encoding) Generate Table : 15 usec (0.855188 %total)
[Apr 28 18:39] <stats> (Mask Encoding)   Encode Table : 71 usec (4.04789 %total)
[Apr 28 18:39] <stats> (Mask Encoding)    Encode Data : 146 usec (8.32383 %total)
[Apr 28 18:39] <stats> (Mask Encoding)        Overall : 1754 usec
[Apr 28 18:39] <stats> (Mask Encoding) Throughput : 10.134 MBPS (output bytes)
[Apr 28 18:39] <stats> (Mask Encoding) Compression ratio : 108.144:1 (17775 bytes used overall)
[Apr 28 18:39] <stats> (Mask Encoding) Pixels covered : 480571 (45.8308 %total)
[Apr 28 18:39] <LZ> Searching for matches with 262144-entry hash table...
[Apr 28 18:39] <stats> (LZ Compress) Initial collisions : 802883
[Apr 28 18:39] <stats> (LZ Compress) Initial matches : 15794 used 2149
[Apr 28 18:39] <stats> (LZ Compress) Matched amount : 12.2043% of file is redundant (127971 of 1048576 pixels)
[Apr 28 18:39] <stats> (LZ Compress) Bytes saved : 511884 bytes
[Apr 28 18:39] <stats> (LZ Compress) Compressed overhead : 10585 bytes to transmit
[Apr 28 18:39] <stats> (LZ Compress) Compression ratio : 48.3594:1
[Apr 28 18:39] <CM> Designing filters...
[Apr 28 18:39] <CM> Replacing default filter 0 with tapped filter 37 that is 8.34773x more preferable : PRED = (0A + 1B + 1C + 0D) / 2
[Apr 28 18:39] <CM> Replacing default filter 10 with tapped filter 36 that is 3.34572x more preferable : PRED = (1A + 0B + 1C + 0D) / 2
[Apr 28 18:39] <CM> Replacing default filter 9 with tapped filter 31 that is 1.80526x more preferable : PRED = (2A + 1B + -1C + 0D) / 2
[Apr 28 18:39] <CM> Scoring filters using 272 entropy-based trials...
[Apr 28 18:40] <CM> Revisiting filter selections from the top... 4096 left
[Apr 28 18:40] <CM> Writing encoded pixel data...
[Apr 28 18:40] <stats> (CM Compress) Spatial Filter Table Size : 110 bits (13 bytes)
[Apr 28 18:40] <stats> (CM Compress) Spatial Filter Compressed Size : 133957 bits (16744 bytes)
[Apr 28 18:40] <stats> (CM Compress) Color Filter Table Size : 65 bits (8 bytes)
[Apr 28 18:40] <stats> (CM Compress) Color Filter Compressed Size : 84536 bits (10567 bytes)
[Apr 28 18:40] <stats> (CM Compress) Y-Channel Compressed Size : 2262503 bits (282812 bytes)
[Apr 28 18:40] <stats> (CM Compress) U-Channel Compressed Size : 2325432 bits (290679 bytes)
[Apr 28 18:40] <stats> (CM Compress) V-Channel Compressed Size : 1462902 bits (182862 bytes)
[Apr 28 18:40] <stats> (CM Compress) A-Channel Compressed Size : 699493 bits (87436 bytes)
[Apr 28 18:40] <stats> (CM Compress) YUVA Overhead Size : 42441 bits (5305 bytes)
[Apr 28 18:40] <stats> (CM Compress) Chaos pixel count : 483618 pixels
[Apr 28 18:40] <stats> (CM Compress) Chaos compression ratio : 2.20722:1
[Apr 28 18:40] <stats> (CM Compress) Overall size : 7238322 bits (904790 bytes)
[Apr 28 18:40] <stats> (CM Compress) Overall compression ratio : 4.63566:1
 $ ll test.gci
-rw-r--r--  1 cat  staff   885K Apr 28 18:40 test.gci 
$ ./gcif -v -d test.gci test.png
[Apr 28 18:40] <main> Decoding input GCIF image file: test.gci
[Apr 28 18:40] <stats> (Mask Decode)   Chosen Color : (0,0,0,0) ...
[Apr 28 18:40] <stats> (Mask Decode) Initialization : 1 usec (0.338983 %total)
[Apr 28 18:40] <stats> (Mask Decode)     Huffman+LZ : 294 usec (99.661 %total)
[Apr 28 18:40] <stats> (Mask Decode)        Overall : 295 usec
[Apr 28 18:40] <stats> (LZ Decode)     Initialization : 0 usec (0 %total)
[Apr 28 18:40] <stats> (LZ Decode) Read Huffman Table : 14 usec (5.28302 %total)
[Apr 28 18:40] <stats> (LZ Decode)         Read Zones : 251 usec (94.717 %total)
[Apr 28 18:40] <stats> (LZ Decode)            Overall : 265 usec
[Apr 28 18:40] <stats> (LZ Decode)         Zone Count : 2149 zones read
[Apr 28 18:40] <stats> (CM Decode)     Initialization : 14 usec (0.035115 %total)
[Apr 28 18:40] <stats> (CM Decode) Read Filter Tables : 3 usec (0.00752464 %total)
[Apr 28 18:40] <stats> (CM Decode)  Read Chaos Tables : 486 usec (1.21899 %total)
[Apr 28 18:40] <stats> (CM Decode)      Decode Pixels : 39366 usec (98.7384 %total)
[Apr 28 18:40] <stats> (CM Decode)            Overall : 39869 usec
[Apr 28 18:40] <stats> (CM Decode)         Throughput : 105.202 MBPS (output bytes/time)
[Apr 28 18:40] <main> Writing output PNG image file: test.png
~~~


Future plans
============

Immediately:

+ Benchmarking

+ Whitepaper

Slated for inclusion in version 1.1 of the file format:

+ Use strong file hash in new verification mode for command-line tool.

+ Support scanline spatial and color filters.
-- Define one filter pair for an entire row.
-- Compare the result of doing this with tightly tuned filters; send the best.

+ Support LZ at the scanline level.
-- Make post-filter LZ a part of the scanline filter mode.
-- This is exciting because right now LZ is useless with the post-filter data.
-- WILL be better for some computer-generated images I am looking at.
-- WILL make us better than PNG for ALL images rather than just the majority.

+ A new spritesheet generator that uses GCIF as an in/output file format.
-- Even better image compression by eliminating a lot of image data.
-- There is a lot of room for improvement in our current spriter.
-- Incorporate it into the GCIF codebase to make it a one-stop shop for games.

+ Java version of the encoder.

+ Support for images as large as 65536x65536 with custom memory allocator and
a container format that breaks the image into smaller chunks that are each GCIF
compressed.

