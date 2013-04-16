#define HAVE_CLOCK
#define HAVE_64BIT_INTEGER
#define QUADTREE_DEPTH	4
#define BASE_BSIZE      8
#define MAX_BSIZE       32
#define MIN_BSIZE       (MAX_BSIZE >> QUADTREE_DEPTH)
#define MAX_CLASS	63
#define NUM_CLASS       -1
#define NUM_GROUP       16
#define PRD_ORDER       -1
#define COEF_PRECISION  6
#define MAX_COEF	(2 << COEF_PRECISION)
#define MAX_UPARA       512
#define UPEL_DIST       3
#define NUM_UPELS       (UPEL_DIST * (UPEL_DIST + 1))
#define MAX_ITERATION   100
#define EXTRA_ITERATION 10
#define NUM_PMODEL      16
#define PM_ACCURACY     3
#define MIN_FREQ        1
#define MAX_SYMBOL	1024		/* must be >> MAX_UPARA */
#define PMCLASS_MAX	16
#define PMCLASS_LEVEL	32
#define OPT_SIDEINFO
#define VLC_MAXLEN      32
#define MAGIC_NUMBER    ('M' << 8) + 'R'
#define BANNER          "encmrp/decmrp version %.1f (Jan. 2005)"
#define VERSION         5
#define uint            unsigned int
#define img_t           unsigned char
#define cost_t          double
#ifdef HAVE_64BIT_INTEGER
#  define RANGE_SIZE 64
#  if defined(_MSC_VER) || defined(__BORLANDC__)
#    define range_t unsigned __int64
#  else
#    define range_t unsigned long long
#  endif
#  define MAX_TOTFREQ (1 << 20)	/* must be < RANGE_BOT */
#else
#  define RANGE_SIZE 32
#  define range_t unsigned int
#  define MAX_TOTFREQ (1 << 14)	/* must be < RANGE_BOT */
#endif
#define RANGE_TOP  ((range_t)1 << (RANGE_SIZE - 8))
#define RANGE_BOT  ((range_t)1 << (RANGE_SIZE - 16))

typedef struct {
    int height;
    int width;
    int maxval;
    img_t **val;
} IMAGE;

typedef struct {
    int size;
    int id;
    uint *freq;
    uint *cumfreq;
    float *cost;
    float *subcost;
} PMODEL;

typedef struct {
    int size;
    int max_len;
    int *len;
    int *index;
    int *off;
    uint *code;
} VLC;

typedef struct {
    range_t low;
    range_t code;
    range_t range;
} RANGECODER;

typedef struct {
    int y, x;
} POINT;

typedef struct {
    int height;
    int width;
    int maxval;
    int num_class;
    int num_group;
    int prd_order;
    int coef_precision;
    int num_pmodel;
    int pm_accuracy;
    int maxprd;
    int f_huffman;
    int quadtree_depth;
    int optimize_loop;
    int **predictor;
    int **th;
    int **upara;
    int **prd;
    int **encval;
    int **err;
    int **org;
    int *ctx_weight;
    int ***roff;
    int qtctx[QUADTREE_DEPTH << 3];
    char **qtmap[QUADTREE_DEPTH];
    char **class;
    char **group;
    char **uquant;
    int **econv;
    img_t *bconv;
    img_t *fconv;
    PMODEL ***pmodels;
    PMODEL **pmlist;
    PMODEL spm;
    VLC **vlcs;
    RANGECODER *rc;
    double *sigma;
    int *mtfbuf;
    int *coef_m;
    cost_t **coef_cost;
    cost_t *th_cost;
    cost_t *class_cost;
    cost_t qtflag_cost[QUADTREE_DEPTH << 3];
} ENCODER;

typedef struct {
    int version;
    int height;
    int width;
    int maxval;
    int num_comp;
    int num_class;
    int num_group;
    int prd_order;
    int num_pmodel;
    int pm_accuracy;
    int maxprd;
    int coef_precision;
    int f_huffman;
    int quadtree_depth;
    int **predictor;
    int **err;
    int *ctx_weight;
    char **qtmap[QUADTREE_DEPTH];
    char **class;
    char **uquant;
    int *pm_idx;
    PMODEL ***pmodels;
    PMODEL spm;
    VLC **vlcs;
    RANGECODER *rc;
    double *sigma;
    int *mtfbuf;
} DECODER;

/* common.c */
FILE *fileopen(char *, char *);
void *alloc_mem(size_t);
void **alloc_2d_array(int, int, int);
IMAGE *alloc_image(int, int, int);
int *gen_hufflen(uint *, int, int);
void gen_huffcode(VLC *);
VLC *make_vlc(uint *, int, int);
VLC ***init_vlc(int, int, int *, int);
void free_vlc(VLC *);
VLC **init_vlcs(PMODEL ***, int, int);
PMODEL ***init_pmodels(int, int, int, int *, double *, int);
void set_spmodel(PMODEL *, int, int);
int *init_ctx_weight(void);
int e2E(int, int, int, int);
int E2e(int, int, int, int);
void mtf_classlabel(char **, int *, int, int, int, int, int);
double cpu_time(void);

/* rc.c */
RANGECODER *rc_init(void);
void rc_encode(FILE *, RANGECODER *, uint, uint, uint);
void rc_finishenc(FILE *, RANGECODER *);
int rc_decode(FILE *, RANGECODER *, PMODEL *, int, int);
void rc_startdec(FILE *, RANGECODER *);
