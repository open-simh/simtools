#ifndef SYSMSG_H
#define SYSMSG_H

#define MSG_TEXT 0
#define MSG_FULL 1
const char *getmsg( unsigned int vmscode, unsigned int flags );

#endif
