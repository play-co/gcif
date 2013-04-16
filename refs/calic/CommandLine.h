//////////////////////////////////////////////////////////////////////
//
// File: cmdline.h
// 
// Description:
//   Contains class CommandLine.
//
//////////////////////////////////////////////////////////////////////

// PROGRAMMER_INFO

#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_STR_LEN
#define MAX_STR_LEN 64
#endif  

class CommandLine {
public:
  int argc;
  char **argv;

  CommandLine(): argc(0), argv(NULL) {}
  CommandLine(int _argc, char **_argv) {
    setArguements(_argc, _argv);
  }
  ~CommandLine();

  void setArguements(int, char **);
  bool getOption(char *);
  bool getParameter(char *, char *);
  bool getParameter(char *, int *);
  bool getParameter(char *, float *);
  bool getParameter(char *, double *);
};

void printUsage(char *usage[]);

#endif __CMDLINE_H__

