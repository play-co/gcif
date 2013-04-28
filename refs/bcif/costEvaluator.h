/*
 *  costEvaluator.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */ 

class costEvaluator{
	int *ccc;
	int **freqs;
	int *totFreqs;
	int *cost;
	int *fCost;
	bool enCalc;
	int sc;
	int passedZones;
	int zoneLim;
	int enZoneLim;
	int sizeFreqs;
	
	inline void setVars()
	{
		freqs = NULL;
		totFreqs = NULL;
		cost = NULL;
		fCost = NULL;

		int app[] = {3, 6, 9, 12, 15, 18, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32, 33, 34, 35, 32, 38, 41, 47, 36, 42, 45, 51, 48, 54, 57, 63, 22, 28, 31, 37, 34, 40, 43, 49, 38, 44, 47, 53, 50, 56, 59, 65, 42, 48, 51, 57, 54, 60, 63, 69, 58, 64, 67, 73, 70, 76, 79, 85, 24, 30, 33, 39, 36, 42, 45, 51, 40, 46, 49, 55, 52, 58, 61, 67, 44, 50, 53, 59, 56, 62, 65, 71, 60, 66, 69, 75, 72, 78, 81, 87, 46, 52, 55, 61, 58, 64, 67, 73, 62, 68, 71, 77, 74, 80, 83, 89, 66, 72, 75, 81, 78, 84, 87, 93, 82, 88, 91, 97, 94, 100, 103, 109, 110};
		ccc = (int *) malloc (sizeof(app));
		memcpy(ccc, app, sizeof(app));
		
		enCalc = true;
		sc = 0;
		passedZones = 0;
		zoneLim = 20;
		enZoneLim = 10;
	}
public:
	
	int* enFillCost(int *f) {
		cost = (int *) calloc(256, sizeof(int));
		int tot = 0;
		for (int i = 0; i < 256; i++) {
			if (f[i] == 0) { f[i] = 1; }
			tot += f[i];
		}
		for (int i = 0; i < 256; i++) {
			float freq = (float)f[i] / tot;
			cost[i] = -(int)round(log(freq) / log(2));
		}
		return cost;
	}

	costEvaluator() { setVars(); }
	
	costEvaluator(int fn) {
		setVars();
		sizeFreqs = fn;
		freqs = (int **) calloc(sizeFreqs, sizeof(int *));
		for(int i=0;i<sizeFreqs;i++)
		{
			freqs[i] = (int *) calloc(256, sizeof(int));
		}
		totFreqs = (int *) calloc(256, sizeof(int));
	}
	
	int* getCosts() {
		for (int i = 7; i < 129; i++) {
			ccc[i] = 22 + ((i - 7) >> 1);
		}
		return ccc;
	}
	
	int** getColCosts(int **f) {
		int **res = (int **) calloc(3,sizeof(int*));
		res[0] = ccc;
		res[1] = ccc;
		res[2] = ccc;		
		return res;
	}
	
	int putVal(int fn, int val) {
        freqs[fn][val]++;
		if (! enCalc) {
			fCost[fn] += cost[val];
		}
		if( val > 128 ) val = 256 -val;
		return ccc[val];
	}
	
	int getFilCost(int fn) {
		if (enCalc) {
			int *nf = (int *) malloc(256*sizeof(int));
			for (int i = 0; i < 256; i++) {
				nf[i] = totFreqs[i] + freqs[fn][i];
			}
			double enf = entropy(nf,256);
			return (int)(1000 * enf);
		} else {
			return fCost[fn];
		}
	}
	
	void remSignalSel(int fn) {
		for (int i2 = 0; i2 < sizeFreqs; i2 ++) {
			freqs[i2] = (int *) calloc(256, sizeof(int));
		}
	}
	
	void signalSel(int fn) {
		for (int i = 0; i < 256; i++) {
			totFreqs[i] = totFreqs[i] + freqs[fn][i];
		}
		if (fCost == NULL) { fCost = (int *) calloc(256, sizeof(int)); }
		memset(fCost, 0, sizeFreqs*sizeof(int));
		for (int i2 = 0; i2 < sizeFreqs; i2 ++) {
			memset(freqs[i2], 0, 256 * sizeof(int));
		}
		passedZones ++;
		if (! enCalc && (passedZones == zoneLim)) {
			fillCosts();
			if (zoneLim < 1000) { zoneLim = zoneLim << 1; } else {
				zoneLim += 1000;
			}
		}
		if (enCalc && (passedZones == enZoneLim)) {
			remEnCalc();
		}
	}
	
	void remEnCalc() {
		enCalc = false;
		fillCosts();
	}
	
	void fillCosts() {
		cost = (int *) calloc(256, sizeof(int));
		fCost = (int *) calloc(256, sizeof(int)); 
		int total = 0;
		for (int i = 0; i < 256; i++) {
			total += totFreqs[i];
		}
		for (int i = 0; i < 256; i++) {
			if (totFreqs[i] == 0) {
				cost[i] = total;
			} else {
				cost[i] = total / totFreqs[i];
			}
			cost[i] = (int)(round(log(cost[i]) / log(2)));	
		}
	}
	
	double entropy(int *simbolsFreq, int length) {
		double e = 0;
		long tot = 0;
		for (int i = 0; i < length; i ++) {
			tot += simbolsFreq[i];
		}
		double *relFreq = (double *) calloc(length, sizeof(double));
		for (int i = 0; i < length; i++) {
			relFreq[i] = (double)simbolsFreq[i] / tot;
		}
		for (int i = 0; i < length; i ++) {
			if (relFreq[i] > 0) {
				e -= relFreq[i] * (log(relFreq[i]) / log(2));
			}
		}
		return e;
	}
};
