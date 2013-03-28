#include "ImageWriter.hpp"
#include "EndianNeutral.hpp"
#include "MappedFile.hpp"
using namespace cat;


const char *ImageWriter::ErrorString(int err) {
	switch (err) {
		case WE_OK:			// No error
			return "No errors";
		case WE_BAD_DIMS:	// Image dimensions are invalid
			return "Image dimensions are invalid";
		case WE_FILE:		// Unable to access file
			return "Unable to access the file";
		default:
			break;
	}

	return "Unknown error code";
}


//// WriteVector

void WriteVector::clear() {
	int words = HEAD_SIZE;
	u32 *ptr = _head;

	// For each rope,
	while (ptr) {
		u32 *nextPtr = *reinterpret_cast<u32**>( ptr + words );

		delete []ptr;

		ptr = nextPtr;
		words <<= 1;
	}

	_head = 0;
	_work = 0;
}

void WriteVector::grow() {
	const int newAllocated = _allocated << 1;

	// If initializing,
	u32 *newWork = new u32[newAllocated + PTR_WORDS];

	// Point current "next" pointer to new workspace
	*reinterpret_cast<u32**>( _work + newAllocated ) = newWork;

	// Set "next" pointer to null
	*reinterpret_cast<u32**>( newWork + newAllocated ) = 0;

	// Update class state
	_work = newWork;
	_allocated = newAllocated;
	_used = 0;
}

void WriteVector::init(u32 hashSeed) {
	clear();

	_hash.init(hashSeed);

	u32 *newWork = new u32[HEAD_SIZE + PTR_WORDS];
	_head = _work = newWork;

	_used = 0;
	_allocated = HEAD_SIZE;
	_size = 0;

	// Set "next" pointer to null
	*reinterpret_cast<u32**>( newWork + HEAD_SIZE ) = 0;
}

void WriteVector::write(u32 *target) {
	u32 *ptr = _head;

	// If any data to write at all,
	if (ptr) {
		int words = HEAD_SIZE;
		u32 *nextPtr = *reinterpret_cast<u32**>( ptr + words );

		// For each full rope,
		while (nextPtr) {
			memcpy(target, ptr, words * WORD_BYTES);
			target += words;

			ptr = nextPtr;
			words <<= 1;
		}

		// Write final partial rope
		memcpy(target, ptr, _used * WORD_BYTES);
	}
}


//// ImageWriter

int ImageWriter::init(int width, int height) {
	// Validate

	if ((width & 7) | (height & 7)) {
		return WE_BAD_DIMS;
	}

	width >>= 3;
	height >>= 3;

	if (width > 65535 || height > 65535) {
		return WE_BAD_DIMS;
	}

	// Initialize

	_words.init(ImageReader::DATA_SEED);

	_info.width = static_cast<u16>( width );
	_info.height = static_cast<u16>( height );

	_work = 0;
	_bits = 0;

	return WE_OK;
}

void ImageWriter::writeBitPush(u32 code) {
	const u32 pushWord = _work | (code << 31);

	_words.push(pushWord);

	_work = 0;
	_bits = 0;
}

void ImageWriter::writeBitsPush(u32 code, int len, int available) {
	const int shift = len - available;

	const u32 pushWord = _work | (code >> shift);

	_words.push(pushWord);

	if (shift) {
		_work = code << (32 - shift);
	} else {
		_work = 0;
	}

	_bits = shift;
}

int ImageWriter::finalizeAndWrite(const char *path) {
	MappedFile file;

	// Calculate file size
	int wordCount = _words.getWordCount();
	int totalBytes = (ImageReader::HEAD_WORDS + wordCount) * sizeof(u32);

	// Map the file

	if (!file.OpenWrite(path, totalBytes)) {
		return WE_FILE;
	}

	MappedView fileView;

	if (!fileView.Open(&file)) {
		return WE_FILE;
	}

	u8 *fileData = fileView.MapView();

	if (!fileData) {
		return WE_FILE;
	}

	u32 *fileWords = reinterpret_cast<u32*>( fileData );

	// Finalize the bit data

	if (_bits) {
		_words.push(_work);
	}

	u32 dataHash = _words.finalizeHash();

	// Write header

	MurmurHash3 hh;
	hh.init(ImageReader::HEAD_SEED);

	fileWords[0] = getLE(ImageReader::HEAD_MAGIC);
	hh.hashWord(ImageReader::HEAD_MAGIC);

	u32 header1 = (_info.width << 16) | _info.height; // Temporary
	fileWords[1] = getLE(header1);
	hh.hashWord(header1);

	fileWords[2] = getLE(dataHash);
	hh.hashWord(dataHash);

	u32 headHash = hh.final(ImageReader::HEAD_WORDS);
	fileWords[3] = getLE(headHash);

	fileWords += ImageReader::HEAD_WORDS;

	_info.headHash = headHash;
	_info.dataHash = dataHash;

	// Copy file data

	_words.write(fileWords);

	return WE_OK;
}

