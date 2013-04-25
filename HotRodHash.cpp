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

#include "HotRodHash.hpp"
using namespace cat;

void FileValidationHash::hashBytes(u8 *bytes, int bc) {
	int wc = bc >> 2;
	u32 *keys = reinterpret_cast<u32*>( bytes );
	for (int ii = 0; ii < wc; ++ii) {
		u32 k1 = keys[ii];

		k1 *= c1;
		k1 = CAT_ROL32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = CAT_ROL32(h1, 13); 
		h1 = h1 * 5 + 0xe6546b64;
	}

	bytes += wc << 2;
	u32 k1 = 0;
	switch (bc & 3) {
		case 3: k1 ^= bytes[2] << 16;
		case 2: k1 ^= bytes[1] << 8;
		case 1: k1 ^= bytes[0];
				k1 *= c1;
				k1 = CAT_ROL32(k1, 15);
				k1 *= c2;
				h1 ^= k1;
	}
}

void HotRodHash::hashBytes(u8 *bytes, int bc) {
	CAT_DEBUG_ENFORCE(bytes != 0 && bc >= 0);

	int wc = bc >> 2;
	u32 *keys = reinterpret_cast<u32*>( bytes );
	hashWords(keys, wc);

	bytes += wc << 2;
	u32 k1 = 0;
	switch (bc & 3) {
	case 3: k1 ^= bytes[2] << 16;
	case 2: k1 ^= bytes[1] << 8;
	case 1: k1 ^= bytes[0];
			h1 += k1 * c1;
			h1 ^= CAT_ROL32(k1, 15);
	default:;
	}
}

