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
#ifndef __UNIXV7_H__
#define __UNIXV7_H__

/*
 * General disk layout:
 *
 * Block
 *       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 0     |                            Reserved                           |
 *       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 1     |                           Super Block                         |
 *       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * 2     |                             I nodes                           |
 *       |                              ...                              |
 *       |                              ...                              |
 *       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *       |                           Data Blocks                         |
 *       |                              ...                              |
 *       |                              ...                              |
 *       |                              ...                              |
 *       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *                                 End of Volume
 */

/*
 * Common data types
 */
typedef int16_t v7_short;
typedef uint16_t v7_ushort;
typedef int16_t v7_int;
typedef uint16_t v7_uint;
typedef int32_t v7_long;
typedef uint32_t v7_ulong;

typedef int32_t v7_daddr_t;
typedef uint16_t v7_ino_t;
typedef int32_t v7_time_t;
typedef int32_t v7_off_t;

/*
 * 32-bit longword values are stored with the high 16-bit word first
 */
#define V7_LONG(v) \
  ((v7_long)((((v) & 0xFFFF) << 16) | (((v) & 0xFFFF0000) >> 16)))
#define V7_ULONG(v) \
  ((v7_ulong)((((v) & 0xFFFF) << 16) | (((v) & 0xFFFF0000) >> 16)))

#define V7_BLOCKSIZE    512
#define V7_BSHIFT       9               /* Log2(V7_BLOCKSIZE) */
#define V7_BMASK        0777            /* V7_BLOCKSIZE - 1 */
#define V7_NINDIR       ((int)(V7_BLOCKSIZE / sizeof(v7_daddr_t)))
#define V7_NSHIFT       7               /* Log2(NINDIR) */
#define V7_NMASK        0177            /* NINDIR - 1 */

#define V7_ROOTINO      ((v7_ino_t)2)   /* i number of all roots */
#define V7_SUPERBLK     1               /* Super block address */
#define V7_DIRSIZ       14              /* Max characters  per directory */
#define V7_NICFREE      50              /* Number of superblock free blocks */
#define V7_NICINOD      100             /* Number of superblock inodes */

#define V7_STEPSIZE     9               /* Default step for freelist spacing */
#define V7_CYLSIZE      400             /* Default cylinder size for spacing */

/*
 * Super-block
 */
#pragma pack(push, 1)
struct v7_filsys {
  v7_ushort             s_isize;        /* Size in blocks of i-list */
  v7_daddr_t            s_fsize;        /* Size in blocks of entire volume */
  v7_short              s_nfree;        /* Number of addresses in s_free */
  v7_daddr_t            s_free[V7_NICFREE];/* Free block list */
  v7_short              s_ninode;       /* Number of i-nodes in s_free */
  v7_ino_t              s_inode[V7_NICINOD];/* Free i-node list */
  char                  s_flock;        /* Lock during free list manip. */
  char                  s_ilock;        /* Lock during i-list manip. */
  char                  s_fmod;         /* Super-block modified flag */
  char                  s_ronly;        /* Mounted read-only flag */
  v7_time_t             s_time;         /* Last super-block update */
  /*
   * Remainder not maintained by this version of the system
   */
  v7_daddr_t            s_tfree;        /* Total free blocks */
  v7_ino_t              s_tinode;       /* Total free inodes */
  v7_short              s_m;            /* Interleave factor */
  v7_short              s_n;            /* " " */
  char                  s_fname[6];     /* File system name */
  char                  s_fpack[6];     /* File system pack name */
};

/*
 * On-disk free space list at the head of each free block.
 */
struct v7_fblk {
  v7_int                df_nfree;       /* Number of addresses in df_free */
  v7_daddr_t            df_free[V7_NICFREE];
};

/*
 * On-disk inode structure
 */
struct v7_dinode {
  v7_ushort             di_mode;        /* Mode and type of file */
  v7_short              di_nlink;       /* Number of links to file */
  v7_short              di_uid;         /* Owner's user ID */
  v7_short              di_gid;         /* Owner's group ID */
  v7_off_t              di_size;        /* Number of bytes in file */
  char                  di_addr[40];    /* Disk block addresses */
  v7_time_t             di_atime;       /* Time last accessed */
  v7_time_t             di_mtime;       /* Time last modified */
  v7_time_t             di_ctime;       /* Time created */
};
#define V7_INOPB        8               /* 8 inodes per block */
#define itod(x)         ((v7_daddr_t)((x + 15) >> 3))
#define itoo(x)         ((v7_uint)((x + 15) & 07))
#define itoff(x)        ((v7_uint)(x + 15))

/*
 * The 40 address bytes:
 *
 *      39 used; 13 addresses of 3 bytes each
 */
#define NADDR           13

/*
 * Mode bits
 */
#define V7_IFMT         0170000         /* Type of file */
#define   V7_IFCHR      0020000         /* Character special */
#define   V7_IFBLK      0060000         /* Block special */
#define   V7_IFMPC      0030000         /* Multiplex character special */
#define   V7_IFMPB      0070000         /* Multiplex block special */
#define   V7_IFDIR      0040000         /* Directory */
#define   V7_IFLNK      0120000         /* Symbolic link */
#define   V7_IFREG      0100000         /* Regular */
#define V7_ISUID        0004000         /* Set user ID on execution */
#define V7_ISGID        0002000         /* Set group ID on execution */
#define V7_ISVTX        0001000         /* Save swapped text even after use */
#define V7_IREAD        0000400         /* Read permission, owner */
#define V7_IWRITE       0000200         /* Write permission, owner */
#define V7_IEXEC        0000100         /* Execute/Search permission, owner */

struct v7_direct {
  v7_ino_t              d_ino;          /* Inode number */
  char                  d_name[V7_DIRSIZ]; /* File name */
};
#pragma pack(pop)

/*
 * Disk block addresses are stored on disk in 2 formats:
 *
 *  1 - In the inode addr array, 3 bytes each which limits disk size to 16GB
 *  2 - In the free block list, 4 byte v7_long values
 */
#define V7_GET3ADDR(p, v) \
  { uint8_t *tmp = (uint8_t *)p; \
    v = *tmp++ << 16; v |= *tmp++; v |= *tmp++ << 8; \
  }
#define V7_PUT3ADDR(p, v) \
  { uint8_t *tmp = (uint8_t *)p; \
    *tmp++ = (v >> 16) & 0xFF; *tmp++ = v & 0xFF; *tmp++ = (v >> 8) & 0xFF; \
  }
#define V7_GET4ADDR(p, v) \
  { uint16_t *tmp = (uint16_t *)p; \
    v = le16toh(*tmp++) << 16; v |= le16toh(*tmp); \
  }
#define V7_PUT4ADDR(p, v) \
  { uint16_t *tmp = (uint16_t *)p; \
    *tmp++ = htole16(v) >> 16; *tmp = htole16(v & 0xFFFF); \
  }

#define V7_READ         0               /* Read operation */
#define V7_WRITE        1               /* Write operation */

/*
 * Structure to describe a file/directory name. * and % may be used as wild
 * card characters within each component of a name.
 */
#define UNIXV7_COMP     100             /* Maximum name depth */

struct unixv7FileSpec {
  int                   depth;          /* Actual depth of this name */
  char                  wildcard;       /* Wildcard options */
  char                  access;         /* Access mode (existing/new file) */
  char                  *comp[UNIXV7_COMP];
};
#define UNIXV7_M_NONE   0000            /* Wild cards not allowed */
#define UNIXV7_M_ALLOW  0001            /* Wild cards allowed */

#define UNIXV7_A_EXIST  0000            /* Access an existing file */
#define UNIXV7_A_NEW    0001            /* Create a new file */

/*
 * Device descriptor
 */
struct UNIXV7device {
  char                  *name;          /* Device name */
  size_t                diskSize;       /* # of blocks on device */
};

/*
 * Structure to define an open file.
 */
struct unixv7OpenFile {
  char                  name[V7_DIRSIZ + 1]; /* file name */
  v7_ino_t              parent;         /* inode number of parent directory */
  v7_ino_t              ino;            /* inode number of file */
  struct v7_dinode      inode;          /* on-disk inode copy */
  enum openMode         mode;           /* Open mode (read/write) */
  struct mountedFS      *mount;         /* Mounted file system descriptor */
  off_t                 offset;         /* Current read/write point */
  uint32_t              block;          /* Current block in buffer */
  char                  *buffer;        /* Private buffer for file I/O */
};

/*
 * Unix V7 specific data area.
 */
struct UNIXV7data {
  unsigned int          blocks;         /* # of blocks in the file system */
  unsigned int          offset;         /* Block offset to partition start */
  uint16_t              user;           /* Default user ID */
  uint16_t              group;          /* Default group ID */
  union {
    struct v7_filsys    sb;
    uint8_t             super;
  }                     superblk;       /* Holds super block */
  uint8_t               buf[512];       /* Disk buffer */
  uint8_t               zero[512];      /* Always contains 0's */
};

#endif
