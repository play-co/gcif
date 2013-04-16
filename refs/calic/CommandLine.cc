//////////////////////////////////////////////////////////////////////
//
// File: cmdline.cc
//
// Description:
//   Implementation of methods of class CommandLine.
//
//////////////////////////////////////////////////////////////////////

// PROGRAMMER_INFO

#include "CommandLine.h"

CommandLine::~CommandLine() {
  if (argv != NULL) {
    for (int i = 0; i < argc; i++)
      delete argv[i];
    delete argv;
  }
}

//////////////////////////////////////////////////////////////////////
//
// Method: CommandLine::getOption()
// Description:
//   To check if a specific option has been specified in the command line.
// 
// Parameter:
//   str: option name to be checked.
// Return:
//   If the option is found, this method returns True. 
//   Otherwise it returns False.
//
//////////////////////////////////////////////////////////////////////
bool CommandLine::getOption(char *str) {
    
  for (int i = 1; i < argc; i++)
    if (argv[i][0] == '-' && (strcmp(argv[i] + 1, str) == 0)) 
      return true;
  return false;
}

//////////////////////////////////////////////////////////////////////
//
// Method: CommandLine::getParameter()
// Description:
//   To retrieve a value corresponds to an option in the command line.
//   The format must be as follow:
//     -option value
//   This value can be string, int, float, or double.
//   
// Parameter:
//   str: option name to be checked.
//   var: to return the value corresponds to the option specified.
// Return:
//   Upon success, 0 will be returned and the value read from the command
//   line will be stored in "var. Otherwise it returns false.
//
//////////////////////////////////////////////////////////////////////
bool CommandLine::getParameter(char *str, char *var) {

  for (int i = 1; i < argc - 1; i++)
    if (argv[i][0] == '-' && (strcmp(argv[i] + 1, str) == 0)) {
      strcpy(var, argv[i+1]);
      return true;
    }
  return false; 
}

bool CommandLine::getParameter(char *str, int *var) {

  for (int i = 1; i < argc - 1; i++)
    if (argv[i][0] == '-' && (strcmp(argv[i] + 1, str) == 0)) {
      *var = atoi(argv[i+1]);
      return true;
    }
  return false;
}

bool CommandLine::getParameter(char *str, float *var) {

  for (int i = 1; i < argc - 1; i++)
    if (argv[i][0] == '-' && (strcmp(argv[i] + 1, str) == 0)) {
      *var = (float) atof(argv[i+1]);
      return true;
    }
  return false;
}

bool CommandLine::getParameter(char *str, double *var) {

  for (int i = 1; i < argc - 1; i++)
    if (argv[i][0] == '-' && (strcmp(argv[i] + 1, str) == 0)) {
      *var = (double) atof(argv[i+1]);
      return true;
    }
  return false;
}

//////////////////////////////////////////////////////////////////////
//
// Method: CommandLine::Init()
// Description:
//   Make a copy of the command line arguements that are passed to main().
//
// Parameters:
//   _argc: # of arguements.
//   _argv: list of arguements.
//
//////////////////////////////////////////////////////////////////////
void CommandLine::setArguements(int _argc, char *_argv[]) {

    argc = _argc;

    argv = new char * [argc];
    for (int i = 0; i < argc; i++) {
       argv[i] = new char [MAX_STR_LEN];
       strcpy(argv[i], _argv[i]);
    }
}

void printUsage(char *usage[]) {
  for (int i = 0; usage[i] != NULL; i++)
    fprintf(stderr, "%s\n", usage[i]);
}

