#include <iostream>
#include <vector>
using namespace std;

#include "Log.hpp"
#include "Clock.hpp"
#include "EndianNeutral.hpp"
#include "BitMath.hpp"
#include "HuffmanDecoder.hpp"
#include "HuffmanEncoder.hpp"
using namespace cat;

#include "lodepng.h"
#include "optionparser.h"
#include "lz4.h"
#include "lz4hc.h"

static CAT_INLINE void byteEncode(vector<unsigned char> &bytes, int data) {
	/*
	 * Delta byte-wise encoding:
	 *
	 * cDDDDDDs cDDDDDDD ...
	 */

	unsigned char s = 0;
	if (data < 0) {
		s = 1;
		data = -data;
	}

	unsigned char e = ((data & 63) << 1) | s;
	if (data < 64) {
		bytes.push_back(e);
	} else {
		e |= 128;
		data >>= 6;
		bytes.push_back(e);

		while (true) {
			e = (data & 127);

			if (data < 128) {
				bytes.push_back(e);
				break;
			} else {
				e |= 128;
				data >>= 7;
				bytes.push_back(e);
			}
		}
	}
}

class MonoConverter {
	vector<unsigned char> image;
	unsigned width, height;
	vector<unsigned short> symbols;

public:
	bool compress(const char *filename) {
		unsigned error = lodepng::decode(image, width, height, filename);

		if (error) {
			CAT_WARN("main") << "Decoder error " << error << ": " << lodepng_error_text(error);
			return false;
		}

		if ((width & 7) | (height & 7)) {
			CAT_WARN("main") << "Image dimensions must be an even multiple of 8x8";
			return false;
		}

		CAT_INFO("main") << "Uncompressed size = " << (width * height / 8) << " bytes";

		// Convert to monochrome image
		u32 bufferStride = (width + 31) >> 5;
		u32 bufferSize = bufferStride * height;
		u32 *buffer = new u32[bufferSize];
		u32 *writer = buffer;
		const unsigned char *reader = (const unsigned char*)&image[0] + 3;

		for (int ii = 0; ii < height; ++ii) {
			for (int jj = 0, jjlen = width >> 5; jj < jjlen; ++jj) {
				u32 bits = (reader[0] == 0);
				bits = (bits << 1) | (reader[4] == 0);
				bits = (bits << 1) | (reader[8] == 0);
				bits = (bits << 1) | (reader[12] == 0);
				bits = (bits << 1) | (reader[16] == 0);
				bits = (bits << 1) | (reader[20] == 0);
				bits = (bits << 1) | (reader[24] == 0);
				bits = (bits << 1) | (reader[28] == 0);
				bits = (bits << 1) | (reader[32] == 0);
				bits = (bits << 1) | (reader[36] == 0);
				bits = (bits << 1) | (reader[40] == 0);
				bits = (bits << 1) | (reader[44] == 0);
				bits = (bits << 1) | (reader[48] == 0);
				bits = (bits << 1) | (reader[52] == 0);
				bits = (bits << 1) | (reader[56] == 0);
				bits = (bits << 1) | (reader[60] == 0);
				bits = (bits << 1) | (reader[64] == 0);
				bits = (bits << 1) | (reader[68] == 0);
				bits = (bits << 1) | (reader[72] == 0);
				bits = (bits << 1) | (reader[76] == 0);
				bits = (bits << 1) | (reader[80] == 0);
				bits = (bits << 1) | (reader[84] == 0);
				bits = (bits << 1) | (reader[88] == 0);
				bits = (bits << 1) | (reader[92] == 0);
				bits = (bits << 1) | (reader[96] == 0);
				bits = (bits << 1) | (reader[100] == 0);
				bits = (bits << 1) | (reader[104] == 0);
				bits = (bits << 1) | (reader[108] == 0);
				bits = (bits << 1) | (reader[112] == 0);
				bits = (bits << 1) | (reader[116] == 0);
				bits = (bits << 1) | (reader[120] == 0);
				bits = (bits << 1) | (reader[124] == 0);

				*writer++ = bits;
				reader += 128;
			}

			u32 ctr = width & 31;
			if (ctr) {
				u32 bits = 0;
				while (ctr--) {
					bits = (bits << 1) | (reader[0] == 0);
					reader += 4;
				}
				*writer++ = bits;
			}
		}

		// Encode y2x delta:

		{
			// Walk backwards from the end
			u32 *lagger = buffer + bufferSize - bufferStride;
			int hctr = height;
			while (--hctr) {
				u32 cb = 0;

				for (int jj = 0; jj < bufferStride; ++jj) {
					u32 above = lagger[jj - (int)bufferStride];
					u32 now = lagger[jj];

					u32 ydelta = now ^ above;
					u32 y2xdelta = ydelta ^ (((ydelta >> 2) | cb) & ((ydelta >> 1) | (cb << 1)));
					cb = ydelta << 30;

					lagger[jj] = y2xdelta;
				}

				lagger -= bufferStride;
			}

			// First line
			u32 cb = 0;
			for (int jj = 0; jj < bufferStride; ++jj) {
				u32 now = lagger[jj];
				lagger[jj] = now ^ ((now >> 1) | cb);
				cb = now << 31;
			}
		}

		// RLE

		vector<unsigned char> rle;

		/*
		 * Delta byte-wise encoding:
		 *
		 * cDDDDDDs cDDDDDDD ...
		 */

		{
			vector<int> rlePrevZeroes;
			int prevCount = 0;
			vector<int> rleCurZeroes;
			vector<int> rleCurDeltas;

			u32 *lagger = buffer;
			int hctr = height, zeroes = 0;

			while (hctr--) {
				int prevIndex = 0;

				for (int jj = 0, jjlen = bufferStride - 1; jj <= jjlen; ++jj) {
					u32 now = lagger[jj];

					if (now) {
						u32 lastbit = 31;
						do {
							u32 bit = BSR32(now);

							zeroes += lastbit - bit;

							int delta;
							if (prevIndex < prevCount) {
								delta = zeroes - rlePrevZeroes[prevIndex++];
							} else {
								delta = zeroes;
							}

							rleCurZeroes.push_back(zeroes);
							rleCurDeltas.push_back(delta);

							zeroes = 0;
							lastbit = bit - 1;
							now ^= 1 << bit;
						} while (now);
					} else {
						zeroes += 32;
					}
				}

				int deltaCount = (int)rleCurDeltas.size();
				byteEncode(rle, deltaCount);

				for (int kk = 0; kk < deltaCount; ++kk) {
					int delta = rleCurDeltas[kk];
					byteEncode(rle, delta);
				}

				rlePrevZeroes = rleCurZeroes;
				prevCount = (int)rlePrevZeroes.size();
				rleCurZeroes.clear();
				rleCurDeltas.clear();

				lagger += bufferStride;
			}
		}

		CAT_INFO("main") << "Post-RLE size = " << rle.size() << " bytes";

		// Compress with LZ4

		vector<unsigned char> lz;
		int lzSize;

		{
			lz.resize(LZ4_compressBound(rle.size()));

			lzSize = LZ4_compressHC((char*)&rle[0], (char*)&lz[0], rle.size());

			lz.resize(lzSize);

			CAT_INFO("main") << "Post-LZ4 size = " << lzSize << " bytes";
		}

		// Collect byte symbol statistics

		int hist[256] = {0};
		int num_syms = 0;

		{
			for (int ii = 0; ii < lzSize; ++ii) {
				if (hist[lz[ii]]++ == 0) {
					++num_syms;
				}
			}

			CAT_INFO("main") << "Huffman: Number of symbols = " << num_syms << " syms";
		}

		// Compress with Huffman encoding

		vector<unsigned char> huffStream;

		{
			huffman::huffman_work_tables state;

			u16 freqs[256];

			u8 symbol_lut[256];

			int freqIndex = 0;
			for (int ii = 0; ii < 256; ++ii) {
				int count = hist[ii];
				if (count) {
					symbol_lut[ii] = (u8)freqIndex;
					freqs[freqIndex++] = count;
				} else {
					symbol_lut[ii] = 255;
				}
			}

			u8 codesizes[256];
			u32 max_code_size;
			u32 total_freq;

			huffman::generate_huffman_codes(&state, num_syms, freqs, codesizes, max_code_size, total_freq);

			CAT_INFO("main") << "Huffman: Max code size = " << max_code_size << " bits";
			CAT_INFO("main") << "Huffman: Total freq = " << total_freq << " rels";

			if (max_code_size > huffman::cMaxExpectedCodeSize) {
				huffman::limit_max_code_size(num_syms, codesizes, huffman::cMaxExpectedCodeSize);
			}

			u16 codes[256];

			huffman::generate_codes(num_syms, codesizes, codes);

			// Encode symbols

			u32 bitcount = 0;

			for (int ii = 0; ii < lzSize; ++ii) {
				u8 byte = lz[ii];
				u8 symbol = symbol_lut[byte];

				u16 code = codes[symbol];
				u8 codesize = codesizes[symbol];

				bitcount += codesize;
			}

			CAT_INFO("main") << "Huffman: Total message size (without table) = " << (bitcount + 7) / 8 << " bytes";

			// Encode table

			vector<unsigned char> huffTable;
			int lzhSize = 256;

			int lastCodeSize = 3;
			for (int ii = 0; ii < 256; ++ii) {
				u8 symbol = symbol_lut[ii];
				u8 codesize = codesizes[symbol];

				int delta = codesize - lastCodeSize;
				lastCodeSize = codesize;

				byteEncode(huffTable, delta);
			}

			// Collect byte symbol statistics

			int hhist[32] = {0};
			int hnum_syms = 0;

			{
				for (int ii = 0; ii < lzhSize; ++ii) {
					if (hhist[huffTable[ii]]++ == 0) {
						++hnum_syms;
					}
				}

				CAT_INFO("main") << "Huffman: Number of table symbols = " << hnum_syms << " syms";
			}

			// Huffman compress header

			huffman::huffman_work_tables hstate;

			u16 hfreqs[32];

			u8 hsymbol_lut[32];

			int hfreqIndex = 0;
			for (int ii = 0; ii < 32; ++ii) {
				int count = hhist[ii];
				if (count) {
					hsymbol_lut[ii] = (u8)hfreqIndex;
					hfreqs[hfreqIndex++] = count;
				} else {
					hsymbol_lut[ii] = 255;
				}
			}

			u8 hcodesizes[32];
			u32 hmax_code_size;
			u32 htotal_freq;

			huffman::generate_huffman_codes(&hstate, hnum_syms, hfreqs, hcodesizes, hmax_code_size, htotal_freq);

			CAT_INFO("main") << "Huffman: Max table code size = " << hmax_code_size << " bits";
			CAT_INFO("main") << "Huffman: Total table freq = " << htotal_freq << " rels";

			if (hmax_code_size > huffman::cMaxExpectedCodeSize) {
				huffman::limit_max_code_size(hnum_syms, hcodesizes, huffman::cMaxExpectedCodeSize);
			}

			u16 hcodes[32];

			huffman::generate_codes(hnum_syms, hcodesizes, hcodes);

			// Encode symbols

			u32 hbitcount = 0;

			for (int ii = 0; ii < lzhSize; ++ii) {
				u8 byte = huffTable[ii];
				u8 symbol = hsymbol_lut[byte];

				u16 code = hcodes[symbol];
				u8 codesize = hcodesizes[symbol];

				hbitcount += codesize;
			}

			CAT_INFO("main") << "Huffman: Table size = " << (hbitcount + 7) / 8 + 16 << " bytes";
		}

		// Convert to image:

		vector<unsigned char> output;
		u8 bits = 0, bitCount = 0;

		for (int ii = 0; ii < height; ++ii) {
			for (int jj = 0; jj < width; ++jj) {
				u32 set = (buffer[ii * bufferStride + jj / 32] >> (31 - (jj & 31))) & 1;
				bits <<= 1;
				bits |= set;
				if (++bitCount >= 8) {
					output.push_back(bits);
					bits = 0;
					bitCount = 0;
				}
			}
		}

		lodepng_encode_file("output.png", (const unsigned char*)&output[0], width, height, LCT_GREY, 1);

		delete []buffer;

		return true;
	}
};

enum  optionIndex { UNKNOWN, HELP, VERBOSE, SILENT, COMPRESS, DECOMPRESS, TEST };
const option::Descriptor usage[] =
{
  {UNKNOWN, 0,"" , ""    ,option::Arg::None, "USAGE: gcif_mono [options] [output file path]\n\n"
                                             "Options:" },
  {HELP,    0,"h", "help",option::Arg::None, "  --[h]elp  \tPrint usage and exit." },
  {VERBOSE,0,"v" , "verbose",option::Arg::None, "  --[v]erbose \tVerbose console output" },
  {SILENT,0,"s" , "silent",option::Arg::None, "  --[s]ilent \tNo console output (even on errors)" },
  {COMPRESS,0,"c" , "compress",option::Arg::Optional, "  --[c]ompress <input PNG file path> \tCompress the given .PNG image." },
  {DECOMPRESS,0,"d" , "decompress",option::Arg::Optional, "  --[d]ecompress <input GCI file path> \tDecompress the given .GCI image" },
  {TEST,0,"t" , "test",option::Arg::Optional, "  --[t]est <input PNG file path> \tTest compression to verify it is lossless" },
  {UNKNOWN, 0,"" ,  ""   ,option::Arg::None, "\nExamples:\n"
                                             "  gcif_mono -tv ./original.png\n"
                                             "  gcif_mono -c ./original.png test.gci\n"
                                             "  gcif_mono -d ./test.gci decoded.png" },
  {0,0,0,0,0,0}
};

int processParameters(option::Parser &parse, option::Option options[]) {
	if (parse.error()) {
		CAT_FATAL("main") << "Error parsing arguments [retcode:1]";
		return 1;
	}

	if (options[SILENT]) {
		Log::ref()->SetThreshold(LVL_SILENT);
	}

	if (options[VERBOSE]) {
		Log::ref()->SetThreshold(LVL_INANE);
	}

	if (options[COMPRESS]) {

		const char *filePath = parse.nonOption(0);
		MonoConverter converter;
		if (!converter.compress(filePath)) {
			CAT_INFO("main") << "Error during conversion [retcode:2]";
			return 2;
		}

		return 0;

	} else if (options[DECOMPRESS]) {
		CAT_FATAL("main") << "TODO";
	} else if (options[TEST]) {
		CAT_FATAL("main") << "TODO";
	}

	option::printUsage(std::cout, usage);
	return 0;
}

int main(int argc, const char *argv[]) {

	Clock::ref()->OnInitialize();

	if (argc > 0) {
		--argc;
		++argv;
	}

	option::Stats  stats(usage, argc, argv);
	option::Option *options = new option::Option[stats.options_max];
	option::Option *buffer = new option::Option[stats.buffer_max];
	option::Parser parse(usage, argc, argv, options, buffer);

	int retval = processParameters(parse, options);

	delete []options;
	delete []buffer;

	Clock::ref()->OnFinalize();

	return retval;
}

