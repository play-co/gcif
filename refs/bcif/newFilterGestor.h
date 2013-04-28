/*
 *  newFilterGestor.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class newFilterGestor {
	char **listFilterFunctions;
	char (*pfilter0) (char,char,char,char);
	char (*pfilter1) (char, char, char, char);
	char (*pfilter2) (char, char, char, char);
	char (*pfilter3) (char, char, char, char);
	char (*pfilter4) (char, char, char, char);
	char (*pfilter5) (char, char, char, char);
	char (*pfilter6) (char, char, char, char);
	char (*pfilter7) (char, char, char, char);
	char (*pfilter8) (char, char, char, char);
	char (*pfilter9) (char, char, char, char);
	char (*pfilter10) (char, char, char, char);
	char (*pfilter11) (char, char, char, char);
public:
	newFilterGestor()
	{
		
		pfilter0 = &newFilterGestor::filter0;
		pfilter1 = &newFilterGestor::filter1;
		pfilter2 = &newFilterGestor::filter2;
		pfilter3 = &newFilterGestor::filter3;
		pfilter4 = &newFilterGestor::filter4;
		pfilter5 = &newFilterGestor::filter5;
		pfilter6 = &newFilterGestor::filter6;
		pfilter7 = &newFilterGestor::filter7;
		pfilter8 = &newFilterGestor::filter8;
		pfilter9 = &newFilterGestor::filter9;
		pfilter10 = &newFilterGestor::filter10;
		pfilter11 = &newFilterGestor::filter11;
		cout <<"pfilter11:"<<sizeof(pfilter11)<<endl;		
	}
	
	char** getFilters()
	{
		return listFilterFunctions;
	}
	
	static char filter0(char left, char low, char ll, char lr) {
		int res = 0;
		int l = (int)left;
		int d = (int)low;
		int ld = (int)ll;
		l = l < 0 ? l + 256 : l;
		d = d < 0 ? d + 256 : d;
		ld = ld < 0 ? ld + 256 : ld;
		if (ld >= l && ld >= d) {
			if (l > d) { res = d; } else { res = l; }
		} else if (ld <= l && ld <= d) {
			if (l > d) { res = l; } else { res = d; }
		} else {
			res = d + l - ld;
		}
		return (char)res;
	}
	static char filter1(char left, char low, char ll, char lr) {
		return 0;
	}
	static char filter2(char left, char low, char ll, char lr) {
		return left;
	}
	static char filter3(char left, char low, char ll, char lr) {
		int lw = (int)left;
		int hl = (int)low;
		lw = lw < 0 ? lw + 256 : lw;
		hl = hl < 0 ? hl + 256 : hl;
		int newVal = (lw + hl) >> 1;
		return (char)newVal;
	}
	static char filter4(char left, char low, char ll, char lr) {
		return low;
	}
	static char filter5(char left, char low, char ll, char lr) {
		return ll;
	}
	static char filter6(char left, char low, char ll, char lr) {
		int newValx = (int)left;
		int newValy = (int)low;
		int newValxy = (int)ll;
		newValx = newValx < 0 ? newValx + 256 : newValx;
		newValy = newValy < 0 ? newValy + 256 : newValy;
		newValxy = newValxy < 0 ? newValxy + 256 : newValxy;
		int newVal = newValx + newValy - newValxy;
		if (newVal > 255) { newVal = 255; }
		if (newVal < 0) { newVal = 0;}
		return (char)newVal;
	}
	static char filter7(char left, char low, char ll, char lr) {
		return lr;
	}
	static char filter8(char left, char low, char ll, char lr) {
		int lf = (int)left;
		int llf = (int)ll;
		int lw = (int)low;
		lf = lf < 0 ? lf + 256 : lf;
		llf = llf < 0 ? llf + 256 : llf;
		lw = lw < 0 ? lw + 256 : lw;
		return (char)(lf + ((lw - llf) >> 1));
	}
	static char filter9(char left, char low, char ll, char lr) {
		int lf = (int)left;
		int llf = (int)ll;
		int lw = (int)low;
		lw = lw < 0 ? lw + 256 : lw;
		lf = lf < 0 ? lf + 256 : lf;
		llf = llf < 0 ? llf + 256 : llf;
		return (char)(lw + ((lf - llf) >> 1));
	}
	static char filter10(char left, char low, char ll, char lr) {
		int lf = (int)left;
		int llf = (int)ll;
		int lw = (int)low;
		int hl = (int)lr;
		lw = lw < 0 ? lw + 256 : lw;
		lf = lf < 0 ? lf + 256 : lf;
		llf = llf < 0 ? llf + 256 : llf;
		hl = hl < 0 ? hl + 256 : hl;
		return (char) ( (lf + llf + lw + hl + 1) >> 2);
	}
	static char filter11(char left, char low, char ll, char lr) {
		int lw = (int)low;
		int hl = (int)lr;
		lw = lw < 0 ? lw + 256 : lw;
		hl = hl < 0 ? hl + 256 : hl;
		return (char)((lw + hl) >> 1);
	}
};


