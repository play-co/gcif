#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mrp.h"

extern POINT dyx[];
extern double sigma_h[], sigma_a[];        
extern double qtree_prob[];

uint getbits(FILE *fp, int n)
{
    static int bitpos = 0;
    static uint bitbuf = 0;
    int x = 0;

    if (n <= 0) return (0);
    while (n > bitpos) {
	n -= bitpos;
	x = (x << bitpos) | bitbuf;
	bitbuf = getc(fp) & 0xff;
	bitpos = 8;
    }
    bitpos -= n;
    x = (x << n) | (bitbuf >> bitpos);
    bitbuf &= ((1 << bitpos) - 1);
    return (x);
}

DECODER *init_decoder(FILE *fp)
{
    DECODER *dec;
    int i;

    dec = (DECODER *)alloc_mem(sizeof(DECODER));
    if (getbits(fp, 16) != MAGIC_NUMBER) {
	fprintf(stderr, "Not a compressed file!\n");
	exit(1);
    }
    dec->version = getbits(fp, 8);
    dec->width = getbits(fp, 16);
    dec->height = getbits(fp, 16);
    dec->maxval = getbits(fp, 16);
    dec->num_comp = getbits(fp, 4);
    dec->num_class = getbits(fp, 6);
    dec->num_group = getbits(fp, 6);
    dec->prd_order = getbits(fp, 7);
    dec->num_pmodel = getbits(fp, 6) + 1;
    dec->coef_precision = getbits(fp, 4) + 1;
    dec->pm_accuracy = getbits(fp, 3) - 1;
    dec->f_huffman = getbits(fp, 1);
    dec->quadtree_depth = (getbits(fp, 1))? QUADTREE_DEPTH : -1;
    dec->maxprd = dec->maxval << dec->coef_precision;
    i = dec->prd_order * 3 + 4;
    dec->predictor = (int **)alloc_2d_array(dec->num_class, i, sizeof(int));
    dec->err = (int **)alloc_2d_array(dec->height, dec->width, sizeof(int));
    dec->ctx_weight = init_ctx_weight();
    if (dec->quadtree_depth > 0) {
	int x, y, xx, yy;
        yy = (dec->height + MAX_BSIZE - 1) / MAX_BSIZE;
        xx = (dec->width + MAX_BSIZE - 1) / MAX_BSIZE;
	for (i = dec->quadtree_depth - 1; i >= 0; i--) {
            dec->qtmap[i] = (char **)alloc_2d_array(yy, xx, sizeof(char));
	    for (y = 0; y < yy; y++) {
		for (x = 0; x < xx; x++) {
		    dec->qtmap[i][y][x] = 0;
		}
	    }
            yy <<= 1;
            xx <<= 1;
	}
    }
    dec->class = (char **)alloc_2d_array(dec->height, dec->width,
					 sizeof(char));
    dec->uquant = (char **)alloc_2d_array(dec->num_class, MAX_UPARA + 1,
					  sizeof(char));
    if (dec->num_pmodel > 1) {
        dec->pm_idx = (int *)alloc_mem(dec->num_group * sizeof(int));
    } else {
        dec->pm_idx = NULL;
    }
    dec->spm.freq = alloc_mem((MAX_SYMBOL * 2 + 1) * sizeof(uint));
    dec->spm.cumfreq = &(dec->spm.freq[MAX_SYMBOL]);
    if (dec->f_huffman == 1) {
	dec->sigma = sigma_h;
    } else {
	dec->sigma = sigma_a;
    }
    dec->mtfbuf = (int *)alloc_mem(dec->num_class * sizeof(int));
    return (dec);
}

int decode_vlc(FILE *fp, VLC *vlc)
{
    int i, k, min, off;
    uint code;

    code = min = off = k = 0;
    for (i = 0; i < vlc->max_len; i++) {
	code = (code << 1) | getbits(fp, 1);
	k = vlc->off[i];
	if (k < 0) {
	    min <<= 1;
	} else {
	    if (code <= vlc->code[vlc->index[k]]) break;
	    min = (vlc->code[vlc->index[k]] + 1) << 1;
	    off = k + 1;
	}
    }
    i = off + code - min;
    return (vlc->index[i]);
}

int decode_golomb(FILE *fp, int m)
{
    int v = 0;
    while (getbits(fp, 1) == 0) {
	v++;
    }
    v = (v << m) | getbits(fp, m);
    return (v);
}

void decode_predictor(FILE *fp, DECODER *dec)
{
    int k, m, cl, coef, sgn, *coef_p, *scany_p, *scanx_p, *size_p;

    if (dec->f_huffman == 1) {
	for (k = 0; k < dec->prd_order; k++) {
	    m = getbits(fp, 4);
	    for (cl = 0; cl < dec->num_class; cl++) {
		coef = decode_golomb(fp, m);
		if (coef > 0) {
		    sgn = getbits(fp, 1);
		    if (sgn) {
			coef = -coef;
		    }
		}
		dec->predictor[cl][k] = coef;
	    }
	}
    } else {
	PMODEL *pm;

	pm = &dec->spm;
	pm->size = MAX_COEF + 18;
	pm->cumfreq[MAX_COEF + 2] = 0;
	for(k = MAX_COEF + 2; k < pm->size; k++) {
	    pm->freq[k] = 1;
	    pm->cumfreq[k + 1] = pm->cumfreq[k] + pm->freq[k];
	}
	for (k = 0; k < dec->prd_order; k++) {
	    m = rc_decode(fp, dec->rc, pm, MAX_COEF + 2, MAX_COEF + 18)
		- (MAX_COEF + 2);
	    set_spmodel(pm, MAX_COEF + 1, m);
	    for (cl = 0; cl < dec->num_class; cl++) {
		coef = rc_decode(fp, dec->rc, pm, 0, MAX_COEF + 1);
		if (coef > 0) {
		    sgn = rc_decode(fp, dec->rc, pm, MAX_COEF+2, MAX_COEF+4)
			    - (MAX_COEF + 2);
		    if (sgn) {
			coef = -coef;
		    }
		}
		dec->predictor[cl][k] = coef;
	    }
	}
    }
    for (cl = 0; cl < dec->num_class; cl++) {
	coef_p = dec->predictor[cl];
	scany_p = coef_p + dec->prd_order;
	scanx_p = scany_p + dec->prd_order;
	size_p = scanx_p + dec->prd_order;
	size_p[1] = size_p[2] = size_p[3] = 0;
	for (k = m = 0; k < dec->prd_order; k++) {
	    if (coef_p[k] != 0) {
		coef_p[m] = coef_p[k];
		scany_p[m] = dyx[k].y;
		scanx_p[m] = dyx[k].x;
		if (dyx[k].y < size_p[1]) size_p[1] = dyx[k].y;
		if (dyx[k].x < size_p[2]) size_p[2] = dyx[k].x;
		if (dyx[k].x > size_p[3]) size_p[3] = dyx[k].x;
		m++;
	    }
	}
	size_p[0] = m;
	size_p[1] = -size_p[1];
	size_p[2] = -size_p[2];
	size_p[3] = dec->width - size_p[3];
    }
    return;
}

void decode_threshold(FILE *fp, DECODER *dec)
{
    int cl, gr, m, u, k;

    if (dec->f_huffman == 1) {
	m = getbits(fp, 4);
	for (cl = 0; cl < dec->num_class; cl++) {
	    k = u = 0;
	    for (gr = 0; gr < dec->num_group; gr++) {
		if (k > MAX_UPARA || gr == dec->num_group - 1) {
		    k = MAX_UPARA + 1;
		} else {
		    if (getbits(fp, 1)) k += decode_golomb(fp, m) + 1;
		}
		for (; u < k; u++) dec->uquant[cl][u] = gr;
	    }
	}
	if (dec->num_pmodel > 1) {
	    for (k = 1; (1 << k) < dec->num_pmodel; k++);
	    for (gr = 0; gr < dec->num_group; gr++) {
		dec->pm_idx[gr] = getbits(fp, k);
	    }
	}
    } else {
	PMODEL *pm;

	pm = &dec->spm;
	pm->size = 16;
	pm->cumfreq[0] = 0;
	for (k = 0; k < pm->size; k++) {
	    pm->freq[k] = 1;
	    pm->cumfreq[k + 1] = pm->cumfreq[k] + pm->freq[k];
	}
	m = rc_decode(fp, dec->rc, pm, 0, pm->size);
	set_spmodel(pm, MAX_UPARA + 2, m);
	for (cl = 0; cl < dec->num_class; cl++) {
	    k = u = 0;
	    for (gr = 0; gr < dec->num_group; gr++) {
		if (k > MAX_UPARA || gr == dec->num_group - 1) {
		    k = MAX_UPARA + 1;
		} else {
		    k += rc_decode(fp, dec->rc, pm, 0, pm->size - k);
		}
		for (; u < k; u++) dec->uquant[cl][u] = gr;
	    }
	}
	if (dec->num_pmodel > 1) {
	    pm->size = dec->num_pmodel;
	    pm->freq[0] = 0;
	    for (k = 0; k < pm->size; k++) {
		pm->freq[k] = 1;
		pm->cumfreq[k + 1] = pm->cumfreq[k] + pm->freq[k];
	    }
	    for (gr = 0; gr < dec->num_group; gr++) {
		dec->pm_idx[gr] = rc_decode(fp, dec->rc, pm, 0, pm->size);
	    }
	}
    }
    return;
}

void decode_qtindex(FILE *fp, DECODER *dec, VLC *vlc, PMODEL *cpm,
		    int tly, int tlx, int blksize, int width, int level)
{
    int i, cl, y, x, bry, brx, ctx;
    char **qtmap;
    PMODEL *pm;

    brx = (tlx + blksize < dec->width) ? (tlx + blksize) : dec->width;
    bry = (tly + blksize < dec->height) ? (tly + blksize) : dec->height;
    if (tlx >= brx || tly >= bry) return;
    if (level > 0) {
	ctx = 0;
	qtmap = dec->qtmap[level - 1];
	y = (tly / MIN_BSIZE) >> level;
	x = (tlx / MIN_BSIZE) >> level;
	if (y > 0) {
            if (qtmap[y - 1][x] == 1) ctx++;
	    if (brx < width && qtmap[y - 1][x + 1] == 1) ctx++;
	}
	if (x > 0 && qtmap[y][x - 1] == 1) ctx++;
	ctx = ((level - 1) * 4 + ctx) << 1;
	if (dec->f_huffman == 1) {
	    i = getbits(fp, 1);
	} else {
	    pm = &dec->spm;
	    i = rc_decode(fp, dec->rc, pm, ctx, ctx + 2) - ctx;
	}
	if (i == 1) {
	    qtmap[y][x] = 1;
	    blksize >>= 1;
	    decode_qtindex(fp, dec, vlc, cpm, tly, tlx,
			   blksize, width, level - 1);
	    decode_qtindex(fp, dec, vlc, cpm, tly, tlx + blksize,
			   blksize, width, level - 1);
	    decode_qtindex(fp, dec, vlc, cpm, tly + blksize, tlx,
			   blksize, width, level - 1);
	    decode_qtindex(fp, dec, vlc, cpm, tly + blksize, tlx + blksize,
			   blksize, brx, level - 1);
	    return;
	}
    }
    if (dec->f_huffman == 1) {
	i = decode_vlc(fp, vlc);
    } else {
	i = rc_decode(fp, dec->rc, cpm, 0, cpm->size);
    }
    mtf_classlabel(dec->class, dec->mtfbuf, tly, tlx,
		   blksize, width, dec->num_class);
    for (cl = 0; cl < dec->num_class; cl++) {
	if (dec->mtfbuf[cl] == i) break;
    }
    for (y = tly; y < bry; y++) {
	for (x = tlx; x < brx; x++) {
	    dec->class[y][x] = cl;
	}
    }
    return;
}

void decode_class(FILE *fp, DECODER *dec)
{
    int i, j, x, y, blksize, level;
    VLC *vlc;
    PMODEL *pm, cpm[1];

    if (dec->quadtree_depth >= 0) {
	level = dec->quadtree_depth;
        blksize = MAX_BSIZE;
    } else {
	level = 0;
        blksize = BASE_BSIZE;
    }
    if (dec->f_huffman == 1) {
	vlc = (VLC *)alloc_mem(sizeof(VLC));
	vlc->size = dec->num_class;
	vlc->max_len = 16;
	vlc->len = (int *)alloc_mem(vlc->size * sizeof(int));
	for (i = 0; i < vlc->size; i++) {
	    vlc->len[i] = getbits(fp, 4) + 1;
	}
	gen_huffcode(vlc);
    } else {
	double p;
	int ctx;
	int qtree_code[QUADTREE_DEPTH << 2], mtf_code[MAX_CLASS];

	pm = &dec->spm;
	if (dec->quadtree_depth > 0) {
	    set_spmodel(pm, 7, -1);
	    for (ctx = 0; ctx < QUADTREE_DEPTH << 2; ctx++) {
		qtree_code[ctx] = rc_decode(fp, dec->rc, pm, 0, pm->size);
	    }
	}
	set_spmodel(pm, PMCLASS_LEVEL, -1);
	for (i = 0; i < dec->num_class; i++) {
	    mtf_code[i] = rc_decode(fp, dec->rc, pm, 0, pm->size);
	    if (pm->cumfreq[pm->size] < MAX_TOTFREQ) {
		for (j = 0; j < pm->size; j++) {
		    if (j < mtf_code[i]) {
			pm->freq[j] /= 2;
		    } else {
			pm->freq[j] *= 2;
		    }
		    if (pm->freq[j] <= 0) pm->freq[j] = 1;
		    pm->cumfreq[j + 1] = pm->cumfreq[j] + pm->freq[j];
		}
	    }
	}
	/* set prob. models */
	if (level > 0) {
	    pm->size = QUADTREE_DEPTH << 3;
	    for (ctx = 0; ctx < QUADTREE_DEPTH << 2; ctx++) {
		i = qtree_code[ctx];
		p = qtree_prob[i];
		pm->freq[(ctx << 1) + 1] = p * (1 << 10);
		p = 1.0 - p;
		pm->freq[(ctx << 1)] = p * (1 << 10);
	    }
	    for (i = 0; i < pm->size; i++) {
		pm->cumfreq[i + 1] = pm->cumfreq[i] + pm->freq[i];
	    }
	}
	cpm->size = dec->num_class;
	cpm->freq = (uint *)alloc_mem((cpm->size * 2 + 1) * sizeof(uint));
	cpm->cumfreq = &cpm->freq[cpm->size];
	cpm->cumfreq[0] = 0;
	for (i = 0; i < dec->num_class; i++) {
	    p = exp(-log(2.0) * ((double)mtf_code[i] + 0.5)
		    * PMCLASS_MAX/PMCLASS_LEVEL);
	    cpm->freq[i] = p * (1 << 10);
	    if (cpm->freq[i] <= 0) cpm->freq[i] = 1;
	    cpm->cumfreq[i + 1] = cpm->cumfreq[i] + cpm->freq[i];
	}
	vlc = NULL;
    }
    for (i = 0; i < dec->num_class; i++) {
	dec->mtfbuf[i] = i;
    }
    for (y = 0; y < dec->height; y += blksize) {
	for (x = 0; x < dec->width; x += blksize) {
	    decode_qtindex(fp, dec, vlc, cpm, y, x,
			   blksize, dec->width, level);
	}
    }
    if (dec->f_huffman == 1) {
	free_vlc(vlc);
    }
    return;
}


int calc_udec(DECODER *dec, int y, int x)
{
    int rx, ry, k, u;
    int **err, *wt_p;

    u = 0;
    err = dec->err;
    wt_p = dec->ctx_weight;
    if (y >= UPEL_DIST && x >= UPEL_DIST && x <= dec->width - UPEL_DIST) {
	for (k = 0; k < NUM_UPELS; k++) {
	    ry = y + dyx[k].y;
	    rx = x + dyx[k].x;
	    u += err[ry][rx] * (*wt_p++);
	}
    } else if (y == 0) {
	if (x == 0) {
	    for (k = 0; k < NUM_UPELS; k++) {
		u += ((dec->maxval + 1) >> 2) * (*wt_p++);
	    }
	} else {
            ry = 0;
	    for (k =0; k < NUM_UPELS; k++) {
                rx = x + dyx[k].x;
                if (rx < 0) rx = 0;
                else if (rx >= x) rx = x - 1;
                u += err[ry][rx] * (*wt_p++);
	    }
	}
    } else {
	if (x == 0) {
	    for (k = 0; k < NUM_UPELS; k++) {
		ry = y + dyx[k].y;
		if (ry < 0) ry = 0;
		else if (ry >= y) ry = y - 1;
		rx = x + dyx[k].x;
		if (rx < 0) rx = 0;
		u += err[ry][rx] * (*wt_p++);
	    }
	} else {
	    for (k = 0; k < NUM_UPELS; k++) {
		ry = y + dyx[k].y;
		if (ry < 0) ry = 0;
		rx = x + dyx[k].x;
		if (rx < 0) rx = 0;
		else if (rx >= dec->width) rx = dec->width - 1;
		u += err[ry][rx] * (*wt_p++);
	    }
	}
    }
    u >>= 6;
    if (u > MAX_UPARA) u = MAX_UPARA;
    return (u);
}

int calc_prd(IMAGE *img, DECODER *dec, int cl, int y, int x)
{
    int k, prd, prd_order, rx, ry, *coef_p, *scany_p, *scanx_p, *size_p;

    coef_p = dec->predictor[cl];
    scany_p = coef_p + dec->prd_order;
    scanx_p = scany_p + dec->prd_order;
    size_p = scanx_p + dec->prd_order;
    prd_order = size_p[0];
    prd = 0;
    if (y >= size_p[1] && x >= size_p[2] && x < size_p[3]) {
	for (k = 0; k < prd_order; k++) {
	    ry = y + (*scany_p++);
	    rx = x + (*scanx_p++);
	    prd += (*coef_p++) * img->val[ry][rx];
	}
    } else if (y == 0) {
	if (x == 0) {
	    for (k = 0; k < prd_order; k++) {
		prd += *coef_p++;
	    }
	    prd *= ((img->maxval + 1) >> 1);
	} else {
	    ry = 0;
	    for (k = 0; k < prd_order; k++) {
		rx = x + (*scanx_p++);
		if (rx < 0) rx = 0;
		else if (rx >= x) rx = x - 1;
		prd += (*coef_p++) * img->val[ry][rx];
	    }
	}
    } else {
	if (x == 0) {
	    for (k = 0; k < prd_order; k++) {
		ry = y + (*scany_p++);
		if (ry < 0) ry = 0;
		else if (ry >= y) ry = y - 1;
		rx = x + (*scanx_p++);
		if (rx < 0) rx = 0;
		prd += (*coef_p++) * img->val[ry][rx];
	    }
	} else {
	    for (k = 0; k < prd_order; k++) {
		ry = y + (*scany_p++);
		if (ry < 0) ry = 0;
		rx = x + (*scanx_p++);
		if (rx < 0) rx = 0;
		else if (rx >= img->width) rx = img->width - 1;
		prd += (*coef_p++) * img->val[ry][rx];
	    }
	}
    }
    if (prd < 0) prd = 0;
    else if (prd > dec->maxprd) prd = dec->maxprd;
    return (prd);
}

IMAGE *decode_image(FILE *fp, DECODER *dec)
{
    int x, y, cl, gr, prd, u, e, E, p;
    IMAGE *img;

    img = alloc_image(dec->width, dec->height, dec->maxval);
    if (dec->f_huffman == 1) {
	VLC *vlc;
	dec->vlcs = init_vlcs(dec->pmodels, dec->num_group, 1);
	for (y = 0; y < dec->height; y++) {
	    for (x = 0; x < dec->width; x++) {
		cl = dec->class[y][x];
		u = calc_udec(dec, y, x);
		gr = dec->uquant[cl][u];
		prd = calc_prd(img, dec, cl, y, x);
		prd >>= (dec->coef_precision - 1);
		p = (prd + 1) >> 1;
		vlc = &dec->vlcs[gr][0];
		dec->err[y][x] = E = decode_vlc(fp, vlc);
		e = E2e(E, p, prd & 1, dec->maxval);
		img->val[y][x] = p + e;
	    }
	}
    } else {
	PMODEL *pm;
	if (dec->pm_accuracy < 0) {
	    for (y = 0; y < dec->height; y++) {
		for (x = 0; x < dec->width; x++) {
		    cl = dec->class[y][x];
		    u = calc_udec(dec, y, x);
		    gr = dec->uquant[cl][u];
		    prd = calc_prd(img, dec, cl, y, x);
		    prd >>= (dec->coef_precision - 1);
		    p = (prd + 1) >> 1;
		    pm = dec->pmodels[gr][0];
		    dec->err[y][x] = E
			= rc_decode(fp, dec->rc, pm, 0, pm->size);
		    e = E2e(E, p, prd & 1, dec->maxval);
		    img->val[y][x] = p + e;
		}
	    }
	} else {
	    int mask, shift, base;
	    mask = (1 << dec->pm_accuracy) - 1;
	    shift = dec->coef_precision - dec->pm_accuracy;
	    for (y = 0; y < dec->height; y++) {
		for (x = 0; x < dec->width; x++) {
		    cl = dec->class[y][x];
		    u = calc_udec(dec, y, x);
		    gr = dec->uquant[cl][u];
		    prd = calc_prd(img, dec, cl, y, x);
		    base = (dec->maxprd - prd + (1 << shift) / 2) >> shift;
		    pm = dec->pmodels[gr][0] + (base & mask);
		    base >>= dec->pm_accuracy;
		    p = rc_decode(fp, dec->rc, pm, base, base+dec->maxval+1)
			- base;
		    img->val[y][x] = p;
		    prd >>= (dec->coef_precision - 1);
		    e = (p << 1) - prd;
		    dec->err[y][x] = (e > 0)? (e - 1) : (-e);
		}
	    }
	}
    }
    return (img);
}

void write_pgm(IMAGE *img, char *filename)
{
    int i, j;
    FILE *fp;
    fp = fileopen(filename, "wb");
    fprintf(fp, "P5\n%d %d\n%d\n", img->width, img->height, img->maxval);
    for (i = 0; i < img->height; i++) {
	for (j = 0; j < img->width; j++) {
	    putc(img->val[i][j], fp);
	}
    }
    fclose(fp);
    return;
}

int main(int argc, char **argv)
{
    int i;
    IMAGE *img;
    DECODER *dec;
    char *infile, *outfile;
    FILE *fp;

    cpu_time();
    setbuf(stdout, 0);
    infile = outfile = NULL;
    for (i = 1; i < argc; i++) {
	if (infile == NULL) {
	    infile = argv[i];
	} else {
	    outfile = argv[i];
	}
    }
    if (infile == NULL || outfile == NULL) {
        printf(BANNER"\n", 0.1 * VERSION);
	printf("usage: decmrp infile outfile\n");
	printf("infile:     Input file\n");
	printf("outfile:    Output file\n");
        exit(0);
    }
    fp = fileopen(infile, "rb");
    dec = init_decoder(fp);
    if (dec->f_huffman == 0) {
	dec->rc = rc_init();
	rc_startdec(fp, dec->rc);
    }
    decode_class(fp, dec);
    decode_predictor(fp, dec);
    decode_threshold(fp, dec);
    dec->pmodels = init_pmodels(dec->num_group, dec->num_pmodel,
				dec->pm_accuracy, dec->pm_idx, dec->sigma,
				dec->maxval + 1);
    img = decode_image(fp, dec);
    fclose(fp);

    write_pgm(img, outfile);
    printf("cpu time: %.2f sec.\n", cpu_time());
    return(0);
}
