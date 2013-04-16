#include "BinaryModeModel.h"
#include <math.h>

void BinaryModeModel::update(int index) {
//  if (incr == 1)
    AdaptiveModel::update(index); 
/*
  else {
    int i, cum, symb, new_freq;

    if (cum_freq[0] >= MaxFrequency) {
      for (cum = 0, i = numb_symb; i >= 0; i--) {
        freq[i] = (freq[i] + 1) >> 1;
        cum_freq[i] = cum;  
        cum += freq[i]; 
      }
      incr = (incr + 1) / 2;
    }
  
    new_freq = freq[index] + incr;
    symb = index_to_symb[index];

    int tmp_idx, tmp_symb;
    for (i = index; new_freq >= freq[i-1] && i > 1; i--) {
      freq[i] = freq[i-1];
      tmp_symb = index_to_symb[i-1];
      index_to_symb[i] = tmp_symb;
      symb_to_index[tmp_symb] = i;
    }

    freq[i] = new_freq;

    if (i < index) {
      index_to_symb[i] = symb;
      symb_to_index[symb] = i;
    }

    while (i) 
      cum_freq[--i] += incr;
  }
*/
}

void BinaryModeModel::init(int ns) {
  AdaptiveModel::init(ns);
  incr = 8192;
}


void ContinuousModeModel::init(int ns) {
  AdaptiveModel::init(ns);

  int i, cum;

  int freqCount = (MaxFrequency * 2 / 4);
  double *freqPercentage = new double[ns];

  double sum = 0.0;
  for (int i = 0; i < ns; i++) {
    freqPercentage[i] = pow(15.0, 0.4 * (double)(-i-1));
    sum += freqPercentage[i];
  }

  for (int i = 0; i < ns; i++) {
    freqPercentage[i] /= sum;
  }

  for (i = 1; i <= ns; i++) {
    freq[i] = (int)((double)freqCount * freqPercentage[i-1]);
    if (freq[i] == 0)
      freq[i] = 1;
  }

  for (cum = 0, i = ns; i >= 0; i--) {
    cum_freq[i] = cum;
    cum += freq[i];
  }

  delete [] freqPercentage;
}

