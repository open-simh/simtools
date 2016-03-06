#ifndef SYSMSG_H
#define SYSMSG_H

#include <stdarg.h>

#define MSG_FACILITY (1 << 0)
#define MSG_SEVERITY (1 << 1)
#define MSG_NAME     (1 << 2)
#define MSG_TEXT     (1 << 3)
#define MSG_WITHARGS (1 << 4)

#define MSG_FULL     (MSG_FACILITY|MSG_SEVERITY|MSG_NAME|MSG_TEXT)

const char *getmsg( unsigned int vmscode, unsigned int flags, ... );

void sysmsg_rundown( void );

#endif
