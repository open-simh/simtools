/*

   Copyright (c) 2025, John Forecast

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   JOHN FORECAST BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of John Forecast shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from John Forecast.

*/

/*
 * Support routines for handling Unix V7 file systems under fsio
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <fnmatch.h>
#include <regex.h>

#include "fsio.h"

/*
 * Table of "set" commands
 */
static char *setCmds[] = {
  "uid",
  "gid",
  NULL
};
#define UNIXV7SET_UID   0
#define UNIXV7SET_GID   1

int unixv7ReadBlock(struct mountedFS *, unsigned int, void *);
int unixv7WriteBlock(struct mountedFS *, unsigned int, void *);
int unixv7ReadInode(struct mountedFS *, v7_ino_t, struct v7_dinode *);
int unixv7WriteInode(struct mountedFS *, v7_ino_t, struct v7_dinode *);

static void breli(struct mountedFS *, v7_daddr_t, int);

extern int args;
extern char **words;
extern int quiet;

#define MAX(a, b)       (((a) >= (b)) ? (a) : (b))
#define MIN(a, b)       (((a) <= (b)) ? (a) : (b))

static char *rwx[] = {
  "---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"
};

/*++
 *      u n i x v 7 D e v i c e s
 *
 * List of UNIX V7 device types supported by this program.
 *
 --*/
struct UNIXV7device UNIXV7Devices[] = {
         /* (Cylinders * Surfaces * Sectors * Sector Size) / 512 */
  { "rk05", (203LL * 2LL * 12LL * 512LL) / 512LL },
  { "rl01", (256LL * 2LL * 40LL * 256LL) / 512LL },
  { "rl02", (512LL * 2LL * 40LL * 256LL) / 512LL },
  { "rk06", (411LL * 3LL * 22LL * 512LL) / 512LL },
  { "rk07", (815LL * 3LL * 22LL * 512LL) / 512LL },
  { NULL, 0 }
};

/*
 * Internal file system operations.
 */

/*++
 *      b a l l o c
 *
 *  Allocate a free block on the UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to mounted file system descriptor
 *
 * Outputs:
 *
 *      The super block and/or the on-disk free list may be updated.
 *
 * Returns:
 *
 *      Allocated block number, 0 if allocation failed
 *
 --*/
static v7_daddr_t balloc(
  struct mountedFS *mount
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct v7_filsys *sb = (struct v7_filsys *)&data->superblk;
  v7_daddr_t bno;
  uint8_t buf[V7_BLOCKSIZE];
  
  if (le16toh(sb->s_nfree) > 0) {
    sb->s_nfree = htole16(le16toh(sb->s_nfree) - 1);
    if ((bno = V7_LONG(le32toh(sb->s_free[le16toh(sb->s_nfree)]))) != 0) {
      /*
       * If we have just exhausted the free block cache in the super block,
       * replenish it from the on-disk free block list.
       */
      if (le16toh(sb->s_nfree) <= 0) {
        struct v7_fblk *fb = (struct v7_fblk *)buf;
        
        if (unixv7ReadBlock(mount, bno, buf) == 0)
          return 0;

        sb->s_nfree = fb->df_nfree;
        memcpy(sb->s_free, fb->df_free, sizeof(fb->df_free));
      }
      sb->s_fmod = 1;

      /*
       * Zero out the newly allocated block
       */
      memset(buf, 0, sizeof(buf));
      if (unixv7WriteBlock(mount, bno, buf) == 0)
        return 0;
      return bno;
    }
  }
  return 0;
}

/*++
 *      b f r e e
 *
 *  Release a block back to the free list on a UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to mounted file system descriptor
 *      bno             - block number to be released
 *
 * Outputs:
 *
 *      The super block and/or the on-disk free list may be updated.
 *
 * Returns:
 *
 *      None
 *
 --*/
static void bfree(
  struct mountedFS *mount,
  v7_daddr_t bno
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct v7_filsys *sb = (struct v7_filsys *)&data->superblk;

  if (le16toh(sb->s_nfree) <= 0)
    sb->s_nfree = 0;

  if (le16toh(sb->s_nfree) >= V7_NICFREE) {
    uint8_t buf[V7_BLOCKSIZE];
    struct v7_fblk * fb = (struct v7_fblk *)buf;

    fb->df_nfree = sb->s_nfree;
    memcpy(fb->df_free, sb->s_free, sizeof(sb->s_free));
    if (unixv7WriteBlock(mount, bno, buf) == 0)
      return;
    sb->s_nfree = 0;
  }
  sb->s_free[le16toh(sb->s_nfree)] = htole32(V7_LONG(bno));
  sb->s_nfree = htole16(le16toh(sb->s_nfree) + 1);
  sb->s_fmod = 1;
}

/*++
 *      b m a p
 *
 *  Map a file logical block number into a physical block number within a UNIX
 *  V7 file system for read access. Note that 0 is an invalid block address
 *  and is used to indicate an error.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      inode           - pointer to a copy of an on-disk inode
 *      ino             - inode number for "inode"
 *      lbn             - logical block number within the file
 *      rwflg           - Read (0) / Write (1) indicator
 *      
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      Physical block number within the UNIX V7 file system, 0 if error
 *
 --*/
static uint32_t bmap(
  struct mountedFS *mount,
  struct v7_dinode *inode,
  v7_ino_t ino,
  uint32_t lbn,
  int rwflg
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  uint8_t *cp = (uint8_t *)inode->di_addr, buf[V7_BLOCKSIZE];
  uint32_t nb, *bap, addr[NADDR];
  int i, j, sh;

  /*
   * Convert the 3-byte block addresses in the inode into 4-byte values
   */
  for (i = 0; i < NADDR; i++, cp += 3)
    V7_GET3ADDR(cp, addr[i]);

  /*
   * Blocks 0..NADDR-4 are direct blocks
   */
  if (lbn < NADDR - 3) {
    if ((nb = addr[lbn]) == 0) {
      if ((rwflg == V7_READ) || ((nb = balloc(mount)) == 0))
        return 0;

      if (unixv7WriteBlock(mount, nb, data->zero) == 0)
        return 0;

      V7_PUT3ADDR(&inode->di_addr[lbn * 3], nb);
      if (unixv7WriteInode(mount, ino, inode) == 0)
        return 0;
    }
    return nb;
  }

  /*
   * Addresses NADDR-3, NADDR-2 and NADDR-1 have single, double and triple
   * indirect blocks. First determine how many levels of indirection.
   */
  sh = 0;
  nb = 1;
  lbn -= NADDR - 3;

  for (j = 3; j > 0; j--) {
    sh += V7_NSHIFT;
    nb <<= V7_NSHIFT;
    if (lbn < nb)
      break;
    lbn -= nb;
  }
  if (j == 0)
    return 0;

  /*
   * Fetch the address from the inode
   */
  if ((nb = addr[NADDR - j]) == 0) {
    if ((rwflg == V7_READ) || ((nb = balloc(mount)) == 0))
      return 0;

    if (unixv7WriteBlock(mount, nb, data->zero) == 0)
      return 0;

    V7_PUT3ADDR(&inode->di_addr[(NADDR - j) * 3], nb);
    if (unixv7WriteInode(mount, ino, inode) == 0)
      return 0;
  }
  
  /*
   * Fetch through the indirect blocks
   */
  for (;j <= 3; j++) {
    uint32_t nbsave = nb;
    
    if (unixv7ReadBlock(mount, nb, buf) == 0)
      return 0;
    bap = (uint32_t *)buf;
    sh -= V7_NSHIFT;
    i = (lbn >> sh) & V7_NMASK;
    if ((nb = V7_ULONG(bap[i])) == 0) {
      if ((rwflg == V7_READ) || ((nb = balloc(mount)) == 0))
        return 0;

      if (unixv7WriteBlock(mount, nb, data->zero) == 0)
        return 0;

      bap[i] = V7_ULONG(nb);
      if (unixv7WriteBlock(mount, nbsave, buf) == 0)
        return 0;
    }
  }
  return nb;
}

/*++
 *      b r e l
 *
 *  Release all blocks associated with an inode.
 *
 * Inputs:
 *
 *      mount           - pointer to mounted file system descriptor
 *      inode           - pointer to the inode
 *
 * Outputs:
 *
 *      The super block and/or the on-disk free block chain may be updated.
 *
 * Returns:
 *
 *      None
 *
 --*/
static void brel(
  struct mountedFS *mount,
  struct v7_dinode *inode
)
{
  int i;
  uint8_t *cp;
  
  for (i = 0, cp = (uint8_t *)inode->di_addr; i < NADDR; i++, cp += 3) {
    v7_daddr_t bn;

    V7_GET3ADDR(cp, bn);

    if (bn != 0) {
      switch (i) {
        default:
          bfree(mount, bn);
          break;

        case NADDR-3:
          breli(mount, bn, 0);
          break;

        case NADDR-2:
          breli(mount, bn, 1);
          break;

        case NADDR-1:
          breli(mount, bn, 3);
          break;
      }
    }
  }
}

/*++
 *      b r e l i
 *
 *  Release all blocks in an indirect block.
 *
 * Inputs:
 *
 *      mount           - pointer to mounted file system descriptor
 *      bno             - block number of the indirect block
 *      depth           - indirect block depth
 *
 * Outputs:
 *
 *      The super block and/or the on-disk free block chain may be updated.
 *
 * Returns:
 *
 *      None
 *
 --*/
static void breli(
  struct mountedFS *mount,
  v7_daddr_t bno,
  int depth
)
{
  uint8_t buf[V7_BLOCKSIZE];
  int i;

  if (unixv7ReadBlock(mount, bno, buf)) {
    for (i = 0; i < V7_NINDIR; i++) {
      v7_daddr_t bn;

      V7_GET4ADDR(&buf[i * 4], bn)

      if (bn != 0) {
        if (depth != 0)
          breli(mount, bn, depth >> 1);
        else bfree(mount, bn);
      }
    }
  }
  bfree(mount, bno);
}

/*++
 *      i a l l o c
 *
 *  Allocate an unused inode on the UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to mounted file system descriptor
 *      di              - buffer to receive content of on-disk inode
 *      mode            - value for V7IFMT field of di_mode
 *                        (Used to mark inode as allocated)
 *
 * Outputs:
 *
 *      The super block and/or the on-disk inode space may be updated.
 *
 * Returns:
 *
 *      Allocated inode number, 0 if allocation failed
 *
 --*/
static v7_ino_t ialloc(
  struct mountedFS *mount,
  struct v7_dinode *dip,
  v7_ushort mode
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct v7_filsys *sb = (struct v7_filsys *)&data->superblk;
  struct v7_dinode *dp;
  v7_ino_t ino;
  uint8_t buf[V7_BLOCKSIZE];
  uint16_t i, j;

  sb->s_fmod = 1;
  
 loop:
  if (le16toh(sb->s_ninode) > 0) {
    sb->s_ninode = htole16(le16toh(sb->s_ninode) - 1);
    if ((ino = le16toh(sb->s_inode[le16toh(sb->s_ninode)])) <= V7_ROOTINO)
      goto loop;

    if (unixv7ReadInode(mount, ino, dip) == 0)
      return 0;

    if (dip->di_mode != 0)
      goto loop;

    dip->di_mode = htole16(mode);
    if (unixv7WriteInode(mount, ino, dip) == 0)
      return 0;
    
    return ino;
  }

  /*
   * The super block cache of inodes is exhausted. Scan the inode area of
   * the file system to replenish it.
   */
  ino = 1;

  for (i = 2; (i != le16toh(sb->s_isize)) &&
                (le16toh(sb->s_ninode) < V7_NICINOD); i++) {
    if (unixv7ReadBlock(mount, i, buf) == 0)
      return 0;

    for (j = 0, dp = (struct v7_dinode *)buf; j < V7_INOPB; j++, dp++) {
      if (dp->di_mode == 0) {
        sb->s_inode[le16toh(sb->s_ninode)] = htole16(ino);
        sb->s_ninode = htole16(le16toh(sb->s_ninode) + 1);
        if (le16toh(sb->s_ninode) >= V7_NICINOD)
          break;
      }
      ino++;
    }
  }
  if (le16toh(sb->s_ninode) > 0)
    goto loop;
  return 0;
}

/*++
 *      i f r e e
 *
 *  Mark an inode as free on a UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to mounted file system descriptor
 *      ino             - inode number to be released
 *
 * Outputs:
 *
 *      The super block and/or the on-disk inode space may be updated.
 *
 * Returns:
 *
 *      None
 *
 --*/

static void ifree(
  struct mountedFS *mount,
  v7_ino_t ino
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct v7_filsys *sb = (struct v7_filsys *)&data->superblk;
  struct v7_dinode di;

  memset(&di, 0, sizeof(di));

  unixv7WriteInode(mount, ino, &di);

  if (le16toh(sb->s_ninode) < V7_NICINOD) {
    sb->s_inode[le16toh(sb->s_ninode)] = htole16(ino);
    sb->s_ninode = htole16(le16toh(sb->s_ninode) + 1);
    sb->s_fmod = 1;
  }
}
/*++
 *      u p d a t e
 *
 *  Update the on-disk copy of the super block if it has been marked as
 *  "modified".
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void update(
  struct mountedFS *mount
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct v7_filsys *sb = (struct v7_filsys *)&data->superblk;

  if (sb->s_fmod != 0) {
    sb->s_fmod = 0;
    if (unixv7WriteBlock(mount, V7_SUPERBLK, sb) == 0)
      sb->s_fmod = 1;
  }
}

/*++
 *      a d d C o m p o n e n t
 *
 *  Add a component to a directory. Note that the caller must check that
 *  the component does not already exist before calling this routine.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      dino            - inode number of the specified directory
 *      ino             - inode number for the component to be added
 *      name            - pointer to component name
 *      isdirect        - 1 if component is a directory, 0 otherwise
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1 is successful, 0 on failure
 *
 --*/
static int addComponent(
  struct mountedFS *mount,
  v7_ino_t dino,
  v7_ino_t ino,
  char *name,
  int isdirect
)
{
  struct v7_dinode inode;
  char cname[V7_DIRSIZ];

  memset(cname, 0, sizeof(cname));
  strncpy(cname, name, V7_DIRSIZ);

  if (unixv7ReadInode(mount, dino, &inode)) {
    if ((le16toh(inode.di_mode) & V7_IFMT) == V7_IFDIR) {
      off_t offset = 0;
      uint32_t lbn = 0xFFFFFFFF, pbn;

      for (;;) {
        uint8_t buf[V7_BLOCKSIZE];
        struct v7_direct *dp;
        int upd = 0;

        if (lbn != (offset >> V7_BSHIFT)) {
          lbn = offset >> V7_BSHIFT;
          if ((pbn = bmap(mount, &inode, ino, lbn, V7_WRITE)) == 0) {
            ERROR("%s: Error mapping lbn %u on inode %u\n", command, lbn, ino);
            return 0;
          }
          if (unixv7ReadBlock(mount, pbn, buf) == 0)
            return 0;
        }
        dp = (struct v7_direct *)&buf[offset & V7_BMASK];

        if (dp->d_ino == 0) {
          dp->d_ino = htole16(ino);
          memcpy(dp->d_name, cname, V7_DIRSIZ);
          offset += sizeof(struct v7_direct);
          
          if (unixv7WriteBlock(mount, pbn, buf) == 0)
            return 0;

          if (offset > V7_LONG(le32toh(inode.di_size))) {
            inode.di_size = htole32(V7_LONG(offset));
            upd = 1;
          }

          if (isdirect) {
            inode.di_nlink = htole16(le16toh(inode.di_nlink) + 1);
            upd = 1;
          }

          if (upd) {
            if (unixv7WriteInode(mount, dino, &inode) == 0)
              return 0;
          }
          return 1;
        }
        offset += sizeof(struct v7_direct);
      }
    } else ERROR("%s: inode %u is not a directory\n", command, ino);
  }
  return 0;
}

/*++
 *      e m p t y D i r e c t o r y
 * 
 *  Allocate a disk block and create an empty directory on it.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      ino             - inode number of the directory
 *      pino            - inode number of the directory's parent
 *
 * Outputs:
 *
 *      The empty directory will be written in an available disk block.
 *
 * Returns:
 *
 *      Disk block allocated for the directory, 0 on failure
 *
 --*/
static v7_daddr_t emptyDirectory(
  struct mountedFS *mount,
  v7_ino_t ino,
  v7_ino_t pino
)
{
  uint8_t buf[V7_BLOCKSIZE];
  struct v7_direct *dip = (struct v7_direct *)buf;
  v7_daddr_t bno;

  if ((bno = balloc(mount)) != 0) {
    memset(buf, 0, sizeof(buf));

    dip->d_ino = htole16(ino);
    strcpy(dip->d_name, ".");
    dip++;
    dip->d_ino = htole16(pino);
    strcpy(dip->d_name, "..");

    if (unixv7WriteBlock(mount, bno, buf) == 0)
      return 0;
  }
  return bno;
}

/*++
 *      l o o k u p C o m p o n e n t
 *
 *  Lookup a component (directory or file) in the specified directory. This
 *  routine does not support wildcards.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      ino             - inode number of the specified directory
 *      name            - pointer to component name
 *      cino            - return inode number of component here
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1  if component found
 *      0  if component not found
 *      -1 if error during lookup
 *
 --*/
static int lookupComponent(
  struct mountedFS *mount,
  v7_ino_t ino,
  char *name,
  v7_ino_t *cino
)
{
  struct v7_dinode inode;
  char cname[V7_DIRSIZ];

  memset(cname, 0, sizeof(cname));
  strncpy(cname, name, V7_DIRSIZ);
    
  if (unixv7ReadInode(mount, ino, &inode)) {
    if ((le16toh(inode.di_mode) & V7_IFMT) == V7_IFDIR) {
      off_t offset = 0;
      uint32_t lbn = 0xFFFFFFFF, pbn;

      while (offset < V7_LONG(le32toh(inode.di_size))) {
        uint8_t buf[V7_BLOCKSIZE];
        struct v7_direct *dp;

        if (lbn != (offset >> V7_BSHIFT)) {
          lbn = offset >> V7_BSHIFT;
          if ((pbn = bmap(mount, &inode, ino, lbn, V7_READ)) == 0) {
            ERROR("%s: Error mapping lbn %u on inode %u\n", command, lbn, ino);
            return -1;
          }
          if (unixv7ReadBlock(mount, pbn, buf) == 0)
            return -1;
        }
        dp = (struct v7_direct *)&buf[offset & V7_BMASK];

        if (dp->d_ino != 0) {
          if (memcmp(cname, dp->d_name, V7_DIRSIZ) == 0) {
            *cino = le16toh(dp->d_ino);
            return 1;
          }
        }
        offset += sizeof(struct v7_direct);
      }
      return 0;
    } else ERROR("%s: inode %u is not a directory\n", command, ino);
  }
  return -1;
}

/*++
 *      i s D i r e c t o r y E m p t y
 *  
 *   Check if a directory is empty (i.e. contains only ". and ".." entries).
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      inode           - pointer to the on-disk inode for the directory
 *      ino             - inode number of the specified directory
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1 if directory is empty, 0 otherwise
 *
 --*/
static int isDirectoryEmpty(
  struct mountedFS *mount,
  struct v7_dinode *inode,
  v7_ino_t ino
)
{
  off_t offset = 0;
  uint32_t lbn = 0xFFFFFFFF, pbn;

  while (offset < V7_LONG(le32toh(inode->di_size))) {
    uint8_t buf[V7_BLOCKSIZE];
    struct v7_direct *dp;

    if (lbn != (offset >> V7_BSHIFT)) {
      lbn = offset >> V7_BSHIFT;
      if ((pbn = bmap(mount, inode, ino, lbn, V7_READ)) == 0) {
        ERROR("%s: Error mapping lbn %u on inode %u\n", command, lbn, ino);
        return 0;
      }
      if (unixv7ReadBlock(mount, pbn, buf) == 0)
        return 0;
    }
    dp = (struct v7_direct *)&buf[offset & V7_BMASK];

    if (dp->d_ino != 0) {
      if ((strcmp(dp->d_name, ".") != 0) && (strcmp(dp->d_name, "..") != 0))
        return 0;
    }
    offset += sizeof(struct v7_direct);
  }
  return 1;
}

/*++
 *      u n l i n k C o m p o n e n t
 *
 *  Lookup a component in the specified directory, remove the directory
 *  entry, release any resources (disk block(s) and inode) and update any
 *  reference counts. If the component is a directory and has more than just
 *  the "." and ".." entries present fail the operation.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      ino             - inode number of the specified directory
 *      name            - pointer to component name
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1  if component found and removed
 *      0  if component not found
 *      -1 if error during lookup
 *
 --*/
static int unlinkComponent(
  struct mountedFS *mount,
  v7_ino_t ino,
  char *name
)
{
  struct v7_dinode inode;
  char cname[V7_DIRSIZ];

  memset(cname, 0, sizeof(cname));
  strncpy(cname, name, V7_DIRSIZ);
    
  if (unixv7ReadInode(mount, ino, &inode)) {
    if ((le16toh(inode.di_mode) & V7_IFMT) == V7_IFDIR) {
      off_t offset = 0;
      uint32_t lbn = 0xFFFFFFFF, pbn;

      while (offset < V7_LONG(le32toh(inode.di_size))) {
        uint8_t buf[V7_BLOCKSIZE];
        struct v7_direct *dp;

        if (lbn != (offset >> V7_BSHIFT)) {
          lbn = offset >> V7_BSHIFT;
          if ((pbn = bmap(mount, &inode, ino, lbn, V7_READ)) == 0) {
            ERROR("%s: Error mapping lbn %u on inode %u\n", command, lbn, ino);
            return -1;
          }
          if (unixv7ReadBlock(mount, pbn, buf) == 0)
            return -1;
        }
        dp = (struct v7_direct *)&buf[offset & V7_BMASK];

        if (dp->d_ino != 0) {
          if (memcmp(cname, dp->d_name, V7_DIRSIZ) == 0) {
            struct v7_dinode inode2;
            
            if (unixv7ReadInode(mount, le16toh(dp->d_ino), &inode2) == 0)
              return -1;
            
            switch (le16toh(inode2.di_mode) & V7_IFMT) {
              case V7_IFDIR:
                if (!isDirectoryEmpty(mount, &inode2, le16toh(dp->d_ino)))
                  return -1;

                inode.di_nlink = htole16(le16toh(inode.di_nlink) - 1);
                if (unixv7WriteInode(mount, ino, &inode) == 0)
                  return -1;
                /* FALLTHROUGH */

              case V7_IFREG:
                brel(mount, &inode2);
                /* FALLTHROUGH */

              default:
                inode2.di_nlink = htole16(le16toh(inode2.di_nlink) - 1);
                if (inode2.di_nlink == 0) {
                  memset(&inode2, 0, sizeof(inode2));
                  if (unixv7WriteInode(mount, le16toh(dp->d_ino), &inode2) == 0)
                    return -1;

                  ifree(mount, le16toh(dp->d_ino));
                  
                  dp->d_ino = 0;
                  if (unixv7WriteBlock(mount, pbn, buf) == 0)
                    return -1;
                }
                break;
            }
            return 1;
          }
        }
        offset += sizeof(struct v7_direct);
      }
      return 0;
    } else ERROR("%s: inode %u is not a directory\n", command, ino);
  }
  return -1;
}

/*++
 *      w a l k T r e e
 *
 *  Recusively walk a UNIX V7 file tree starting at the root. When a match
 *  is found, a supplied action routine is called. The tree walk may support
 *  regex-style wildcard characters so the action routine may be called more
 *  than once for each call to walkTree().
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      spec            - pointer to the file specifcation block
 *      idx             - current index in "spec" - start at 0
 *      ino             - directory inode number to start walk
 *      action          - action routine to be called when a full match is
 *                        found
 *      arg             - argument to be supplied to action()
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void walkTree(
  struct mountedFS *mount,
  struct unixv7FileSpec *spec,
  int idx,
  v7_ino_t ino,
  void (*action)(struct mountedFS *, struct unixv7FileSpec *, char *, v7_ino_t, v7_ino_t, uintptr_t),
  uintptr_t arg
)
{
  struct v7_dinode inode;

  if (unixv7ReadInode(mount, ino, &inode)) {
    if ((le16toh(inode.di_mode) & V7_IFMT) == V7_IFDIR) {
      off_t offset = 0;
      uint32_t lbn = 0xFFFFFFFF, pbn;

      /*
       * If we expect to create a new file, call the action routine now to
       * allow it to create the final component.
       */
      if ((spec->access == UNIXV7_A_NEW) && (idx == (spec->depth - 1))) {
        (*action)(mount, spec, spec->comp[idx], ino, 0, arg);
        return;
      }
  
      while (offset < V7_LONG(le32toh(inode.di_size))) {
        uint8_t buf[V7_BLOCKSIZE];
        struct v7_direct *dp;

        if (lbn != (offset >> V7_BSHIFT)) {
          lbn = offset >> V7_BSHIFT;
          if ((pbn = bmap(mount, &inode, ino, lbn, V7_READ)) == 0) {
            ERROR("Error mapping lbn %u on inode %u\n", lbn, ino);
            return;
          }
          if (unixv7ReadBlock(mount, pbn, buf) == 0)
            return;
        }
        dp = (struct v7_direct *)&buf[offset & V7_BMASK];

        if (dp->d_ino != 0) {
          char name[V7_DIRSIZ + 1];
          v7_ino_t fino = le16toh(dp->d_ino);
          
          memset(name, 0, sizeof(name));
          memcpy(name, dp->d_name, V7_DIRSIZ);

          switch (spec->wildcard) {
            case UNIXV7_M_NONE:
              if (strcmp(spec->comp[idx], name) == 0) {
                if (idx == (spec->depth - 1))
                  (*action)(mount, spec, name, ino, fino, arg);
                else walkTree(mount, spec, idx + 1, fino, action, arg);
              }
              break;

            case UNIXV7_M_ALLOW:
              if (fnmatch(spec->comp[idx], name, FNM_PERIOD) == 0) {
                if (idx == (spec->depth - 1))
                  (*action)(mount, spec, name, ino, fino, arg);
                else walkTree(mount, spec, idx + 1, fino, action, arg);
              }
              break;
          }
        }
        offset += sizeof(struct v7_direct);
      }
    }
  }
}

/*++
 *      u n i x v 7 P a r s e F i l e s p e c
 * 
 *  Parse a character string representing a UNIX V7 file specification. The
 *  file specification is split into individual components separated by one
 *  of more '/' characters. If wildcard characters are not allowed, each
 *  component may not exceed 14 characters in length.
 *
 * Inputs:
 *
 *      ptr             - pointer to the file specification string
 *      spec            - pointer to the file specifcation block
 *      wildcard        - wildcard processing options:
 *                        0 (UNIXV7_M_NONE)     - wildcards not allowed
 *                        1 (UNIXV7_M_ALLOW)    - wildcards allowed
 *      access          - access mode options:
 *                        0 (UNIXV7_A_EXIST)    - expects an existin file
 *                        1 (UNIXV7_A_NEW)      - expects to create a new file
 *
 * Outputs:
 *
 *      The file specification block will be filled with the file information.
 *
 * Returns:
 *
 *      1 if parse successful, 0 otherwise
 *
 --*/
int unixv7ParseFilespec(
  char *ptr,
  struct unixv7FileSpec *spec,
  char wildcard,
  char access
)
{
  char *p;
  
  memset(spec, 0, sizeof(struct unixv7FileSpec));

  spec->wildcard = wildcard;
  spec->access = access;
  
  while ((ptr != NULL) && (*ptr != '\0')) {
    /*
     * Remove any leading '/' characters.
     */
    while (*ptr == '/')
      ptr++;

    if ((p = strchr(ptr, '/')) != NULL)
      *p++ = '\0';

    if (wildcard == UNIXV7_M_NONE) {
      if (strlen(ptr) > V7_DIRSIZ)
        return 0;
    }

    spec->comp[spec->depth++] = ptr;
    ptr = p;
  }
  return 1;
}

/*++
 *      u n i x v 7 P r o t e c t i o n
 * 
 *  Build a protection string based on the mode value from an inode.
 *
 * Inputs:
 *
 *      prot            - pointer to buffer to receive the protection string
 *      type            - type of file (first character of protection)
 *      mode            - mode value from inode
 *
 * Outputs:
 *
 *      The protection string is modified
 *
 * Returns:
 *
 *      None
 *
 --*/
void unixv7Protection(
  char *prot,
  char type,
  v7_ushort mode
)
{
  prot[0] = type;
  prot[1] = '\0';

  strcat(prot, rwx[(mode & (V7_IREAD|V7_IWRITE|V7_IEXEC)) >> 6]);
  strcat(prot, rwx[(mode & ((V7_IREAD|V7_IWRITE|V7_IEXEC) >> 3)) >> 3]);
  strcat(prot, rwx[(mode & ((V7_IREAD|V7_IWRITE|V7_IEXEC) >> 6))]);

  /*
   * Check for special values
   */
  if (((mode & V7_IEXEC) != 0) && ((mode & V7_ISUID) != 0))
    prot[3] = 's';
  if (((mode & (V7_IEXEC >> 3)) != 0) && ((mode & V7_ISGID) != 0))
    prot[6] = 's';
  if ((mode & V7_ISVTX) != 0)
    prot[9] = 't';
}

/*++
 *      u n i x v 7 D e v i c e
 *
 *  Build a device major/minor number based on the address in an inode.
 *
 * Inputs:
 *
 *      device          - pointer to buffer to receive the device string
 *      inode           - pointer to the on-disk format inode
 *
 * Outputs:
 *
 *      The device string is modified
 *
 * Returns:
 *
 *      None
 *
 --*/
void unixv7Device(
  char *device,
  struct v7_dinode *inode
)
{
  uint8_t major = (uint8_t)inode->di_addr[2];
  uint8_t minor = (uint8_t)inode->di_addr[1];
  
  sprintf(device, "%3u,%-3u", major, minor);
}

/*++
 *      u n i x v 7 T i m e s t a m p
 * 
 *  The Unix V7 filesystem uses a 32-bit signed timestamp which will overflow
 *  in Jan 2038. Return a suitable timestamp for the current date/time unless
 *  the timestamp would overflow in which case the maximum value will be
 *  returned. This is made more complicated because it depends on the
 *  capabilities of the system running this code which may use 64-bit signed,
 *  32-bit signed or 32-bit unsigned time values.
 *
 * Inputs:
 *
 *      None
 *
 * Outputs:
 *
 *      ...
 *
 * Returns:
 *
 *      A suitable 32-bit positive signed timestamp value
 *
 --*/
v7_time_t unixv7Timestamp(void)
{
  time_t now = time(NULL);

  if (sizeof(time_t) == 8) {
    if (now > 0x7FFFFFFF)
      return 0x7FFFFFFF;
    return now & 0xFFFFFFFF;
  }

  if (sizeof(time_t) == 4) {
    if ((uint32_t)now > 0x7FFFFFFF)
      return now & 0x7FFFFFFF;
    return now;
  }
  return 0;
}

/*++
 *      d u m p D i r e c t o r y
 *
 *  Dump internal information about the contents of a Unix V7 directory.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      ino             - inode number for the directory
 *      indent          - indent level for output
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void dumpDirectory(
  struct mountedFS *mount,
  v7_ino_t ino,
  int indent
)
{
  struct v7_dinode inode;

  if (unixv7ReadInode(mount, ino, &inode)) {
    if ((le16toh(inode.di_mode) & V7_IFMT) == V7_IFDIR) {
      off_t offset = 0;
      uint32_t lbn = 0xFFFFFFFF, pbn;

      while (offset < V7_LONG(le32toh(inode.di_size))) {
        uint8_t buf[V7_BLOCKSIZE];
        struct v7_direct *dp;
        
        if (lbn != (offset >> V7_BSHIFT)) {
          lbn = offset >> V7_BSHIFT;
          if ((pbn = bmap(mount, &inode, ino, lbn, V7_READ)) == 0) {
            ERROR("Error mapping lbn %u on inode %u\n", lbn, ino);
            return;
          }
          if (unixv7ReadBlock(mount, pbn, buf) == 0)
            return;
        }
        dp = (struct v7_direct *)&buf[offset & V7_BMASK];

        if (dp->d_ino != 0) {
          struct v7_dinode finode;
          
          if (unixv7ReadInode(mount, le16toh(dp->d_ino), &finode)) {
            char prot[16], name[V7_DIRSIZ + 1], other[32];

            memset(name, 0, sizeof(name));
            memcpy(name, dp->d_name, V7_DIRSIZ);

            other[0] = '\0';

            switch (le16toh(finode.di_mode) & V7_IFMT) {
              case V7_IFCHR:
                unixv7Protection(prot, 'c', le16toh(finode.di_mode));
                unixv7Device(other, &finode);
                break;
                
              case V7_IFBLK:
                unixv7Protection(prot, 'b', le16toh(finode.di_mode));
                unixv7Device(other, &finode);
                break;

              case V7_IFMPC:
                unixv7Protection(prot, 'm', le16toh(finode.di_mode));
                break;

              case V7_IFMPB:
                unixv7Protection(prot, 'M', le16toh(finode.di_mode));
                break;

              case V7_IFDIR:
                unixv7Protection(prot, 'd', le16toh(finode.di_mode));
                break;

              case V7_IFLNK:
                unixv7Protection(prot, 'l', le16toh(finode.di_mode));
                break;

              case V7_IFREG:
                unixv7Protection(prot, '-', le16toh(finode.di_mode));
                sprintf(other, "%u", V7_LONG(le32toh(finode.di_size)));
                break;

              default:
                printf("Unknown mode %o\n", le16toh(finode.di_mode));
                return;
            }
            printf("%*s%-6u %s %-6d %-14s %s\n",
                   indent, "", le16toh(dp->d_ino), prot,
                   le16toh(finode.di_nlink), name, other);

            /*
             * Recursively display directories.
             */
            if ((le16toh(finode.di_mode) & V7_IFMT) == V7_IFDIR) {
              if ((strcmp(name, ".") != 0) && (strcmp(name, "..") != 0))
                dumpDirectory(mount, le16toh(dp->d_ino), indent + 2);
            }
          } else {
            ERROR("Error reading file inode (%u)\n", le16toh(dp->d_ino));
            return;
          }
        }
        offset += sizeof(struct v7_direct);
      }
    }
    return;
  }
  ERROR("Error reading directory inode (%u)\n", ino);
}

/*++
 *      b a d b l o c k
 *
 *  Perform some sanity checks on a data block address:
 *
 *      - Check for duplicate block address (optional)
 *      - Check if block address is after the i-node space and 
 *        within the disk size as specified in the superblock
 *
 * Inputs:
 *
 *      sb              - pointer to the super block
 *      block           - logical block address being checked
 *      map             - if not NULL, pointer to bitmap to check for
 *                        duplicate block addresses
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1 if block does not pass sanity checks, 0 otherwise
 *
 --*/
static int badblock(
  struct v7_filsys *sb,
  uint32_t block,
  uint32_t *map
)
{
  /*
   * The -f switch may be used to bypass the duplicate block check.
   */
  if (!SWISSET('f') && (map != NULL)) {
    int offset = block / 32, bit = block & 31;

    if ((map[offset] & (1 << bit)) != 0) {
      ERROR("Duplicate free block (%u)\n", block);
      return 1;
    }
    map[offset] |= 1 << bit;
  }
  
  if ((block < le16toh(sb->s_isize)) ||
      (block >= V7_ULONG(le32toh(sb->s_fsize)))) {
    ERROR("Invalid free block (%u)\n", block);
    return 1;
  }
  return 0;
}

/*++
 *      v a l i d a t e F r e e B l o c k
 *
 *  Validate a free block and, if it is part of the free list, any block that
 *  it points to are valid.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      sb              - pointer to the super block
 *      map             - bitmap to detect duplicate free blocks
 *      block           - logical block address being checked
 *      onlist          - 1 if this block is part of the free list, 0 otherwise
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1 if this block and all blocks it point at are valid, 0 otherwise
 *
 --*/
static int validateFreeBlock(
  struct mountedFS *mount,
  struct v7_filsys *sb,
  uint32_t *map,
  uint32_t block,
  int onlist
)
{
  uint8_t buf[V7_BLOCKSIZE];
  struct v7_fblk *fb = (struct v7_fblk *)buf;
  uint32_t nxtblock;
  
  if (!badblock(sb, block, map)) {
    if (onlist) {
      if (unixv7ReadBlock(mount, block, buf) != 0) {
        int16_t i;

        if (le16toh(fb->df_nfree) > 0)
          for (i = le16toh(fb->df_nfree) - 1; i >= 0; i--) {
            if ((nxtblock = V7_ULONG(le32toh(fb->df_free[i]))) == 0)
              break;

            if (validateFreeBlock(mount, sb, map, nxtblock, i == 0) == 0)
                return 0;
          }
        return 1;
      }
    } else return 1;
  }
  return 0;
}

/*++
 *      v a l i d a t e
 *
 *  Validate the integrity of a Unix V7 file system. Unix V7 file systems do
 *  not include any form of signature on the disk so we have to run a set of
 *  heuristics to verify the integrity of the file system.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      dev             - pointer to specific device type (NULL if none)
 *
 * Outputs:
 *
 *      The mount point specific buffer will be overwritten.
 *
 * Returns:
 *
 *      1 if file system is valid, 0 otherwise
 *
 --*/
static int validate(
  struct mountedFS *mount,
  struct UNIXV7device *dev
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct v7_filsys *sb = (struct v7_filsys *)&data->superblk;
  struct v7_dinode inode;
  uint32_t pbn, block, *map = NULL;
  uint8_t buf[V7_BLOCKSIZE];
  struct v7_direct *dp = (struct v7_direct *)buf;
  
  if (unixv7ReadBlock(mount, V7_SUPERBLK, sb) != 0) {
    /*
     * If an explicit device is provided we can perform an extra check on the
     * super block, otherwise we just have to trust the super block to provide
     * the size of the device.
     */
    if (dev != NULL)
      if (V7_ULONG(le32toh(sb->s_fsize)) > dev->diskSize) {
        ERROR("Superblock disk size too large for \"%s\"\n", dev->name);
        return 0;
      }

    /*
     * Make sure the values in the superblock are valid among themselves.
     */
    if (le16toh(sb->s_isize) < V7_ULONG(le32toh(sb->s_fsize))) {
      if (le16toh(sb->s_nfree) <= V7_NICFREE) {
        int16_t i;

        /*
         * s_nfree <= 0 indicates that all available blocks are in use
         * so we have nothing to check.
         */
        if (le16toh(sb->s_nfree) > 0) {
          size_t mapsize = (V7_ULONG(le32toh(sb->s_fsize)) + 31) / 8;
          
          if ((map = malloc(mapsize)) == NULL) {
            ERROR("Panic: Memory allocation failure\n");
            exit(4);
          }
          memset(map, 0, mapsize);
              
          for (i = le16toh(sb->s_nfree) - 1; i >= 0; i--) {
            if ((block = V7_LONG(le32toh(sb->s_free[i]))) == 0)
              break;
                
            if (validateFreeBlock(mount, sb, map, block, i == 0) == 0)
              goto invalid;
          }
        }

        /*
         * Check the free inodes in the superblock.
         */
        if (le16toh(sb->s_ninode) > 0) {
          for (i = le16toh(sb->s_ninode); i >= 0; i--) {
            if (le16toh(sb->s_inode[i]) >=
                ((le16toh(sb->s_isize) - 2) * V7_INOPB)) {
              ERROR("Bad inode number in superblock (%u)\n",
                    le16toh(sb->s_inode[i]));
              return 0;
            }
            if (le16toh(sb->s_inode[i]) == V7_ROOTINO) {
              ERROR("Root inode is free in superblock\n");
              return 0;
            }
          }
        }

        /*
         * Check root inode for validity
         */
        if (unixv7ReadInode(mount, V7_ROOTINO, &inode) == 0) {
          ERROR("Unable to read root inode (2)\n");
          return 0;
        }

        if ((inode.di_mode & V7_IFMT) != V7_IFDIR) {
          ERROR("Root inode is not a directory\n");
          return 0;
        }

        /*
         * Read the first block of the root directory and check that the
         * first 2 entries are for "." and "..".
         */
        V7_GET3ADDR(&inode.di_addr[0], pbn);

        if (pbn == 0) {
          ERROR("Root directory starts at block 0\n");
          return 0;
        }

        if (unixv7ReadBlock(mount, pbn, buf) == 0) {
          ERROR("Unable to read first block of root directory\n");
          return 0;
        }

        if ((le16toh(dp->d_ino) != V7_ROOTINO) ||
            (strcmp(dp->d_name, ".") != 0)) {
          ERROR("Malformed first directory entry\n");
          return 0;
        }

        dp++;

        if ((le16toh(dp->d_ino) != V7_ROOTINO) ||
            (strcmp(dp->d_name, "..") != 0)) {
          ERROR("Malformed second directory entry\n");
          return 0;
        }
        data->blocks = V7_ULONG(le32toh(sb->s_fsize));

        if (!quiet) {
          char fpack[8], fname[8];

          memset(fpack, 0, sizeof(fpack));
          memset(fname, 0, sizeof(fname));
          memcpy(fpack, sb->s_fpack, sizeof(sb->s_fpack));
          memcpy(fname, sb->s_fname, sizeof(sb->s_fname));
          
          printf("Pack name: %6s, File system name: %6s, Total blocks: %u\n",
                 fpack, fname, data->blocks);
        }
        if (map != NULL)
          free(map);
        return 1;
      } else ERROR("Too many free blocks\n");
    } else ERROR("Superblock i-list larger than disk\n");
  }
 invalid:
  if (map != NULL)
    free(map);
  
  return 0;
}

/*++
 *      u n i x v 7 D i s p l a y D i r
 *  
 *  Display information about a file or directory.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      spec            - pointer to the file specification block (unused)
 *      name            - last component of file/directory name
 *      dino            - inode number of parent directory (unused)
 *      ino             - inode number of the file/directory
 *      arg             - unused argument
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7DisplayDir(
  struct mountedFS *mount,
  struct unixv7FileSpec *UNUSED(spec),
  char *name,
  v7_ino_t UNUSED(dino),
  v7_ino_t ino,
  uintptr_t UNUSED(arg)
)
{
  struct v7_dinode inode;
  char prot[16], fname[V7_DIRSIZ + 1], other[32];

  if (unixv7ReadInode(mount, ino, &inode)) {
    printf("%s:\n", name);
    
    if ((le16toh(inode.di_mode) & V7_IFMT) == V7_IFDIR) {
      off_t offset = 0;
      uint32_t lbn = 0xFFFFFFFF, pbn;
      
      while (offset < V7_LONG(le32toh(inode.di_size))) {
        uint8_t buf[V7_BLOCKSIZE];
        struct v7_direct *dp;

        if (lbn != (offset >> V7_BSHIFT)) {
          lbn = offset >> V7_BSHIFT;
          if ((pbn = bmap(mount, &inode, ino, lbn, V7_READ)) == 0) {
            ERROR("Error mapping lbn %u on inode %u\n", lbn, ino);
            return;
          }
          if (unixv7ReadBlock(mount, pbn, buf) == 0)
            return;
        }
        dp = (struct v7_direct *)&buf[offset & V7_BMASK];

        if (dp->d_ino != 0) {
          memset(fname, 0, sizeof(fname));
          memcpy(fname, dp->d_name, V7_DIRSIZ);

          other[0] = '\0';
          
          if ((strcmp(fname, ".") != 0) && (strcmp(fname, "..") != 0)) {
            if (SWISSET('f')) {
              struct v7_dinode finode;

              if (unixv7ReadInode(mount, le16toh(dp->d_ino), &finode)) {
                switch (le16toh(finode.di_mode) & V7_IFMT) {
                  case V7_IFCHR:
                    unixv7Protection(prot, 'c', le16toh(finode.di_mode));
                    unixv7Device(other, &finode);
                    break;
                
                  case V7_IFBLK:
                    unixv7Protection(prot, 'b', le16toh(finode.di_mode));
                    unixv7Device(other, &finode);
                    break;

                  case V7_IFMPC:
                    unixv7Protection(prot, 'm', le16toh(finode.di_mode));
                    break;

                  case V7_IFMPB:
                    unixv7Protection(prot, 'M', le16toh(finode.di_mode));
                    break;

                  case V7_IFDIR:
                    unixv7Protection(prot, 'd', le16toh(finode.di_mode));
                    break;

                  case V7_IFLNK:
                    unixv7Protection(prot, 'l', le16toh(finode.di_mode));
                    break;

                  case V7_IFREG:
                    unixv7Protection(prot, '-', le16toh(finode.di_mode));
                    sprintf(other, "%u", V7_LONG(le32toh(finode.di_size)));
                    break;

                  default:
                    printf("Unknown mode %o\n", le16toh(finode.di_mode));
                    return;
                }
                printf(" %s %-6d %-14s %s\n", prot, le16toh(finode.di_nlink),
                       fname, other);
              }
            } else printf("%s\n", fname);
          }
        }
        offset += sizeof(struct v7_direct);
      }
    } else {
      if (SWISSET('f')) {
        switch (le16toh(inode.di_mode) & V7_IFMT) {
          case V7_IFCHR:
            unixv7Protection(prot, 'c', le16toh(inode.di_mode));
            unixv7Device(other, &inode);
            break;
                
          case V7_IFBLK:
            unixv7Protection(prot, 'b', le16toh(inode.di_mode));
            unixv7Device(other, &inode);
            break;

          case V7_IFMPC:
            unixv7Protection(prot, 'm', le16toh(inode.di_mode));
            break;

          case V7_IFMPB:
            unixv7Protection(prot, 'M', le16toh(inode.di_mode));
            break;

          case V7_IFDIR:
            unixv7Protection(prot, 'd', le16toh(inode.di_mode));
            break;

          case V7_IFLNK:
            unixv7Protection(prot, 'l', le16toh(inode.di_mode));
            break;

          case V7_IFREG:
            unixv7Protection(prot, '-', le16toh(inode.di_mode));
            sprintf(other, "%u", V7_LONG(le32toh(inode.di_size)));
            break;

          default:
            printf("Unknown mode %o\n", le16toh(inode.di_mode));
            return;
        }
      printf(" %s %-6d %-14s %s\n", prot, le16toh(inode.di_nlink),
             name, other);
      } else printf("%s\n", name);
    }
    printf("\n");
  }
}

/*++
 *      u n i x v 7 O p e n R
 *
 *  Callback routine from walkTree() when opening a file for reading.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      spec            - pointer to the file specification block (unused)
 *      name            - last component of file/directory name
 *      dino            - inode number of parent directory
             - inode number of the file/directory
 *      arg             - open file descriptor passed here
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7OpenR(
  struct mountedFS *mount,
  struct unixv7FileSpec *UNUSED(spec),
  char *name,
  v7_ino_t dino,
  v7_ino_t ino,
  uintptr_t arg
)
{
  struct unixv7OpenFile *file = (struct unixv7OpenFile *)arg;

  if (unixv7ReadInode(mount, ino, &file->inode)) {
    /*
     * Allocate local buffer space for the file
     */
    if ((file->buffer = malloc(V7_BLOCKSIZE)) != NULL) {
      strcpy(file->name, name);
      file->parent = dino;
      file->ino = ino;
      file->mode = M_RD;
      file->mount = mount;
      file->block = 0xFFFFFFFF;
    }
  }
}

/*++
 *      u n i x v 7 O p e n W
 *
 *  Callback routine from walkTree() when opening a file for writing.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      spec            - pointer to the file specification block
 *      name            - last component of file/directory name
 *      dino            - inode number of parent directory
 *      ino             - inode number of the file/directory
 *      arg             - open file descriptor passed here
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7OpenW(
  struct mountedFS *mount,
  struct unixv7FileSpec *UNUSED(spec),
  char *name,
  v7_ino_t dino,
  v7_ino_t UNUSED(ino),
  uintptr_t arg
)
{
  struct unixv7OpenFile *file = (struct unixv7OpenFile *)arg;
  v7_ino_t temp;
  
  /*
   * We do not support superceding an existing file so check if the target
   * file already exists.
   */
  switch (lookupComponent(mount, dino, name, &temp)) {
    case 1:
      ERROR("%s: \"%s\" already exists\n", command, name);
      return;

    case 0:
      break;
      
    default:
      return;
  }

  /*
   * Allocate local buffer space for the file
   */
  if ((file->buffer = malloc(V7_BLOCKSIZE)) != NULL) {
    if ((file->ino = ialloc(mount, &file->inode, V7_IFREG)) != 0) {
      strcpy(file->name, name);
      file->parent = dino;
      file->mode = M_WR;
      file->mount = mount;
      file->block = 0xFFFFFFFF;
    } else {
      ERROR("%s: Failed to allocate an inode\n", command);
      free(file->buffer);
    }
  } else ERROR("%s: Failed to allocate file buffer\n", command);
}

/*++
 *      u n i x v 7 R e a d B y t e s
 *
 *  Read a sequence of bytes from an open file.
 *
 * Inputs:
 *
 *      file            - pointer to open file descriptor
 *      buf             - pointer to buffer to receive the data
 *      len             - # of bytes of data to read
 *
 * Outputs:
 *
 *      The buffer will be overwritten by up to "len" bytes
 *
 * Returns:
 *
 *      # of bytes read from the file (may be less than "len"), 0 if EOF
 *
 --*/
int unixv7ReadBytes(
  struct unixv7OpenFile *file,
  char *buf,
  int len
)
{
  struct mountedFS *mount = file->mount;
  int count = 0, cpylen, bufoffset;
  
  if (file->offset < V7_LONG(le32toh(file->inode.di_size))) {
    off_t available = V7_LONG(le32toh(file->inode.di_size)) - file->offset;

    if (available < len)
      len = available;

    while (len) {
      if (file->block != (file->offset >> V7_BSHIFT)) {
        uint32_t pbn;
        
        file->block = file->offset >> V7_BSHIFT;
        if ((pbn = bmap(mount, &file->inode, file->ino, file->block, V7_READ)) == 0) {
          ERROR("%s: Error mapping lbn %u on inode %u for read\n",
                command, file->block, file->ino);
          return 0;
        }
        if (unixv7ReadBlock(mount, pbn, file->buffer) == 0)
          return 0;
      }
      bufoffset = file->offset & (V7_BLOCKSIZE - 1);
      cpylen = V7_BLOCKSIZE - bufoffset;
      if (len < cpylen)
        cpylen = len;

      memcpy(buf, &file->buffer[bufoffset], cpylen);
      count += cpylen;
      buf += cpylen;
      len -= cpylen;
      file->offset += cpylen;
    }
    return count;
  }
  return 0;
}

/*++
 *      u n i x v 7 W r i t e B y t e s
 *
 *  Write a sequence of bytes to an open file.
 *
 * Inputs:
 *
 *      file            - pointer to an open file descriptor
 *      buf             - pointer a buffer with the data to be written
 *      len             - # of bytes of data to write
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      # of bytes written to the file (may be less than "len"), 0 if error
 *
 --*/
int unixv7WriteBytes(
  struct unixv7OpenFile *file,
  char *buf,
  int len
)
{
  struct mountedFS *mount = file->mount;
  int count = 0, cpylen, bufoffset;

  while (len) {
    file->block = file->offset >> V7_BSHIFT;
    
    bufoffset = file->offset & (V7_BLOCKSIZE - 1);
    cpylen = V7_BLOCKSIZE - bufoffset;

    if (len < cpylen)
      cpylen = len;

    memcpy(&file->buffer[bufoffset], buf, cpylen);
    count += cpylen;
    buf += cpylen;
    len -= cpylen;
    file->offset += cpylen;

    if ((file->offset & (V7_BLOCKSIZE - 1)) == 0) {
      uint32_t pbn;
      
      /*
       * The current buffer is full, write the block out to disk.
       */
      if ((pbn = bmap(mount, &file->inode, file->ino, file->block, V7_WRITE)) == 0) {
        ERROR("%s: Error mapping lbn %u on inode %u for write\n",
              command, file->block, file->ino);
        return 0;
      }
      if (unixv7WriteBlock(mount, pbn, file->buffer) == 0)
        return 0;
    }
  }
  return count;
}

/*++
 *      u n i x v 7 R e a d B l o c k
 *
 *  Read a block from a UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      block           - logical block # in the range 0 - N
 *      buf             - buffer to receive data, if NULL use the mount
 *                        point specific buffer
 *
 * Outputs:
 *
 *      The block will be read into the specified buffer
 *
 * Returns:
 *
 *      1 if successful, 0 otherwise
 *
 --*/
int unixv7ReadBlock(
  struct mountedFS *mount,
  unsigned int block,
  void *buf
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  void *buffer = buf == NULL ? data->buf : buf;
  int status;

  if (block >= data->blocks) {
    ERROR("Attempt to read block (%u) outside file system \"%s\"\n",
          block, mount->name);
    return 0;
  }

#ifdef DEBUG
  if ((mount->flags & FS_DEBUG) != 0)
    fprintf(DEBUGout, ">> %s: (unixv7) Reading logical block %o\n",
            mount->name, block);
#endif

  status = FSioReadBlock(mount, data->offset + block, buffer);

  if (status == 0)
    ERROR("I/O error on \"%s\"\n", mount->name);

  return status;
}

/*++
 *      u n i x v 7 W r i t e B l o c k
 *
 *  Write a block to a UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      block           - logical block # in the range 0 - N
 *      buf             - buffer containing data, if NULL use the mount
 *                        point specific buffer
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1 if successful, 0 otherwise
 *
 --*/
int unixv7WriteBlock(
  struct mountedFS *mount,
  unsigned int block,
  void *buf
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  void *buffer = buf == NULL ? data->buf : buf;
  int status;

  if (block >= data->blocks) {
    ERROR("Attempt to write block (%u) outside file system \"%s\"\n",
          block, mount->name);
    return 0;
  }

#ifdef DEBUG
  if ((mount->flags & FS_DEBUG) != 0)
    fprintf(DEBUGout, ">> %s: (unixv7) Writing logical block %o\n",
            mount->name, block);
#endif

  status = FSioWriteBlock(mount, data->offset + block, buffer);

  if (status == 0)
    ERROR("I/O error on \"%s\"\n", mount->name);

  return status;
}

/*++
 *      u n i x v 7 R e a d I n o d e
 *
 *  Read an inode from a UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      ino             - the inode number to be read
 *      buf             - pointer to buffer to receive the data
 *
 * Outputs:
 *
 *      The buffer will be overwritten by the inode data.
 *
 * Returns:
 *
 *      1 if read was successful, 0 otherwise
 *
 --*/
int unixv7ReadInode(
  struct mountedFS *mount,
  v7_ino_t ino,
  struct v7_dinode *buf
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  off_t offset = (itoff(ino) * sizeof(struct v7_dinode)) +
                        (data->offset * V7_BLOCKSIZE);

  return FSioReadBlob(mount, offset, sizeof(struct v7_dinode), buf);
}

/*++
 *      u n i x v 7 W r i t e I n o d e
 *
 *  Write an inode to a UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *      ino             - the inode number to be read
 *      buf             - pointer to buffer with the data
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1 if write was successful, 0 otherwise
 *
 --*/
int unixv7WriteInode(
  struct mountedFS *mount,
  v7_ino_t ino,
  struct v7_dinode *buf
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  off_t offset = (itoff(ino) * sizeof(struct v7_dinode)) +
                        (data->offset * V7_BLOCKSIZE);

  return FSioWriteBlob(mount, offset, sizeof(struct v7_dinode), buf);
}

/*++
 *      u n i x v 7 M o u n t
 *
 *  Verify that the open container file is a UNIX V7 file system. Since UNIX V7
 *  does not write an explicit signatture on the disk we can only verify some
 *  of the super-block entries.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *                        (not in the mounted file system list)
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      -1 if problem with command input values,
 *      1 if a valid UNIX V7 file system,
 *      0 otherwise
 *
 --*/
static int unixv7Mount(
  struct mountedFS *mount
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct stat stat;
  unsigned int offset = 0;
  
  if (fstat(fileno(mount->container), &stat) == 0) {
    if (SWISSET('o')) {
      char *endptr;

      offset = strtoul(SWGETVAL('o'), &endptr, 10);

      if (offset >= (stat.st_size / V7_BLOCKSIZE)) {
        fprintf(stderr, "mount: offset past end of file\n");
        return -1;
      }
    }
  
    data->offset = offset;
    data->blocks = (int)(stat.st_size / V7_BLOCKSIZE) - offset;

    if (SWISSET('t')) {
      struct UNIXV7device *dev = UNIXV7Devices;
      char *type = SWGETVAL('t');
      
      while (dev->name != NULL) {
        if (strcmp(type, dev->name) == 0)
          return validate(mount, dev);
        dev++;
      }
      fprintf(stderr, "mount: unknown disk type - \"%s\"\n", type);
      return -1;
    }
    return validate(mount, NULL);
  }
  return 0;
}

/*++
 *      u n i x v 7 U m o u n t
 *
 *  Unmount the UNIX V7 file system, releasing any storage allocated.
 *
 * Inputs:
 *
 *      mount           - pointer to the mounted file system descriptor
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7Umount(
  struct mountedFS *UNUSED(mount)
)
{
}

/*++
 *      u n i x v 7 S i z e
 *
 *  Return the size of a UNIX V7 container file.
 *
 * Inputs:
 *
 *      None
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      Size of the container file in blocks.
 *
 --*/
static size_t unixv7Size(void)
{
  struct UNIXV7device *dev = UNIXV7Devices;

  if (SWISSET('t')) {
    while (dev->name != NULL) {
      if (strcmp(dev->name, SWGETVAL('t')) == 0)
        return dev->diskSize;
      dev++;
    }
  }

  if (SWISSET('b'))
    return strtoul(SWGETVAL('b'), NULL, 10);

  return UNIXV7Devices[0].diskSize;
}

/*++
 *      u n i x v 7 N e w f s
 *
 *  Create an empty UNIX V7 file system.
 *
 * Inputs:
 *
 *      mount           - pointer to a mounted file system descriptor
 *                        (not in mounted file system list)
 *      size            - the size (in bytes) of the file system (unused)
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1 if the file system was successfully created, 0 otherwise
 *
 --*/
static int unixv7Newfs(
  struct mountedFS *mount,
  size_t size
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct UNIXV7device *dev = UNIXV7Devices;
  char *type = SWGETVAL('t');
  v7_ushort inodeBlocks;
  uint8_t buf[V7_BLOCKSIZE], blk[V7_BLOCKSIZE];
  struct v7_dinode *dp = (struct v7_dinode *)buf;
  struct v7_fblk *fp = (struct v7_fblk *)blk;
  struct v7_filsys *filsys = (struct v7_filsys *)&data->superblk;
  uint16_t inodes, i;
  int32_t tfree = 0;
  v7_time_t now = unixv7Timestamp();
  v7_daddr_t nextblk, nextfree, rootdir;
    
  if (SWISSET('t')) {
    while (dev->name != NULL) {
      if (strcmp(type, dev->name) == 0)
        goto found;

      dev++;
    }
    fprintf(stderr, "newfs: Unknown device type \"%s\"\n", type);
    return 0;
  }

  if (SWISSET('b')) {
    size = strtoul(SWGETVAL('b'), NULL, 10);
    goto found2;
  }

 found:
  size = dev->diskSize;
 found2:
  inodeBlocks = size / 25;

  data->blocks = size;
        
  /*
   * The '-i' switch allows for overriding the number of blocks to
   * be used for inodes.
   */
  if (SWISSET('i')) {
    char *endptr;
    uint16_t n = strtoul(SWGETVAL('i'), &endptr, 10);

    if ((n < V7_INOPB) || (n > 65500)) {
      fprintf(stderr,
              "newfs: -i parameter must be in the range %d - 65500\n", V7_INOPB);
      return 0;

      if ((n / V7_INOPB) > (size / 2)) {
        fprintf(stderr,
                "newfs: inodes cannot occupy more than 50%% of the disk\n");
        return 0;
      }
      inodeBlocks = (n + V7_INOPB - 1) / V7_INOPB;
    }
  }

  inodes = inodeBlocks * V7_INOPB;
        
  memset(buf, 0, sizeof(buf));
  memset(blk, 0, sizeof(blk));

  /*
   * Write the last block in the container file
   */
  if (unixv7WriteBlock(mount, size - 1, blk) == 0)
    return 0;
  
  filsys->s_isize = htole16(inodeBlocks + 2);
  filsys->s_fsize = htole32(V7_LONG(size));

  filsys->s_m = V7_STEPSIZE;
  filsys->s_n = V7_CYLSIZE;
  
  /*
   * Note becuase of the above checks we can always assume that there
   * more than enough free blocks to fill s_free[].
   */
  nextblk = nextfree = le16toh(filsys->s_isize);
  filsys->s_nfree = htole16(V7_NICFREE);
  for (i = 0; i < V7_NICFREE; i++) {
    filsys->s_free[i] = htole32(V7_LONG(nextfree));
    tfree++;
    nextfree++;
  }

  /*
   * Build the on-disk free block list.
   */
  while (nextfree < (v7_daddr_t)size) {
    fp->df_free[le16toh(fp->df_nfree)] = htole32(V7_LONG(nextfree));
    fp->df_nfree = htole16(le16toh(fp->df_nfree) + 1);
    tfree++;
    nextfree++;
    if (le16toh(fp->df_nfree) == V7_NICFREE) {
      if (unixv7WriteBlock(mount, nextblk, blk) == 0)
        return 0;
      fp->df_nfree = 0;
      nextblk = V7_LONG(le32toh(fp->df_free[0]));
    }
  }

  /*
   * Write any partial free list entry
   */
  if (fp->df_nfree != 0) {
    if (unixv7WriteBlock(mount, nextblk, blk) == 0)
      return 0;
    nextblk = V7_LONG(le32toh(fp->df_free[0]));
  }

  /*
   * Terminate the list with a zero entry
   */
  fp->df_nfree = htole16(1);
  fp->df_free[0] = 0;
  if (unixv7WriteBlock(mount, nextblk, blk) == 0)
    return 0;
  
  filsys->s_tfree = V7_LONG(htole32(tfree));
  
  /*
   * Zero out all of the inodes (marking them as free) and build the
   * super block cache starting at inode ROOTINO + 1.
   */
  for (i = 2; i != le16toh(filsys->s_isize); i++) {
    if (unixv7WriteBlock(mount, i, buf) == 0)
      return 0;
    filsys->s_tinode = htole16(le16toh(filsys->s_tinode) + V7_INOPB);
  }
  filsys->s_ninode = htole16(MIN(V7_NICINOD, inodes));
  for (i = 0; i < le16toh(filsys->s_ninode); i++)
    filsys->s_inode[i] = htole16(i + 3);

  filsys->s_fmod = 1;
  
  /*
   * Hand build the first 2 inodes:
   *      1. A zero length regular file with zero links
   *      2. The root directory
   */
  filsys->s_tinode = htole16(le16toh(filsys->s_tinode) - 2);

  if ((rootdir = emptyDirectory(mount, V7_ROOTINO, V7_ROOTINO)) == 0)
    return 0;
  
  dp->di_mode = htole16(V7_IFREG);
  dp->di_atime = dp->di_mtime = dp->di_ctime = htole32(V7_LONG(now));
        
  dp++;
  dp->di_mode = htole16(V7_IFDIR | 0777);
  dp->di_nlink = htole16(2);
  dp->di_size = htole32(V7_ULONG(2 * sizeof(struct v7_direct)));
  V7_PUT3ADDR(&dp->di_addr[0], rootdir);
  dp->di_atime = dp->di_mtime = dp->di_ctime = htole32(V7_LONG(now));
  if (unixv7WriteBlock(mount, 2, buf) == 0)
    return 0;
        
  filsys->s_time = htole32(V7_LONG(now));

  if (SWISSET('f') && (SWGETVAL('f') != NULL))
    strncpy(filsys->s_fname, SWGETVAL('f'), sizeof(filsys->s_fname));

  if (SWISSET('p') && (SWGETVAL('p') != NULL))
    strncpy(filsys->s_fpack, SWGETVAL('p'), sizeof(filsys->s_fpack));

  /*
   * Write out the super block.
   */
  update(mount);
  return 1;
}

/*++
 *      u n i x v 7 M k d i r
 *
 *  Create a new empty directory.
 *
 * Inputs:
 *
 *      mount           - pointer to the mounted file system descriptor
 *      unit            - partition number (unused)
 *      fname           - pointer to filename string
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      1 if directory was successfully created, 0 otherwise
 *
 --*/
static int unixv7Mkdir(
  struct mountedFS *mount,
  uint8_t UNUSED(unit),
  char *fname
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  struct unixv7FileSpec spec;
  v7_ino_t ino = V7_ROOTINO;
  int i;
  
  if (unixv7ParseFilespec(fname, &spec, UNIXV7_M_NONE, UNIXV7_A_NEW) == 0) {
    fprintf(stderr, "mkdir: syntax error in directory specification \"%s\"\n",
            fname);
    return 0;
  }

  if (spec.depth == 0) {
    fprintf(stderr, "mkdir: directory specification missing\n");
    return 0;
  }

  for (i = 0; i < spec.depth; i++) {
    struct v7_dinode inode;
    v7_ino_t next;

    if (strlen(spec.comp[i]) > V7_DIRSIZ) {
      fprintf(stderr, "mkdir: name component too long \"%s\"\n", spec.comp[i]);
      return 0;
    }
        
    switch (lookupComponent(mount, ino, spec.comp[i], &next)) {
      /*
       * Error detected, bail out now.
       */
      case -1:
        return 0;

      case 0:
        /*
         * The component does not exist, see if we should create it
         */
        if (SWISSET('p') || (i == (spec.depth - 1))) {
          struct v7_dinode di;
          v7_daddr_t blk;
        
          if ((next = ialloc(mount, &di, V7_IFDIR)) == 0)
            goto fail;

          if ((blk = emptyDirectory(mount, next, ino)) == 0)
            goto fail;

          di.di_nlink = htole16(2);
          di.di_mode = htole16(le16toh(di.di_mode) | 0755);
          di.di_uid = htole16(data->user);
          di.di_gid = htole16(data->group);
          di.di_size = htole32(V7_ULONG(2 * sizeof(struct v7_direct)));
          V7_PUT3ADDR(&di.di_addr[0], blk);
          di.di_atime = di.di_mtime = di.di_ctime = htole32(unixv7Timestamp());

          if (unixv7WriteInode(mount, next, &di) == 0)
            goto fail;

          if (addComponent(mount, ino, next, spec.comp[i], 1) == 0)
            goto fail;
        } else {
          fprintf(stderr, "mkdir: missing intermediate directory \"%s\"\n",
                  spec.comp[i]);
          goto fail;
        }
        break;

      case 1:
        /*
         * The component already exists, make sure that it is a directory
         */
        if (unixv7ReadInode(mount, next, &inode) == 0)
          goto fail;
        if ((le16toh(inode.di_mode) & V7_IFMT) != V7_IFDIR) {
          fprintf(stderr, "mkdir: \"%s\" exists and is not a directory\n",
                  spec.comp[i]);
          goto fail;
        }
        break;
    }
    ino = next;
  }
  update(mount);
  return 1;
 fail:
  update(mount);
  return 0;
}

/*++
 *      u n i x v 7 S e t
 *
 *  Set mount poinmt specific values.
 *
 * Inputs:
 *
 *      mount           - pointer to the mounted file system descriptor
 *      unit            - device unit number (unused)
 *      present         - device unit number present (unused)
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7Set(
  struct mountedFS *mount,
  uint8_t UNUSED(unit),
  uint8_t UNUSED(present)
)
{
  struct UNIXV7data *data = &mount->unixv7data;
  int idx = 0;
  uint16_t id;
  
  while (setCmds[idx] != NULL) {
    if (strcmp(words[1], setCmds[idx]) == 0) {
      switch (idx) {
        case UNIXV7SET_UID:
          if (args == 3) {
            if (sscanf(words[2], "%hu", &id) == 1) {
              data->user = id;
            } else fprintf(stderr,
                          "unixv7: UID syntax error \"%s\"\n", words[2]);
          } else fprintf(stderr, "unixv7: Invalid syntax for \"set uid\"\n");
          return;
          
        case UNIXV7SET_GID:
          if (args == 3) {
            if (sscanf(words[2], "%hu", &id) == 1) {
              data->group = id;
            } else fprintf(stderr,
                          "unixv7: GID syntax error \"%s\"\n", words[2]);
          } else fprintf(stderr, "unixv7: Invalid syntax for \"set gid\"\n");
          return;

        default:
          fprintf(stderr, "unixv7: \"%s\" not implemented\n", words[1]);
          return;
      }
    }
    idx++;
  }
  fprintf(stderr, "unixv7: Unknown set command \"%s\"\n", words[1]);
}

/*++
 *      u n i x v 7 I n f o
 *
 *  Display information about the internal structure of the Unix V7 file
 *  system.
 *
 * Inputs:
 *
 *      mount           - pointer to the mounted file system descriptor
 *      unit            - device unit number (unused)
 *      present         - device unit number present (unused)
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7Info(
  struct mountedFS *mount,
  uint8_t UNUSED(unit),
  uint8_t UNUSED(present)
)
{
  printf("/\n");
  dumpDirectory(mount, V7_ROOTINO, 0);
}

/*++
 *      u n i x v 7 D i r
 *
 *  Produce a full or brief directory listing.
 *
 * Inputs:
 *
 *      mount           - pointer to the mounted file system descriptor
 *      unit            - device unit number (unused)
 *      fname           - pointer to filename string
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7Dir(
  struct mountedFS *mount,
  uint8_t UNUSED(unit),
  char *fname
)
{
  struct unixv7FileSpec spec;

  if (unixv7ParseFilespec(fname, &spec, UNIXV7_M_ALLOW, UNIXV7_A_EXIST) == 0) {
    fprintf(stderr, "dir: syntax error in file spec \"%s\"\n", fname);
    return;
  }

  if (spec.depth == 0)
    unixv7DisplayDir(mount, &spec, "/", V7_ROOTINO, V7_ROOTINO, 0);
  else walkTree(mount, &spec, 0, V7_ROOTINO, unixv7DisplayDir, 0);
}

/*++
 *      u n i x v 7 O p e n F i l e R
 *
 *  Open a UNIX V7 file for reading.
 *
 * Inputs:
 *
 *      mount           - pointer to the mounted file system descriptor
 *      unit            - device unit number (unused)
 *      fname           - pointer to filename string
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      Pointer to open file descriptor, NULL if open fails
 *
 --*/
static void *unixv7OpenFileR(
  struct mountedFS *mount,
  uint8_t UNUSED(unit),
  char *fname
)
{
  struct unixv7OpenFile *file;
  struct unixv7FileSpec spec;

  if (unixv7ParseFilespec(fname, &spec, UNIXV7_M_NONE, UNIXV7_A_EXIST) == 0) {
    fprintf(stderr, "Failed to parse filename \"%s\"\n", fname);
    return NULL;
  }

  if ((file = malloc(sizeof(struct unixv7OpenFile))) != NULL) {
    memset(file, 0, sizeof(struct unixv7OpenFile));

    file->mode = M_UNKNOWN;
    walkTree(mount, &spec, 0, V7_ROOTINO, unixv7OpenR, (uintptr_t)file);

    if (file->mode == M_UNKNOWN) {
      fprintf(stderr, "%s: Failed for open \"%s\" for reading\n",
              command, fname);
      free(file);
      return NULL;
    }
  }
  return file;
}

/*++
 *      u n i x v 7 O p e n F i l e W
 *
 *  Open a UNIX V7 file for writing.
 *
 * Inputs:
 *
 *      mount           - pointer to the mounted file system descriptor
 *      unit            - device unit number (unused)
 *      fname           - pointer to filename string
 *      size            - estimated file size (in bytes) (unused)
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      Pointer to open file descriptor, NULL if open fails
 *
 --*/
static void *unixv7OpenFileW(
  struct mountedFS *mount,
  uint8_t UNUSED(unit),
  char *fname,
  off_t UNUSED(size)
)
{
  struct unixv7OpenFile *file;
  struct unixv7FileSpec spec;

  if (unixv7ParseFilespec(fname, &spec, UNIXV7_M_NONE, UNIXV7_A_NEW) == 0) {
    fprintf(stderr, "Failed to parse filename \"%s\"\n", fname);
    return NULL;
  }

  if ((file = malloc(sizeof(struct unixv7OpenFile))) != NULL) {
    memset(file, 0, sizeof(struct unixv7OpenFile));

    file->mode = M_UNKNOWN;
    walkTree(mount, &spec, 0, V7_ROOTINO, unixv7OpenW, (uintptr_t)file);

    if (file->mode == M_UNKNOWN) {
      fprintf(stderr, "%s: Failed for open \"%s\" for writing\n",
              command, fname);
      free(file);
      return NULL;
    }
  }
  return file;
}

/*++
 *      u n i x v 7 F i l e S i z e
 *
 *  Return an estimate of the size of a currently open file. This routine
 *  return the size stored in the file's inode.
 *
 * Inputs:
 *
 *      filep           - pointer to open file descriptor
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      The file's current size.
 *
 --*/
static off_t unixv7FileSize(
  void *filep
)
{
  struct unixv7OpenFile *file = filep;

  return V7_LONG(le32toh(file->inode.di_size));
}

/*++
 *      u n i x v 7 C l o s e F i l e
 * 
 *  Close an open UNIX V7 file.
 *
 * Inputs:
 *
 *      filep           - pointer to open file descriptor
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7CloseFile(
  void *filep
)
{
  struct unixv7OpenFile *file = filep;
  struct mountedFS *mount = file->mount;
  struct UNIXV7data *data = &mount->unixv7data;
  
  if (file->mode == M_WR) {
    if ((file->offset & (V7_BLOCKSIZE - 1)) != 0) {
      uint32_t pbn;
      
      /*
       * Partial block is in memory, flush it out to disk
       */
      file->block = file->offset >> V7_BSHIFT;
      if ((pbn = bmap(mount, &file->inode, file->ino, file->block, V7_WRITE)) == 0) {
        ERROR("%s: Error mapping lbn %u on inode %u flushing last block\n",
              command, file->block, file->ino);
        goto done;
      }
      if (unixv7WriteBlock(mount, pbn, file->buffer) == 0)
        goto done;
    }
    
    file->inode.di_mode = htole16(le16toh(file->inode.di_mode) | 0644);
    file->inode.di_nlink = htole16(1);
    file->inode.di_uid = htole16(data->user);
    file->inode.di_gid = htole16(data->group);
    file->inode.di_size = htole32(V7_LONG(file->offset));
    file->inode.di_atime =
      file->inode.di_mtime =
        file->inode.di_ctime = htole32(V7_LONG(unixv7Timestamp()));

    if (unixv7WriteInode(mount, file->ino, &file->inode))
      if (addComponent(mount, file->parent, file->ino, file->name, 0))
        update(mount);
  }

 done:
  if (file->buffer != NULL)
    free(file->buffer);
  free(file);
}

/*++
 *      u n i x v 7 R e a d F i l e
 *
 *  Read data from a UNIX V7 file to a supplied buffer.
 *
 * Inputs:
 *
 *      filep           - pointer to open file descriptor
 *      buf             - pointer to buffer
 *      buflen          - length of the supplied buffer
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      # of bytes of data read, 0 means EOF or error
 *
 --*/
static size_t unixv7ReadFile(
  void *filep,
  void *buf,
  size_t buflen
)
{
  struct unixv7OpenFile *file = filep;
  char *bufr = buf;

  if (SWISSET('a')) {
    char ch;
    size_t count = 0;

    /*
     * Make sure we always have space for the terminating <CR>LF>.
     */
    buflen--;

    /*
     * Read a full or partial line from the open file.
     */
    while ((buflen != 0) && (unixv7ReadBytes(file, &ch, 1) == 1)) {
      if (ch == '\n') {
        bufr[count++] = '\r';
        bufr[count++] = '\n';
        break;
      }
      bufr[count++] = ch;
      buflen--;
    }
    return count;
  }
  return unixv7ReadBytes(file, bufr, buflen);
}

/*++
 *      u n i x v 7 W r i t e F i l e
 *
 *  Write data to a UNIX V7 file. If ASCII mode is active and the buffer
 *  is terminated with <CRLF> convert it to <LF>.
 *
 * Inputs:
 *
 *      file            - pointer to open file descriptor
 *      buf             - pointer to the buffer to be written
 *      buflen          - length of the supplied buffer
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      # of bytes of data written, 0 means error
 *
 --*/
static size_t unixv7WriteFile(
  void *file,
  void *buf,
  size_t buflen
)
{
  if (SWISSET('a')) {
    char *bufw = buf;

    if (buflen >= 2)
      if ((bufw[buflen - 2] == '\r') && (bufw[buflen - 1] == '\n')) {
        bufw[buflen - 2] = '\n';
        buflen--;
      }
  }
  return unixv7WriteBytes(file, buf, buflen);
}

/*++
 *      u n i x v 7 D e l e t e F i l e
 * 
 *  Delete a file from a UNIX V7 file system.
 *
 * Inputs:
 *
 *      filep           - pointer to open file descriptor (open for read)
 *      fname           - pointer to filename string (unused)
 *
 * Outputs:
 *
 *      None
 *
 * Returns:
 *
 *      None
 *
 --*/
static void unixv7DeleteFile(
  void *filep,
  char *UNUSED(fname)
)
{
  struct unixv7OpenFile *file = filep;
  struct mountedFS *mount = file->mount;

  if (unlinkComponent(mount, file->parent, file->name) == 0)
    ERROR("%s: \"%s\" not found\n", command, file->name);

  update(mount);

  unixv7CloseFile(file);
}

/*++
 *      u n i x v 7 F S
 *
 *  Descriptor for accessing Unix V7 file systems.
 *
 --*/
struct FSdef unixv7FS = {
  NULL,
  "unixv7",
  "unixv7           Unix V7 file system\n",
  0,
  V7_BLOCKSIZE,
  unixv7Mount,
  unixv7Umount,
  unixv7Size,
  unixv7Newfs,
  unixv7Mkdir,
  unixv7Set,
  unixv7Info,
  unixv7Dir,
  unixv7OpenFileR,
  unixv7OpenFileW,
  unixv7FileSize,
  unixv7CloseFile,
  unixv7ReadFile,
  unixv7WriteFile,
  unixv7DeleteFile,
  NULL,                                 /* No tape support functions */
  NULL,
  NULL,
  NULL
};
