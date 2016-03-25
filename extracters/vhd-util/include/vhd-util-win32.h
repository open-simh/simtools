/*
 * Compatibility file for Windows.
 * Compiler-forced #include at line 1 of every source.
 */

#include <windows.h>
#include <errno.h>
#include <io.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include <wingetopt.h>
#include "relative-path.h"
#define O_DIRECT 0

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define open _open
#define close _close

static inline int ftruncate(int fd, __int64 length)
{
 HANDLE fh = (HANDLE)_get_osfhandle(fd);

 if (!fh || _lseeki64(fd, length, SEEK_SET))
     return -1;
 return SetEndOfFile(fh) ? 0 : -1;
}

#define fdatasync fsync

static inline int
fsync (int fd)
{
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  DWORD err;

  if (h == INVALID_HANDLE_VALUE)
    {
      errno = EBADF;
      return -1;
    }

  if (!FlushFileBuffers (h))
    {
      err = GetLastError ();
      switch (err)
        {
        case ERROR_ACCESS_DENIED:
          return 0;

        case ERROR_INVALID_HANDLE:
          errno = EINVAL;
          break;

        default:
          errno = EIO;
        }
      return -1;
    }

  return 0;
}

/* Emulated in libvhd */

char *basename( char *path );
