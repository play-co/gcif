#include <pnm.h>
#include <CommandLine.h>

#include "CALIC.h"

static char *usage[] = {
  "Usage: encode [options] -in <input: PGM image> -out <output: encoded image>",
  "Options:",
  "  -b 	 	Turn off binary mode.",
  "  -v			Switch to verbose mode.",
  "  -debug 	%d	Switch to debug mode.",
  "                     1: Print minimal debug info.",
  "                     2: Dump error to 'error.out'",
  NULL
};

int main(int argc, char *argv[]) {

  CommandLine cmdline(argc, argv);

  bool binaryMode;
  bool verboseMode;
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

  // if -v is set, then turn on CALIC binary mode.
  verboseMode = (cmdline.getOption("v") == true);

  // if -debug is not set, then set debug level to 0 (no debug).
  if (cmdline.getParameter("debug", &debugLevel) == false)
    debugLevel = 0;

  // -------------------------------------------------------------

  CALICImageCodec *codec = new CALICImageCodec();
  codec->setInputImageFilename(inputFilename);
  codec->setOutputImageFilename(outputFilename);
  codec->setBinaryModeEnabled(binaryMode);
  codec->setDebug(debugLevel > 0);
  codec->setVerbose(verboseMode);

  codec->encode();

  if (codec->isVerbose())
    codec->printStat();

  if (debugLevel > 1) {
    ShortImage * errImg = codec->getErrorImage();
    int w = errImg->w;;
    int h = errImg->h;

    FILE *ofp;

    char errorFilename[256];
    sprintf(errorFilename, "%s.err", outputFilename);

    if ((ofp = fopen(errorFilename, "w")) == NULL) {
      perror(errorFilename);
    }
    else {
      PGMImage *img = new PGMImage(w, h);

      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
          fprintf(ofp, "%d %d   %d\n", x, y, errImg->getPixel(x, y));
          img->setPixel(x, y, (errImg->getPixel(x, y) < 0) ? 0 : 255);
        }

      img->write("error.pgm");	// Dump the error to a PGM file (to visualize the error).
      delete img;

      fclose(ofp);
      printf("Prediction error succesfully recorded in '%s'.\n", errorFilename);
    }
  }

  delete codec;
}

