/* $Header$ */

#include "copyright.h"

#ifndef __DB_HEADER_H
#define __DB_HEADER_H

#include <stdio.h>

#include "db.h"

#define DB_VERSION_STRING "***Foxen9 TinyMUCK DUMP Format***"

/* return masks from db_identify */
#define DB_ID_VERSIONSTRING	0x00000001 /* has a returned **version */
#define DB_ID_DELTAS		0x00000002 /* doing a delta file */

#define DB_ID_GROW 		0x00000010 /* grow parameter will be set */

/* identify which format a database is (or try to) */
extern int db_read_header( FILE *f, const char **version, int *load_format, int *grow, int *parmcnt );

/* Read a database reference from a file. */
extern dbref getref(FILE *);

/* read a string from file, don't require any allocation - however the result */
/* will always point to the same static buffer, so make a copy if you need it */
/* to hang around */
extern const char* getstring_noalloc(FILE *f);

#endif /* DB_HEADER_H */
