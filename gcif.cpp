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
#include "ImageFilter.hpp"
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
	unsigned char b0 = data;

	if (data >>= 7) {
		u8 b1 = data;

		if (data >>= 7) {
			u8 b2 = data;

			if (data >>= 7) {
				u8 b3 = data;

				if (data >>= 7) {
					u8 b4 = data;

					bytes.push_back(b4 | 128);
				}

				bytes.push_back(b3 | 128);
			}

			bytes.push_back(b2 | 128);
		}

		bytes.push_back(b1 | 128);
	}

	bytes.push_back(b0 & 127);
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

		// Generate ImageMask
		ImageMask imageMask;
		if (!imageMask.initFromRGBA(&image[0], width, height)) {
			CAT_WARN("main") << "Unable to generate image mask";
			return false;
		}

		// RGB filter

		ImageFilter filter;
		FilterDesc filterDesc;
		filter.filterImage(&image[0], width, height, filterDesc);

		// Filter statistics

		const int fil_syms = CF_COUNT * SF_COUNT;
		u16 fil_freqs[fil_syms];

		{
			int hist[fil_syms] = {0};
			int max_sym = 0;

			for (int ii = 0; ii < height; ii += 8) {
				for (int jj = 0; jj < width; jj += 8) {
					int f = filterDesc.getFilter(jj, ii);
					int count = ++hist[f];
					cout << f << " ";
					if (max_sym < count) {
						max_sym = count;
					}
				}
				cout << endl;
			}

			// Scale to fit in 16-bit frequency counter
			while (max_sym > 0xffff) {
				max_sym = 0;

				for (int ii = 0; ii < fil_syms; ++ii) {
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

			for (int ii = 0; ii < fil_syms; ++ii) {
				fil_freqs[ii] = static_cast<u16>( hist[ii] );
			}
		}

		// Compress filters with Huffman encoding

		vector<u32> fhuffStream;
		int fhuffBits = 0;

		{
			huffman::huffman_work_tables state;

			u8 codesizes[fil_syms];
			u32 max_code_size;
			u32 total_freq;

			huffman::generate_huffman_codes(&state, fil_syms, fil_freqs, codesizes, max_code_size, total_freq);

			CAT_INFO("main") << "Filters: Huffman: Max code size = " << max_code_size << " bits";
			CAT_INFO("main") << "Filters: Huffman: Total freq = " << total_freq << " rels";

			if (max_code_size > huffman::cMaxExpectedCodeSize) {
				huffman::limit_max_code_size(fil_syms, codesizes, huffman::cMaxExpectedCodeSize);
			}

			// Encode table

			vector<unsigned char> huffTable;

			int lag0 = 3;
			u32 sum = 0;
			for (int ii = 0; ii < fil_syms; ++ii) {
				u8 symbol = ii;
				u8 codesize = codesizes[symbol];

				int delta = codesize - lag0;
				lag0 = codesize;

				if (delta <= 0) {
					delta = -delta << 1;
				} else {
					delta = ((delta - 1) << 1) | 1;
				}

				huffTable.push_back(delta);
				sum += delta;
			}

			//CAT_INFO("main") << "Huffman: Encoded table hash = " << hex << murmurHash(&huffTable[0], huffTable.size());
			CAT_INFO("main") << "Filters: Huffman: Encoded table size = " << huffTable.size() << " bytes";

			// Find K shift
			sum >>= 8;
			u32 shift = sum > 0 ? BSR32(sum) : 0;
			u32 shiftMask = (1 << shift) - 1;

			CAT_INFO("main") << "Filters: Golomb: Chose table pivot " << shift << " bits";

			// Write out shift: number from 0..5, so round up to 0..7 in 3 bits
			CAT_ENFORCE(shift <= 7);

			u32 bitWorks = 0; // Bit encoding workspace

			//bitWrite(huffStream, bitWorks, huffBits, shift, 3);

			int hbitcount = 3;

			for (int ii = 0; ii < huffTable.size(); ++ii) {
				int symbol = huffTable[ii];
				int q = symbol >> shift;

				for (int jj = 0; jj < q; ++jj) {
					// write a 1
					//bitWrite(huffStream, bitWorks, huffBits, 1, 1);
					++hbitcount;
				}
				// write a 0
				//bitWrite(huffStream, bitWorks, huffBits, 0, 1);
				++hbitcount;

				if (shift) {
					//bitWrite(huffStream, bitWorks, huffBits, symbol & shiftMask, shift);
				}
				hbitcount += shift;
			}

			CAT_INFO("main") << "Filters: Golomb: Table size = " << (hbitcount + 7) / 8 << " bytes";

			// Encode data to Huffman stream

			u16 codes[fil_syms];

			huffman::generate_codes(fil_syms, codesizes, codes);

			for (int ii = 0; ii < height; ii += 8) {
				for (int jj = 0; jj < width; jj += 8) {
					u8 symbol = filterDesc.getFilter(jj, ii);

					u16 code = codes[symbol];
					u8 codesize = codesizes[symbol];

					hbitcount += codesize;
					//bitWrite(huffStream, bitWorks, huffBits, code, codesize);
				}
			}

			// Push remaining bits
//			if ((huffBits & 31)) {
//				huffStream.push_back(bitWorks);
//			}

			CAT_INFO("main") << "Filters: Huffman: Total compressed filters size = " << (hbitcount + 7) / 8 << " bytes";
		}

		// TODO: Write here

		return success;
	}

	int _width, _height, _stride;
	int _writeRow;
	u32 *_image;

	int _sum, _rowLeft;
	bool _rowStarted;

	u32 *_row;
	int _bitOffset;
	bool _bitOn;
	int _lastSum;
	double _rleTime;

	bool decodeRLE(u8 *rle, int len) {
		double t0 = Clock::ref()->usec();

		if (len <= 0) {
			_rleTime += Clock::ref()->usec() - t0;
			return false;
		}

		u32 sum = _sum;
		bool rowStarted = _rowStarted;
		int rowLeft = _rowLeft;
		u32 *row = _row;
		const int stride = _stride;
		int bitOffset = _bitOffset;
		bool bitOn = _bitOn;

		for (int ii = 0; ii < len; ++ii) {
			u8 symbol = rle[ii];

			sum <<= 7;
			if CAT_UNLIKELY(symbol & 128) {
				sum |= symbol & 127;
			} else {
				sum |= symbol;

				// If has read row length yet,
				if (rowStarted) {
					int wordOffset = bitOffset >> 5;
					int newBitOffset = bitOffset + sum;
					int newOffset = newBitOffset >> 5;
					int shift = 31 - (newBitOffset & 31);

					// If at the first row,
					if CAT_UNLIKELY(_writeRow <= 0) {
						/*
						 * First row is handled specially:
						 *
						 * For each input X:
						 * 1. Write out X bits of current state
						 * 2. Flip the state
						 * 3. Then write out one bit of the new state
						 * When out of input, pad to the end with current state
						 *
						 * Demo:
						 *
						 * (1) 1 0 1 0 0 0 1 1 1
						 *     0 1 1 1 0 0 1 0 0
						 * {1, 0, 0, 2}
						 * --> 1 0 1 0 0 0 1 1 1
						 */

						// If previous state was 0,
						if (bitOn ^= 1) {
							// Fill bottom bits with 0s (do nothing)

							// For each intervening word and new one,
							for (int ii = wordOffset + 1; ii < newOffset; ++ii) {
								row[ii] = 0;
							}

							// Write a 1 at the new location
							row[newOffset] = 1 << shift;
						} else {
							u32 bitsUsedMask = 0xffffffff >> (bitOffset & 31);

							if (newOffset <= wordOffset) {
								row[newOffset] |= bitsUsedMask & (0xfffffffe << shift);
							} else {
								// Fill bottom bits with 1s
								row[wordOffset] |= bitsUsedMask;

								// For each intervening word,
								for (int ii = wordOffset + 1; ii < newOffset; ++ii) {
									row[ii] = 0xffffffff;
								}

								// Set 1s for new word, ending with a 0
								row[newOffset] = 0xfffffffe << shift;
							}
						}
					} else {
						/*
						 * 0011110100
						 * 0011001100
						 * {2,0,2,0}
						 * 0011110100
						 *
						 * 0111110100
						 * 1,0,0,0,0,1
						 * 0110110
						 *
						 * Same as first row except only flip on when we get X = 0
						 * And we will XOR with previous row
						 */

						//if (_writeRow > 120 && _writeRow < 200) cout << sum << ":" << bitOn << " ";

						// If previous state was toggled on,
						if (bitOn) {
							u32 bitsUsedMask = 0xffffffff >> (bitOffset & 31);

							if (newOffset <= wordOffset) {
								//cout << "S(" << row[newOffset] << "," << bitsUsedMask << "," << shift << ") ";
								row[newOffset] ^= bitsUsedMask & (0xfffffffe << shift);
								//cout << "S(" << row[newOffset] << "," << bitsUsedMask << "," << shift << ") ";
							} else {
								//cout << "M ";
								// Fill bottom bits with 1s
								row[wordOffset] ^= bitsUsedMask;

								// For each intervening word,
								for (int ii = wordOffset + 1; ii < newOffset; ++ii) {
									row[ii] ^= 0xffffffff;
								}

								// Set 1s for new word, ending with a 0
								row[newOffset] ^= (0xfffffffe << shift);
							}

							bitOn ^= 1;
							_lastSum = 0;
						} else {
							// Fill bottom bits with 0s (do nothing)

							//cout << "Z ";
							row[newOffset] ^= (1 << shift);

							if (sum == 0 && _lastSum) {
								bitOn ^= 1;
							}
							_lastSum = 1;
						}
					}

					bitOffset += sum + 1;

					// If just finished this row,
					if (--rowLeft <= 0) {
						int wordOffset = bitOffset >> 5;

						if CAT_LIKELY(_writeRow > 0) {
							// If last bit written was 1,
							if (bitOn) {
								// Fill bottom bits with 1s

								row[wordOffset] ^= 0xffffffff >> (bitOffset & 31);

								// For each remaining word,
								for (int ii = wordOffset + 1; ii < stride; ++ii) {
									row[ii] ^= 0xffffffff;
								}
							}
						} else {
							// If last bit written was 1,
							if (bitOn) {
								// Fill bottom bits with 1s
								row[wordOffset] |= 0xffffffff >> (bitOffset & 31);

								// For each remaining word,
								for (int ii = wordOffset + 1; ii < stride; ++ii) {
									row[ii] = 0xffffffff;
								}
							} else {
								// Fill bottom bits with 0s (do nothing)

								// For each remaining word,
								for (int ii = wordOffset + 1; ii < stride; ++ii) {
									row[ii] = 0;
								}
							}
						}

						//if (_writeRow > 120 && _writeRow < 200) cout << endl;

						if (++_writeRow >= _height) {
							// done!
							_rleTime += Clock::ref()->usec() - t0;
							return true;
						}

						rowStarted = false;
						row += stride;
					}
				} else {
					rowLeft = sum;

					// If row was empty,
					if (rowLeft == 0) {
						//if (_writeRow > 120 && _writeRow < 200) cout << "(empty)" << endl;
						// Decode as an exact copy of the row above it
						if (_writeRow > 0) {
							u32 *copy = row - stride;
							for (int ii = 0; ii < stride; ++ii) {
								row[ii] = copy[ii];
							}
						} else {
							for (int ii = 0; ii < stride; ++ii) {
								row[ii] = 0xffffffff;
							}
						}

						if (++_writeRow >= _height) {
							// done!
							_rleTime += Clock::ref()->usec() - t0;
							return true;
						}

						row += stride;
					} else {
						rowStarted = true;

						// Reset row decode state
						bitOn = false;
						bitOffset = 0;
						_lastSum = 0;

						// Setup first word
						if CAT_LIKELY(_writeRow > 0) {
							u32 *copy = row - stride;
							for (int ii = 0; ii < stride; ++ii) {
								row[ii] = copy[ii];
							}
						} else {
							row[0] = 0;
						}
					}
				}

				sum = 0;
			}
		}

		_bitOffset = bitOffset;
		_bitOn = bitOn;
		_row = row;
		_sum = sum;
		_rowStarted = rowStarted;
		_rowLeft = rowLeft;

		_rleTime += Clock::ref()->usec() - t0;
		return false;
	}

	bool decode(u16 width, u16 height, u32 *words, int wordCount, u32 dataHash) {
		CAT_INFO("main") << "Huffman+table hash: " << hex << MurmurHash3::hash(words, wordCount * 4);

		_width = width;
		_height = height;
		_stride = ((u32)width + 31) >> 5;
		_writeRow = 0;
		_image = new u32[(u32)height * _stride];

		_sum = 0;
		_rowLeft = 0;
		_rowStarted = false;
		_row = _image;
		_rleTime = 0;

		huffman::HuffmanDecoder decoder;

		decoder.init(words, wordCount);

		u8 *lz = new u8[65536];
		u16 lzIndex = 0, lzLast = 0;
		const int BATCH_RATE = 8192; 

		// LZ4
		{
			while (!decoder.isEOF()) {
				// Read token
				u8 token = decoder.next();

				// TODO: Change LZ4 encoding to avoid EOF checks here
				// Read Literal Length
				int literalLength = token >> 4;
				if (literalLength == 15) {
					int s;
					do {
						s = decoder.next();
						literalLength += s;
					} while (s == 255 && CAT_UNLIKELY(!decoder.isEOF()));
				}

				// Decode literal symbols
				for (int ii = 0; ii < literalLength; ++ii) {
					u8 symbol = decoder.next();
					lz[lzIndex++] = symbol;

					// Decode [wrapped] RLE sequence
					if CAT_UNLIKELY((u16)(lzIndex - lzLast) >= BATCH_RATE) {
						if CAT_UNLIKELY(lzLast > lzIndex) {
							if (decodeRLE(&lz[lzLast], 65536 - lzLast)) {
								return true;
							}

							lzLast = 0;
						}

						if CAT_UNLIKELY(decodeRLE(&lz[lzLast], lzIndex - lzLast)) {
							return true;
						}

						lzLast = lzIndex;
					}
				}

				// Read match offset
				u8 offset0 = decoder.next();
				u8 offset1 = decoder.next();
				u16 offset = ((u16)offset1 << 8) | offset0;

				// Read match length
				int matchLength = token & 15;
				if (matchLength == 15) {
					int s;
					do {
						s = decoder.next();
						matchLength += s;
					} while (s == 255 && CAT_UNLIKELY(!decoder.isEOF()));
				}
				matchLength += 4;

				// Copy match data
				//cout << "rep:" << matchLength << " off:" << offset;
				for (int ii = 0; ii < matchLength; ++ii) {
					u8 symbol = lz[ (u16)(lzIndex - offset) ];
					lz[lzIndex++] = symbol;

					// Decode [wrapped] RLE sequence
					if CAT_UNLIKELY((u16)(lzIndex - lzLast) >= BATCH_RATE) {
						if CAT_UNLIKELY(lzLast > lzIndex) {
							if (decodeRLE(&lz[lzLast], 65536 - lzLast)) {
								return true;
							}

							lzLast = 0;
						}

						if CAT_UNLIKELY(decodeRLE(&lz[lzLast], lzIndex - lzLast)) {
							return true;
						}

						lzLast = lzIndex;
					}
				}
			}

			// Decode [wrapped] RLE sequence
			if CAT_UNLIKELY(lzLast > lzIndex) {
				if (decodeRLE(&lz[lzLast], 65536 - lzLast)) {
					return true;
				}

				lzLast = 0;
			}

			if CAT_UNLIKELY(decodeRLE(&lz[lzLast], lzIndex - lzLast)) {
				return true;
			}
		}

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

								double t0 = Clock::ref()->usec();

								if (!decode(width, height, words + GCIF_HEAD_WORDS, fileWords - GCIF_HEAD_WORDS, dataHash)) {
									CAT_WARN("main") << "Decoder failed";
								} else {
									success = true;
									double t1 = Clock::ref()->usec();

									CAT_WARN("main") << "Time took: " << t1 - t0 << " usec";
									CAT_WARN("main") << "RLE took: " << _rleTime << " usec";

									CAT_WARN("main") << "Processed input at " << (fileWords - GCIF_HEAD_WORDS) * 4 / (t1 - t0) << " MB/S";
									CAT_WARN("main") << "Generated at " << (width * (u32)height / 8) / (t1 - t0) << " MB/S";
								}
							}
						}
					}
				}
			}
		}

		if (success) {
			CAT_WARN("main") << "Writing output image file: " << outfile;
			// Convert to image:

			vector<unsigned char> output;
			u8 bits = 0, bitCount = 0;

			for (int ii = 0; ii < _height; ++ii) {
				for (int jj = 0; jj < _width; ++jj) {
					u32 set = (_image[ii * _stride + jj / 32] >> (31 - (jj & 31))) & 1;
					bits <<= 1;
					bits |= set;
					if (++bitCount >= 8) {
						output.push_back(bits);
						bits = 0;
						bitCount = 0;
					}
				}
			}

			lodepng_encode_file(outfile, (const unsigned char*)&output[0], _width, _height, LCT_GREY, 1);
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

