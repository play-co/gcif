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

#include <iostream>
#include <vector>
using namespace std;

#include "decoder/Enforcer.hpp"
#include "decoder/GCIFReader.h"
using namespace cat;

#include "optionparser.h"


//// Commands

static int decompress(const char *filename) {
	cout << "Decoding input GCIF image file: " << filename << endl;

	int err;

	GCIFImage image;
	if ((err = gcif_read_file(filename, &image))) {
		cout << "Error while decompressing the image: " << gcif_read_errstr(err) << endl;
		return err;
	}

	const int bytes = 4 * image.width * image.height;
	cout << "Image decompressed to buffer " << image.width << "x" << image.height << " : " << bytes << " bytes" << endl;

	return GCIF_RE_OK;
}


//// Command-line parameter parsing

enum  optionIndex { UNKNOWN, HELP, L0, L1, L2, L3, VERBOSE, SILENT, COMPRESS, DECOMPRESS, /*TEST,*/ BENCHMARK, PROFILE, REPLACE };
const option::Descriptor usage[] =
{
  {UNKNOWN, 0,"" , ""    ,option::Arg::None, "USAGE: ./gcif [options] [output file path]\n\n"
                                             "Options:" },
  {HELP,    0,"h", "help",option::Arg::None, "  --[h]elp  \tPrint usage and exit." },
  {UNKNOWN, 0,"" ,  ""   ,option::Arg::None, "\nExamples:\n"
                                             "  ./gcif ./test.gci" },
  {0,0,0,0,0,0}
};

int processParameters(option::Parser &parse, option::Option options[]) {
	if (parse.error()) {
		cout << "Error parsing arguments [retcode:1]" << endl;
		return 1;
	}

	if (parse.nonOptionsCount() != 1) {
		cout << "Input error: Please provide input file paths" << endl;
	} else {
		const char *inFilePath = parse.nonOption(0);
		int err;

		if ((err = decompress(inFilePath))) {
			cout << "Error during conversion [retcode:" << err << "]" << endl;
			return err;
		}

		return 0;
	}

	option::printUsage(std::cout, usage);
	return 0;
}


//// Entrypoint

int main(int argc, const char *argv[]) {

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

	return retval;
}

