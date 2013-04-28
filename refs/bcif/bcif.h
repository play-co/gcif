/*
 *  bcif.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class newBitReader;
class bmpWriter;
class filterer;
class HTreeReaderGestor;

class bcif{
	int maxDecisions;
	int filterZoneDim;
	int filterZoneDimBits;
	int colorFilterZoneDim;
	int colorFilterZoneDimBits;
	int width;
	int heigth;
	bool hashPrint;
	bool writeParts;
	int *loga2, *loga2d;          // Lookup tables
	bool advancedColorFilter;
	
	ofstream debug;
	bool fdebug;
	int* HTRtypes;                // Lookup tables for Huffman codes
	int* HTRKeyNum;
	int** HTRLookupsLeft;
	int** HTRLookupsRight;
	int** HTRLookupsValues;
	int** HTRLookupsAuxLeft;
	int** HTRLookupsAuxRight;
	int** HTRLookupsAuxValues;
	int* HTRzzeros;
	int* HTRzmaxzeroseq;
	int* HTRzafter;
	
	int** FLEnd;
	int** FLBits;
	
	int** FLAuxEnd;
	int** FLAuxBits;
	
	int FLbitnum;
	
	huffmanReader ** hreaders;
	int verbose;
	
	int leftInfo, lowInfo, caos, caos2, curCaos;
	int* mod256a;
	
public:
	int version;
	int subVersion;
	int beta;
	bcif()
	{
		verbose = 0;
		FLbitnum = 8;      
		maxDecisions = 64;
		filterZoneDim = 8;
		filterZoneDimBits = 3;
		colorFilterZoneDim = 8;
		colorFilterZoneDimBits = 3;
		version = 1;
		subVersion = 0;
		beta = 1;
		hashPrint = false;
		writeParts = false;
		// Base 2 logarithm, extednded with log2(0) = -1
		int app[]={-1,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9};
		loga2 = (int *) calloc (1024,sizeof(int));
		loga2d = (int *) calloc (1024,sizeof(int));
		
		fdebug = false;
		if(fdebug) debug.open("debug.txt",ios::out|ios::trunc);
		memcpy(loga2,app,1024*sizeof(int));
		memcpy(loga2d,app,1024*sizeof(int));
		for (int ldi = 0; ldi < 1024; ldi++) {
			if (loga2d[ldi] > 6) {
				loga2d[ldi] = 6;
			}
		}
		mod256a = (int *)calloc(256, sizeof(int));
		for (int ldi = 0; ldi < 256; ldi++) {
			if (ldi > 128) {
				mod256a[ldi] = 256 - ldi;
			} else {
				mod256a[ldi] = ldi;
			}
		}
	
		advancedColorFilter = false;
	}
	
	~bcif(){
	
	}
	
	void setVerbose(int v) {
		verbose = v;
	}
	
	//debugMethod
	void writetoDebug(int val) {
		debug << val << endl;
	}
	
	void writetoDebug(int index,int val) {
		debug << index << " " << val << endl;
	}
	
	void closeDebug() {
		debug.close();
	}
	
	
	void decompress(string source);
	
	void hdecompress(newBitReader *br, bmpWriter *bmpOut);
	void hdecompressBetter(newBitReader *br, bmpWriter *bmpOut);
	void readFilters(newBitReader *br, char *res, int filterDim);
	
	char* readAdvancedColorFilters(newBitReader *br, int w, int h, int filterZoneDim);
	char* readColorFilters(newBitReader *br, int w, int h, int filterZoneDim);
	char filterOfZone(int i, int i2, int width, char *lineFilter);
	char colorFilterOfZone(int i, int i2, int width, char *colorLineFilter);
	inline int decide(int x, int y, int col, int **precInfo, int **info, int curFil, int curColFil, int left, int low);
	char safeFilter(int x, int y, char left, char low, char ll, char lr, int curFil, filterer *fil, int width);
	char safeFilter(int x, int y, char left, char low[], int curFil, filterer *fil, int width);
	int mod256(int val);
	
	void hdecompressBetterInt(newBitReader *br, bmpWriter *bmpOut);
	int filterOfZoneInt(int i, int i2, int width, int *lineFilter);
	int colorFilterOfZoneInt(int i, int i2, int width, int *colorLineFilter);
	void readFilters(newBitReader *br, int *res, int filterDim);
	int* readColorFiltersInt(newBitReader *br, int w, int h, int filterZoneDim);
	int safeFilter(int x, int y, int left, int low, int ll, int lr, int curFil, filterer *fil, int width);
	int safeFilter(int x, int y, int left, int low[], int curFil, filterer *fil, int width);
	
	inline int getHReader(HTreeReaderGestor *hrg, newBitReader *br, int where);
	void decompress(newBitReader *br, bmpWriter *bmpOut);
	inline int decided(int x, int y, int col, int *precInfo, int *info, int *pcinfo, int left, int low);
	inline int decideSafe0(int x, int y, int *precInfo, int *info, int left, int low);
	inline int decideSafe12(int x, int y, int col, int *precInfo, int *info, int *pcinfo, int left, int low);
	
	void compress(string source,string dest);
	void compress(string source);
	void writeFilters(char *filters,int length, binaryWrite *bw);
	void writeColFilters(char *colorFilters,int length, binaryWrite *bw);
	void compressMatrix(byteMatrix *image, binaryWrite *out, char *filters, char *colorFilters);
	int decideMatrix(int x, int y, int col, int **precInfo, int **info, int curFil, int curColFil, int left, int low);
	int readHVal(newBitReader *bm, int *lkey, int start);
	int readZHVal(newBitReader *bm, int *lkey, int *alkey, int &after, int &zeros, int maxZeroSeq, int where);
	int readVal(newBitReader *br, int where);
	void genFastLookup(int **left, int **right, int** vals, int lnum, int *zseq, int* types, int isaux);
	void printHTree(HTree *ht, int keynum, int ind);
	void createFastLookups(int num, huffmanReader **hreaders);
	int readFLHVal(newBitReader *bm, int *lkey, int* end, int* bits);
	void fillFLookups(HTree *curht, int* curend, int FLsize, int i, int nowaux);
	void readBcifIntest(newBitReader *br, fileBmpWriter *bmpOut);
	void writeBcifHeader(BmpImage *si, binaryWrite *bw);
	void decompress(string source, string dest);
};
