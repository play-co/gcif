/*
	Copyright (c) 2013 Christopher A. Taylor.  All rights reserved.

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

#ifndef CAT_MAPPED_FILE_HPP
#define CAT_MAPPED_FILE_HPP

#include "Platform.hpp"

#ifdef CAT_OS_WINDOWS
#include "WindowsInclude.hpp"
#endif

/*
	Memory-mapped files are a fairly good compromise between performance and flexibility.

	Compared with asynchronous io, memory-mapped files are:
		+ Much easier to implement in a portable way
		+ Automatically paged in and out of RAM
		+ Automatically read-ahead cached
		+ Simple enough to put in my Common library
		+ Almost as good for performance

	When asynch io is not available or blocking is acceptable then this is a
	great alternative with low overhead and similar performance.

	For sequential file access, use MappedSequentialReader with a MappedFile
	that has been opened with random_access = false.

	For random file access, use MappedView with a MappedFile that has been
	opened with random_access = true.  Random access is usually used for a
	database-like file type, which is much better implemented using asynch io.
*/

namespace cat {


class MappedFile;
class MappedView;
class MappedFileReader;


// Read-only memory mapped file
class CAT_EXPORT MappedFile
{
	friend class MappedView;

#if defined(CAT_OS_WINDOWS)
	HANDLE _file;
#else
	int _file;
#endif

	bool _readonly;
    u64 _len;

public:
    MappedFile();
    ~MappedFile();

	// Opens the file for shared read-only access with other applications
	// Returns false on error (file not found, etc)
	bool OpenRead(const char *path, bool read_ahead = false, bool no_cache = false);

	// Creates and opens the file for exclusive read/write access
	bool OpenWrite(const char *path, u64 size);

	void Close();

	CAT_INLINE bool IsReadOnly() { return _readonly; }
	CAT_INLINE u64 GetLength() { return _len; }
	CAT_INLINE bool IsValid() { return _len != 0; }
};


// View of a portion of the memory mapped file
class CAT_EXPORT MappedView
{
	friend class MappedSequentialReader;

#if defined(CAT_OS_WINDOWS)
	HANDLE _map;
#else
	void *_map;
#endif

	MappedFile *_file;
	u8 *_data;
	u64 _offset;
	u32 _length;

public:
	MappedView();
	~MappedView();

	bool Open(MappedFile *file); // Returns false on error
	u8 *MapView(u64 offset = 0, u32 length = 0); // Returns 0 on error, 0 length means whole file
	void Close();

	CAT_INLINE bool IsValid() { return _data != 0; }
	CAT_INLINE MappedFile *GetFile() { return _file; }
	CAT_INLINE u8 *GetFront() { return _data; }
	CAT_INLINE u64 GetOffset() { return _offset; }
	CAT_INLINE u32 GetLength() { return _length; }
};


// Memory mapped file sequential reader
class CAT_EXPORT MappedSequentialReader
{
	MappedView _view;
	u32 _offset; // Offset in bytes into the mapped view for next read

public:
	static const u32 READ_AHEAD_CACHE = 16000000;	// 16 MB read ahead cache
	static const u32 MAX_READ_SIZE = 512000000;		// 512 MB read limit (per read)

	bool Open(MappedFile *file);		// Returns false on error
	u8 *Peek(u32 bytes);				// Returns 0 if read would be beyond end of file

	CAT_INLINE void Skip(u32 bytes)
	{
		_offset += bytes;
	}

	CAT_INLINE u8 *Read(u32 bytes)
	{
		u8 *data = Peek(bytes);
		Skip(bytes);
		return data;
	}

	int ReadLine(char *outs, int len);	// Returns < 0 on eof, otherwise the line length

	CAT_INLINE void Close() { _view.Close(); }

	CAT_INLINE bool IsValid() { return _view.IsValid(); }
	CAT_INLINE u64 GetLength() { return _view.GetFile()->GetLength(); }
	CAT_INLINE u64 GetOffset() { return _view.GetOffset() + _offset; }
	CAT_INLINE u64 GetRemaining() { return GetLength() - GetOffset(); }
};


// Simplified version that does not allow for caching between views of a file
class CAT_EXPORT SequentialFileReader : public MappedSequentialReader
{
	MappedFile _file;

public:
	CAT_INLINE bool Open(const char *path) { MappedSequentialReader::Close(); return _file.OpenRead(path) && MappedSequentialReader::Open(&_file); }
	CAT_INLINE void Close() { MappedSequentialReader::Close(); _file.Close(); }
};


} // namespace cat

#endif // CAT_MAPPED_FILE_HPP
