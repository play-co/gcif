GCIF
====

Game Closure Image Format

This is a Work-In-Progress towards a new RGBA image format that is suited for
mobile game spritehseets.

The format is released under the BSD license as forever free and open-source
software.  Contributions, discussions, and a healthy dose of criticism are all
welcome.


What works right now
====================

The codec supports full RGBA.  The compressor and decompressor are close to
being called version 1.0.  Only a few images cause crashes or other issues.

Early test results indicate that GCIF files are ~60% the size of PNG sprites,
and the decompression speed is comparable or better than libpng.

The compression rate for this speed reaches for the Pareto frontier for
lossless image compression without using any multithreading.

The code is well-written in our opinion, easy to read and adapt, and easy to
incorporate into mobile development.

We plan to release a Java version for the encoder after the RGBA compression is
functional in C++ code, so that the encoder can be run on any platform without
having to compile it.  The decoder will be split off so that only a minimal
amount of code needs to be added to support this file format.  We're shooting
for one large C++ file, though it may end up being a small number of files.


Compression performance
=======================

It is too early to release real benchmarks here, but getting closer.

BCIF out-performs lossless WebP for a lot of images, so we chose it as the
baseline comparison for RGB images.

From one of our more challenging game sprite-sheets chosen at random:

~~~
-rw-r--r--  1 cat  staff   3.0M Mar 31 20:40 noalpha.bmp
-rw-r--r--  1 cat  staff   1.1M Mar 31 20:40 noalpha.png
-rw-r--r--  1 cat  staff   877K Apr  2 14:05 noalpha.bcif
-rw-r--r--  1 cat  staff   814K Apr 24 17:05 noalpha.gci
~~~

In this case we get a file that is 75% the size of the equivalent PNG image.


Credit Where It's Due
=====================

The image format is built upon the work of several amazing software developers:

Stefano Brocchi
+ Image codec design inspiration
+ BCIF: http://www.dsi.unifi.it/DRIIA/RaccoltaTesi/Brocchi.pdf

Austin Appleby
+ Fast validation hash
+ MurmurHash3: https://code.google.com/p/smhasher/wiki/MurmurHash3

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


### Step 0. Fully-Transparent Pixel Encoding

Fully-transparent pixels are combined into a monochrome raster and a filter is
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

Pixels that are fully-transparent are skipped over during encoding/decoding.


### Step 1. 2D LZ

A custom 2D LZ77 algorithm is run to find repeated rectangular regions in the
image.  A rolling hash is used to do initial lookups on 3x3 regions, which is
the minimum size allowed for a match.  Hash collisions are checked, and then
matches are expanded.

If the resulting matches are accepted, they exclude further matches from
overlapping with them.  This approach gets RLE for free.  Each rectangular
region takes 10 bytes to represent, which is then compressed with Huffman
encoding and written to the file.  The resulting overhead is close to 5.5 bytes
per zone, with each zone covering at least 64 bytes of original image data.


### Step 2. Filtering

Spatial and color filters are applied to the input data in 4x4 pixel blocks as
in BCIF.  The pair of filters that best estimate each block are chosen, as
measured by the L1 norm of the resulting pixel color component values, with 0
and 255 being the best result, and numbers around 128 being the worst.

The two filters are spatial and color.  The spatial filters are:

~~~
 Filter inputs:
         E
 F C B D
   A ?

	// In the order they are applied in the case of a tie:
	SF_Z,			// 0
	SF_D,			// D
	SF_C,			// C
	SF_B,			// B
	SF_A,			// A
	SF_AB,			// (A + B)/2
	SF_BD,			// (B + D)/2
	SF_CLAMP_GRAD,	// CBloom: 12: ClampedGradPredictor
	SF_SKEW_GRAD,	// CBloom: 5: Gradient skewed towards average
	SF_PICK_LEFT,	// New: Pick A or C based on which is closer to F
	SF_PRED_UR,		// New: Predict gradient continues from E to D to current
	SF_ABC_CLAMP,	// A + B - C clamped to [0, 255]
	SF_PAETH,		// Paeth filter
	SF_ABC_PAETH,	// If A <= C <= B, A + B - C, else Paeth filter
	SF_PLO,			// Offset PL
	SF_ABCD,		// (A + B + C + D + 1)/4
	SF_AD,			// (A + D)/2
~~~

In addition to these default filters, 80 linear filters involving A,B,C, and D
are selected to be used based on the input image, and will replace filters in
the above table where they are preferred.

And the color filters are:

~~~
	// In order of preference (based on entropy scores from test images):
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
~~~

Spatial filters are applied before color filters so that the image smoothness
does not get disturbed by the weird value-aliasing of the color filters.

The encoder exhaustively tries all of these SF+CF combinations, and the best
are then subjected to further entropy analysis.  This additional step greatly
improves compression by increasing the rate at which symbols are reused in the
post-filtered data, which makes the data easier to compress.

The entropy analysis is accelerated by a 24-bit fixed-point approximation that
allows us to try all of the options in an acceptable amount of time.  By being
fast we are able to try more options so compression improves.

After statistics are collected for the whole image, entropy analysis is re-run
on the first 4000-ish selections to choose better filters with knowledge about
the full image.  This further improves compression by tuning all of the filters
equally well across the whole image.


### Step 3. Order-1 Chaos Modeling and Encoding

For each color plane, the BCIF "chaos" metric is used to sort each remaining
filtered pixel into one of 8 bins.  The chaos metric is a rough approximation
to order-1 statistics.  The metric is defined as the sum of the highest set bit
index in the left and up post-filter values for each color plane.  Recall that
after spatial and color filtering, the image data is mostly eliminated and
replaced with a few values near zero.  Smaller values (and zeroes especially)
lead to better compression, so the "chaos" of a location in the image after
filtering is exactly how large the nearby values are.

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

When the number of pixels to be encoded falls below a certain threshold, all 8
chaos bins require too much overhead to make them worthwhile, so the compressor
switches off the chaos metric to cut the overhead down to 1/8th its size.

Since there are 8 chaos bins, that means 8 Huffman tables are transmitted for
each of the RGBA color planes.  Therefore, it was essential to develop a good
compression algorithm for the Huffman tables themselves.  So, the tables are
filtered and compressed using the same entropy encoding used on the image data.
This table compression is exceptionally good.  It compresses about 8KB of table
data down into about 3KB using several tricks including truncation.


Future plans
============

We have tried a lot of crazy things to improve compression, and only a few of
them stuck to the project.  Here are some more wild ideas:

+ Support a full palette mode for small or GIF-quantized images.
-- First determine the palette.
-- Sort the palette so that the image data attains maximum smoothness.
-- Apply all of the normal spatial filtering without color filters.
-- Encode it as one channel instead of RGBA.

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

+ Benchmarks in the README!


Example usage
=============

~~~
$ ./gcif --help
USAGE: ./gcif [options] [output file path]

Options:
  --[h]elp                             Print usage and exit.
  --[v]erbose                          Verbose console output
  -0                                   Compression level 0 : Faster
  -1                                   Compression level 1 : Better
  -2                                   Compression level 2 : Harder (default)
  --[s]ilent                           No console output (even on errors)
  --[c]ompress <input PNG file path>   Compress the given .PNG image.
  --[d]ecompress <input GCI file path> Decompress the given .GCI image
  --[t]est <input PNG file path>       Test compression to verify it is lossless
  --[b]enchmark <test set path>        Test compression ratio and decompression
                                       speed for a whole directory at once
  --[p]rofile <input GCI file path>    Decode same GCI file 100x to enhance
                                       profiling of decoder

Examples:
  ./gcif -tv ./original.png
  ./gcif -c ./original.png test.gci
  ./gcif -d ./test.gci decoded.png

 $ ll original.png
-rw-r--r--  1 cat  staff   1.3M Mar 26 00:25 original.png
 $ 
 $ ./gcif -2 -v -c original.png test.gci
[Dec 31 16:00] <main> Reading input PNG image file: original.png
[Dec 31 16:00] <main> Encoding image: test.gci
[Dec 31 16:00] <mask> Writing mask...
[Dec 31 16:00] <stats> (Mask Encoding)     Post-RLE Size : 24065 bytes
[Dec 31 16:00] <stats> (Mask Encoding)      Post-LZ Size : 21211 bytes
[Dec 31 16:00] <stats> (Mask Encoding) Post-Huffman Size : 17720 bytes (141758 bits)
[Dec 31 16:00] <stats> (Mask Encoding)        Table Size : 72 bytes (576 bits)
[Dec 31 16:00] <stats> (Mask Encoding)      Filtering : 136 usec (6.55738 %total)
[Dec 31 16:00] <stats> (Mask Encoding)            RLE : 757 usec (36.4995 %total)
[Dec 31 16:00] <stats> (Mask Encoding)             LZ : 977 usec (47.107 %total)
[Dec 31 16:00] <stats> (Mask Encoding)      Histogram : 26 usec (1.25362 %total)
[Dec 31 16:00] <stats> (Mask Encoding) Generate Table : 15 usec (0.72324 %total)
[Dec 31 16:00] <stats> (Mask Encoding)   Encode Table : 38 usec (1.83221 %total)
[Dec 31 16:00] <stats> (Mask Encoding)    Encode Data : 125 usec (6.027 %total)
[Dec 31 16:00] <stats> (Mask Encoding)        Overall : 2074 usec
[Dec 31 16:00] <stats> (Mask Encoding) Throughput : 8.57811 MBPS (output bytes)
[Dec 31 16:00] <stats> (Mask Encoding) Pixels covered : 480571 (45.8308 %total)
[Dec 31 16:00] <stats> (Mask Encoding) Compression ratio : 108.048:1 (17791 bytes used overall)
[Dec 31 16:00] <LZ> Searching for matches with 262144-entry hash table...
[Dec 31 16:00] <stats> (LZ Compress) Initial collisions : 802883
[Dec 31 16:00] <stats> (LZ Compress) Initial matches : 15794 used 2149
[Dec 31 16:00] <stats> (LZ Compress) Matched amount : 12.2043% of file is redundant (127971 of 1048576 pixels)
[Dec 31 16:00] <stats> (LZ Compress) Bytes saved : 511884 bytes
[Dec 31 16:00] <stats> (LZ Compress) Compressed overhead : 10600 bytes to transmit
[Dec 31 16:00] <stats> (LZ Compress) Compression ratio : 48.2909:1
[Dec 31 16:00] <CM> Designing filters...
[Dec 31 16:00] <CM> Replacing default filter 10 with tapped filter 37 that is 3.3709x more preferable : PRED = (0A + 1B + 1C + 0D) / 2
[Dec 31 16:00] <CM> Replacing default filter 9 with tapped filter 36 that is 2.67119x more preferable : PRED = (1A + 0B + 1C + 0D) / 2
[Dec 31 16:00] <CM> Replacing default filter 15 with tapped filter 31 that is 1.61122x more preferable : PRED = (2A + 1B + -1C + 0D) / 2
[Dec 31 16:00] <CM> Scoring filters using entropy metric 256... (may take a while)
[Dec 31 16:00] <CM> Writing encoded pixel data...
[Dec 31 16:00] <stats> (CM Compress) Spatial Filter Table Size : 110 bits (13 bytes)
[Dec 31 16:00] <stats> (CM Compress) Spatial Filter Compressed Size : 139365 bits (17420 bytes)
[Dec 31 16:00] <stats> (CM Compress) Color Filter Table Size : 65 bits (8 bytes)
[Dec 31 16:00] <stats> (CM Compress) Color Filter Compressed Size : 85527 bits (10690 bytes)
[Dec 31 16:00] <stats> (CM Compress) Y-Channel Compressed Size : 2260947 bits (282618 bytes)
[Dec 31 16:00] <stats> (CM Compress) U-Channel Compressed Size : 2333080 bits (291635 bytes)
[Dec 31 16:00] <stats> (CM Compress) V-Channel Compressed Size : 1459401 bits (182425 bytes)
[Dec 31 16:00] <stats> (CM Compress) A-Channel Compressed Size : 705917 bits (88239 bytes)
[Dec 31 16:00] <stats> (CM Compress) YUVA Overhead Size : 46554 bits (5819 bytes)
[Dec 31 16:00] <stats> (CM Compress) Chaos pixel count : 483618 pixels
[Dec 31 16:00] <stats> (CM Compress) Chaos compression ratio : 2.20109:1
[Dec 31 16:00] <stats> (CM Compress) Overall size : 7258104 bits (907263 bytes)
[Dec 31 16:00] <stats> (CM Compress) Overall compression ratio : 4.62303:1
[Dec 31 16:00] <main> Success.
 $ ./gcif -v -d test.gci test.png
[Dec 31 16:00] <main> Decoding input GCIF image file: test.gci
[Dec 31 16:00] <stats> (Mask Decode) Initialization : 5 usec (0.806452 %total)
[Dec 31 16:00] <stats> (Mask Decode)     Huffman+LZ : 330 usec (53.2258 %total)
[Dec 31 16:00] <stats> (Mask Decode)     RLE+Filter : 285 usec (45.9677 %total)
[Dec 31 16:00] <stats> (Mask Decode)        Overall : 620 usec
[Dec 31 16:00] <stats> (LZ Decode)     Initialization : 0 usec (0 %total)
[Dec 31 16:00] <stats> (LZ Decode) Read Huffman Table : 16 usec (5.51724 %total)
[Dec 31 16:00] <stats> (LZ Decode)         Read Zones : 274 usec (94.4828 %total)
[Dec 31 16:00] <stats> (LZ Decode)            Overall : 290 usec
[Dec 31 16:00] <stats> (LZ Decode)         Zone Count : 2149 zones read
[Dec 31 16:00] <stats> (CM Decode)     Initialization : 3 usec (0.00660531 %total)
[Dec 31 16:00] <stats> (CM Decode) Read Filter Tables : 4 usec (0.00880708 %total)
[Dec 31 16:00] <stats> (CM Decode)  Read Chaos Tables : 462 usec (1.01722 %total)
[Dec 31 16:00] <stats> (CM Decode)      Decode Pixels : 44949 usec (98.9674 %total)
[Dec 31 16:00] <stats> (CM Decode)            Overall : 45418 usec
[Dec 31 16:00] <stats> (CM Decode)         Throughput : 92.3489 MBPS (output bytes/time)
[Dec 31 16:00] <main> Writing output PNG image file: test.png
[Dec 31 16:00] <main> Success.
 $ ll test.gci
-rw-r--r--  1 cat  staff   886K Apr 24 15:06 test.gci
~~~


Stay tuned! =) -cat

