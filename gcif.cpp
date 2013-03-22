#include <iostream>
#include <vector>
using namespace std;

#include "Log.hpp"
#include "Clock.hpp"
#include "EndianNeutral.hpp"
#include "BitMath.hpp"
#include "HuffmanDecoder.hpp"
#include "HuffmanEncoder.hpp"
#include "MappedFile.hpp"
#include "MurmurHash3.hpp"
using namespace cat;

#include "lodepng.h"
#include "optionparser.h"
#include "lz4.h"
#include "lz4hc.h"

static const u32 GCIF_HEAD_WORDS = 4;
static const u32 GCIF_MAGIC = 0x46494347;
static const u32 GCIF_HEAD_SEED = 0x120CA71D;

/*
 * File format:
 *
 * GCIF
 * <width(16)> <height(16)>
 * MurmurHash3-32(DATA_SEED, all words after header)
 * MurmurHash3-32(HEAD_SEED, all words above)
 *
 * Golomb-coded Huffman table
 * Huffman-coded {
 *   LZ4-coded {
 *     RLE+Delta-coded {
 *       y2xdelta-coded {
 *         Monochrome image raster
 *       }
 *     }
 *   }
 * }
 */

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

static CAT_INLINE void bitWrite(vector<u32> &words, u32 &currentWord, int &bitCount, u32 code, int codelen) {
	int bitWordOffset = bitCount & 31;
	int available = 32 - bitWordOffset;
	bitCount += codelen;

	currentWord |= (u32)(code << bitWordOffset);

	codelen -= available;

	if (codelen >= 0) {
		words.push_back(currentWord);
		currentWord = code >> available;
	}
}





























class MonoConverter {
	vector<unsigned char> image;
	unsigned width, height;
	vector<unsigned short> symbols;

public:
	bool compress(const char *filename, const char *outfile) {
		bool success = false;

		unsigned error = lodepng::decode(image, width, height, filename);

		CAT_INFO("main") << "Original image hash: " << hex << MurmurHash3::hash(&image[0], image.size());

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

		CAT_INFO("main") << "Monochrome image hash: " << hex << MurmurHash3::hash(&buffer[0], bufferSize * 4);

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

		CAT_INFO("main") << "Monochrome y2x delta image hash: " << hex << MurmurHash3::hash(&buffer[0], bufferSize * 4);

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

			// ydelta for count
			int lastDeltaCount = 0;

			while (hctr--) {
				int prevIndex = 0;

				// for xdelta:
				int lastZeroes = 0;

				for (int jj = 0, jjlen = bufferStride - 1; jj <= jjlen; ++jj) {
					u32 now = lagger[jj];

					if (now) {
						u32 lastbit = 31;
						do {
							u32 bit = now > 0 ? BSR32(now) : 0;

							zeroes += lastbit - bit;

#define USE_YDELTA_RLE 0
#define USE_XDELTA_RLE 0
#define USE_YDELTA_COUNT 1

#if USE_YDELTA_RLE
							int delta;
							if (prevIndex < prevCount) {
								delta = zeroes - rlePrevZeroes[prevIndex++];
							} else {
								delta = zeroes;
							}
#elif USE_XDELTA_RLE
							int delta = zeroes - lastZeroes;
							lastZeroes = zeroes;
#else
							int delta = zeroes;
#endif

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
#if USE_YDELTA_COUNT
				byteEncode(rle, deltaCount - lastDeltaCount);
				lastDeltaCount = deltaCount;
#else
				byteEncode(rle, deltaCount);
#endif

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

		CAT_INFO("main") << "RLE data hash: " << hex << MurmurHash3::hash(&rle[0], rle.size());

		CAT_INFO("main") << "Post-RLE size = " << rle.size() << " bytes";

		delete []buffer;

		// Compress with LZ4

		vector<unsigned char> lz;
		int lzSize;

		{
			lz.resize(LZ4_compressBound(rle.size()));

			lzSize = LZ4_compressHC((char*)&rle[0], (char*)&lz[0], rle.size());

			lz.resize(lzSize);

			CAT_INFO("main") << "Post-LZ4 size = " << lzSize << " bytes";
		}

		CAT_INFO("main") << "LZ data hash: " << hex << MurmurHash3::hash(&lz[0], lz.size());

		// Collect byte symbol statistics

		const int num_syms = 256;
		u16 freqs[256];

		{
			int hist[256] = {0};
			int max_sym = 0;

			for (int ii = 0; ii < lzSize; ++ii) {
				int count = ++hist[lz[ii]];
				if (max_sym < count) {
					max_sym = count;
				}
			}

			// Scale to fit in 16-bit frequency counter
			while (max_sym > 0xffff) {
				max_sym = 0;

				for (int ii = 0; ii < 256; ++ii) {
					int count = hist[ii];
					if (count) {
						count >>= 1;

						if (!count) {
							count = 1;
						}

						if (max_sym < count) {
							max_sym = count;
						}
					}
				}
			}

			for (int ii = 0; ii < 256; ++ii) {
				freqs[ii] = static_cast<u16>( hist[ii] );
			}
		}

		// Compress with Huffman encoding

		vector<u32> huffStream;
		int huffBits = 0;

		{
			huffman::huffman_work_tables state;

			u8 codesizes[256];
			u32 max_code_size;
			u32 total_freq;

			huffman::generate_huffman_codes(&state, num_syms, freqs, codesizes, max_code_size, total_freq);

			CAT_INFO("main") << "Huffman: Max code size = " << max_code_size << " bits";
			CAT_INFO("main") << "Huffman: Total freq = " << total_freq << " rels";

			if (max_code_size > huffman::cMaxExpectedCodeSize) {
				huffman::limit_max_code_size(num_syms, codesizes, huffman::cMaxExpectedCodeSize);
			}

			// Encode table

			vector<unsigned char> huffTable;

			int lag0 = 3, lag1 = 3;
			u32 sum = 0;
			for (int ii = 0; ii < 256; ++ii) {
				u8 symbol = ii;
				u8 codesize = codesizes[symbol];

				int delta = codesize;
				if (ii < 16) {
					delta -= lag0;
				} else {
					delta -= lag1;
				}
				lag1 = lag0;
				lag0 = codesize;

				if (delta < 0) {
					delta = (-delta << 1) | 1;
				} else {
					delta <<= 1;
				}

				huffTable.push_back(delta);
				sum += delta;
			}

			//CAT_INFO("main") << "Huffman: Encoded table hash = " << hex << murmurHash(&huffTable[0], huffTable.size());
			CAT_INFO("main") << "Huffman: Encoded table size = " << huffTable.size() << " bytes";

			// Find K shift
			sum >>= 8;
			u32 shift = sum > 0 ? BSR32(sum) : 0;
			u32 shiftMask = (1 << shift) - 1;

			CAT_INFO("main") << "Golomb: Chose table pivot " << shift << " bits";

			// Write out shift: number from 0..5, so round up to 0..7 in 3 bits
			CAT_ENFORCE(shift <= 7);

			u32 bitWorks = 0; // Bit encoding workspace

			bitWrite(huffStream, bitWorks, huffBits, shift, 3);

			int hbitcount = 3;

			for (int ii = 0; ii < huffTable.size(); ++ii) {
				int symbol = huffTable[ii];
				int q = symbol >> shift;

				for (int jj = 0; jj < q; ++jj) {
					// write a 1
					bitWrite(huffStream, bitWorks, huffBits, 1, 1);
					++hbitcount;
				}
				// write a 0
				bitWrite(huffStream, bitWorks, huffBits, 0, 1);
				++hbitcount;

				if (shift) {
					bitWrite(huffStream, bitWorks, huffBits, symbol & shiftMask, shift);
				}
				hbitcount += shift;
			}

			CAT_INFO("main") << "Golomb: Table size = " << (hbitcount + 7) / 8 << " bytes";

			// Encode data to Huffman stream

			u16 codes[256];

			huffman::generate_codes(num_syms, codesizes, codes);

			for (int ii = 0; ii < lzSize; ++ii) {
				u8 symbol = lz[ii];

				u16 code = codes[symbol];
				u8 codesize = codesizes[symbol];

				bitWrite(huffStream, bitWorks, huffBits, code, codesize);
			}

			// Push remaining bits
			if ((huffBits & 31)) {
				huffStream.push_back(bitWorks);
			}

			CAT_INFO("main") << "Huffman+table hash: " << hex << MurmurHash3::hash(&huffStream[0], huffStream.size() * 4);

			CAT_INFO("main") << "Huffman: Total compressed message size = " << (huffBits + 7) / 8 << " bytes";
		}
/*
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
*/

		MappedFile file;

		/*
		 * "GCIF" + width(2) + height(2) + huffbytes
		 */

		int huffWords = (huffBits + 31) / 32;

		if (file.OpenWrite(outfile, (GCIF_HEAD_WORDS + huffWords) * sizeof(u32))) {
			MappedView fileView;

			if (fileView.Open(&file)) {
				u8 *fileData = fileView.MapView();

				if (fileData) {
					u32 *words = reinterpret_cast<u32*>( fileData );

					MurmurHash3 hash;
					hash.init(huffman::GCIF_DATA_SEED);
					hash.hashWords(&huffStream[0], huffWords);
					u32 dataHash = hash.final(huffWords);

					hash.init(GCIF_HEAD_SEED);

					words[0] = getLE(GCIF_MAGIC);
					hash.hashWord(GCIF_MAGIC);
					u32 header1 = (width << 16) | height; // Temporary
					words[1] = getLE(header1);
					hash.hashWord(header1);
					words[2] = getLE(dataHash);
					hash.hashWord(dataHash);

					u32 headerHash = hash.final(GCIF_HEAD_WORDS);
					words[3] = getLE(headerHash);

					words += GCIF_HEAD_WORDS;

					for (int ii = 0; ii < huffWords; ++ii) {
						words[ii] = getLE(huffStream[ii]);
					}

					success = true;
				}
			}
		}

		return success;
	}

	bool decode(u16 width, u16 height, u32 *words, int wordCount, u32 dataHash) {
		CAT_INFO("main") << "Huffman+table hash: " << hex << MurmurHash3::hash(words, wordCount * 4);

		huffman::HuffmanDecoder decoder;

		decoder.init(words, wordCount);

		return true;
	}

	bool decompress(const char *filename, const char *outfile) {
		bool success = false;

		MappedFile file;

		if (file.OpenRead(filename)) {
			MappedView fileView;

			if (fileView.Open(&file)) {
				u8 *fileData = fileView.MapView();

				if (fileData) {
					u32 *words = reinterpret_cast<u32*>( fileData );
					u32 fileSize = fileView.GetLength();
					u32 fileWords = fileSize / 4;

					MurmurHash3 hash;
					hash.init(GCIF_HEAD_SEED);

					if (fileWords < GCIF_HEAD_WORDS) { // iunno
						CAT_WARN("main") << "File is too short to be a GCIF file: " << filename;
					} else {
						u32 word0 = getLE(words[0]);
						hash.hashWord(word0);

						if (GCIF_MAGIC != word0) {
							CAT_WARN("main") << "File is not a GCIF formatted file (magic mismatch): " << filename;
						} else {
							u32 word1 = getLE(words[1]);
							hash.hashWord(word1);

							u32 dataHash = getLE(words[2]);
							hash.hashWord(dataHash);

							u32 word3 = getLE(words[3]);
							if (word3 != hash.final(GCIF_HEAD_WORDS)) {
								CAT_WARN("main") << "File header is corrupted: " << filename;
							} else {
								u16 width = word1 >> 16;
								u16 height = word1 & 0xffff;
								CAT_WARN("main") << "Image: Dimensions " << width << " x " << height;

								if (!decode(width, height, words + GCIF_HEAD_WORDS, fileWords - GCIF_HEAD_WORDS, dataHash)) {
									CAT_WARN("main") << "Decoder failed";
								} else {
									success = true;
								}
							}
						}
					}
				}
			}
		}

		return success;
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
		if (parse.nonOptionsCount() != 2) {
			CAT_WARN("main") << "Input error: Please provide input and output file paths";
		} else {
			const char *inFilePath = parse.nonOption(0);
			const char *outFilePath = parse.nonOption(1);
			MonoConverter converter;

			if (!converter.compress(inFilePath, outFilePath)) {
				CAT_INFO("main") << "Error during conversion [retcode:2]";
				return 2;
			}

			return 0;
		}
	} else if (options[DECOMPRESS]) {
		if (parse.nonOptionsCount() != 2) {
			CAT_WARN("main") << "Input error: Please provide input and output file paths";
		} else {
			const char *inFilePath = parse.nonOption(0);
			const char *outFilePath = parse.nonOption(1);
			MonoConverter converter;

			if (!converter.decompress(inFilePath, outFilePath)) {
				CAT_INFO("main") << "Error during conversion [retcode:3]";
				return 3;
			}

			return 0;
		}
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

