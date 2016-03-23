#ifndef _PHYVHD_H
#define _PHYVHD_H 1

struct DEV;

unsigned phyvhd_snapshot( const char *filename, const char *parent );
unsigned phyvhd_init( struct DEV *dev );
unsigned phyvhd_close( struct DEV *dev );
void phyvhd_show( struct DEV *dev, size_t column );

int phyvhd_available( int query );

#endif
