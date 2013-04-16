#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mrp.h"

const POINT dyx[] = {
    /* 1 */
    { 0,-1}, {-1, 0},
    /* 2 */
    { 0,-2}, {-1,-1}, {-2, 0}, {-1, 1},
    /* 3 */
    { 0,-3}, {-1,-2}, {-2,-1}, {-3, 0}, {-2, 1}, {-1, 2},
    /* 4 */
    { 0,-4}, {-1,-3}, {-2,-2}, {-3,-1}, {-4, 0}, {-3, 1}, {-2, 2}, {-1, 3},
    /* 5 */
    { 0,-5}, {-1,-4}, {-2,-3}, {-3,-2}, {-4,-1}, {-5, 0}, {-4, 1}, {-3, 2},
    {-2, 3}, {-1, 4},
    /* 6 */
    { 0,-6}, {-1,-5}, {-2,-4}, {-3,-3}, {-4,-2}, {-5,-1}, {-6, 0}, {-5, 1},
    {-4, 2}, {-3, 3}, {-2, 4}, {-1, 5},
    /* 7 */
    { 0,-7}, {-1,-6}, {-2,-5}, {-3,-4}, {-4,-3}, {-5,-2}, {-6,-1}, {-7, 0},
    {-6, 1}, {-5, 2}, {-4, 3}, {-3, 4}, {-2, 5}, {-1, 6},
    /* 8 */
    { 0,-8}, {-1,-7}, {-2,-6}, {-3,-5}, {-4,-4}, {-5,-3}, {-6,-2}, {-7,-1},
    {-8, 0}, {-7, 1}, {-6, 2}, {-5, 3}, {-4, 4}, {-3, 5}, {-2, 6}, {-1, 7},
};
double sigma_h[] = {0.85, 1.15, 1.50, 1.90, 2.55, 3.30, 4.25, 5.60,
                    7.15, 9.20,12.05,15.35,19.95,25.85,32.95,44.05};
double sigma_a[] = {0.15, 0.26, 0.38, 0.57, 0.83, 1.18, 1.65, 2.31,
                    3.22, 4.47, 6.19, 8.55,11.80,16.27,22.42,30.89};
double qtree_prob[] = {0.05, 0.2, 0.35, 0.5, 0.65, 0.8, 0.95};

FILE *fileopen(char *filename, char *mode)
{
    FILE *fp;
    fp = fopen(filename, mode);
    if (fp == NULL) {
        fprintf(stderr, "Can\'t open %s!\n", filename);
        exit(1);
    }
    return (fp);
}

void *alloc_mem(size_t size)
{
    void *ptr;
    if ((ptr = (void *)malloc(size)) == NULL) {
        fprintf(stderr, "Can\'t allocate memory (size = %d)!\n", (int)size);
        exit(1);
    }
    return (ptr);
}

void **alloc_2d_array(int height, int width, int size)
{
    void **mat;
    char *ptr;
    int k;

    mat = (void **)alloc_mem(sizeof(void *) * height + height * width * size);
    ptr = (char *)(mat + height);
    for (k = 0; k < height; k++) {
	mat[k] =  ptr;
	ptr += width * size;
    }
    return (mat);
}

IMAGE *alloc_image(int width, int height, int maxval)
{
    IMAGE *img;
    img = (IMAGE *)alloc_mem(sizeof(IMAGE));
    img->width = width;
    img->height = height;
    img->maxval = maxval;
    img->val = (img_t **)alloc_2d_array(img->height, img->width,
                                        sizeof(img_t));
    return (img);
}

int *gen_hufflen(uint *hist, int size, int max_len)
{
    int i, j, k, l, *len, *index, *bits, *link;

    len = (int *)alloc_mem(size * sizeof(int));
    index = (int *)alloc_mem(size * sizeof(int));
    bits = (int *)alloc_mem(size * sizeof(int));
    link = (int *)alloc_mem(size * sizeof(int));
    for (i = 0; i < size; i++) {
        len[i] = 0;
        index[i] = i;
        link[i] = -1;
    }
    /* sort in decreasing order of frequency */
    for (i = size -1; i > 0; i--) {
	for (j = 0; j < i; j++) {
	    if (hist[index[j]] < hist[index[j + 1]]) {
                k = index[j + 1];
                index[j + 1] = index[j];
                index[j] = k;
	    }
	}
    }
    for (i = 0; i < size; i++) {
        bits[i] = index[i];	/* reserv a sorted index table */
    }
    for (i = size - 1; i > 0; i--) {
        k = index[i - 1];
        l = index[i];
        hist[k] += hist[l];
        len[k]++;
	while (link[k] >= 0) {
            k = link[k];
            len[k]++;
	}
        link[k] = l;
        len[l]++;
	while (link[l] >= 0) {
            l = link[l];
            len[l]++;
	}
	for (j = i - 1; j > 0; j--) {
	    if (hist[index[j - 1]] < hist[index[j]]) {
                k = index[j];
                index[j] = index[j - 1];
                index[j - 1] = k;
	    } else {
                break;
	    }
	}
    }
    /* limit the maximum code length to max_len */
    for (i = 0; i < size; i++) {
	index[i] = bits[i];	/* restore the index table */
        bits[i] = 0;
    }
    for (i = 0; i < size; i++) {
        bits[len[i]]++;
    }
    for (i = size - 1; i > max_len; i--) {
	while (bits[i] > 0) {
            j = i - 2;
            while(bits[j] == 0) j--;
            bits[i] -= 2;
            bits[i - 1]++;
            bits[j + 1] += 2;
            bits[j]--;
	}
    }
    for (i = k = 0; i < size; i++) {
	for (j = 0; j < bits[i]; j++) {
            len[index[k++]] = i;
	}
    }
    free(link);
    free(bits);
    free(index);
    return (len);
}

void gen_huffcode(VLC *vlc)
{
    int i, j, *idx, *len;
    uint k;

    vlc->index = idx = (int *)alloc_mem(vlc->size * sizeof(int));
    vlc->off = (int *)alloc_mem(vlc->max_len * sizeof(int));
    vlc->code = (uint *)alloc_mem(vlc->size * sizeof(int));
    len = vlc->len;
    /* sort in increasing order of code length */
    for (i = 0; i < vlc->size; i++) {
        idx[i] = i;
    }
    for (i = vlc->size -1; i > 0; i--) {
	for (j = 0; j < i; j++) {
	    if (len[idx[j]] > len[idx[j + 1]]) {
                k = idx[j + 1];
                idx[j + 1] = idx[j];
                idx[j] = k;
	    }
	}
    }
    k = 0;
    for (j = 0; j < vlc->max_len; j++) {
	vlc->off[j] = -1;
    }
    j = len[idx[0]];
    for (i = 0; i < vlc->size; i++) {
	if (j < len[idx[i]]) {
	    k <<= (len[idx[i]] - j);
	    j = len[idx[i]];
	}
	vlc->code[idx[i]] = k++;
	vlc->off[j - 1] = i;
    }
    return;
}

VLC *make_vlc(uint *hist, int size, int max_len)
{
    VLC *vlc;

    vlc = (VLC *)alloc_mem(sizeof(VLC));
    vlc->size = size;
    vlc->max_len = max_len;
    vlc->len = gen_hufflen(hist, size, max_len);
    gen_huffcode(vlc);
    return (vlc);
}

void free_vlc(VLC *vlc)
{
    free(vlc->code);
    free(vlc->off);
    free(vlc->index);
    free(vlc->len);
    free(vlc);
    return;
}

VLC **init_vlcs(PMODEL ***pmodels, int num_group, int num_pmodel)
{
    VLC **vlcs, *vlc;
    PMODEL *pm;
    int gr, k;

    vlcs = (VLC **)alloc_2d_array(num_group, num_pmodel, sizeof(VLC));
    for (gr = 0; gr < num_group; gr++) {
	for (k = 0; k < num_pmodel; k++) {
	    vlc = &vlcs[gr][k];
	    pm = pmodels[gr][k];
	    vlc->size = pm->size;
	    vlc->max_len = VLC_MAXLEN;
	    vlc->len = gen_hufflen(pm->freq, pm->size, VLC_MAXLEN);
	    gen_huffcode(vlc);
	}
    }
    return (vlcs);
}

/*
  Natural logarithm of the gamma function
  cf. "Numerical Recipes in C", 6.1
  http://www.ulib.org/webRoot/Books/Numerical_Recipes/bookcpdf.html
*/
double lngamma(double xx)
{
    int j;
    double x,y,tmp,ser;
    double cof[6] = {
	76.18009172947146,	-86.50532032941677,
	24.01409824083091,	-1.231739572450155,
	0.1208650973866179e-2,	-0.5395239384953e-5
    };

    y = x = xx;
    tmp = x + 5.5 - (x + 0.5) * log(x + 5.5);
    ser = 1.000000000190015;
    for (j=0;j<=5;j++)
	ser += (cof[j] / ++y);
    return (log(2.5066282746310005 * ser / x) - tmp);
}

void set_freqtable(PMODEL *pm, double *pdfsamp, int num_subpm, int num_pmodel,
		   int center, int idx, double sigma)
{
    double shape, beta, norm, sw, x;
    int i, j, n;

    if (center == 0) sigma *= 2.0;
    if (idx < 0) {
	shape = 2.0;
    } else {
	shape = 3.2 * (idx + 1) / (double)num_pmodel;
    }
    /* Generalized Gaussian distribution */
    beta = exp(0.5*(lngamma(3.0/shape)-lngamma(1.0/shape))) / sigma;
    sw = 1.0 / (double)num_subpm;
    n = pm->size * num_subpm;
    center *= num_subpm;
    if (center == 0) {    /* one-sided distribution */
	for (i = 0; i < n; i++) {
	    x = (double)i * sw;
	    pdfsamp[i] = exp(-pow(beta * x, shape));
	}
    } else {
	for (i = center; i < n; i++) {
	    x = (double)(i - (double)center + 0.5) * sw;
	    pdfsamp[i + 1] = exp(-pow(beta * x, shape));
	}
	for (i = 0; i <= center; i++) {
	    pdfsamp[center - i] =  pdfsamp[center + i + 1];
	}
	for (i = 0; i < n; i++) {
	    if (i == center) {
		pdfsamp[i] = (2.0 + pdfsamp[i] + pdfsamp[i + 1]) / 2.0;
	    } else {
		pdfsamp[i] = pdfsamp[i] + pdfsamp[i + 1];
	    }
	}
    }
    for (j = 0; j < num_subpm; j++) {
	norm = 0.0;
	for (i = 0; i < pm->size; i++) {
	    norm += pdfsamp[i * num_subpm + j];
	}
	norm = (double)(MAX_TOTFREQ - pm->size * MIN_FREQ) / norm;
	norm += 1E-8;	/* to avoid machine dependent rounding errors */
	pm->cumfreq[0] = 0;
	for (i = 0; i < pm->size; i++) {
	    pm->freq[i] = norm * pdfsamp[i * num_subpm + j] + MIN_FREQ;
	    pm->cumfreq[i + 1] = pm->cumfreq[i] + pm->freq[i];
	}
	pm++;
    }
    return;
}

PMODEL ***init_pmodels(int num_group, int num_pmodel, int pm_accuracy,
		       int *pm_idx, double *sigma, int size)
{
    PMODEL ***pmodels, *pmbuf, *pm;
    int gr, i, j, num_subpm, num_pm, ssize, idx;
    double *pdfsamp;

    if (pm_accuracy < 0) {
	num_subpm = 1;
	ssize = 1;
    } else {
	num_subpm = 1 << pm_accuracy;
	ssize = size;
	size = size + ssize - 1;
    }
    num_pm = (pm_idx != NULL)? 1 : num_pmodel;
    pmodels = (PMODEL ***)alloc_2d_array(num_group, num_pm,
					 sizeof(PMODEL *));
    pmbuf = (PMODEL *)alloc_mem(num_group * num_pm * num_subpm
				* sizeof(PMODEL));
    for (gr = 0; gr < num_group; gr++) {
	for (i = 0; i < num_pm; i++) {
	    pmodels[gr][i] = pmbuf;
	    for (j = 0; j < num_subpm; j++) {
		pm = pmbuf++;
		pm->id = i;
		pm->size = size;
		pm->freq = (uint *)alloc_mem((size * 2 + 1) * sizeof(uint));
		pm->cumfreq = &pm->freq[size];
		if (pm_idx == NULL) {
		    pm->cost = (float *)alloc_mem((size + ssize)
						  * sizeof(float));
		    pm->subcost = &pm->cost[size];
		}
	    }
	}
    }
    pdfsamp = alloc_mem((size * num_subpm + 1) * sizeof(double));
    for (gr = 0; gr < num_group; gr++) {
	for (i = 0; i < num_pm; i++) {
	    if (pm_idx != NULL) {
		idx = pm_idx[gr];
	    } else if (num_pm > 1) {
		idx = i;
	    } else {
		idx = -1;
	    }
	    set_freqtable(pmodels[gr][i], pdfsamp, num_subpm, num_pmodel,
			  ssize - 1, idx, sigma[gr]);
	}
    }
    free(pdfsamp);
    return (pmodels);
}

/* probaility model for coefficients and thresholds */
void set_spmodel(PMODEL *pm, int size, int m)
{
    int i, sum;
    double p;

    pm->size = size;
    if (m >= 0) {
	p = 1.0 / (double)(1 << (m % 8));
	sum = 0;
	for (i = 0; i < pm->size; i++) {
	    pm->freq[i] = exp(-p * i) * (1 << 10);
	    if (pm->freq[i] == 0) pm->freq[i]++;
	    sum += pm->freq[i];
	}
	if (m & 8) pm->freq[0] = (sum - pm->freq[0]);	/* weight for zero */
    } else {
	for (i = 0; i < pm->size; i++) {
	    pm->freq[i] = 1;
	}
    }
    pm->cumfreq[0] = 0;
    for (i = 0; i < pm->size; i++) {
	pm->cumfreq[i + 1] = pm->cumfreq[i] + pm->freq[i];
    }
    return;
}

int *init_ctx_weight(void)
{
    int *ctx_weight, k;
    double dy, dx;

    ctx_weight = (int *)alloc_mem(NUM_UPELS * sizeof(int));
    for (k = 0; k < NUM_UPELS; k++) {
	dy = dyx[k].y;
	dx = dyx[k].x;
	ctx_weight[k] = 64.0 / sqrt(dy * dy + dx * dx) + 0.5;
    }
    return (ctx_weight);
}

int e2E(int e, int prd, int flag, int maxval)
{
    int E, th;

    E = (e > 0)? e : -e;
    th = (prd < ((maxval + 1) >> 1))? prd : maxval - prd;
    if (E > th) {
        E += th;
    } else if (flag) {
	E = (e < 0)? (E << 1) - 1 : (E << 1);
    } else {
	E = (e > 0)? (E << 1) - 1 : (E << 1);
    }
    return (E);
}

int E2e(int E, int prd, int flag, int maxval)
{
    int e, th;

    th = (prd < ((maxval + 1) >> 1))? prd : maxval - prd;
    if (E > (th << 1)) {
	e = (prd < ((maxval + 1) >> 1))? E - th : th - E;
    } else if (flag) {
	e = (E & 1)? -((E >> 1) + 1) : (E >> 1);
    } else {
	e = (E & 1)? (E >> 1) + 1 : -(E >> 1);
    }
    return (e);
}

void mtf_classlabel(char **class, int *mtfbuf, int y, int x,
		    int bsize, int width, int num_class)
{
    int i, j, k, ref[3];

    if (y == 0) {
	if (x == 0) {
	    ref[0] = ref[1] = ref[2] = 0;
	} else {
	    ref[0] = ref[1] = ref[2] = class[y][x-1];
	}
    } else {
	ref[0] = class[y-1][x];
	ref[1] = (x == 0)? class[y-1][x] : class[y][x-1];
	ref[2] = (x + bsize >= width)?
	    class[y-1][x] : class[y-1][x+bsize];
	if (ref[1] == ref[2]) {
	    ref[2] = ref[0];
	    ref[0] = ref[1];
	}
    }

    /* move to front */
    for (k = 2; k >= 0; k--) {
	if ((j = mtfbuf[ref[k]]) == 0) continue;
	for (i = 0; i < num_class; i++) {
	    if (mtfbuf[i] < j) {
		mtfbuf[i]++;
	    }
	}
	mtfbuf[ref[k]] = 0;
    }
    return;
}

double cpu_time(void)
{
#include <time.h>
#ifndef HAVE_CLOCK
#  include <sys/times.h>
    struct tms t;
#endif
#ifndef CLK_TCK
#  define CLK_TCK 60
#endif
    static clock_t prev = 0;
    clock_t cur, dif;

#ifdef HAVE_CLOCK
    cur = clock();
#else
    times(&t);
    cur = t.tms_utime + t.tms_stime;
#endif
    if (cur > prev) {
	dif = cur - prev;
    } else {
	dif = (unsigned)cur - prev;
    }
    prev = cur;

#ifdef HAVE_CLOCK
    return ((double)dif / CLOCKS_PER_SEC);
#else
    return ((double)dif / CLK_TCK);
#endif
}
