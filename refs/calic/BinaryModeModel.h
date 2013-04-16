#ifndef __BINARYMODEMODEL_H__
#define __BINARYMODEMODEL_H__

#include "Arithm.h"

class BinaryModeModel: public AdaptiveModel {

  int incr;

public:
  BinaryModeModel() : AdaptiveModel() {}
  ~BinaryModeModel() {}

  virtual void init(int _numb_symb);

  friend class ArithmeticEncoder;
  friend class ArithmeticDecoder;

protected:

//  int selectSymbol(long symb, long *low, long *high);
//  void updateInterval(int symb, long *low, long *high);
  virtual void update(int index);

};

// Line AdaptiveModel except it starts the model with a lapacian-liked 
// distribution of the symbol.
class ContinuousModeModel: public AdaptiveModel {
public:
  ContinuousModeModel() : AdaptiveModel() {}
  ~ContinuousModeModel() {}

  friend class ArithmeticEncoder;
  friend class ArithmeticDecoder;

  virtual void init(int ns);
};

#endif

