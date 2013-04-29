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

#ifndef CAT_CONFIG_HPP
#define CAT_CONFIG_HPP

namespace cat {


// Enable statistics collection (disable when building decoder only)
//#define CAT_COLLECT_STATS

// Disable inane-level (verbose) logging in Release mode
#define CAT_RELEASE_DISABLE_INANE

// Modify level of detail for enforcer strings
#define CAT_USE_ENFORCE_EXPRESSION_STRING
#define CAT_USE_ENFORCE_FILE_LINE_STRING

// Bloat the file size a lot to check for desynchronization points in decoder
//#define CAT_DESYNCH_CHECKS

// This definition overrides CAT_BUILD_DLL below.  Neuters CAT_EXPORT macro so symbols are
// neither exported or imported.
#define CAT_NEUTER_EXPORT

// This definition changes the meaning of the CAT_EXPORT macro on Windows.  When defined,
// the CAT_EXPORT macro will export the associated symbol.  When undefined, it will import it.
//#define CAT_BUILD_DLL

// If you know the endianness of your target, uncomment one of these for better performance.
//#define CAT_ENDIAN_BIG
//#define CAT_ENDIAN_LITTLE

// Adjust if your architecture uses larger than 128-byte cache line
#define CAT_DEFAULT_CACHE_LINE_SIZE 128
#define CAT_DEFAULT_CPU_COUNT 1
#define CAT_DEFAULT_PAGE_SIZE 65536
#define CAT_DEFAULT_ALLOCATION_GRANULARITY CAT_DEFAULT_PAGE_SIZE
#define CAT_DEFAULT_SECTOR_SIZE 512

// Enable leak debug mode of the common runtime heap allocator
//#define CAT_DEBUG_LEAKS


} // namespace cat

#endif // CAT_CONFIG_HPP

