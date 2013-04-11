GCIF
====

Game Closure Image Format

This is a Work-In-Progress towards a new RGBA image format that works well for
our spritehseets.

It is expected to produce files about half the size of the equivalent PNG
formatted images.  And it is expected to decompress three times faster than the
equivalent PNG formatted image (with PNGCRUSH) using libpng.


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

To optimize for low decompression time, we restricted ourself to considering
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

Static Huffman encoding is then performed to eliminate a lot of repetition.

The Huffman table is encoded using Golomb codes and written out, and then
the Huffman symbols for each LZ output byte are written out.

Pixels that are fully-transparent are skipped over during encoding/decoding.


### Step 1. 2D LZ

A custom 2D LZ77 algorithm is run to find repeated rectangular regions in the
image.  A rolling hash is used to do initial lookups on 4x4 regions, which is
the minimum size allowed for a match.  Hash collisions are checked, and then
matches are expanded.

If the resulting matches are accepted, they exclude further matches from
overlapping with them.  This approach gets RLE for free.  Each rectangular
region takes 10 bytes to represent, which is then compressed with Huffman
encoding and written to the file.


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
	SF_ABC_CLAMP,	// A + B - C clamped to [0, 255]
	SF_PAETH,		// Paeth filter
	SF_ABC_PAETH,	// If A <= C <= B, A + B - C, else Paeth filter
	SF_PL,			// Use ABC to determine if increasing or decreasing
	SF_PLO,			// Offset PL
	SF_ABCD,		// (A + B + C + D + 1)/4

	SF_PICK_LEFT,	// Pick A or C based on which is closer to F
	SF_PRED_UR,		// Predict gradient continues from E to D to current

	SF_AD,			// (A + D)/2
~~~

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

This generates encoded pixels and a filter description.  The filter description
is compressed with Huffman encoding and written to the file.


### Step 3. Chaos Context Encoding

For each RGB plane, the BCIF "chaos" metric is used to sort each remaining
filtered pixel into one of 8 bins.  Symbol statistics are collected for each
bin and each RGB plane.


### Step 4. Interleaved Encoding

Then each pixel is iterated over and alpha-masked or LZ-masked pixels are
omitted.  The pixels are Huffman encoded based on color plane and chaos metric.


What works right now
====================

The latest version has what may be a fully working compressor, which gives you
a good idea about the compression ratios achieved by the codec.

The grayscale alpha channel is currently not compressed and needs to be done.

The decompressor is a work in progress and the final version may compress a
little better or worse than this one.

Currently, 1-bit alpha channel compression works, which is a major advance we
are pushing over BCIF for the final version of the codec.  You can try it out
with the test images and view statistics during compression and decompression
at the console.

Here's an example:

~~~
 $ ./gcif
USAGE: ./gcif [options] [output file path]

Options:
  --[h]elp                             Print usage and exit.
  --[v]erbose                          Verbose console output
  --[s]ilent                           No console output (even on errors)
  --[c]ompress <input PNG file path>   Compress the given .PNG image.
  --[d]ecompress <input GCI file path> Decompress the given .GCI image
  --[t]est <input PNG file path>       Test compression to verify it is lossless

Examples:
  ./gcif -tv ./original.png
  ./gcif -c ./original.png test.gci
  ./gcif -d ./test.gci decoded.png
~~~

Compression:

~~~
 $ ./gcif -c original.png test.gci
[Dec 31 16:00] <png> Inflate took 45773 usec
[Dec 31 16:00] <png> Processed input at 29.38 MB/S
[Dec 31 16:00] <png> Generated at 91.6551 MB/S
[Dec 31 16:00] <stats> (Mask Encoding)     Post-RLE Size : 24949 bytes
[Dec 31 16:00] <stats> (Mask Encoding)      Post-LZ Size : 21953 bytes
[Dec 31 16:00] <stats> (Mask Encoding) Post-Huffman Size : 18041 bytes (144325 bits)
[Dec 31 16:00] <stats> (Mask Encoding)        Table Size : 66 bytes (521 bits) [Golomb pivot = 0 bits]
[Dec 31 16:00] <stats> (Mask Encoding)      Filtering : 96 usec (4.64891 %total)
[Dec 31 16:00] <stats> (Mask Encoding)            RLE : 345 usec (16.707 %total)
[Dec 31 16:00] <stats> (Mask Encoding)             LZ : 1475 usec (71.4286 %total)
[Dec 31 16:00] <stats> (Mask Encoding)      Histogram : 20 usec (0.968523 %total)
[Dec 31 16:00] <stats> (Mask Encoding) Generate Table : 9 usec (0.435835 %total)
[Dec 31 16:00] <stats> (Mask Encoding)   Encode Table : 6 usec (0.290557 %total)
[Dec 31 16:00] <stats> (Mask Encoding)    Encode Data : 114 usec (5.52058 %total)
[Dec 31 16:00] <stats> (Mask Encoding)        Overall : 2065 usec
[Dec 31 16:00] <stats> (Mask Encoding) Throughput : 63.4731 MBPS (input bytes)
[Dec 31 16:00] <stats> (Mask Encoding) Throughput : 8.76804 MBPS (output bytes)
[Dec 31 16:00] <stats> (Mask Encoding) Ratio : 13.8138% (18106 bytes) of original data set (131072 bytes)
[Dec 31 16:00] <main> Wrote test.gci
~~~

Decompression:

~~~
 $ ./gcif -d test.gci output.png
[Dec 31 16:00] <stats> (Mask Decoding) Table Pivot : 0
[Dec 31 16:00] <stats> (Mask Decoding) Initialization : 7 usec (1.4 %total)
[Dec 31 16:00] <stats> (Mask Decoding)  Read Codelens : 4 usec (0.8 %total)
[Dec 31 16:00] <stats> (Mask Decoding)  Setup Huffman : 6 usec (1.2 %total)
[Dec 31 16:00] <stats> (Mask Decoding)     Huffman+LZ : 250 usec (50 %total)
[Dec 31 16:00] <stats> (Mask Decoding)     RLE+Filter : 233 usec (46.6 %total)
[Dec 31 16:00] <stats> (Mask Decoding)        Overall : 500 usec
[Dec 31 16:00] <stats> (Mask Decoding) Throughput : 36.216 MBPS (input bytes)
[Dec 31 16:00] <stats> (Mask Decoding) Throughput : 262.144 MBPS (output bytes)
[Dec 31 16:00] <main> Writing output image file: output.png
[Dec 31 16:00] <main> Read success!
 $ open decoded_mono.png
~~~

Note the output is actually dumped to "decoded_mono.png" and not the file you
specify.  This is just for testing the monochrome compression and will be
removed as the codec matures into full RGBA.

Some interesting things to note:

The PNG decoding rate is about 100 MB/s and our decoding rate is over twice as
fast.  This is about how it will be in the full RGBA version as well.

The compression ratio achieved by our monochrome encoder on this data is better
than PNG on all the files we've tested so far.  Sometimes by a little sometimes
by a lot.  Note this doesn't indicate performance for full RGBA data though.

The code is well-written in our opinion, easy to read and adapt, and easy to
incorporate into mobile development.

We plan to release a Java version for the encoder after the RGBA compression is
functional in C++ code, so that the encoder can be run on any platform without
having to compile it.  The decoder will be split off so that only a minimal
amount of code needs to be added to support this file format.  We're shooting
for one large C++ file, though it may end up being a small number of files.


Stay tuned! =) -cat
