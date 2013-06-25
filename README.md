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


### Note: Palette sorting

In various places in the image compression, a palette of values is used to
represent a large 2D image.  For instance, the spatial filter selections for
each tile of the image, or the colors corresponding to each color in a palette
mode image.

Choosing the palette index for each of the <= 256 colors is essential for
producing good compression results using a PNG-like filter-based approach.

Palette index assignment does not affect LZ or mask results, nor any
direct improvement in entropy encoding.

However, when neighboring pixels have similar values, the filters are more
effective at predicting them, which increases the number of post-filter zero
pixels and reduces overall entropy.

A simple approximation to good choices is to just sort by luminance, so the
brighest pixels get the highest palette index.  However you can do better,
and luminance cannot be measured for filter matrices.

Since this is designed to improve filter effectiveness, the criterion for a
good palette selection is based on how close each pixel index is to its up,
up-left, left, and up-right neighbor pixel indices.  If you also include the
reverse relation, all 8 pixels around the center pixel should be scored.

The algorithm is:

+ (1) Assign each palette index by popularity, most popular gets index 0.
+ (2) From palette index 1:
+ *** (1) Score each color by how often palette index 0 appears in filter zone.
+ *** (2) Add in how often the color appears in index 0's filter zone.
+ *** (3) Choose the one that scores highest to be index 1.
+ (3) For palette index 2+, score by filter zone closeness for index 0 and 1.
+ (4) After index 8, it cares about closeness to the last 8 indices only.

The closeness to the last index is more important than earlier indices, so
those are scored higher.  Also, left/right neighbors are scored twice as
high as other neighbors, matching the natural horizontal correlation of
most images.


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
  --[t]est <input PNG file path>       Test compression to verify it is lossless
  --[b]enchmark <test set path>        Test compression ratio and decompression
                                       speed for a whole directory at once
  --[p]rofile <input GCI file path>    Decode same GCI file 100x to enhance
                                       profiling of decoder
  --[r]eplace <directory path>         Compress all images in the given
                                       directory, replacing the original if the
                                       GCIF version is smaller without changing
                                       file name

Examples:
  ./gcif -c ./original.png test.gci
  ./gcif -d ./test.gci decoded.png
~~~

~~~
 $ ./gcif -v -t natural.png
[Jun 24 21:26] <mask> Writing mask for 4-plane color (0,0,0,0) ...
[Jun 24 21:26] <stats> (Mask Encoding)      Chosen Color : (0,0,0,0) ...
[Jun 24 21:26] <stats> (Mask Encoding)     Post-RLE Size : 1048 bytes
[Jun 24 21:26] <stats> (Mask Encoding)      Post-LZ Size : 32 bytes
[Jun 24 21:26] <stats> (Mask Encoding) Post-Huffman Size : 32 bytes (256 bits)
[Jun 24 21:26] <stats> (Mask Encoding)        Table Size : 8 bytes (61 bits)
[Jun 24 21:26] <stats> (Mask Encoding)      Filtering : 126 usec (30.5085 %total)
[Jun 24 21:26] <stats> (Mask Encoding)            RLE : 64 usec (15.4964 %total)
[Jun 24 21:26] <stats> (Mask Encoding)             LZ : 135 usec (32.6877 %total)
[Jun 24 21:26] <stats> (Mask Encoding)      Histogram : 0 usec (0 %total)
[Jun 24 21:26] <stats> (Mask Encoding) Generate Table : 0 usec (0 %total)
[Jun 24 21:26] <stats> (Mask Encoding)   Encode Table : 88 usec (21.3075 %total)
[Jun 24 21:26] <stats> (Mask Encoding)    Encode Data : 0 usec (0 %total)
[Jun 24 21:26] <stats> (Mask Encoding)        Overall : 413 usec
[Jun 24 21:26] <stats> (Mask Encoding) Throughput : 0.094431 MBPS (output bytes)
[Jun 24 21:26] <stats> (Mask Encoding) Compression ratio : 19144.7:1 (39 bytes used overall)
[Jun 24 21:26] <stats> (Mask Encoding) Pixels covered : 189652 (18.0866 %total)
[Jun 24 21:26] <LZ> Searching for matches with 524288-entry hash table...
[Jun 24 21:26] <stats> (LZ Compress) Initial collisions : 592298
[Jun 24 21:26] <stats> (LZ Compress) Initial matches : 75651 used 1295
[Jun 24 21:26] <stats> (LZ Compress) Matched amount : 20.1441% of file is redundant (211226 of 1048576 pixels)
[Jun 24 21:26] <stats> (LZ Compress) Bytes saved : 844904 bytes
[Jun 24 21:26] <stats> (LZ Compress) Compression ratio : 123.524:1 (6840 bytes to transmit)
[Jun 24 21:26] <stats> (Palette) Disabled.
[Jun 24 21:26] <RGBA> Designing spatial filters...
[Jun 24 21:26] <RGBA> Designing SF/CF tiles for 256x256...
[Jun 24 21:26] <RGBA> Revisiting filter selections from the top... 4096 left
[Jun 24 21:26] <RGBA> Sorting spatial filters...
[Jun 24 21:26] <RGBA> Executing tiles to generate residual matrix...
[Jun 24 21:26] <RGBA> Compressing alpha channel...
[Jun 24 21:26] <RGBA> Designing chaos...
[Jun 24 21:26] <RGBA> Compressing spatial filter matrix...
[Jun 24 21:26] <RGBA> Compressing color filter matrix...
[Jun 24 21:26] <RGBA> Writing tables...
[Jun 24 21:26] <RGBA> Writing interleaved pixel/filter data...
[Jun 24 21:26] <stats> (RGBA Compress) Alpha channel encoder:
[Jun 24 21:26] <Mono> Using row-filtered encoder for 1024x1024 image
[Jun 24 21:26] <Mono>  -   Basic Overhead : 1 bits (0 bytes)
[Jun 24 21:26] <Mono>  - Encoder Overhead : 0 bits (0 bytes)
[Jun 24 21:26] <Mono>  -  Filter Overhead : 13 bits (1 bytes)
[Jun 24 21:26] <Mono>  -  Monochrome Data : 0 bits (0 bytes)
[Jun 24 21:26] <stats> (RGBA Compress) Spatial filter encoder:
[Jun 24 21:26] <Mono> Designed monochrome writer using 32x32 tiles to express 13 (0 palette) filters for 256x256 image with 6 chaos bins
[Jun 24 21:26] <Mono>  -   Basic Overhead : 103 bits (12 bytes)
[Jun 24 21:26] <Mono>  - Encoder Overhead : 533 bits (66 bytes)
[Jun 24 21:26] <Mono>  -  Filter Overhead : 1695 bits (211 bytes)
[Jun 24 21:26] <Mono>  -  Monochrome Data : 176903 bits (22112 bytes)
[Jun 24 21:26] <Mono>  - Recursively using filter encoder:
[Jun 24 21:26] <Mono> Using row-filtered encoder for 32x32 image
[Jun 24 21:26] <Mono>  -   Basic Overhead : 1 bits (0 bytes)
[Jun 24 21:26] <Mono>  - Encoder Overhead : 0 bits (0 bytes)
[Jun 24 21:26] <Mono>  -  Filter Overhead : 207 bits (25 bytes)
[Jun 24 21:26] <Mono>  -  Monochrome Data : 1487 bits (185 bytes)
[Jun 24 21:26] <stats> (RGBA Compress) Color filter encoder:
[Jun 24 21:26] <Mono> Designed monochrome writer using 64x64 tiles to express 21 (0 palette) filters for 256x256 image with 2 chaos bins
[Jun 24 21:26] <Mono>  -   Basic Overhead : 159 bits (19 bytes)
[Jun 24 21:26] <Mono>  - Encoder Overhead : 474 bits (59 bytes)
[Jun 24 21:26] <Mono>  -  Filter Overhead : 9215 bits (1151 bytes)
[Jun 24 21:26] <Mono>  -  Monochrome Data : 132820 bits (16602 bytes)
[Jun 24 21:26] <Mono>  - Recursively using filter encoder:
[Jun 24 21:26] <Mono> Using row-filtered encoder for 64x64 image
[Jun 24 21:26] <Mono>  -   Basic Overhead : 1 bits (0 bytes)
[Jun 24 21:26] <Mono>  - Encoder Overhead : 0 bits (0 bytes)
[Jun 24 21:26] <Mono>  -  Filter Overhead : 263 bits (32 bytes)
[Jun 24 21:26] <Mono>  -  Monochrome Data : 8951 bits (1118 bytes)
[Jun 24 21:26] <stats> (RGBA Compress)     Basic Overhead : 7 bits (0 bytes, 0.000140621% of RGBA) with 8 chaos bins
[Jun 24 21:26] <stats> (RGBA Compress) SF Choice Overhead : 180 bits (22 bytes, 0.00361596% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)  SF Table Overhead : 844 bits (105 bytes, 0.0169548% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)  CF Table Overhead : 833 bits (104 bytes, 0.0167339% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)   Y Table Overhead : 6152 bits (769 bytes, 0.123586% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)   U Table Overhead : 6531 bits (816 bytes, 0.131199% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)   V Table Overhead : 6670 bits (833 bytes, 0.133991% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)   A Table Overhead : 14 bits (1 bytes, 0.000281242% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)      SF Compressed : 178390 bits (22298 bytes, 3.58362% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)      CF Compressed : 141835 bits (17729 bytes, 2.84928% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)       Y Compressed : 1315554 bits (164444 bytes, 26.4277% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)       U Compressed : 1600450 bits (200056 bytes, 32.1509% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)       V Compressed : 1721312 bits (215164 bytes, 34.5789% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)       A Compressed : 0 bits (0 bytes, 0% of RGBA)
[Jun 24 21:26] <stats> (RGBA Compress)  Overall RGBA Data : 4977928 bits (622241 bytes, 98.9063% of total)
[Jun 24 21:26] <stats> (RGBA Compress)   RGBA write count : 648655 pixels for 1024x1024 pixel image (61.8606 % of total)
[Jun 24 21:26] <stats> (RGBA Compress)    RGBA Compression Ratio : 4.1698:1 compression ratio
[Jun 24 21:26] <stats> (RGBA Compress)              Overall Size : 5032972 bits (629121 bytes)
[Jun 24 21:26] <stats> (RGBA Compress) Overall Compression Ratio : 6.66692:1
[Jun 24 21:26] <stats> (Mask Decode)   Chosen Color : (0,0,0,0) ...
[Jun 24 21:26] <stats> (Mask Decode) Initialization : 2 usec (28.5714 %total)
[Jun 24 21:26] <stats> (Mask Decode)     Huffman+LZ : 5 usec (71.4286 %total)
[Jun 24 21:26] <stats> (Mask Decode)        Overall : 7 usec
[Jun 24 21:26] <stats> (LZ Decode) Read Huffman Table : 17 usec (6.71937 %total)
[Jun 24 21:26] <stats> (LZ Decode)         Read Zones : 236 usec (93.2806 %total)
[Jun 24 21:26] <stats> (LZ Decode)            Overall : 253 usec
[Jun 24 21:26] <stats> (LZ Decode)         Zone Count : 1295 zones read
[Jun 24 21:26] <stats> (Palette Decode)    Disabled.
[Jun 24 21:26] <stats> (RGBA Decode) Read Filter Tables : 72 usec (0.0978726 %total)
[Jun 24 21:26] <stats> (RGBA Decode)   Read RGBA Tables : 240 usec (0.326242 %total)
[Jun 24 21:26] <stats> (RGBA Decode)      Decode Pixels : 73253 usec (99.5759 %total)
[Jun 24 21:26] <stats> (RGBA Decode)            Overall : 73565 usec
[Jun 24 21:26] <stats> (RGBA Decode)         Throughput : 57.0149 MBPS (output bytes/time)
[Jun 24 21:26] <stats> (RGBA Decode)   Image Dimensions : 1024 x 1024 pixels
[Jun 24 21:26] <main> natural.png => 2.87318x smaller than PNG and decompresses 1.53324x faster
~~~


Road Map
========

For version 1.1 tagged release:

+ Optimization

+ Generate Static Library for Distribution

+ Integrate with DevKit

After 1.1 release:

+ Benchmarking

+ Whitepaper

Slated for inclusion in version 1.2 of the file format:

+ Use strong file hash in new verification mode for command-line tool.

+ A new spritesheet generator that uses GCIF as an in/output file format.
-- Even better image compression by eliminating a lot of image data.
-- There is a lot of room for improvement in our current spriter.
-- Incorporate it into the GCIF codebase to make it a one-stop shop for games.

+ Java version of the encoder.
