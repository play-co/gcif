/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of GCIF nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include "ImageMaskWriter.hpp"
#include "EndianNeutral.hpp"
#include "BitMath.hpp"
#include "HuffmanEncoder.hpp"
#include "HuffmanDecoder.hpp"
#include "Filters.hpp"
#include "GCIFWriter.hpp"
#include "Log.hpp"
#ifdef CAT_COLLECT_STATS
#include "Clock.hpp"
#endif // CAT_COLLECT_STATS
using namespace cat;

#include <map>
using namespace std;

#include "lz4.h"
#include "lz4hc.h"

#ifdef DUMP_FILTER_OUTPUT
#include "lodepng.h"
#endif // DUMP_FILTER_OUTPUT


//// Masker

void Masker::applyFilter() {
	// Walk backwards from the end
	const int stride = _stride;
	u32 *lagger = _mask + _size - stride;
	u32 *writer = _filtered + _size - stride;
	int hctr = _height;
	while (--hctr) {
		u32 cb = 0; // assume no delta from row above

		for (int jj = 0; jj < stride; ++jj) {
			u32 above = lagger[jj - stride];
			u32 now = lagger[jj];

			u32 ydelta = now ^ above;
			u32 y2xdelta = ydelta ^ (((ydelta >> 2) | cb) & ((ydelta >> 1) | (cb << 1)));
			cb = ydelta << 30;

			writer[jj] = y2xdelta;
		}

		lagger -= stride;
		writer -= stride;
	}

	// First line
	u32 cb = 1 << 31; // assume it is on the edges
	for (int jj = 0; jj < stride; ++jj) {
		u32 now = lagger[jj];
		writer[jj] = now ^ ((now >> 1) | cb);
		cb = now << 31;
	}

#ifdef DUMP_FILTER_OUTPUT
	{
		CAT_WARN("mask") << "Writing delta.png image file";

		// Convert to image:

		vector<unsigned char> output;
		u8 bits = 0, bitCount = 0;

		for (int ii = 0; ii < _height; ++ii) {
			for (int jj = 0; jj < _width; ++jj) {
				u32 set = (_filtered[ii * _stride + jj / 32] >> (31 - (jj & 31))) & 1;
				bits <<= 1;
				bits |= set;
				if (++bitCount >= 8) {
					output.push_back(bits);
					bits = 0;
					bitCount = 0;
				}
			}
		}

		lodepng_encode_file("delta.png", (const unsigned char*)&output[0], _width, _height, LCT_GREY, 1);
	}
#endif
}

void Masker::clear() {
	if (_mask) {
		delete []_mask;
		_mask = 0;
	}
	if (_filtered) {
		delete []_filtered;
		_filtered = 0;
	}
}

int Masker::initFromRGBA(const u8 *rgba, u32 color, u32 color_mask, int width, int height, const GCIFKnobs *knobs, int min_ratio) {
	_knobs = knobs;
	_rgba = rgba;
	_width = width;
	_height = height;
	_color = color;
	_color_mask = color_mask;
	_min_ratio = min_ratio;
	_stride = (width + 31) >> 5;
	_size = height * _stride;

	clear();

	_mask = new u32[_size];
	_filtered = new u32[_size];

	// Fill in bitmask
	int covered = 0;
	const u32 *pixel = reinterpret_cast<const u32 *>( _rgba );
	u32 *writer = _mask;

	// For each scanline,
	for (int y = 0; y < _height; ++y) {
		u32 bits = 0, seen = 0;

		// For each pixel,
		for (int x = 0, xend = _width; x < xend; ++x) {
			bits <<= 1;

			if ((*pixel & color_mask) == color) {
				++covered;
				bits |= 1;
			}

			if (++seen >= 32) {
				*writer++ = bits;
				seen = 0;
				bits = 0;
			}

			++pixel;
		}

		if (seen > 0) {
			*writer++ = bits;
		}
	}

	_covered = covered;

#ifdef DUMP_FILTER_OUTPUT
	{
		CAT_WARN("mask") << "Writing mask.png";

		vector<unsigned char> output;
		u8 bits = 0, bitCount = 0;

		for (int ii = 0; ii < _height; ++ii) {
			for (int jj = 0; jj < _width; ++jj) {
				u32 set = (_mask[ii * _stride + jj / 32] >> (31 - (jj & 31))) & 1;
				bits <<= 1;
				bits |= set;
				if (++bitCount >= 8) {
					output.push_back(bits);
					bits = 0;
					bitCount = 0;
				}
			}
		}

		lodepng_encode_file("mask.png", (const unsigned char*)&output[0], _width, _height, LCT_GREY, 1);
	}
#endif

#ifdef CAT_COLLECT_STATS
	Stats.covered = covered;
#endif // CAT_COLLECT_STATS

	return WE_OK;
}

static CAT_INLINE void byteEncode(vector<u8> &bytes, u32 data) {
	while (data >= 255) {
		bytes.push_back(255);
		data -= 255;
	}

	bytes.push_back(data);
}

void Masker::performRLE() {
	u32 *lagger = _filtered;
	const int stride = _stride;

	vector<int> deltas;

	for (int ii = 0, iilen = _height; ii < iilen; ++ii) {
		// for xdelta:
		int zeroes = 0;

		for (int jj = 0; jj < stride; ++jj) {
			u32 now = lagger[jj];

			if (now) {
				u32 bit, lastbit = 31;
				do {
					bit = BSR32(now);

					zeroes += lastbit - bit;

					deltas.push_back(zeroes);

					zeroes = 0;
					lastbit = bit - 1;
					now ^= 1 << bit;
				} while (now);

				zeroes += bit;
			} else {
				zeroes += 32;
			}
		}

		const int deltaCount = static_cast<int>( deltas.size() );

		byteEncode(_rle, deltaCount);

		for (int kk = 0; kk < deltaCount; ++kk) {
			int delta = deltas[kk];
			byteEncode(_rle, delta);
		}

		deltas.clear();

		lagger += stride;
	}
}

void Masker::performLZ() {
	_lz.resize(LZ4_compressBound(static_cast<int>( _rle.size() )));

	const int lzSize = LZ4_compressHC((char*)&_rle[0], (char*)&_lz[0], _rle.size());

	_lz.resize(lzSize);

#ifdef CAT_COLLECT_STATS
	Stats.rleBytes = _rle.size();
	Stats.lzBytes = _lz.size();
#endif // CAT_COLLECT_STATS

	// Determine if encoder should be used
	_using_encoder = _lz.size() >= _knobs->mask_huffThresh;
}

u32 Masker::simulate() {
	const int lzSize = static_cast<int>( _lz.size() );

	int bits = 32;

	if (_using_encoder) {
		for (int ii = 0; ii < lzSize; ++ii) {
			u8 sym = _lz[ii];
			bits += _encoder.simulateWrite(sym);
		}
	} else {
		bits += lzSize * 8;
	}

	return bits;
}

bool Masker::evaluate() {
#ifdef CAT_COLLECT_STATS
	Clock *clock = Clock::ref();
	double t0 = clock->usec();
#endif // CAT_COLLECT_STATS

	applyFilter();

#ifdef CAT_COLLECT_STATS
	double t1 = clock->usec();
#endif // CAT_COLLECT_STATS

	performRLE();

#ifdef CAT_COLLECT_STATS
	double t2 = clock->usec();
#endif // CAT_COLLECT_STATS

	performLZ();

#ifdef CAT_COLLECT_STATS
	double t3 = clock->usec();
#endif // CAT_COLLECT_STATS

	u16 freqs[256];
	collectFreqs(256, _lz, freqs);

#ifdef CAT_COLLECT_STATS
	double t4 = clock->usec();
#endif // CAT_COLLECT_STATS

	if (_using_encoder) {
		CAT_ENFORCE(_encoder.init(freqs));
	}

#ifdef CAT_COLLECT_STATS
	double t5 = clock->usec();
#endif // CAT_COLLECT_STATS

	u32 simulated_bits = simulate();
	u32 ratio = _covered * 32 / simulated_bits;
	_enabled = (ratio >= _min_ratio);

#ifdef CAT_COLLECT_STATS
	double t6 = clock->usec();

	Stats.filterUsec = t1 - t0;
	Stats.rleUsec = t2 - t1;
	Stats.lzUsec = t3 - t2;
	Stats.histogramUsec = t4 - t3;
	Stats.generateTableUsec = t5 - t4;
	Stats.dataSimulateUsec = t6 - t5;
	Stats.overallUsec = t6 - t0;
#endif // CAT_COLLECT_STATS

	return _enabled;
}

void Masker::writeEncodedLZ(ImageWriter &writer) {
	const int lzSize = static_cast<int>( _lz.size() );

#ifdef CAT_COLLECT_STATS
	u32 data_bits = 0;
#endif // CAT_COLLECT_STATS

	if (_using_encoder) {
		for (int ii = 0; ii < lzSize; ++ii) {
			u8 sym = _lz[ii];

			int bits = _encoder.writeSymbol(sym, writer);
#ifdef CAT_COLLECT_STATS
			data_bits += bits;
#endif // CAT_COLLECT_STATS
		}
	} else {
		for (int ii = 0; ii < lzSize; ++ii) {
			writer.writeBits(_lz[ii], 8);
#ifdef CAT_COLLECT_STATS
			data_bits += 8;
#endif
		}
	}

#ifdef CAT_COLLECT_STATS
	Stats.data_bits = data_bits;
#endif // CAT_COLLECT_STATS
}

void Masker::write(ImageWriter &writer) {
	int table_bits = 0;

#ifdef CAT_COLLECT_STATS
	Clock *clock = Clock::ref();
	double t0 = clock->usec();
#endif // CAT_COLLECT_STATS

	writer.writeBit(_enabled);

	if (_enabled) {
		CAT_INANE("mask") << "Writing mask for color (" << (_color & 255) << "," << ((_color >> 8) & 255) << "," << ((_color >> 16) & 255) << "," << ((_color >> 24) & 255) << ") ...";

		writer.writeWord(_color);
		table_bits += 32;

		table_bits += writer.write9(_rle.size());
		table_bits += writer.write9(_lz.size());
		writer.writeBit(_using_encoder);
		++table_bits;

		if (_using_encoder) {
			table_bits = _encoder.writeTable(writer);
		}
	}

#ifdef CAT_COLLECT_STATS
	double t1 = clock->usec();
#endif // CAT_COLLECT_STATS

	if (_enabled) {
		writeEncodedLZ(writer);
	}

#ifdef CAT_COLLECT_STATS
	double t2 = clock->usec();

	Stats.table_bits = table_bits;
	Stats.tableEncodeUsec = t1 - t0;
	Stats.dataEncodeUsec = t2 - t1;
	Stats.overallUsec += t2 - t0;

	Stats.compressedDataBits = Stats.data_bits + Stats.table_bits;
	Stats.compressionRatio = Stats.covered * 32 / (double)Stats.compressedDataBits;
#endif // CAT_COLLECT_STATS
}

#ifdef CAT_COLLECT_STATS

bool Masker::dumpStats() {
	if (!_enabled) {
		CAT_INANE("stats") << "(Mask Encoding)   Disabled.";
	} else {
		CAT_INANE("stats") << "(Mask Encoding)      Chosen Color : (" << (_color & 255) << "," << ((_color >> 8) & 255) << "," << ((_color >> 16) & 255) << "," << ((_color >> 24) & 255) << ") ...";
		CAT_INANE("stats") << "(Mask Encoding)     Post-RLE Size : " <<  Stats.rleBytes << " bytes";
		CAT_INANE("stats") << "(Mask Encoding)      Post-LZ Size : " <<  Stats.lzBytes << " bytes";
		CAT_INANE("stats") << "(Mask Encoding) Post-Huffman Size : " << (Stats.data_bits + 7) / 8 << " bytes (" << Stats.data_bits << " bits)";
		CAT_INANE("stats") << "(Mask Encoding)        Table Size : " <<  (Stats.table_bits + 7) / 8 << " bytes (" << Stats.table_bits << " bits)";

		CAT_INANE("stats") << "(Mask Encoding)      Filtering : " <<  Stats.filterUsec << " usec (" << Stats.filterUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Encoding)            RLE : " <<  Stats.rleUsec << " usec (" << Stats.rleUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Encoding)             LZ : " <<  Stats.lzUsec << " usec (" << Stats.lzUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Encoding)      Histogram : " <<  Stats.histogramUsec << " usec (" << Stats.histogramUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Encoding) Generate Table : " <<  Stats.generateTableUsec << " usec (" << Stats.generateTableUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Encoding)   Encode Table : " <<  Stats.tableEncodeUsec << " usec (" << Stats.tableEncodeUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Encoding)    Encode Data : " <<  Stats.dataEncodeUsec << " usec (" << Stats.dataEncodeUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Encoding)        Overall : " <<  Stats.overallUsec << " usec";

		CAT_INANE("stats") << "(Mask Encoding) Throughput : " << Stats.compressedDataBits/8 / Stats.overallUsec << " MBPS (output bytes)";
		CAT_INANE("stats") << "(Mask Encoding) Compression ratio : " << Stats.compressionRatio << ":1 (" << Stats.compressedDataBits/8 << " bytes used overall)";
	}

	CAT_INANE("stats") << "(Mask Encoding) Pixels covered : " << Stats.covered << " (" << Stats.covered * 100. / (_width * _height) << " %total)";

	return true;
}

#endif // CAT_COLLECT_STATS



//// ImageMaskWriter

u32 ImageMaskWriter::dominantColor() {
	map<u32, u32> tracker;

	// Histogram all image colors
	const u32 *pixel = reinterpret_cast<const u32 *>( _rgba );
	int count = _width * _height;
	while (count--) {
		u32 p = *pixel++;
		if ((getLE(p) & 0xff000000) != 0) {
			tracker[p]++;
		}
	}

	// Determine dominant color
	u32 domColor = getLE(0xff000000), domScore = 0;
	for (map<u32, u32>::iterator ii = tracker.begin(); ii != tracker.end(); ++ii) {
		if (domScore < ii->second) {
			domScore = ii->second;
			domColor = ii->first;
		}
	}

	return domColor;
}

int ImageMaskWriter::initFromRGBA(const u8 *rgba, int width, int height, const GCIFKnobs *knobs) {

	if (!rgba || width < FILTER_ZONE_SIZE || height < FILTER_ZONE_SIZE) {
		return WE_BAD_DIMS;
	}

	if ((width & FILTER_ZONE_SIZE_MASK) || (height & FILTER_ZONE_SIZE_MASK)) {
		return WE_BAD_DIMS;
	}

	_knobs = knobs;
	_rgba = rgba;
	_width = width;
	_height = height;

	int err;

	const u32 ALPHA_COLOR = 0;
	const u32 ALPHA_MASK = getLE(0xff000000);
	if ((err = _alpha.initFromRGBA(rgba, ALPHA_COLOR, ALPHA_MASK, width, height, knobs, _knobs->mask_minAlphaRat))) {
		return err;
	}

	u32 domColor = dominantColor();
	const u32 COLOR_MASK = 0xffffffff;

	if ((err = _color.initFromRGBA(rgba, domColor, COLOR_MASK, width, height, knobs, _knobs->mask_minColorRat))) {
		return err;
	}

	return WE_OK;
}

void ImageMaskWriter::write(ImageWriter &writer) {
	bool use_alpha = _alpha.evaluate();
	bool use_color = _color.evaluate();

	_alpha.write(writer);
	_color.write(writer);

#ifdef CAT_COLLECT_STATS
	Stats.compressedDataBits = 0;
	if (use_alpha) {
		Stats.compressedDataBits += _alpha.Stats.compressedDataBits;
	}
	if (use_color) {
		Stats.compressedDataBits += _color.Stats.compressedDataBits;
	}
#endif // CAT_COLLECT_STATS
}

