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

#ifndef ENTROPY_ENCODER_HPP
#define ENTROPY_ENCODER_HPP

#include "ImageWriter.hpp"
#include "HuffmanEncoder.hpp"
#include <vector>

/*
 * Game Closure Entropy-Based Compression
 *
 * This reuseable class produces a Huffman encoding of a data stream that
 * is expected to contain runs of zeroes.
 *
 * Statistics before and after zeroes are recorded separately, so that the
 * after-zero statistics can use fewer symbols for shorter codes.
 *
 * Zero runs are encoded with a set of ZRLE_SYMS symbols that directly encode
 * the lengths of these runs.  When the runs are longer than the ZRLE_SYMS
 * count, up to two FF bytes are written out and subtracted from the run count.
 * The remaining bytes are 16-bit words that repeat on FFFF.
 *
 * The two Huffman tables are written using the compressed representation in
 * HuffmanEncoder.
 *
 * Alternatively, if normal Huffman encoding is more effective, it is used,
 * making this class much more generic.
 */

namespace cat {


//// EntropyEncoder

class EntropyEncoder {
	int _num_syms, _zrle_syms;
	int _bz_syms; // NUM_SYMS + ZRLE_SYMS
	int _az_syms; // NUM_SYMS
	int _bz_tail_sym; // BZ_SYMS - 1

	FreqHistogram _bz_hist;
	FreqHistogram _az_hist;
	HuffmanEncoder _bz;
	HuffmanEncoder _az;

	FreqHistogram _basic_hist;
	std::vector<u16> _basic_syms;
	HuffmanEncoder _basic;
	bool _using_basic;

	u16 _lastSymbol;

	int _run;
	std::vector<int> _runList;
	int _runListReadIndex;

	int simulateRun(int run) {
		if (run <= 0) {
			return 0;
		}

		int bits;

		if (run < _zrle_syms) {
			bits = _bz.simulateWrite(_num_syms + run - 1);
		} else {
			bits = _bz.simulateWrite(_bz_tail_sym);

			ImageWriter::simulate255255(run - _zrle_syms);
		}

		return bits;
	}

	int writeRun(int run, ImageWriter &writer) {
		if (run <= 0) {
			return 0;
		}

		int bits;

		if (run < _zrle_syms) {
			bits = _bz.writeSymbol(_num_syms + run - 1, writer);
		} else {
			bits = _bz.writeSymbol(_bz_tail_sym, writer);

			writer.write255255(run - _zrle_syms);
		}

		return bits;
	}

public:
	void init(int num_syms, int zrle_syms) {
		_run = 0;
		_lastSymbol = 0xffff;

		_num_syms = num_syms;
		_zrle_syms = zrle_syms;
		_bz_syms = _num_syms + _zrle_syms;
		_az_syms = _num_syms;
		_bz_tail_sym = _bz_syms - 1;

		_bz_hist.init(_bz_syms);
		_az_hist.init(_az_syms);
		_basic_hist.init(_num_syms);

		_runList.clear();
		_basic_syms.clear();
	}

	void add(u16 symbol) {
		CAT_DEBUG_ENFORCE(symbol < _num_syms);

		_basic_hist.add(symbol);
		_basic_syms.push_back(symbol);

		if (symbol == _lastSymbol) {
			++_run;
		} else {
			const int run = _run;

			if (run > 0) {
				int code = _num_syms + run - 1;
				if (code > _bz_tail_sym) {
					code = _bz_tail_sym;
				}
				_bz_hist.add(code);

				_runList.push_back(run);
				_run = 0;

				_az_hist.add(symbol);
			} else {
				_bz_hist.add(symbol);
			}
		}

		_lastSymbol = symbol;
	}

	int finalize() {
		const int run = _run;

		// If a zero run is in progress at the end,
		if (run > 0) {
			// Record it
			_runList.push_back(run);

			// Record symbols that will be emitted
			int code = _num_syms + run - 1;
			if (code > _bz_tail_sym) {
				code = _bz_tail_sym;
			}
			_bz_hist.add(code);
		}

		// Initialize Huffman encoders with histograms
		_bz.init(_bz_hist);
		_az.init(_az_hist);

		reset();

		_using_basic = false;

		// Evaluate basic encoder
		_basic.init(_basic_hist);

		int basic_cost = 0;
		for (int ii = 0, iiend = (int)_basic_syms.size(); ii < iiend; ++ii) {
			u16 symbol = _basic_syms[ii];

			basic_cost += _basic.simulateWrite(symbol);
		}

		int az_cost = 64; // Bias for overhead cost
		int runCount = 0;
		int readIndex = 0;
		u16 lastSym = 0xffff;
		for (int ii = 0, iiend = (int)_basic_syms.size(); ii < iiend; ++ii) {
			u16 symbol = _basic_syms[ii];

			// If zero,
			if (symbol == lastSym) {
				// If starting a zero run,
				if (runCount == 0) {
					CAT_DEBUG_ENFORCE(readIndex < (int)_runList.size());

					// Write stored zero run
					int runLength = _runList[readIndex++];

					az_cost += simulateRun(runLength);
				}

				++runCount;
			} else {
				// If just out of a zero run,
				if (runCount > 0) {
					runCount = 0;
					az_cost += _az.simulateWrite(symbol);
				} else {
					az_cost += _bz.simulateWrite(symbol);
				}
			}

			lastSym = symbol;
		}

		if (basic_cost < az_cost + 64) {
			_using_basic = true;
			return basic_cost;
		} else {
			return az_cost;
		}
	}

	int writeTables(ImageWriter &writer) {
		int bits = 0;

		if (!_using_basic) {
			writer.writeBit(1);

			bits += _az.writeTable(writer);
			bits += _bz.writeTable(writer);
		} else {
			writer.writeBit(0);

			bits += _basic.writeTable(writer);
		}

		return bits;
	}

	void reset() {
		_run = 0;
		_lastSymbol = 0xffff;

		// Set the run list read index for writing
		_runListReadIndex = 0;
	}

	int simulate(u16 symbol) {
		CAT_DEBUG_ENFORCE(symbol < _num_syms);

		if (_using_basic) {
			return _basic.simulateWrite(symbol);
		}

		int bits = 0;

		// If zero,
		if (symbol == _lastSymbol) {
			// If starting a zero run,
			if (_run == 0) {
				CAT_DEBUG_ENFORCE(_runListReadIndex < (int)_runList.size());

				// Write stored zero run
				int runLength = _runList[_runListReadIndex++];

				bits += simulateRun(runLength);
			}

			++_run;
		} else {
			// If just out of a zero run,
			if (_run > 0) {
				_run = 0;
				bits += _az.simulateWrite(symbol);
			} else {
				bits += _bz.simulateWrite(symbol);
			}
		}

		_lastSymbol = symbol;

		return bits;
	}

	int write(u16 symbol, ImageWriter &writer) {
		CAT_DEBUG_ENFORCE(symbol < _num_syms);

		if (_using_basic) {
			return _basic.writeSymbol(symbol, writer);
		}

		int bits = 0;

		// If zero,
		if (symbol == _lastSymbol) {
			// If starting a zero run,
			if (_run == 0) {
				CAT_DEBUG_ENFORCE(_runListReadIndex < (int)_runList.size());

				// Write stored zero run
				int runLength = _runList[_runListReadIndex++];

				bits += writeRun(runLength, writer);
			}

			++_run;
		} else {
			// If just out of a zero run,
			if (_run > 0) {
				_run = 0;
				bits += _az.writeSymbol(symbol, writer);
			} else {
				bits += _bz.writeSymbol(symbol, writer);
			}
		}

		_lastSymbol = symbol;

		return bits;
	}
};


} // namespace cat

#endif // ENTROPY_ENCODER_HPP

