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

#ifndef SMART_ARRAY_HPP
#define SMART_ARRAY_HPP

#include "Platform.hpp"
#include "Enforcer.hpp"

namespace cat {


//// SmartArray

template<class T> class SmartArray {
	T *_data;
	int _alloc;
	int _size;

protected:
	void alloc(int size) {
		_data = new T[size];
		_alloc = _size = size;
	}

	void grow(int size) {
		if (_data) {
			delete []_data;
		}
		alloc(size);
	}

	void cleanup() {
		if (_data) {
			delete []_data;
			_data = 0;
		}
	}

public:
	CAT_INLINE SmartArray() {
		_data = 0;
	}
	CAT_INLINE virtual ~SmartArray() {
		cleanup();
	}

	CAT_INLINE void resize(int size) {
		if (!_data) {
			alloc(size);
		} else if (size > _alloc) {
			grow(size);
		}
	}

	CAT_INLINE void fill_00() {
		CAT_DEBUG_ENFORCE(_data);

		memset(_data, 0x00, _size * sizeof(T));
	}

	CAT_INLINE void fill_ff() {
		CAT_DEBUG_ENFORCE(_data);

		memset(_data, 0xff, _size * sizeof(T));
	}

	CAT_INLINE int size() {
		return _size;
	}

	CAT_INLINE T *get() {
		CAT_DEBUG_ENFORCE(_data);

		return _data;
	}

	CAT_INLINE T &operator[](int index) {
		CAT_DEBUG_ENFORCE(_data);
		CAT_DEBUG_ENFORCE(index < _size);

		return _data[index];
	}
};


} // namespace cat

#endif // SMART_ARRAY_HPP

