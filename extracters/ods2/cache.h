/* Cache.h    Definitions for cache routines */

/*
 *      This is part of ODS2 written by Paul Nankervis,
 *      email address:  Paulnank@au1.ibm.com
 *
 *      ODS2 is distributed freely for all members of the
 *      VMS community to use. However all derived works
 *      must maintain comments in their source to acknowledge
 *      the contributions of the original author and
 *      subsequent contributors.   This is free software; no
 *      warranty is offered,  and while we believe it to be useful,
 *      you use it at your own risk.
 */

#ifndef _CACHE_H
#define _CACHE_H

#ifdef VAXC     /* Stupid VAX C doesn't allow "signed" keyword */
#define signed
#else
#define signed signed
#endif

#include "ods2.h"

struct CACHE {
    struct CACHE *nextlru;      /* next object on least recently used list */
    struct CACHE *lastlru;      /* last object on least recently used list */
    struct CACHE *left;         /* left branch of binary tree */
    struct CACHE *right;        /* right branch of binary tree */
    struct CACHE **parent;      /* address of pointer to this object */
    void *(*objmanager) ( struct CACHE * cacheobj, int flushonly );
    uint32_t hashval;           /* object hash value */
    short refcount;             /* object reference count */
    signed char balance;        /* object tree imbalance factor */
    signed char objtype;        /* object type (for debugging) */
#define OBJTYPE_DEV  1
#define OBJTYPE_FCB  2
#define OBJTYPE_WCB  3
#define OBJTYPE_VIOC 7
#define OBJTYPE_DIR  8
};

void cache_show(void);
int cache_refcount(struct CACHE *cacheobj);
void cache_delete(struct CACHE *cacheobj);
void cache_purge(int all);
void cache_flush(void);
void cache_remove(struct CACHE *cacheobj);
void cache_touch(struct CACHE * cacheobj);
void cache_untouch(struct CACHE * cacheobj,int recycle);
void *cache_find( void **root, uint32_t hashval, void *keyval, vmscond_t *retsts,
                  int (*compare_func) ( uint32_t hashval, void *keyval,
                                        void *node ),
                  void *(*create_func) ( uint32_t hashval, void *keyval,
                                         vmscond_t *retsts ) );

#endif /* #ifndef _CACHE_H */
