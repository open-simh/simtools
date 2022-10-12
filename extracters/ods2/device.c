/* Device.c  Module to remember and find devices...*/

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

/* Should have mechanism to return actual device name... */

/*  This module is simple enough - it just keeps track of
 *  device names and initialization...
 */

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ods2.h"
#include "access.h"
#include "cache.h"
#include "device.h"
#include "ssdef.h"

#if !defined( DEBUG ) && defined( DEBUG_DEVICE )
#define DEBUG DEBUG_DEVICE
#else
#ifndef DEBUG
#define DEBUG 0
#endif
#endif

static void *device_create( uint32_t devsiz, void *keyval, vmscond_t *retsts );
static int device_compare( uint32_t keylen, void *keyval, void *node );

static struct DEV *dev_root = NULL;


/************************************************************ device_create() */

/* device_create() creates a device object...
 */

static void *device_create( uint32_t devsiz, void *keyval, vmscond_t *retsts ) {

    register struct DEV *dev;

    dev = (struct DEV *) calloc( 1, offsetof( struct DEV, devnam ) + devsiz + 2 );
    if ( dev == NULL ) {
        *retsts = SS$_INSFMEM;
        return NULL;
    }
    dev->cache.objtype = OBJTYPE_DEV;
    memcpy( dev->devnam, keyval, devsiz );
    memcpy( dev->devnam + devsiz, ":", 2 );

    *retsts = SS$_NORMAL;
    return dev;
}

/************************************************************ device_done() */

/* device_done() releases a device reference and
 * eventually the device structure.
 */

void device_done( struct DEV *dev ) {
    if( dev->cache.refcount < 1 ) {
#if DEBUG
        printf( "Device done with no reference\n" );
#endif
        abort();
    }
    cache_untouch( &dev->cache, TRUE );
    if( dev->cache.refcount == 0 ) /* Safe because on LRU list */
        cache_delete( &dev->cache );
    return;
}

/*********************************************************** device_compare() */

/* device_compare() compares a device name to that of a device object... */

static int device_compare( uint32_t keylen, void *keyval, void *node ) {

    register int cmp;
    register const char *keynam;
    register const char *devnam;

    keynam = (const char *) keyval;
    devnam = ((const struct DEV *) node)->devnam;

    cmp = 0;
    while ( keylen-- > 0 ) {
        cmp = toupper( *keynam++ ) - toupper( *devnam++ );
        if ( cmp != 0 ) {
            break;
        }
    }
    if ( cmp == 0 && *devnam != '\0' && *devnam != ':' ) {
        cmp = -1;
    }
    return cmp;
}

/************************************************************ device_lookup() */

/* device_lookup() is to to find devices... */

vmscond_t device_lookup( size_t devlen, char *devnam, int create,
                        struct DEV **retdev ) {

    vmscond_t sts;
    size_t devsiz;
    register struct DEV *dev;

    for ( devsiz = 0; devsiz < devlen; devsiz++ ) {
        if ( devnam[devsiz] == ':' ) {
            break;
        }
    }
    dev = (struct DEV *) cache_find( (void *) &dev_root, (uint32_t)devsiz, devnam, &sts,
                                     device_compare,
                                     create ? device_create : NULL );
    if ( dev == NULL ) {
        if ( $MATCHCOND(sts, SS$_ITEMNOTFOUND) ) {
            sts = SS$_NOSUCHDEV;
        }
    } else {
        if( retdev == NULL )
            device_done( dev );
        else
            *retdev = dev;
        sts = SS$_NORMAL;
    }
    return sts;
}
