/*
 *  colorFilterer.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class colorFilterer{
public:
	virtual char colFilter(char c0, char c1, char c2, int pos)=0;
};
class colorFilter0 : public colorFilterer {
public:
	char colFilter(char c0, char c1, char c2, int pos);
};
class colorFilter1 : public colorFilterer {
public:
	char colFilter(char c0, char c1, char c2, int pos);
};
class colorFilter2 : public colorFilterer {
public:
	char colFilter(char c0, char c1, char c2, int pos);
};
class colorFilter3 : public colorFilterer {
public:
	char colFilter(char c0, char c1, char c2, int pos);
};
class colorFilter4 : public colorFilterer {
public:
	char colFilter(char c0, char c1, char c2, int pos);
};
class colorFilter5 : public colorFilterer {
public:
	char colFilter(char c0, char c1, char c2, int pos);
};
