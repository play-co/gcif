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
#include "EntropyDecoder.hpp"
using namespace cat;

bool EntropyDecoder::init(int num_syms, int zrle_syms, int huff_lut_bits, ImageReader &reader) {
	_num_syms = num_syms;

	CAT_DEBUG_ENFORCE(num_syms > 0 && zrle_syms > 0);

	// If using AZ symbols,
	if (reader.readBit()) {
		_zrle_offset = zrle_syms - 1;

		if (!_az.init(num_syms, reader, huff_lut_bits)) {
			return false;
		}

		if (!_bz.init(num_syms + zrle_syms, reader, huff_lut_bits)) {
			return false;
		}
	} else {
		// Cool: Does not slow down decoder to conditionally turn off zRLE!
		if (!_bz.init(num_syms, reader, huff_lut_bits)) {
			return false;
		}
	}

	_afterZero = false;
	_zeroRun = 0;

	return true;
}

u16 EntropyDecoder::next(ImageReader &reader) {
	// If in a zero run,
	if (_zeroRun > 0) {
		--_zeroRun;
		return 0;
	}

	// If after zero,
	if (_afterZero) {
		_afterZero = false;
		return _az.next(reader);
	}

	// Read before-zero symbol
	const int num_syms = _num_syms;
	u16 sym = (u16)_bz.next(reader);

	// If not a zero run,
	if (sym < num_syms) {
		return sym;
	}

	// Decode zero run
	u32 zeroRun = sym - num_syms;

	// If extra bits were used,
	if (zeroRun >= _zrle_offset) {
		CAT_DEBUG_ENFORCE(zeroRun == _zrle_offset);

		zeroRun += reader.read255255();
	}

	_zeroRun = zeroRun;
	_afterZero = true;
	return 0;
}

