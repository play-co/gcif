/*
 *  colorFilterer.cpp
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

#include "colorFilterer.h"

char colorFilter0::colFilter(char c0, char c1, char c2, int pos) {
	if (pos == 0) { return 0; } else
		if (pos == 1) { return c0; } else {
			return c1;
		}
}

char colorFilter1::colFilter(char c0, char c1, char c2, int pos) {
	return 0;
}

char colorFilter2::colFilter(char c0, char c1, char c2, int pos) {
	if (pos == 0) { return 0; } else
	return c0;
}

char colorFilter3::colFilter(char c0, char c1, char c2, int pos) {
	if (pos == 2) { return 0; } else {
		return c2;
	}
}

char colorFilter4::colFilter(char c0, char c1, char c2, int pos) {
	if (pos == 2) { return 0; } else
		if (pos == 1) { return c2; } else {
			return (char)(c1 + c2);
		}
}

char colorFilter5::colFilter(char c0, char c1, char c2, int pos) {
	if (pos == 1) { return 0; } else {
		return c1;
	}
}
