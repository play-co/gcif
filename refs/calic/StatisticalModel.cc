#include "StatisticalModel.h"

void StatisticalModel::reset() {
  for (int i = 0; i < size; i++)
    model[i] = modelCount[i] = 0;
}

int StatisticalModel::write(char *filename) {
  FILE *fp = fopen(filename, "w");

  if (fp == NULL) {
    perror(filename);
    return -1;
  }
  int retValue = write(fp);
  fclose(fp);
  return retValue;
}

int StatisticalModel::write(FILE *fp) {

  fwrite((void *)&size, sizeof(int), 1, fp);
  fwrite((void *)&maxErrorCount, sizeof(int), 1, fp);
  fwrite((void *)model, sizeof(int), size, fp);
  fwrite((void *)modelCount, sizeof(int), size, fp);

  return 0;
}

int StatisticalModel::read(char *filename) {
  FILE *fp = fopen(filename, "r");

  if (fp == NULL) {
    perror(filename);
    return -1;
  }

  int retValue = read(fp);
  if (retValue == -1)
    fprintf(stderr, "%s: short file.\n", filename);

  fclose(fp);
  return retValue;
}

int StatisticalModel::read(FILE *fp) {
  if (fread((void *)&size, sizeof(int), 1, fp) != 1 ||
      fread((void *)&maxErrorCount, sizeof(int), 1, fp) != 1||
      fread((void *)model, sizeof(int), size, fp) != (unsigned int)size ||
      fread((void *)modelCount, sizeof(int), size, fp) != (unsigned int)size)
    return -1;
  return 0;
}

