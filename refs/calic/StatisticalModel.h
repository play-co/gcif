#ifndef __STATISTICALMODEL_H__
#define __STATISTICALMODEL_H__

#include <stdio.h>

class StatisticalModel {
public:
  int maxErrorCount;            // When the # of errors modeled by a context reaches this
                                // value, halve both the accumulated errors and the count
                                // correspond to that context. (See pp. 441, eqn. 9)

  int size;                     // Total # of contexts used to model the error.

  int *model;                   // Keep the contexts that are used to model the errors.
                                // Essentially, each entry in the table is used to
                                // accumulate the error values.

  int *modelCount;              // Keep the number of errors modeled (accumulated) by each
                                // 'model' entry.

  StatisticalModel(int _size, int _maxErrorCount) {
    size = _size;
    maxErrorCount = _maxErrorCount;
    model = new int [size];
    modelCount = new int [size];
    reset();
  }

  ~StatisticalModel() {
    delete [] model;
    delete [] modelCount;
  }

  void reset(void);
  void update(int context, int error) {
    model[context] += error;
    modelCount[context] ++;

    if (modelCount[context] == maxErrorCount) {
      model[context] /= 2;
      modelCount[context] /= 2;
    }
  }

  int getExpectation(int context) {
    if (modelCount[context] > 0)
      return (model[context] + modelCount[context]/2) /
             modelCount[context];

    return 0;
  }

  bool isNegative(int context) {
    return (model[context] < 0);
  }

  int write(char *filename);
  int read(char *filename);

  int write(FILE *fp);
  int read(FILE *fp);

};

#endif

