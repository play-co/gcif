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

#include "MappedFile.hpp"
#include "Log.hpp"
#include "SystemInfo.hpp"
using namespace cat;

#if defined(CAT_OS_LINUX) || defined(CAT_OS_OSX)
# include <sys/mman.h>
# include <sys/stat.h>
# include <sys/fcntl.h>
# include <errno.h>
#endif

MappedFile::MappedFile()
{
	_len = 0;

#if defined(CAT_OS_WINDOWS)

	_file = INVALID_HANDLE_VALUE;

#else

	_file = -1;

#endif
}

MappedFile::~MappedFile()
{
	Close();
}

bool MappedFile::OpenRead(const char *path, bool read_ahead, bool no_cache)
{
	Close();

	_readonly = true;

#if defined(CAT_OS_WINDOWS)

	u32 access_pattern = !read_ahead ? FILE_FLAG_RANDOM_ACCESS : FILE_FLAG_SEQUENTIAL_SCAN;

	_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, access_pattern, 0);
	if (_file == INVALID_HANDLE_VALUE)
	{
		CAT_WARN("MappedFile") << "CreateFileA error " << GetLastError() << " for " << path;
		return false;
	}

	if (!GetFileSizeEx(_file, (LARGE_INTEGER*)&_len))
	{
		CAT_WARN("MappedFile") << "GetFileSizeEx error " << GetLastError() << " for " << path;
		return false;
	}

#else

	_file = open(path, O_RDONLY, (mode_t)0444);

	if (_file == -1) {
		CAT_WARN("MappedFile") << "open error " << errno << " for " << path;
		return false;
	} else {
		_len = lseek(_file, 0, SEEK_END);

		if (_len <= 0) {
			CAT_WARN("MappedFile") << "lseek error " << errno << " for " << path;
			return false;
		} else {
			if (read_ahead) {
				fcntl(_file, F_RDAHEAD, 1);
			}

			if (no_cache) {
				fcntl(_file, F_NOCACHE, 1);
			}
		}
	}

#endif

	return true;
}

bool MappedFile::OpenWrite(const char *path, u64 size)
{
	Close();

	_readonly = false;
	_len = size;

#if defined(CAT_OS_WINDOWS)

	const u32 access_pattern = FILE_FLAG_SEQUENTIAL_SCAN;

	_file = CreateFileA(path, GENERIC_WRITE|GENERIC_READ, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, access_pattern, 0);
	if (_file == INVALID_HANDLE_VALUE)
	{
		CAT_WARN("MappedFile") << "CreateFileA error " << GetLastError() << " for " << path;
		return false;
	}

	// Set file size
	if (!SetFilePointerEx(_file, *(LARGE_INTEGER*)&_len, 0, FILE_BEGIN)) {
		CAT_WARN("MappedFile") << "SetFilePointerEx error " << GetLastError() << " for " << path;
		return false;
	}
	if (!SetEndOfFile(_file)) {
		CAT_WARN("MappedFile") << "SetEndOfFile error " << GetLastError() << " for " << path;
		return false;
	}

#else

	_file = open(path, O_RDWR|O_CREAT|O_TRUNC, (mode_t)0666);

	if (_file == -1) {
		CAT_WARN("MappedFile") << "open error " << errno << " for " << path;
		return false;
	} else {
		if (-1 == lseek(_file, size - 1, SEEK_SET)) {
			CAT_WARN("MappedFile") << "lseek error " << errno << " for " << path;
			return false;
		} else {
			if (1 != write(_file, "", 1)) {
				CAT_WARN("MappedFile") << "write error " << errno << " for " << path;
				return false;
			}
		}
	}

#endif

	return true;
}

void MappedFile::Close()
{
#if defined(CAT_OS_WINDOWS)

	if (_file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_file);
		_file = INVALID_HANDLE_VALUE;
	}

#else

	if (_file != -1) {
		close(_file);
		_file = -1;
	}

#endif

	_len = 0;
}


//// MappedView

MappedView::MappedView()
{
	_data = 0;
	_length = 0;
	_offset = 0;

#if defined(CAT_OS_WINDOWS)

	_map = 0;

#else

	_map = MAP_FAILED;

#endif
}

MappedView::~MappedView()
{
	Close();
}

bool MappedView::Open(MappedFile *file)
{
	Close();

	if (!file || !file->IsValid()) return false;

	_file = file;

#if defined(CAT_OS_WINDOWS)

	const u32 flags = file->IsReadOnly() ? PAGE_READONLY : PAGE_READWRITE;
	_map = CreateFileMapping(file->_file, 0, flags, 0, 0, 0);
	if (!_map)
	{
		CAT_WARN("MappedView") << "CreateFileMapping error " << GetLastError();
		return false;
	}

#endif

	return true;
}

u8 *MappedView::MapView(u64 offset, u32 length)
{
	if (length == 0) {
		length = _file->GetLength();
	}

	if (offset) {
		u32 granularity = SystemInfo::ref()->GetAllocationGranularity();

		CAT_DEBUG_ENFORCE(CAT_IS_POWER_OF_2(granularity)) << "Allocation granularity is not a power of 2!";

		// Bring offset back to the previous allocation granularity
		u32 mask = granularity - 1;
		u32 masked = (u32)offset & mask;
		if (masked)
		{
			offset -= masked;
			length += masked;
		}
	}

#if defined(CAT_OS_WINDOWS)

	if (_data && !UnmapViewOfFile(_data))
	{
		CAT_INANE("MappedView") << "UnmapViewOfFile error " << GetLastError();
	}

	u32 flags = FILE_MAP_READ;
	if (!_file->IsReadOnly()) {
		flags |= FILE_MAP_WRITE;
	}

	_data = (u8*)MapViewOfFile(_map, flags, (u32)(offset >> 32), (u32)offset, length);
	if (!_data)
	{
		CAT_WARN("MappedView") << "MapViewOfFile error " << GetLastError();
		return 0;
	}

#else

	int prot = PROT_READ;
	if (!_file->_readonly) {
		prot |= PROT_WRITE;
	}

	_map = mmap(0, length, prot, MAP_SHARED, _file->_file, offset);

	if (_map == MAP_FAILED) {
		CAT_WARN("MappedView") << "mmap error " << errno;
		return 0;
	}

	_data = reinterpret_cast<u8*>( _map );

#endif

	_offset = offset;
	_length = length;

	return _data;
}

void MappedView::Close()
{
#if defined(CAT_OS_WINDOWS)

	if (_data)
	{
		UnmapViewOfFile(_data);
		_data = 0;
	}
	if (_map)
	{
		CloseHandle(_map);
		_map = 0;
	}

#else

	if (_map != MAP_FAILED)
	{
		munmap(_map, _length);
		_map = MAP_FAILED;
	}
	_data = 0;

#endif

	_length = 0;
	_offset = 0;
}


//// MappedSequentialReader

bool MappedSequentialReader::Open(MappedFile *file)
{
	_offset = 0;

	return _view.Open(file);
}

u8 *MappedSequentialReader::Peek(u32 bytes)
{
	CAT_DEBUG_ENFORCE(bytes <= MAX_READ_SIZE);

	u32 map_offset = _offset;
	u32 map_size = _view.GetLength();

	// If bytes read is available,
	if (bytes <= map_size - map_offset)
		return _view.GetFront() + map_offset;

	u64 file_offset = GetOffset();
	u64 file_remaining = GetLength() - file_offset;

	// If requested data is beyond the end of the file,
	if (bytes > file_remaining)
		return 0;

	u32 acquire = bytes;
	if (acquire < READ_AHEAD_CACHE)
	{
		if (READ_AHEAD_CACHE > file_remaining)
			acquire = (u32)file_remaining;
		else
			acquire = READ_AHEAD_CACHE;
	}

	// Map new view of file
	u8 *data = _view.MapView(file_offset, acquire);

	_offset = 0;

	return data;
}

int MappedSequentialReader::ReadLine(char *outs, int len)
{
	// Check if any data is available for reading
	u8 *data = Read(1);
	if (!data) return -1;

	int count = 0;

	// While there is room in the output buffer,
	while (len > 1)
	{
		char ch = (char)*data;

		// If character is a line delimiter token,
		if (ch == '\r')
		{
			// Peek at next character
			u8 *next = Peek(1);
			if (!next) break;

			// If next character is a NL/CR pair,
			if ((char)*next == '\n')
			{
				// Skip it so that next call will not treat it as a blank line
				Skip(1);
			}

			break;
		}
		else if (ch == '\n')
		{
			// Peek at next character
			u8 *next = Peek(1);
			if (!next) break;

			// If next character is a NL/CR pair,
			if ((char)*next == '\r')
			{
				// Skip it so that next call will not treat it as a blank line
				Skip(1);
			}

			break;
		}
		else
		{
			// Copy other characters directly
			*outs++ = ch;
			--len;
			++count;
		}

		data = Read(1);
		if (!data) break;
	}

	*outs = '\0';
	return count;
}
