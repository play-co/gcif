#include "Arithm.h"
#include <stdio.h>

AdaptiveModel::AdaptiveModel() {
  numb_symb = 0;
  freq = cum_freq = index_to_symb = symb_to_index = NULL;
}

AdaptiveModel::~AdaptiveModel() {
  delete [] freq;
}

void AdaptiveModel::init(int ns) {

  int i, cum;

  if ((ns < 2) || (ns > MaxSymbols)) {
    fprintf(stderr, "invalid < Adaptive_Model > definition\n");
    exit(1);
  }

  if (numb_symb != ns) {
    delete [] freq;

    numb_symb = ns;  
    i = ns + 1;
    freq = new int [4 * i];

    if (freq == NULL) {
      Error("< Adaptive_Model >: insufficient memory");
    }

    cum_freq = freq + i;  
    symb_to_index = cum_freq + i;
    index_to_symb = symb_to_index + i; 
  }

  for (i = 0; i < ns; i++) {
    symb_to_index[i] = i + 1;  
    index_to_symb[i+1] = i; 
  }

  freq[0] = 0;
  for (i = 1; i <= ns; i++)
    freq[i] = 1;

  for (cum = 0, i = ns; i >= 0; i--) {
    cum_freq[i] = cum;  
    cum += freq[i]; 
  }
}

void AdaptiveModel::reset() {
  init(numb_symb);
}

void AdaptiveModel::updateInterval(int symb, long *low, long *high) {
  int index;
  long range;

  if ((symb < 0) || (symb >= numb_symb))
    Error("invalid < Adaptive_Model > symbol");

  index = symb_to_index[symb];  
  range = *high - *low + 1;

  *high = *low + (range * cum_freq[index-1]) / cum_freq[0] - 1;
  *low = *low + (range * cum_freq[index]) / cum_freq[0];

  update(index);
}

void AdaptiveModel::update(int index) {
  int i, cum, symb_i, symb_index;

  if (cum_freq[0] == MaxFrequency) {
    for (cum = 0, i = numb_symb; i >= 0; i--) {
      freq[i] = (freq[i] + 1) >> 1;
      cum_freq[i] = cum;  cum += freq[i]; 
    }
  }

  for (i = index; freq[i] == freq[i-1]; i--) ;

  if (i < index) {
    symb_i = index_to_symb[i];
    symb_index = index_to_symb[index];
    index_to_symb[i] = symb_index;
    index_to_symb[index] = symb_i;
    symb_to_index[symb_i] = index;
    symb_to_index[symb_index] = i; }

  freq[i]++;
  while (i) 
    cum_freq[--i]++;
}

int AdaptiveModel::selectSymbol(long value, long *low, long *high) {

  int index, cum, symbol;
  long range;

  if ((value < *low) || (value > *high))
    Error("invalid < Adaptive_Model > value");

  range = *high - *low + 1;
  cum = (int) (((value - *low + 1) * cum_freq[0] - 1) / range);

  for (index = 1; cum_freq[index] > cum; index++);

  *high = *low + (range * cum_freq[index-1]) / cum_freq[0] - 1;
  *low = *low + (range * cum_freq[index]) / cum_freq[0];

  symbol = index_to_symb[index];
  update(index);

  return symbol;
}

int AdaptiveModel::write(char *filename) {

  FILE *fp = fopen(filename, "w");
  if (fp == NULL) {
    perror(filename);
    return -1;
  }
  write(fp);
  fclose(fp);
  return 0;
}

int AdaptiveModel::write(FILE *fp) {
  int i = numb_symb + 1;
  fwrite((void *)&numb_symb, sizeof(int), 1, fp);
  fwrite((void *)freq, sizeof(int), 4 * i, fp);
  return 0;
}

int AdaptiveModel::read(char *filename) {

  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    perror(filename);
    return -1;
  }

  if (read(fp) == -1) {
    fprintf(stderr, "%s: read error\n", filename);
    return -1;
  }

  fclose(fp);
  return 0;
}

int AdaptiveModel::read(FILE *fp) {
  if (fread((void *)&numb_symb, sizeof(int), 1, fp) != 1) {
    fprintf(stderr, "< Adaptive_Model >: fail to read from data file\n");
    return -1;
  }
   
  delete [] freq;
  int i = numb_symb + 1;
  freq = new int [4 * i];

  if (freq == NULL) {
    Error("< Adaptive_Model >: insufficient memory");
  }

  unsigned int n = 4 * i;
  if (fread((void *)freq, sizeof(int), n, fp) != n) {
    fprintf(stderr, "< Adaptive_Model >: fail to read from data file\n");
    return -1;
  }

  cum_freq = freq + i;
  symb_to_index = cum_freq + i;
  index_to_symb = symb_to_index + i;

  return 0;
}

AdaptiveModel *AdaptiveModel::clone() {
  AdaptiveModel *model = new AdaptiveModel();
  model->numb_symb = numb_symb;
  int i = numb_symb + 1;

  model->freq = new int [4 * i];

  if (model->freq == NULL) {
    Error("< Adaptive_Model >: insufficient memory");
  }

  memcpy((void *)(model->freq), (void *)freq, 4 * i * sizeof(int));
  cum_freq = freq + i;
  symb_to_index = cum_freq + i;
  index_to_symb = symb_to_index + i;

  return model;
}

void AdaptiveModel::copy(AdaptiveModel *model) {
  ASSERT(model != NULL, "NULL pointer.");
  numb_symb = model->numb_symb;
  int i = numb_symb + 1;
  memcpy((void *)freq, (void *)(model->freq), 4 * i * sizeof(int));
  cum_freq = freq + i;
  symb_to_index = cum_freq + i;
  index_to_symb = symb_to_index + i;
}

