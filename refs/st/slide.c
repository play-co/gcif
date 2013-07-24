/* slide.c
   Copyright 1999, N. Jesper Larsson, all rights reserved.
   
   This file contains an implementation of a sliding window index using a
   suffix tree. It corresponds to the algorithms and representation presented
   in the Ph.D. thesis "Structures of String Matching and Data Compression",
   Lund University, 1999.

   This software may be used freely for any purpose. However, when distributed,
   the original source must be clearly stated, and, when the source code is
   distributed, the copyright notice must be retained and any alterations in
   the code must be clearly marked. No warranty is given regarding the quality
   of this software.*/

#include <stdlib.h>
#include <limits.h>
#include <time.h>

#define RANDOMIZE_NODE_ORDER 1
#define K (UCHAR_MAX+1)

typedef unsigned char SYMB;
enum { SLIDE_OK, SLIDE_PARAMERR, SLIDE_MALLOCERR };

/* Node numbering:
  
   Node 0 is nil.
   Node 1 is root.
   Nodes 2...mmax-1 are non-root internal nodes.
   Nodes mmax...2*mmax-1 are leaves.*/

struct Node {
   int pos;                     /* edge label start.*/
   int depth;                   /* string depth.*/
   int suf;                     /* suffix link; sign bit is cred.*/
   SYMB child;                  /* number of children minus one, except
                                   for the root which always has
                                   child==1.*/
};

static int mmax;                /* max size of window.*/
static int hashsz;              /* number of hash table slots.*/
static SYMB *x;                 /* the input string buffer.*/
static struct Node *nodes;      /* array of internal nodes.*/
static int *hash;               /* hash table slot heads.*/
static int *next;               /* next in hash table or free list.*/
static int freelist;            /* list of unused nodes.*/
static SYMB *fsym;              /* first symbols of leaf edges*/

static int ins, proj;           /* active point.*/
static int front, tail;         /* limits of window.*/
static int r, a;                /* preserved values for canonize.*/

/* Sign bit is used to flag cred bit and end of hash table slot.*/
#define SIGN            INT_MIN

/* Macros used to keep indices inside the circular buffer (avoiding
   modulo operations for speed). M0 is for subtractions to stay
   nonnegative, MM for additions to stay below mmax.*/
#define M0(i)           ((i)<0 ? (i)+mmax : (i))
#define MM(i)           ((i)<mmax ? (i) : (i)-mmax)

/* Hash function. If this is changed, the calculation of hashsz in
   initslide must be changed accordingly.*/
#define HASH(u, c)      ((u)^(c))
#define UNHASH(h, c)    ((h)^(c))

/* Macro to get child from hashtable, v=child(u, c). This macro does not
   support the implicit outgoing edges of nil, they must be handled
   specially.*/
#define GETCHILD(v, u, c) {                                     \
   v=hash[HASH(u, c)];                                          \
   while (v>0) {                                                \
      if ((v<mmax ? x[nodes[v].pos] : fsym[v-mmax])==(c))       \
         break;                                                 \
      v=next[v];                                                \
   }                                                            \
}

/* Macro to get parent from hashtable. c is the first symbol of the
   incoming edge label of v, u=parent(v).*/
#define GETPARENT(u, v, c) {                    \
   int gp_w=(v);                                \
   while ((gp_w=next[gp_w])>=0)                 \
      ;                                         \
   u=UNHASH(gp_w&~SIGN, c);                     \
}

/* Macro to insert edge (u, v) into hash table so that child(u, c)==v.*/
#define CREATEEDGE(u, v, c) {                   \
   int ce_h=HASH(u, c);                         \
   next[v]=hash[ce_h];                          \
   hash[ce_h]=(v);                              \
}

/* Macro to remove the edge (u, v). c is the first symbol of the edge
   label. Makes use of the fact that the hash and next arrays are located
   next to each other in memory.*/
#define DELETEEDGE(u, v, c) {                   \
   int de_w, de_i, de_h=HASH(u, c);             \
   de_w=hash[de_i=de_h];                        \
   while (de_w!=(v)) {                          \
      de_i=de_w+hashsz;                         \
      de_w=next[de_w];                          \
   }                                            \
   hash[de_i]=next[v];                          \
}

/* Function initslide:

   Initialize empty suffix tree. The buffer parameter should point to an
   array of size max_window_size which is used as a circular buffer. */
int initslide(int max_window_size, SYMB *buffer)
{
   int i, j, nodediff, nodemask;

   mmax=max_window_size;
   if (mmax<2)
      return SLIDE_PARAMERR;
   x=buffer;                    /* the global buffer pointer.*/

   /* calculate the right value for hashsz, must be harmonized with the
      definition of the hash function.*/
   if (mmax>K) {                /* i=max{mmax, K}-1; j=min{mmax, K}-1.*/
      i=mmax-1;
      j=K-1;
   } else {
      i=K-1;
      j=mmax-1;
   }
   while (j) {                  /* OR in all possible one bits from j.*/
      i|=j;
      j>>=1;
   }
   hashsz=i+1;                  /* i is now maximum hash value.*/

   /* allocate memory.*/
   nodes=malloc((mmax+1)*sizeof *nodes);
   fsym=malloc(mmax*sizeof *fsym);
   hash=malloc((hashsz+2*mmax)*sizeof *hash);
   if (! nodes || ! fsym || ! hash)
      return SLIDE_MALLOCERR;
   next=hash+hashsz;            /* convenient for DELETEEDGE.*/

#if RANDOMIZE_NODE_ORDER
   /* Put nodes into free list in random order, to avoid degenaration of
      hash table. This method does NOT yield a uniform distribution over
      the permutations, but it's fast, and random enough for our
      purposes.*/
   srand(time(0));
   nodediff=(rand()%mmax)|1;
   for (i=mmax>>1, nodemask=mmax-1; i; i>>=1)
      nodemask|=i;              /* nodemask is 2^ceil(log_2(mmax))-1.*/
   j=0;
   do {
      i=j;
      while ((j=(j+nodediff)&nodemask)>=mmax || j==1)
         ;
      next[i]=j;
   } while (j);
   freelist=next[0];
#else
   /* Put nodes in free list in order according to numbers. The risk of
      the hash table is larger than if the order is randomized, but this
      is actually often faster, due to caching effects.*/
   freelist=i=2;
   while (i++<mmax)
      next[i-1]=i;
#endif

   for (i=0; i<hashsz; ++i)
      hash[i]=i|SIGN;           /* list terminators used by GETPARENT.*/

   nodes[0].depth=-1;
   nodes[1].depth=0;
   nodes[1].suf=0;
   nodes[1].child=1;            /* stays 1 forever.*/

   ins=1;                       /* initialize active point.*/
   proj=0;
   tail=front=0;                /* initialize window limits.*/
   r=0;

   return 0;
}

/* Function releaseslide:*/
void releaseslide()
{
   free(nodes);
   free(fsym);
   free(hash);
}

/* Macro for canonize subroutine:

   r is return value. To avoid unnecessary access to the hash table, r is
   preserved between calls. If r is not 0 it is assumed that
   r==child(ins, a), and (ins, r) is the edge of the insertion point.*/
#define CANONIZE(r, a, ins, proj) {             \
   int ca_d;                                    \
   if (proj && ins==0) {                        \
      ins=1; --proj; r=0;                       \
   }                                            \
   while (proj) {                               \
      if (r==0) {                               \
         a=x[M0(front-proj)];                   \
         GETCHILD(r, ins, a);                   \
      }                                         \
      if (r>=mmax)                              \
         break;                                 \
      ca_d=nodes[r].depth-nodes[ins].depth;     \
      if (proj<ca_d)                            \
         break;                                 \
      proj-=ca_d; ins=r; r=0;                   \
   }                                            \
}

/* Macro for Update subroutine:

   Send credits up the tree, updating pos values, until a nonzero credit
   is found. Sign bit of suf links is used as credit bit.*/
#define UPDATE(v, i) {                          \
   int ud_u, ud_v=v, ud_f, ud_d;                \
   int ud_i=i, ud_j, ud_ii=M0(i-tail), ud_jj;   \
   SYMB ud_c;                                   \
   while (ud_v!=1) {                            \
      ud_c=x[nodes[ud_v].pos];                  \
      GETPARENT(ud_u, ud_v, ud_c);              \
      ud_d=nodes[ud_u].depth;                   \
      ud_j=M0(nodes[ud_v].pos-ud_d);            \
      ud_jj=M0(ud_j-tail);                      \
      if (ud_ii>ud_jj)                          \
         nodes[ud_v].pos=MM(ud_i+ud_d);         \
      else {                                    \
         ud_i=ud_j; ud_ii=ud_jj;                \
      }                                         \
      if ((ud_f=nodes[ud_v].suf)>=0) {          \
         nodes[ud_v].suf=ud_f|SIGN;             \
         break;                                 \
      }                                         \
      nodes[ud_v].suf=ud_f&~SIGN;               \
      ud_v=ud_u;                                \
   }                                            \
}

/* Function advancefront:

   Moves front, the right endpoint of the window, forward by positions
   positions, increasing its size.*/
void advancefront(int positions)
{
   int s, u, v;                 /* nodes.*/
   int j;
   SYMB b, c;

   while (positions--) {
      v=0;
      c=x[front];
      while (1) {
         CANONIZE(r, a, ins, proj);
         if (r<1) {             /* if active point at node.*/
            if (ins==0) {       /* if ins is nil.*/
               r=1;             /* r is child of ins for any c.*/
               break;           /* endpoint found.*/
            }
            GETCHILD(r, ins, c);
            if (r>0) {          /* if ins has a child for c.*/
               a=c;             /* a is first symbol in (ins, r) label.*/
               break;           /* endpoint found.*/    
            } else
               u=ins;           /* will add child below u.*/
         } else {               /* active point on edge.*/
            j=(r>=mmax ? MM(r-mmax+nodes[ins].depth) : nodes[r].pos);
            b=x[MM(j+proj)];    /* next symbol in (ins, r) label.*/
            if (c==b)           /* if same as front symbol.*/
               break;           /* endpoint found.*/
            else {              /* edge must be split.*/
               u=freelist;      /* u is new node.*/
               freelist=next[u];
               nodes[u].depth=nodes[ins].depth+proj;
               nodes[u].pos=M0(front-proj);
               nodes[u].child=0;
               nodes[u].suf=SIGN; /* emulate update (skipped below).*/
               DELETEEDGE(ins, r, a);
               CREATEEDGE(ins, u, a);
               CREATEEDGE(u, r, b);
               if (r<mmax)
                  nodes[r].pos=MM(j+proj);
               else
                  fsym[r-mmax]=b;
            }
         }
         s=mmax+M0(front-nodes[u].depth);
         CREATEEDGE(u, s, c);   /* add new leaf s.*/
         fsym[s-mmax]=c;
         if (u!=1)              /* don't count children of root.*/
            ++nodes[u].child;
         if (u==ins)            /* skip update if new node.*/
            UPDATE(u, M0(front-nodes[u].depth));
         nodes[v].suf=u|(nodes[v].suf&SIGN);
         v=u;
         ins=nodes[ins].suf&~SIGN;
         r=0;                   /* force getting new r in canonize.*/
      }
      nodes[v].suf=ins|(nodes[v].suf&SIGN);
      ++proj;                   /* move active point down.*/
      front=MM(front+1);
   }
}

/* Function advancetail:

   Moves tail, the left endpoint of the window, forward by positions
   positions, decreasing its size.*/
void advancetail(int positions)
{
   int s, u, v, w;              /* nodes.*/
   SYMB b, c;
   int i, d;

   while(positions--) {
      CANONIZE(r, a, ins, proj);
      v=tail+mmax;              /* the leaf to delete.*/
      b=fsym[tail];
      GETPARENT(u, v, b);
      DELETEEDGE(u, v, b);
      if (v==r) {               /* replace instead of delete?*/
         i=M0(front-(nodes[ins].depth+proj));
         CREATEEDGE(ins, mmax+i, b);
         fsym[i]=b;
         UPDATE(ins, i);
         ins=nodes[ins].suf&~SIGN;
         r=0;                   /* force getting new r in canonize.*/
      } else if (u!=1 && --nodes[u].child==0) {
         /* u has only one child left, delete it.*/
         c=x[nodes[u].pos];
         GETPARENT(w, u, c);
         d=nodes[u].depth-nodes[w].depth;
         b=x[MM(nodes[u].pos+d)];
         GETCHILD(s, u, b);     /* the remaining child of u.*/
         if (u==ins) {
            ins=w;
            proj+=d;
            a=c;                /* new first symbol of (ins, r) label*/
         } else if (u==r)
            r=s;                /* new child(ins, a).*/
         if (nodes[u].suf<0)    /* send waiting credit up tree.*/
            UPDATE(w, M0(nodes[u].pos-nodes[w].depth))
         DELETEEDGE(u, s, b);
         DELETEEDGE(w, u, c);
         CREATEEDGE(w, s, c);
         if (s<mmax)
            nodes[s].pos=M0(nodes[s].pos-d);
         else
            fsym[s-mmax]=c;
         next[u]=freelist;      /* mark u unused.*/
         freelist=u;
      }
      tail=MM(tail+1);
   }
}

/* Function longestmatch:

   Search for the longest string in the tree matching pattern. maxlen is
   the length of the pattern (i.e. the maximum length of the match);
   *matchlen is assigned the length of the match. The returned value is
   the starting position of the match in the indexed buffer, or -1 if the
   match length is zero.

   The parameters wrappos and wrapto support searching for a pattern
   residing in a circular buffer: wrappos should point to the position
   just beyond the end of the buffer, and wrapto to the start of the
   buffer. If the pattern is not in a circular buffer, call with zero
   values for these parameters.*/
int longestmatch(SYMB *pattern, int maxlen, int *matchlen,
                 SYMB *wrappos, SYMB *wrapto)
{
   int u=1,                     /* deepest node on the search path.*/
      ud=0,                     /* depth of u.*/
      l=0,                      /* current length of match.*/
      e=0,                      /* positions left to check on incoming
                                   edge label of u.*/
      start=-1,                 /* start of the match.*/
      p, v, vd;
   SYMB c;

   while (l<maxlen) {
      c=*pattern;
      if (e==0) {               /* if no more symbols in current label.*/
         if (u>=mmax)
            break;              /* can't go beyond leaf, stop.*/
         GETCHILD(v, u, c);     /* v is next node on search path.*/
         if (v<1)
            break;              /* no child for c, stop.*/
         if (v>=mmax) {         /* if v is a leaf.*/
            start=v-mmax;       /* start of string represented by v.*/
            vd=M0(front-start); /* depth of v.*/
            p=MM(v+ud);         /* first position of edge label.*/
         } else {               /* v is an internal node.*/
            vd=nodes[v].depth;  /* depth of v.*/
            p=nodes[v].pos;     /* first position of edge label.*/
            start=M0(p-ud);     /* start of string represented by v.*/
         }
         e=vd-ud-1;             /* symbols left in current label.*/
         u=v;                   /* make the switch for next iteration.*/
         ud=vd;
      } else {                  /* symbols left to check in same label.*/
         p=MM(p+1);             /* next position in current label.*/
         if (x[p]!=c)
            break;              /* doesn't match, stop.*/
         --e;                   /* one less symbol left.*/
      }
      ++l;                      /* match length.*/
      if (++pattern==wrappos)   /* wrap if reached end of buffer.*/
         pattern=wrapto;
   }
   *matchlen=l;
   return start;
}
