/*
 *  main.cpp
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

#include <stdlib.h> 
#include <string.h> 
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <vector>
#include <sstream>

using namespace std;

#ifdef WIN32
#include <stdio.h> 
#include <sys/time.h>
#else 
#ifdef UNIX
#include <unistd.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#endif 
#endif

#include <dirent.h>


#include "newBitReader.h"
#include "bmpWriter.h"
#include "fileBmpWriter.h"
#include "binaryWrite.h"
#include "HTree.h"
#include "huffmanReader.h"
#include "zeroHuffmanReader.h"
#include "oneValHuffmanReader.h"
#include "standardHuffmanReader.h"
#include "HTreeReaderGestor.h"
#include "newFilterGestor.h"

#include "HTreeWriterGestor.h"

#include "readHeader.h"
#include "bitMatrix.h"
#include "byteMatrix.h"
#include "costEvaluator.h"
#include "BmpImage.h"

#include "bcif.h"


static void printBcifIntro(bcif *b) {
    printf("\n");
    printf("    BCIF lossless image compression algorithm version %d.%d", b->version, b->subVersion);
    if (b->beta == 0) { printf("\n"); }
    if (b->beta == 1) { printf(" beta.\n"); }
    if (b->beta == 2) { printf(" alpha.\n"); }
    if (b->beta > 2) { printf(" unstable pre-alpha, level %d. \n", b->beta); }
    printf("    By Stefano Brocchi (stefano.brocchi@researchandtechnology.net) \n");
    printf("    Website: www.researchandtechnology.net/bcif/ \n\n");
}

void displayBcifHelp() {
	printf("BCIF syntax: \n \n");
	printf("  bcif inputfile [-c|-d] [outputfile] [-h]\n\n");
	printf("Parameters: \n\n");
	printf("-c         : Compress   (BMP -> BCIF)\n");
	printf("-d         : Decompress (BCIF -> BMP)\n");
	printf("-h         : Print help information\n");
	printf("inputfile  : The file to be compressed or decompressed\n");
	printf("outputfile : The destination file. If not specified, default is the same \n");
	printf("             filename of inputfile with an appropriate extension. If already \n");
	printf("             existing, the outputfile is overwritten without prompting.\n\n");
}

int exeParams(int argc, char **args, bcif *encoder) {
    char *sourceFile = NULL;
    char *destFile = NULL;
    string errString = "";
    bool ok = true;
    bool help = false;
    bool compress = false;
    bool actionDef = false;
    bool decompress = false;
    bool hashCode = false;
    for (int i = 1; i < argc; i++) {
      char *cur = args[i];
      if (cur[0] != '-') {
        if (sourceFile == NULL) { sourceFile = cur; } else
          if (destFile == NULL) { destFile = cur; } else {
            ok = false; 
            errString.append("Unrecognized parameter, or extra filename: ");
            errString.append(cur);
            errString.append(" \n");
          }
      } else {
        cur++;
        if (cur[0] == 'c' && cur[1] == (char)0) {
          if (actionDef) {
	          ok = false;
	          errString.append("Select only one of the options -c (compress) or -d (decompress).\n");
          }
	      compress = true;
          actionDef = true;
        } else if (cur[0] == 'd' && cur[1] == (char)0) {
          if (actionDef) {
	          ok = false;
	          errString.append("Select only one of the options -c (compress) or -d (decompress).\n");
          }
          decompress = true;
          actionDef = true;
        } else if (cur[0] == 'h' && cur[1] == 'c' && cur[2] == (char)0) {              // Not implemented
          hashCode = true;
        } else if (cur[0] == 'h' && cur[1] == (char)0) {
          help = true;
        } else if (cur[0] == 'v' && cur[1] == (char)0) {
          encoder->setVerbose(1);
        } else {
          ok = false;
          errString.append("Unrecognized option: ");
          errString.append(cur);
          errString.append(" \n");          
          printf("None\n");
        }
      }
    }
    if (sourceFile == NULL && help == false) {
      ok = false;
      errString.append("No source file specified ! Use -h for help \n");
    }
    if (actionDef == false && help == false) {
      ok = false;
      errString.append("No action specified ! \n");
      errString.append("Specify -c (compress, BMP -> BCIF) \n");
      errString.append("     or -d (decompress, BCIF -> BMP)\n");
      errString.append("     or -h for help \n");
    }    
    if (! ok) {
	    printf("%s", errString.c_str());
	    return 1;
    } else {
	    if (help) { displayBcifHelp(); return 0; } else {
		    if (compress) {
			    if (destFile != NULL) {
				    encoder->compress(*(new string(sourceFile)), *(new string(destFile)));
			    } else {
				    encoder->compress(*(new string(sourceFile)));
			    }
		    } else if (decompress) {
			    if (destFile != NULL) {
				    encoder->decompress(*(new string(sourceFile)), *(new string(destFile)));
			    } else {
				    encoder->decompress(*(new string(sourceFile)));
			    }			    
		    }
			return 0;
		}
	}
}

int main (int argc, char **argv) {
	
	bcif encoder;
	printBcifIntro(&encoder);
	return exeParams(argc, argv, &encoder);

}


