/*
 *  BmpImage.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

#include <sys/time.h>
 
class BmpImage{
	
	int rgbBlue;
	int rgbGreen;
	int rgbRed;
	char *intest;
	byteMatrix *image;
	char *lineFilter;
	char *colorLineFilter;
	int *advancedColorFilter;
	char *map;
	char *invmap;
	char *tableMap;
	char *newVersion;
	char *newSubVersion;
	int **simbolFreq;
	
	int colorFilterNum;
	int filterNum;
	int filterZoneDim;
	int filterZoneDimBits;
	int colorFilterZoneDim;
	int colorFilterZoneDimBits;
	
	int *cost;
	int *layerCost;
	int * errZones;
	int * mappedVals;
	int defMappedValNum;
	int *defMappedVals;
	
	bool layerXor;
	bool zigzag;
	int filterStep;
	bool imageCheck;
	bool layerCheck;
	
	bool filterA;
	int enStart;
	string filename;
	
	int lossy;
	
	bool newCM;
	
	bool entRep;
	bool console;
	char version;
	char subVersion;
	int *greenerrs;
	int *rederrs;
	int *absValue;
	
	
	int lengthlineFilter;
	int fileSizeForPrint;
	inline void setVars(string fileSource) {       // Function for better constructor readability
		absValue = (int*)calloc(513, sizeof(int));
		for (int i = 0; i <= 512; i++) {
			absValue[i] = i % 256;
		}
		rgbBlue = 0;
		rgbGreen = 0;
		rgbRed = 0;
		intest = NULL;
		image = NULL;
		lineFilter = NULL;
		colorLineFilter = NULL;
		advancedColorFilter = NULL;
		map = NULL;
		invmap = NULL;
		tableMap = NULL;
		newVersion = (char *) calloc(1,sizeof(char));
		newSubVersion = (char *) calloc(1,sizeof(char));
		simbolFreq = NULL;
		
		colorFilterNum = 6;
		filterNum = 12;
		filterZoneDim = 8;
		filterZoneDimBits = 3;
		colorFilterZoneDim = 8;
		colorFilterZoneDimBits = 3;
		
		cost = NULL;
		int arrAppCostLayer[] = { 3, 6, 9, 12, 16, 20, 22, 24 };
		layerCost = (int *) malloc(sizeof(arrAppCostLayer));
		memcpy(layerCost, arrAppCostLayer, sizeof(arrAppCostLayer));
		//free(arrAppCostLayer);
		mappedVals = NULL;
		defMappedValNum = 1;
		
		int appdefMappedVals[] = {0, 1, 2, 3, 4, 5, 6, 8, 7, 9, 10, 
			11, 12, 13, 14, 16, 15, 17, 18, 
			19, 20, 21, 22, 24, 23, 25, 26, 
			27, 28, 29, 30, 32, 31};
		defMappedVals = (int *) malloc(sizeof(appdefMappedVals));
		memcpy(defMappedVals, appdefMappedVals, sizeof(appdefMappedVals));
		//free(appdefMappedVals);
		
		layerXor = false;
		zigzag = true;
		filterStep = 1;
		imageCheck = false;
		layerCheck = false;
		
		filterA = false;
		enStart = 200000;
		filename = fileSource;
		
		lossy = 0;
		
		newCM = false;
		
		entRep = false;
		console = false;
		version = 0;
		subVersion = 39;
	}
	
	
	inline char filterHL(int i, int i2, int i3) {
		int res = 0;
		if (i > 0 && i2 < info.biHeight - 1) {
			res = (int)image->getUpLeftVal();
		} else {
			res = (int)filterBorder(i, i2, i3);
		}
		return (char)res;
	}
	
	inline char filterLR(int i, int i2, int i3) {
		int res = 0;
		if (i < info.biWidth - 1 && i2 > 0) {
			res = (int)image->getRightDownVal();
		} else {
			res = (int)filterBorder(i, i2, i3);
		}
		return (char)res;
	}
	
	inline char filterLowLeft(int i, int i2, int i3) {
		if (i > 0 && i2 > 0) {
			return (char)image->getLowLeftVal();
		} else { return filterBorder(i, i2, i3); }
	}
	
	inline char filterMedO(int i, int i2, int i3) {
		if (i > 0 && i2 > 0 && i2 < info.biHeight - 1) {
			int lw = (int)image->getLeftVal();
			int hl = (int)image->getUpLeftVal();
			lw = lw < 0 ? lw + 256 : lw;
			hl = hl < 0 ? hl + 256 : hl;
			return (char)((lw + hl) >> 1);
		} else { return filterBorder(i, i2, i3); }
	}
	
	inline char filterMedU(int i, int i2, int i3) {
		if (i2 > 0 && i < info.biWidth - 1) {
			int lw = (int)image->getLowVal();
			int hl = (int)image->getRightDownVal();
			lw = lw < 0 ? lw + 256 : lw;
			hl = hl < 0 ? hl + 256 : hl;
			return (char)((lw + hl) >> 1);
		} else { return filterBorder(i, i2, i3); }
	}
	
	inline char filterLinearO(int i, int i2, int i3) {
		if (i > 0 && i2 > 0 && i2 < info.biHeight - 1) {
			int lf = (int)image->getIntVal(i - 1, i2, i3);
			int lw = (int)image->getIntVal(i, i2 - 1, i3);
			int hl = (int)image->getIntVal(i - 1, i2 + 1, i3);
			return (char)(lw + hl - lf);
		} else { return filterLinear(i, i2, i3); }
	}
	
	inline char filterMed(int i, int i2, int i3) {
		char res = 0;
		if (i > 0 && i2 > 0) {
			int lw = (int)image->getLeftVal();
			int hl = (int)image->getLowVal();
			lw = lw < 0 ? lw + 256 : lw;
			hl = hl < 0 ? hl + 256 : hl;
			int newVal = (lw + hl) >> 1;
			res = (char)newVal;
		} else { res = filterBorder(i, i2, i3); }
		return res;
	}
	
	inline char filterPaeth(int i, int i2, int i3) {
		char res = 0;
		if (i > 0 && i2 > 0) {
			int newValx = (int)image->getIntVal(i - 1, i2, i3);
			int newValy = (int)image->getIntVal(i, i2 - 1, i3);
			int newValxy = (int)image->getIntVal(i - 1, i2 - 1, i3);
			int dx = newValx - newValxy;
			int dy = newValy - newValxy;
			int newVal = newValxy + dx  + dy;
			int dl = abs(newVal - newValx);              // Paeth filter
			int dr = abs(newVal - newValy);
			int dll = abs(newVal - newValxy);
			if (dl < dr && dl < dll) {
				newVal = newValx;
			}
			else {
				if (dr < dll) {
					newVal = newValy;
				}
				else {
					newVal = newValxy;
				}
			}
			//if (newVal > 127) { newVal -= 256; }
			res = (char)newVal;
		} else { res = filterBorder(i, i2, i3); }
		return res;
	}
	
	inline char filterLow(int i, int i2, int i3) {
		char res = 0;
		if (i2 > 0) { res = image->getLowVal(); } else { res = filterBorder(i, i2, i3); }
		return res;
	}
	
	inline char filterLeft(int i, int i2, int i3) {
		char res = 0;
		if (i > 0) { res = image->getLeftVal(); } else { res = filterBorder(i, i2, i3); }
		return res;
	}
	
	inline char filterPLO(int i, int i2, int i3) {
		int res = 0;
		if ( i > 0 && i2 > 0 && i2 < info.biHeight - 1) {
			int l = (int)image->getIntVal(i - 1, i2 + 1, i3);
			int d = (int)image->getIntVal(i, i2 - 1, i3);
			int ld = (int)image->getIntVal(i - 1, i2, i3);
			if (ld >= l && ld >= d) {
				if (l > d) { res = d; } else { res = l; }
			} else if (ld <= l && ld <= d) {
				if (l > d) { res = l; } else { res = d; }
			} else {
				res = d + l - ld;
			}
		} else {
			res = filterPL(i, i2, i3);
		}
		return (char)res;
	}
	
	inline char filterPL(int i, int i2, int i3) {
		int res = 0;
		if ( i > 0 && i2 > 0) {	
			int l = (int)image->getLeftVal();
			int d = (int)image->getLowVal();
			int ld = (int)image->getLowLeftVal();
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
		} else {
			res = filterBorder(i, i2, i3);
		}
		return (char)res;
	}
	
	inline char filterNext(int i, int i2, int i3) {
		if (i > 0 && i2 > 0) {
			int lf = (int)image->getLeftVal();
			int ll = (int)image->getLowLeftVal();
			int lw = (int)image->getLowVal();
			lf = lf < 0 ? lf + 256 : lf;
			ll = ll < 0 ? ll + 256 : ll;
			lw = lw < 0 ? lw + 256 : lw;
			return (char)(lf + ((lw - ll) >> 1));
		} else { return filterBorder(i, i2, i3); }
	}
	
    inline char filterBorder(int i, int i2, int i3) {
		if (i == 0 && i2 == 0) {
			return 0;
		} else if (i == 0) {
			return image->getLowVal();
		} else return image->getLeftVal();
    }
public:
	BitmapFileHeader h;
	BitmapInfoHeader info;
	
	BmpImage(string fileSource) {
		setVars(fileSource);
		ifstream fileInput;
		fileInput.open(fileSource.c_str(),ios::in|ios::binary);
		
		if (! fileInput.is_open()) {
			cout << "Input file not found, or cannot open input: " << fileSource << endl;
			exit(1);
		}
		
		// Computing file length
		long begin,end;
		begin = fileInput.tellg();
		fileInput.seekg (0, ios::end);
		end = fileInput.tellg();
		fileInput.seekg (0, ios::beg);
		long fileSize=end-begin;
		
		fileSizeForPrint = fileSize;
		// Read BMP header of file
		fileInput.read((char *) &h, sizeof h);
		fileInput.read((char *) &info, sizeof info);
		
		if (h.bfType1 != 'B' || h.bfType2 != 'M') {
			cout << "ERROR: Not a valid bitmap file: " << fileSource << endl;
			fileInput.close();
			exit(1);
		}
		if (info.biBitCount != 24) {
			cout << "ERROR: Image must have a 24 bit color depth instead of " << info.biBitCount << endl;
			fileInput.close();
			exit(1);
		}
		
		// printBmpInfo();		
		info.biHeight = abs(info.biHeight);
		
		if(h.bfOffBits > 54) {
			printf("WARNING: BMP header longer than expected ! \n");
			printf("Attempting to proceed anyway, but discarding %d header bytes. \n", h.bfOffBits - 54);  
			char *off = (char*)calloc(h.bfOffBits - 54, sizeof(char));
			fileInput.read(off, sizeof(char) * (h.bfOffBits - 54));
		}
		
		// Buffer input image
		char *buffer;
		buffer = (char *) calloc(fileSize, sizeof(char));
		fileInput.read(buffer, fileSize);
		fileInput.close();
		
		int sizeRead = fileSize - sizeof(h) - sizeof(info);
		register int xaxis = 0;
		register int yaxis = 0;
		register int caxis = 0;
		image = new byteMatrix(info.biWidth, info.biHeight, 3);
		register int i = 0;
		int adl = (4 - info.biWidth * 3 % 4) % 4;
		simbolFreq = (int **) calloc(3,sizeof(int *));
		simbolFreq[0] = (int *) calloc(256, sizeof(int));
		simbolFreq[1] = (int *) calloc(256, sizeof(int));
		simbolFreq[2] = (int *) calloc(256, sizeof(int));
		
		image->firstVal();  // May be useless
		
		for (i = 0; i < sizeRead && yaxis < info.biHeight; i++) {
			char val = (char)buffer[i];
			image->setCurVal(val);
			image->nextVal();
			if(val < 0) val =  val + 256;
			caxis ++;
			if (caxis == 3) {
				caxis = 0;
				xaxis++;
				if (xaxis == info.biWidth) {
					if (adl > 0) { i += adl; }
					xaxis = 0;
					yaxis++;
				}
				image->setPoint(xaxis, yaxis, caxis);				
			}
		}
		free(buffer);
	}

	void printBmpInfo() {
    	printf("Type: %c%c \n", h.bfType1, h.bfType2);
    	printf("Size: %d bytes \n", h.bfSize);
    	printf("Offset: %d bytes \n", h.bfOffBits);
    	printf("Bitmap info size: %d bytes \n", info.biSize);
    	printf("Width: %d pixels \n", info.biWidth);
    	printf("Height: %d pixels\n", info.biHeight);
    	printf("Planes (1): %d \n", info.biPlanes);
    	printf("Bits per pixel: %d bits\n", info.biBitCount);
    	printf("Compression: %d \n", info.biCompression);
    	printf("Image size: %d \n", info.biSizeImage);
    	printf("Horizontal resolution: %d \n", info.biXPelsPerMeter);
    	printf("Vertical resolution: %d \n", info.biYPelsPerMeter);
    	printf("Color indexes: %d \n", info.biClrUsed);
    	//printf("Important color indexes (0 is all): %d \n" + info.biClrImportant);
    	//printf("Blue: %d \n" + info.rgbBlue);
    	//printf("Green: %d \n" + info.rgbGreen);
    	//printf("Red: %d \n" + info.rgbRed);
    	printf("\n");
  	}
  	
	// Copy the image
	BmpImage(BmpImage *source, int imgCopy) {
		
		memcpy(&h, &source->h, sizeof(h));
		memcpy(&info, &source->info, sizeof(info));
		
		image = new byteMatrix(info.biWidth, info.biHeight, 3);
		int xaxis = 0;
		int yaxis = 0;
		int caxis = 0;
		int i = 0;
		byteMatrix *f = source->getImage();
		char x;
		while (imgCopy == 1 && yaxis < info.biHeight + 1 ) {
			x = f->getVal(xaxis, yaxis, caxis);
			image->setVal(xaxis, yaxis, caxis, x);
			caxis ++;
			i++;
			if (caxis == 3) {
				caxis = 0;
				xaxis++;
				if (xaxis == info.biWidth) {
					xaxis = 0;
					yaxis++;
				}
			}
		}
	}
	
	BmpImage() {
		cout << "Wrong constructor. " << endl;
		exit(1);
	}
	
	int getResX() {
		return info.biXPelsPerMeter;
	}

	int getResY() {
		return info.biYPelsPerMeter;
	}
	
	void createIntest(int imageWidth, int imageHeight)
	{
		// Header information
		h.bfType1='B';
		h.bfType2='M';
	    int adl = (4 - imageWidth * 3 % 4) % 4;
	    int linewidth = imageWidth * 3 + adl;	
		h.bfSize = linewidth * imageHeight + 54;
		h.bfReserved1 = h.bfReserved2 = 0;
		h.bfOffBits = 54;
		
		// Image header information
		info.biSize = 40;
		info.biWidth = imageWidth;
		info.biHeight = imageHeight;
		info.biPlanes = 1;
		info.biBitCount = 24;
		info.biCompression = 0;
    	info.biSizeImage = h.bfSize - 54;
		info.biClrUsed = 0;
		info.biClrImportant = 0;		
	}
	
	void swapColors(int c1, int c2) {
		for (int i = 0; i < info.biWidth; i ++) {
			for (int i2 = 0; i2 < info.biHeight; i2 ++) {
				int v = image->getVal(i, i2, c1);
				image->setVal(i, i2, c1, image->getVal(i, i2, c2));
				image->setVal(i, i2, c2, v);
			}
		}
	}
	
	void roundBits(int lim) {
		int slim = 1 << (lim - 1);
		int mod = 1 << lim;
		int exL = mod - 1;
		int exH = 255 - exL;
		int newval = 0;
		for (int i = 0; i < info.biWidth; i++) {
			for (int i2 = 0; i2 < info.biHeight; i2 ++) {
				for (int i3 = 0; i3 < 3; i3 ++) {
					newval = image->getIntVal(i, i2, i3);
					if ((newval & exL) > slim) { newval += slim; }
					if (newval > 255) { newval = 255; }
					newval = newval & exH;
					image->setVal(i, i2, i3, newval);
				}
			}
		}
	}
	
	void square(int ix, int iy, int fx, int fy, int ft) {
		if (ft == 1) { square(ix, iy, fx, fy, 100, 100, 100); } else
			if (ft == 2) { square(ix, iy, fx, fy, 200, 200, 200); } else
				if (ft == 3) { square(ix, iy, fx, fy, 250, 0, 0); } else
					if (ft == 4) { square(ix, iy, fx, fy, 0, 250, 250); } else
						if (ft == 5) { square(ix, iy, fx, fy, 0, 0, 250); } else
							if (ft == 6) { square(ix, iy, fx, fy, 0, 250, 0); }
	}
	
	void square(int ix, int iy, int fx, int fy, int c0, int c1, int c2) {
		int squareSide = 1;
		for (int i = ix; i <= fx; i ++) {
			for (int i2 = 0; i2 < squareSide; i2 ++) {
				if (i < info.biWidth && i2 + iy < info.biHeight) {
					image->setVal(i, i2 + iy, 0, c0);
					image->setVal(i, i2 + iy, 0, c1);
					image->setVal(i, i2 + iy, 0, c2);
				}
				if (i < info.biWidth && fy - i2 < info.biHeight) {
					image->setVal(i, fy - i2, 0, c0);
					image->setVal(i, fy - i2, 0, c1);
					image->setVal(i, fy - i2, 0, c2);
				}
			}
		}
		for (int i2 = iy; i2 <= fy; i2 ++) {
			for (int i = 0; i < squareSide; i ++) {
				if (i + ix < info.biWidth && i2 < info.biHeight) {
					image->setVal(i + ix, i2, 0, c0);
					image->setVal(i + ix, i2, 0, c1);
					image->setVal(i + ix, i2, 0, c2);
				}
				if (fx - i < info.biWidth && i2 < info.biHeight) {
					image->setVal(fx - i, i2, 0, c0);
					image->setVal(fx - i, i2, 0, c1);
					image->setVal(fx - i, i2, 0, c2);
				}
			}
		}
	}
	
	byteMatrix* getImage() {
		return image;
	}
	
	int getWidth() {
		return info.biWidth;
	}
	
	char* getZoneFilters() {
		return lineFilter;
	}
	
	int getlengthlineFilter()
	{
		return lengthlineFilter;
	}
	
	char* getColorZoneFilters() {
		return colorLineFilter;
	}
	
	 void setZoneFilters(char *f) {
		 cout << "setZoneFilters called"<<endl;
		lineFilter = f;
	}
	
	 void setColorZoneFilters(char *cf) {
		colorLineFilter = cf;
	}
	
	 char* getInvMap() {
		return invmap;
	}
		
	 int getHeight() {
		return info.biHeight;
	}
	
	int getNewVersion() {
		return newVersion[0];
	}
	
	int getNewSubVersion() {
		return newSubVersion[0];
	}
	
	int getVersion() {
		return version;
	}
	
	int getSubVersion() {
		return subVersion;
	}
	
	void readLineFilters(ifstream *inFile) {
		int zoneNum = (int)(((info.biWidth - 1) / filterZoneDim + 1) * ((info.biHeight - 1) / filterZoneDim + 1));
		int cZoneNum = (int)(((info.biWidth - 1) / colorFilterZoneDim + 1) * ((info.biHeight - 1) / colorFilterZoneDim + 1));
		
		char *b;
		b = (char *) calloc(1,sizeof(char));
			
		lineFilter = (char *) calloc(zoneNum,sizeof(char));
		int filterDim = 0;
		if (filterNum > 1) {
			filterDim = bitMatrix::log2(filterNum - 1) + 1;
		}
			
		bitMatrix *lbm;
		lbm = new bitMatrix(inFile, ((zoneNum * filterDim - 1) >> 3) + 1);
		for (int i = 0; i < zoneNum; i ++) {
			lineFilter[i] = (char)(lbm->fread(filterDim));
		}
			
		colorLineFilter = (char *) calloc(zoneNum,sizeof(char));
		int colorFilterDim = 0;
		if (colorFilterNum > 1) {
			colorFilterDim = bitMatrix::log2(colorFilterNum - 1) + 1;
		}
		bitMatrix *lbmc = new bitMatrix(inFile, ((cZoneNum * colorFilterDim - 1) >> 3) + 1);
		for (int i = 0; i < zoneNum; i ++) {
			colorLineFilter[i] = (char)(lbmc->fread(colorFilterDim));
		}
			
		
	}
	
	void writeLineFilters(ofstream *fout) {
		int sizeLineFilter = 0;
		if (lineFilter == NULL) {
			sizeLineFilter = (((info.biWidth - 1) / filterZoneDim + 1) * ((info.biHeight - 1) / filterZoneDim + 1));
			lineFilter = (char *) calloc(sizeLineFilter,sizeof(char));
		}
		
		int filterDim = 0;
		if (filterNum > 1) {
			filterDim = bitMatrix::log2(filterNum - 1) + 1;
		}
		bitMatrix *lbm;
		lbm = new bitMatrix(sizeLineFilter * filterDim, 0);
		for (int i = 0; i < sizeLineFilter; i++) {
			lbm->fwrite(lineFilter[i], filterDim);
		}
		lbm->writeStream(fout);
		if (colorLineFilter == NULL) {
			colorLineFilter = (char *) calloc((int)(((info.biWidth - 1) / colorFilterZoneDim + 1) * ((info.biHeight - 1) / colorFilterZoneDim + 1)),sizeof(char));
		}
		int colorFilterDim = 0;
		if (colorFilterNum > 1) {
			colorFilterDim = bitMatrix::log2(colorFilterNum - 1) + 1;
		}
		bitMatrix *lbmc;
		lbmc = new bitMatrix(colorFilterDim * colorFilterDim, 0);
		for (int i = 0; i < colorFilterDim; i++) {
			lbmc->fwrite(colorLineFilter[i], colorFilterDim);
		}
		lbmc->writeStream(fout);
	}
	
	
	int bestFil(int i, int i2, int i3) {
		int cur = image->getIntVal(i, i2, i3);
		int left = abs(image->getIntVal(i - 1, i2, i3) - cur);
		int low = abs(image->getIntVal(i, i2 - 1, i3) - cur);
		int ll = abs(image->getIntVal(i - 1, i2 - 1, i3) - cur);
		int lr = abs(image->getIntVal(i + 1, i2 - 1, i3) - cur);
		if (left <= low && left <= ll && left <= lr) {
			return 2;
		} else if (low <= ll && low <= lr) {
			return 4;
		} else if (ll <= lr) {
			return 5;
		} else {
			return 7;
		}
	}
	
	/**
	 * Applies the filters to the image, from right to left and from high to low.
	 */
	
	void applyFilter();
	void applyFilter(bool boolean);
	
	void applyColFilter();
	
	int getZoneNum() {
		int x = (int)(((info.biWidth - 1) / colorFilterZoneDim + 1) * ((info.biHeight - 1) / colorFilterZoneDim + 1));
		return x;
	}
	
	void applyAdvancedColFilter();
	
	void removeColFilter();
	
	void removeBothFilters();
	
	void removeFilter();
	
	void setDefaultMap() {
		invmap = new char[256];
		for (int i = 0; i < 256; i++) {
			invmap[i] = (char)i;
		}
	}
	
	int colorFilterOfZone(int i, int i2) {
		int xZones = (int)(((info.biWidth - 1) >> colorFilterZoneDimBits) + 1);
		int zoneRef = (int)((i >> colorFilterZoneDimBits) + xZones * (i2 >> colorFilterZoneDimBits));
		return colorLineFilter[zoneRef];
	}
	
	int filterOfZone(int i, int i2) {
		int xZones = (int)(((info.biWidth - 1) >> filterZoneDimBits) + 1);
		int zoneRef = (int)((i >> filterZoneDimBits) + xZones * (i2 >> filterZoneDimBits));
		return lineFilter[zoneRef];
	}
	
    /**
     * Removes the filters, the color filters and the remapping from the image.
     * This is more efficent than calling the single methods sequencially. To
     * work correctly, this method presumes that all the filters have been applied
     * and the information about filters and color filters has been read with the
     * readZoneFilters and readColorZoneFilters functions.
     */
	
	void removeAllFilters();
	
	int getIntVal(int x, int y, int color) {
		return image->getIntVal(x, y, color);
	}
	
	void fillCost() {
		cost = (int *) calloc(256, sizeof(int));
		
		for (int i = 0; i < 256; i ++) {
			if (i < 128) {
				int cur = i;
				for (int i2 = 0; i2 < 7; i2++) {
					if ( (cur & (1 << i2)) != 0) {
						cost[i] += layerCost[i2 + 1];
					}
				}
			} else {
				cost[i] = 50 + ((i - 128) >> 2);
				
			}
		}
	}
	
	inline void calcFilErrs(int val, int left, int low, int ll, int dr, int x, int y, int* res) {
		val = abs256(val);
		left = abs256(left);
		low = abs256(low);
		ll = abs256(ll);
		dr = abs256(dr);
		res[1] = val;
		res[2] = val - left;
		res[3] = val - ((left + low) >> 1);
		res[4] = val - low;
		res[5] = val - ll;
		int lp = (left + low - ll);
		if (lp < 0) { lp = 0; } else if (lp > 255) { lp = 255; }
		res[6] = val - lp;
		res[7] = val - dr;
		res[8] = val - (left + ((low - ll) >> 1));
		res[9] = val - (low + ((left - ll) >> 1));
		res[10] = val - ((low + left + ll + dr + 1) >> 2);
		res[11] = val - ((dr + low) >> 1);
		if (ll >= left && ll >= low) {
			if (left > low) { res[0] = res[4]; } else { res[0] = res[2]; }
		} else if (ll <= left && ll <= low) {
			if (left > low) { res[0] = res[2]; } else { res[0] = res[4]; }
		} else {
			res[0] = res[6];
		}
		if (x == 0) {
			if (y == 0) {
				for (int i = 0; i < 12; i++) { res[i] = val; }
			} else {
				int bres = val - low;
				res[0] = bres; res[3] = bres; res[5] = bres; res[6] = bres; res[7] = bres;
				res[2] = bres; res[8] = bres; res[9] = bres; res[10] = bres; res[11] = bres;
			}
		}
		if (y == 0 && x > 0) {
			int bres = val - left;
			res[2] = bres;
			res[0] = bres; res[3] = bres; res[5] = bres; res[6] = bres;
			res[8] = bres; res[9] = bres; res[10] = bres; res[11] = bres; res[4] = bres;
		}
		if (x >= info.biWidth - 1 && y > 0) {
			int bres = val - left;
			res[7] = bres; res[10] = bres; res[11] = bres;
		}
	}	
	
	void filterDeterminate();
	
	void colorFilterDeterminate();
	
	/*void detZoneErrs() {
		int zoneNum = (int)(((info.biWidth - 1) / colorFilterZoneDim + 1) * ((info.biHeight - 1) / colorFilterZoneDim + 1));
		int val;
		int totErr = 0;
		errZones = new int[zoneNum];
		image->firstVal();
		for (int i = 0; i < info.biWidth; i ++) {
			for (int i2 = 0; i2 < info.biHeight; i2 ++) {
				for (int i3 = 0; i3 < 3; i3++) {
					totErr += abs(image->getCurVal());
					image->nextVal();
				}
			}
		}
		int medErr = totErr / zoneNum;
		int minx = 0;
		int miny = 0;
		int maxx = colorFilterZoneDim - 1;
		int maxy = colorFilterZoneDim - 1;
		for (int i5 = 0; i5 < zoneNum; i5 ++) {
			int curErr = 0;
			for (int i3 = miny; i3 <= maxy; i3 ++) {
				for (int i2 = minx; i2 <= maxx; i2 ++) {
					image->setPoint(i2, i3, 0);
					for (int i4 = 0; i4 < 3; i4++) {
						val = image->getCurVal();
						curErr += abs(val);
						image->nextVal();
					}
				}
			}
			if (curErr > medErr) {
				errZones[i5] = 1;
			} else {
				errZones[i5] = 0;
			}
			minx += colorFilterZoneDim;
			if (minx > info.biWidth - 1) {
				minx = 0;
				miny += colorFilterZoneDim;
			}
			maxx = (int)min(minx + colorFilterZoneDim - 1, info.biWidth - 1);
			maxy = (int)min(miny + colorFilterZoneDim - 1, info.biHeight - 1);
		}
		minx = 0;
		miny = 0;
		maxx = colorFilterZoneDim - 1;
		maxy = colorFilterZoneDim - 1;
		for (int i5 = 0; i5 < zoneNum; i5 ++) {
			for (int i3 = miny; i3 <= maxy; i3 ++) {
				for (int i2 = minx; i2 <= maxx; i2 ++) {
					image->setPoint(i2, i3, 0);
					for (int i4 = 0; i4 < 3; i4++) {
						if (errZones[i5] == 1) {
							image->setCurVal((char)200);
						}
						image->nextVal();
					}
				}
			}
			minx += colorFilterZoneDim;
			if (minx > info.biWidth - 1) {
				minx = 0;
				miny += colorFilterZoneDim;
			}
			maxx = (int)min(minx + colorFilterZoneDim - 1, info.biWidth - 1);
			maxy = (int)min(miny + colorFilterZoneDim - 1, info.biHeight - 1);
		}
	}*/
	
	int * getZoneErrs() {
		return errZones;
	}
	
	inline int abs256 (int a) {
		return absValue[a + 256];
	}
	
	inline void calcFilColErrs(int p0, int p1, int p2, int **res) {
		int ap0 = abs256(p0);
		int ap1 = abs256(p1);
		int ap2 = abs256(p2);
		int *t = res[0];
		t[0] = ap0;                    // Changed from res[][]
		t[1] = ap0;
		t[2] = ap0;
		t[3] = abs256(p0 - p2);
		t[4] = abs256(p0 - p1);
		t[5] = abs256(p0 - p1); t = res[1];
		t[0] = abs256(p1 - p0);
		t[1] = ap1;
		t[2] = abs256(p1 - p0);
		t[3] = abs256(p1 - p2);
		t[4] = abs256(p1 - p2);
		t[5] = ap1;             t = res[2];
		t[0] = abs256(p2 - p1);
		t[1] = ap2;
		t[2] = abs256(p2 - p0);
		t[3] = ap2;
		t[4] = ap2;
		t[5] = abs256(p2 - p1);
	}
	
	/*
	 * Estimates the value of a point of the image using a linear interpolation with the points immediately
	 * left, low and lower left. Called these lf, lw and ll the formula is lf + lw - ll.
	 * @param i The x of the point to extimate
	 * @param i2 The y of the point to extimate
	 * @param i3 The color of the point to extimate
	 * @return The guess of the value
	 */
	
	char filterLinear(int i, int i2, int i3) {
		char res = 0;
		if (i > 0 && i2 > 0) {
			int newValx = image->getLeftVal();
			int newValy = image->getLowVal();
			int newValxy = image->getLowLeftVal();
			newValx = abs256(newValx);
			newValy = abs256(newValy);
			newValxy = abs256(newValxy);
			int dx = newValx - newValxy;
			int dy = newValy - newValxy;
			int newVal = newValxy + dx  + dy;
			if (newVal > 255) { newVal = 255; }
			if (newVal < 0) { newVal = 0;}
			res = (char)newVal;
		} else {
			res = filterBorder(i, i2, i3);
		}
		return res;
	}
	
	char filterNextNext(int i, int i2, int i3) {
		if (i > 0 && i2 > 0) {
			int lf = image->getLeftVal();
			int ll = image->getLowLeftVal();
			int lw = image->getLowVal();
			lw = lw < 0 ? lw + 256 : lw;
			lf = lf < 0 ? lf + 256 : lf;
			ll = ll < 0 ? ll + 256 : ll;
			return (char)(lw + ((lf - ll) >> 1));
		} else { return filterBorder(i, i2, i3); }
	}
	
	char filterNextNextNext(int i, int i2, int i3) {
		if (i > 0 && i2 > 0 && i2 < info.biHeight - 1) {
			int lf = image->getLeftVal();
			int ll = image->getLowLeftVal();
			int lw = image->getLowVal();
			int hl = image->getUpLeftVal();
			lw = lw < 0 ? lw + 256 : lw;
			lf = lf < 0 ? lf + 256 : lf;
			ll = ll < 0 ? ll + 256 : ll;
			hl = hl < 0 ? hl + 256 : hl;
			return (char) ( (lf + ll + lw + hl + 1) >> 2);
		} else { return filterBorder(i, i2, i3); }
	}
	
	char filterNextNextNextRU(int i, int i2, int i3) {
		if (i > 0 && i2 > 0 && i < info.biWidth - 1) {
			int lf = image->getLeftVal();
			int ll = image->getLowLeftVal();
			int lw = image->getLowVal();
			int hl = image->getRightDownVal();
			lw = lw < 0 ? lw + 256 : lw;
			lf = lf < 0 ? lf + 256 : lf;
			ll = ll < 0 ? ll + 256 : ll;
			hl = hl < 0 ? hl + 256 : hl;
			return (char) ( (lf + ll + lw + hl + 1) >> 2);
		} else { return filterBorder(i, i2, i3); }
	}
	
	
	
	/**
	 * Estimates the value of a point of the image using the specified filter.
	 * @param i The x of the point to extimate
	 * @param i2 The y of the point to extimate
	 * @param i3 The color of the point to extimate
	 * @param fn The number of the filter to use
	 * @return The guess of the value
	 */
	
	char filter(int i, int i2, int i3, int fn) {
		char res = 0;
		if (fn == 0) { res = filterPL(i, i2, i3); }
		else if (fn == 2) { res = filterLeft(i, i2, i3); }
		else if (fn == 3) { res = filterMed(i, i2, i3); }
		else if (fn == 4) { res = filterLow(i, i2, i3); }
		else if (fn == 5) { res = filterLowLeft(i, i2, i3); }
		else if (fn == 6) { res = filterLinear(i, i2, i3); }
		else if (fn == 7) { res = filterLR(i, i2, i3); }
		else if (fn == 8) { res = filterNext(i, i2, i3); }
		else if (fn == 9) { res = filterNextNext(i, i2, i3); }
		else if (fn == 10) { res = filterNextNextNextRU(i, i2, i3); }
		else if (fn == 11) { res = filterMedU(i, i2, i3); }
		else if (fn == 12) { res = filterPaeth(i, i2, i3); }
		else if (fn == 13) { res = filterLinearO(i, i2, i3); }
		else if (fn == 14) { res = filterPLO(i, i2, i3); }
		return (char)res;
	}
	
	int filter(int i, int i2, int i3, int fn,bool boolean) {
		return fn;
	}
	
	
	/**
	 * Guesses the value of the point of the image calling the filter(int i, int i2, int i3, int fn) method
	 * with the appropriate values depending on the filters chosen by the filterDeterminate call. If no
	 * call to this method has been done, the linear filter is used for all the image
	 * @param i The x of the point
	 * @param i2 The y of the point
	 * @param i3 The color of the point
	 * @return The guessed value
	 */
	
	char filter(int i, int i2, int i3) {
		int xZones = (int)(((info.biWidth - 1) >> filterZoneDimBits) + 1);
		int zoneRef = (int)((i >> filterZoneDimBits) + xZones * (i2 >> filterZoneDimBits));
		if (lineFilter == NULL) {
			return filter(i, i2, i3, 5);
		} else {
			return filter(i, i2, i3, lineFilter[abs(zoneRef)]);
		}
	}
	
	void reportDist(int bitNum);
	
	/**
	 * Computes an hash code using every point of the image-> Intestation is not used
	 * @return The hash code
	 */
	
	int hashCode() {
		long res = 0;
		for (int i = 0; i < info.biWidth; i ++) {
			for (int i2 = 0; i2 < info.biHeight; i2++) {
				for (int i3 = 0; i3 < 3; i3++) {
					res += image->getIntVal(i, i2, i3) * (i + 1) * (i2 + 1) * (i3 + 1);
				}
			}
		}
		return (int)res;
	}
	
    /*
     * Sets use of the zig zag visit order in RLE and Huffman layer compression.
     * @param zz True to activate zig zag order, false to deactivate it
     */
	
	void setZigzag(bool zz) {
		zigzag = zz;
	}
	
    /**
     * Sets the maximum number of filters to be used for compression
     * @param fn The maximum number of used filters
     */
	
	void setFilterNum (int fn) {
		filterNum = fn;
	}
	
    /**
     * Sets the maximum number of color filters to be used for compression
     * @param cfn The maximum number of used color filters
     */
	
	void setColorFilterNum (int cfn) {
		//cout << "called setColorFilterNum"<<endl;
		colorFilterNum = cfn;
	}
	
    /**
     * Sets the number of used pixels in filter and color filter determination.
     * Points used will be one every n for the specified argument.
     * @param fs The number of point from where one point will be considered
     * for filter determination
     */
	
	void setFilterStep(int fs) {
		filterStep = fs;
	}
	
    /**
     * Sets layer comparison during compression or off.
     * @param lx True to set comparison on, false to set it off.
     */
	
	void setLayerXor(bool lx) {
		layerXor = lx;
	}
	
    /**
     * Sets the dimension of a filtering zone.
     * @param zd The side of the square representing a filtering zone, in pixels
     */
	
	void setZoneDim(int zd) {
		filterZoneDim = zd;
		filterZoneDimBits = bitMatrix::log2(zd - 1) + 1;
	}
	
    /**
     * Enables or disables hash check after image decompression to verify correct
     * image reconstruction.
     * @param hc True to enable check, false to disable it
     */
	
	void setHashCheck(bool hc) {
		imageCheck = hc;
	}
	
    /**
     * Enables or disables hash check after decompression of every layer
     * to verify correct image reconstruction.
     * @param lc True to enable check, false to disable it
     */
	
	void setLayerCheck(bool lc) {
		layerCheck = lc;
	}
	
    /**
     * Sets the dimension of a color filtering zone.
     * @param czd The side of the square representing a color filtering zone,
     * in pixels
     */
	
	void setColorZoneDim(int czd) {
		colorFilterZoneDim = czd;
		colorFilterZoneDimBits = bitMatrix::log2(czd - 1) + 1;
	}
	
	
    /**
     * Writes the remapping vector on the specified output stream.
     * @param out The output stream where to write.
     */
	
	void writeMap(ofstream *fout) {
		if (map == NULL) {
			map = new char[256];
			for (int i = 1; i < 256; i++) {
				map[i] = (char)i;
			}
		}
		fout->write(map,256);
	}
	
    /**
     * Reads the remapping vector from the specified input stream.
     * @param in The input stream where to read from.
     */
	
	void readMap(ifstream *fin) {
		map = new char[256];
		fin->read(map,256);
		invmap = new char[256];
		for (int i = 0; i < 256; i++) {
			char cur = map[i];
			invmap[cur >= 0 ? cur : cur + 256] = (char)(i >= 0 ? i : i + 256);
		}
	}
	
    /**
     * Inverts remapping of the image values. If remapping array has not been read
     * with the readMap method, this function will do nothing
     */
	
	void invRemap() {
		if (invmap != NULL) {
			for (int i = 0; i < info.biWidth; i++) {
				for (int i2 = 0; i2 < info.biHeight; i2++) {
					for (int i3 = 0; i3 < 3; i3++) {
						int cur = image->getIntVal(i, i2, i3);
						image->setVal(i, i2, i3, (char)invmap[cur >= 0 ? cur : cur + 256]);
					}
				}
			}
		}
	}
	
	
	void bitprint(char b);
	
    /**
     * Returns the value of a specified color filter for a point.
     * @param x The horizontal coordinate of the point
     * @param y The vertical coordinate of the point
     * @param z The color coordinate of the point
     * @param cf The number of the color filter to use
     * @return The extimated value
     */
	
	char colorFilter(int x, int y, int z, int cf) {
		if (cf == 0) { return cFilterPrec(x, y, z); } else
		if (cf == 2) { return cFilterFirst(x, y, z); } else
		if (cf == 5) { return cFilter1(x, y, z); } else
		if (cf == 6) { return cFilter10(x, y, z); } else
		if (cf == 7) { return cFilter20(x, y, z); } else
		if (cf == 8) { return cFilter21(x, y, z); } else
		if (cf == 3) { return cFilter3(x, y, z); } else
		if (cf == 4) { return cFilter31(x, y, z); } else
		return 0;
	}
	
    /**
     * Returns the value of the color filter for a point associated
     * with a point
     * @param x The horizontal coordinate of the point
     * @param y The vertical coordinate of the point
     * @param z The color coordinate of the point
     * @return The extimated value
     */
	
	char colorFilter(int x, int y, int z) {
		int xZones = (int)(((info.biWidth - 1) >> colorFilterZoneDimBits) + 1);
		int zoneRef = (int)((x >> colorFilterZoneDimBits) + xZones * (y >> colorFilterZoneDimBits));
		return colorFilter(x, y, z, colorLineFilter[zoneRef]);
	}
	
	char cFilterPrec(int x, int y, int z) {
		if (z == 0) { return 0; } else
			return (char)image->getCurVal(/*x, y, z*/ - 1);
	}
	
	char cFilterFirst(int x, int y, int z) {
		if (z == 0) { return 0; } else
			return (char)image->getCurVal(/*x, y,*/ -z);
	}
	
	char cFilter1(int x, int y, int z) {
		if (z == 1) { return 0; } else {
			return (char)image->getCurVal(1 - z);
		}
	}
	
	char cFilter10(int x, int y, int z) {
		if ((z == 0) | (z == 2)) { return 0; } else
			return (char)image->getCurVal(/*x, y,*/ -1);
	}
	
	char cFilter20(int x, int y, int z) {
		if ((z == 0) | (z == 1)) { return 0; } else
			return (char)image->getCurVal(/*x, y,*/ -2);
	}
	
	char cFilter21(int x, int y, int z) {
		if ((z == 0) | (z == 1)) { return 0; } else
			return (char)image->getCurVal(/*x, y,*/ -1);
	}
	
	char cFilter3(int x, int y, int z) {
		if (z == 2) { return 0; } else
			return (char)image->getCurVal(/*x, y,*/ 2 - z);
	}
	
	char cFilter31(int x, int y, int z) {
		if (z == 2) { return 0; } else
			if (z == 1) { return (char)image->getCurVal(1); }
			else {
				return (char)(image->getCurVal(1) + image->getCurVal(2));
			}
	}
	
	char cFilterHigh(int x, int y, int z) {
		if (z == 0) { return 0; } else
			if (z == 1) { 
				return cFilterPrec(x, y, z); 
			} else {
				int v0 = getIntVal(x, y, 0);
				return (char) ((v0 + getIntVal(x, y, 1)) >> 1);
			}
	}
	
	/**
	 * Activates or deactivates the console. While console is set to true, during compression and
	 * decompression a detailed log of the algorithm's operations will be wrote on standard output.
	 * @param c The new value of console.
	 */
	
	void setConsole(bool c) {
		console = c;
	}
	
	void println() {
		if (console) { cout << endl; }
	}
	
	void println(string arg) {
		if (console) { cout << arg << endl; }
	}
	
	void print(string arg) {
		if (console) { cout << arg << endl; }
	}
	
};
