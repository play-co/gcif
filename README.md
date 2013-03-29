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


Specification
=============

Starting from the BCIF spec we decided to make some improvements.

To optimize for low decompression time, we restricted ourself to considering
only the fastest compression primitives: Filtering, LZ, and static Huffman.

BCIF does filtering and static Huffman coding, but left out LZ, which we feel
is a mistake since LZ is great for representing repeated data patterns, which
can be encoded byte-wise, reducing the number of Huffman symbols to decode.
In our tests so far, adding an LZ step improves compression ratio and speed.

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


### Step 1. Filtering

Spatial and color filters are applied to the input data in 8x8 pixel blocks as
in BCIF.  The pair of filters that best estimate each block are chosen, as
measured by the L1 norm of the resulting pixel color component values, with 0
and 255 being the best result, and numbers around 128 being the worst.

This generates encoded pixels and a filter description.  The filter description
is compressed with LZ and Huffman codes then written to the file.


### Step 2. Huffman Encoding

For each RGB plane, the BCIF "chaos" metric is used to sort each remaining
filtered pixel into one of 8 bins.  Symbol statistics are collected for each
bin and each RGB plane.


### Step 3. LZ

Now that the length of each symbol is known in bits, LZ can be run on each RGB
plane with knowledge of the trade-offs between matching and bit-wise literal
copies.  This encoding changes the symbol statistics.


### Step 4. Interleaved Encoding

Finally the LZ-encoded data is written out.  Since the decoder will be in lock-
step with the encoder, RGB bits may be ommitted during each write.

##### LZ modifications

Since the LZ4 decoding is more just "in spirit" of the original decoder we have
decided to change the LZ4 encoding format.

LZ4 additive literal decoding scheme is not ideal because it results in larger
files and requires more EOF checks since it cannot be unrolled on the decoder.

Our LZ4 encoded block uses the high bit of the literal length field bytes to
indicate that more bytes are part of the length field, instead of 255.

It may also be interesting to increase the max LZ match offset from -65535 to
a higher number of bits, since for 1024x1024 images, this restricts its look-
back to just 64 scanlines.



What works right now
====================

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
