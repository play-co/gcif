/*
 *  filterer.cpp
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

#include "filterer.h"

char filter0::filter(char left, char low, char ll, char lr) {
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
char filter1::filter(char left, char low, char ll, char lr) {
	return 0;
}
char filter2::filter(char left, char low, char ll, char lr) {
	return left;
}
char filter3::filter(char left, char low, char ll, char lr) {
	int lw = (int)left;
	int hl = (int)low;
	lw = lw < 0 ? lw + 256 : lw;
	hl = hl < 0 ? hl + 256 : hl;
	int newVal = (lw + hl) >> 1;
	return (char)newVal;
}
char filter4::filter(char left, char low, char ll, char lr) {
	return low;
}
char filter5::filter(char left, char low, char ll, char lr) {
	return ll;
}
char filter6::filter(char left, char low, char ll, char lr) {
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
char filter7::filter(char left, char low, char ll, char lr) {
	return lr;
}
char filter8::filter(char left, char low, char ll, char lr) {
	int lf = (int)left;
	int llf = (int)ll;
	int lw = (int)low;
	lf = lf < 0 ? lf + 256 : lf;
	llf = llf < 0 ? llf + 256 : llf;
	lw = lw < 0 ? lw + 256 : lw;
	return (char)(lf + ((lw - llf) >> 1));
}
char filter9::filter(char left, char low, char ll, char lr) {
	int lf = (int)left;
	int llf = (int)ll;
	int lw = (int)low;
	lw = lw < 0 ? lw + 256 : lw;
	lf = lf < 0 ? lf + 256 : lf;
	llf = llf < 0 ? llf + 256 : llf;
	return (char)(lw + ((lf - llf) >> 1));
}
char filter10::filter(char left, char low, char ll, char lr) {
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
char filter11::filter(char left, char low, char ll, char lr) {
	int lw = (int)low;
	int hl = (int)lr;
	lw = lw < 0 ? lw + 256 : lw;
	hl = hl < 0 ? hl + 256 : hl;
	return (char)((lw + hl) >> 1);
}
