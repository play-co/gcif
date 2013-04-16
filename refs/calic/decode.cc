#include <pnm.h>
#include <CommandLine.h>

#include "CALIC.h"

static char *usage[] = {
  "Usage: encode [options] -in <input: encoded image> -out <output: PGM image>",
  "Options:",
  "  -b 	 	Turn off binary mode.",
  "  -debug 	%d	Switch to debug mode.",
  "                     1: Print minimal debugging info.",
  NULL
};

int main(int argc, char *argv[]) {

  CommandLine cmdline(argc, argv);

  bool binaryMode;
  int debugLevel;
  char inputFilename[256], outputFilename[256];

  // --------------- Parse command line parameters --------------

  if (cmdline.getParameter("in", inputFilename) == false ||
      cmdline.getParameter("out", outputFilename) == false) {
    printUsage(usage);
    exit(1);
  }

  // if -b is not set, then turn on CALIC binary mode.
  binaryMode = (cmdline.getOption("b") == false);

  // if -debug is not set, then set debug level to 0 (no debug).
  if (cmdline.getParameter("debug", &debugLevel) == false)
    debugLevel = 0;

  // -------------------------------------------------------------

  CALICImageCodec *codec = new CALICImageCodec();
  codec->setInputImageFilename(inputFilename);
  codec->setOutputImageFilename(outputFilename);
  codec->setBinaryModeEnabled(binaryMode);
  codec->setDebug(debugLevel > 0);

  codec->decode();

  delete codec;
}

