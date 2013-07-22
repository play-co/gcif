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

#include "EntropyEncoder.hpp"
using namespace cat;

int EntropyEncoder::simulateZeroRun(int run) {
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

int EntropyEncoder::writeZeroRun(int run, ImageWriter &writer) {
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

void EntropyEncoder::init(int num_syms, int zrle_syms) {
	_zeroRun = 0;

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

#ifdef CAT_DEBUG
	_basic_recall = 0;
#endif
}

void EntropyEncoder::add(u16 symbol) {
	_basic_syms.push_back(symbol);

	// Convert fake zero to a zero
	if (symbol == FAKE_ZERO) {
		CAT_DEBUG_ENFORCE(_zeroRun > 0);
		symbol = 0;
	}

	CAT_DEBUG_ENFORCE(symbol < _num_syms);

	_basic_hist.add(symbol);

	if (symbol == 0) {
		++_zeroRun;
	} else {
		const int zeroRun = _zeroRun;

		if (zeroRun > 0) {
			int code = _num_syms + zeroRun - 1;
			if (code > _bz_tail_sym) {
				code = _bz_tail_sym;
			}
			_bz_hist.add(code);

			_runList.push_back(zeroRun);
			_zeroRun = 0;

			_az_hist.add(symbol);
		} else {
			_bz_hist.add(symbol);
		}
	}
}

int EntropyEncoder::finalize() {
	const int zeroRun = _zeroRun;

	// If a zero run is in progress at the end,
	if (zeroRun > 0) {
		// Record it
		_runList.push_back(zeroRun);

		// Record symbols that will be emitted
		int code = _num_syms + zeroRun - 1;
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

		// If not a fake zero,
		if (symbol < FAKE_ZERO) {
			basic_cost += _basic.simulateWrite(symbol);
		}
	}

	int az_cost = 64; // Bias for overhead cost
	int run = 0, readIndex = 0;
	for (int ii = 0, iiend = (int)_basic_syms.size(); ii < iiend; ++ii) {
		u16 symbol = _basic_syms[ii];

		// Convert fake zero to a zero
		if (symbol == FAKE_ZERO) {
			symbol = 0;
		}

		// If zero,
		if (symbol == 0) {
			// If starting a zero run,
			if (run == 0) {
				CAT_DEBUG_ENFORCE(readIndex < (int)_runList.size());

				// Write stored zero run
				int runLength = _runList[readIndex++];

				az_cost += simulateZeroRun(runLength);
			}

			++run;
		} else {
			// If just out of a zero run,
			if (run > 0) {
				run = 0;
				az_cost += _az.simulateWrite(symbol);
			} else {
				az_cost += _bz.simulateWrite(symbol);
			}
		}
	}

	if (basic_cost < az_cost + 64) {
		_using_basic = true;
		return basic_cost;
	} else {
		return az_cost;
	}
}

int EntropyEncoder::price(u16 symbol) {
	CAT_DEBUG_ENFORCE(symbol == _basic_syms[_basic_recall++]);

	// Convert fake zero to a zero
	if (symbol == FAKE_ZERO) {
		CAT_DEBUG_ENFORCE(_using_basic || _zeroRun > 0);
		return 0;
	}

	CAT_DEBUG_ENFORCE(symbol < _num_syms);

	if (_using_basic) {
		if (symbol == 0) {
			_zeroRun++;
		} else {
			_zeroRun = 0;
		}
		return _basic.simulateWrite(symbol);
	}

	// If zero,
	if (symbol == 0) {
		// If starting a zero run,
		if (_zeroRun == 0) {
			CAT_DEBUG_ENFORCE(_runListReadIndex < (int)_runList.size());

			// Write stored zero run
			int runLength = _runList[_runListReadIndex++];

			int bits = simulateZeroRun(runLength);

			// Calculate average cost of encoding zeroes in this run
			int zeroCost = bits / runLength;
			if (zeroCost <= 0) {
				zeroCost = 1; // min = 1 bit
			}
			_zeroCost = zeroCost;
		}

		++_zeroRun;
		return _zeroCost;
	} else {
		// If just out of a zero run,
		if (_zeroRun > 0) {
			_zeroRun = 0;
			return _az.simulateWrite(symbol);
		} else {
			return _bz.simulateWrite(symbol);
		}
	}
}

int EntropyEncoder::writeTables(ImageWriter &writer) {
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

int EntropyEncoder::write(u16 symbol, ImageWriter &writer) {
	CAT_DEBUG_ENFORCE(symbol == _basic_syms[_basic_recall++]);

	// Convert fake zero to a zero
	if (symbol == FAKE_ZERO) {
		CAT_DEBUG_ENFORCE(_using_basic || _zeroRun > 0);
		return 0;
	}

	CAT_DEBUG_ENFORCE(symbol < _num_syms);

	if (_using_basic) {
		if (symbol == 0) {
			_zeroRun++;
		} else {
			_zeroRun = 0;
		}
		return _basic.writeSymbol(symbol, writer);
	}

	int bits = 0;

	// If zero,
	if (symbol == 0) {
		// If starting a zero run,
		if (_zeroRun == 0) {
			CAT_DEBUG_ENFORCE(_runListReadIndex < (int)_runList.size());

			// Write stored zero run
			int runLength = _runList[_runListReadIndex++];

			bits += writeZeroRun(runLength, writer);
		}

		++_zeroRun;
	} else {
		// If just out of a zero run,
		if (_zeroRun > 0) {
			_zeroRun = 0;
			bits += _az.writeSymbol(symbol, writer);
		} else {
			bits += _bz.writeSymbol(symbol, writer);
		}
	}

	return bits;
}
