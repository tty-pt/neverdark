/* $Header$ */

#include "copyright.h"
#include "config.h"

#include <ctype.h>
#include <string.h>

#include "db.h"
#include "db_header.h"
#include "props.h"
#include "params.h"
#include "defaults.h"
#include "interface.h"

#include "externs.h"

struct object *db = 0;
dbref db_top = 0;
dbref recyclable = NOTHING;
int db_load_format = 0;

#define OBSOLETE_ANTILOCK            0x8	/* negates key (*OBSOLETE*) */
#define OBSOLETE_GENDER_MASK      0x3000	/* 2 bits of gender */
#define OBSOLETE_GENDER_SHIFT         12	/* 0x1000 is 12 bits over (for shifting) */
#define OBSOLETE_GENDER_UNASSIGNED   0x0	/* unassigned - the default */
#define OBSOLETE_GENDER_NEUTER       0x1	/* neuter */
#define OBSOLETE_GENDER_FEMALE       0x2	/* for women */
#define OBSOLETE_GENDER_MALE         0x3	/* for men */

#ifndef DB_INITIAL_SIZE
#define DB_INITIAL_SIZE 10000
#endif							/* DB_INITIAL_SIZE */

#ifdef DB_DOUBLING

dbref db_size = DB_INITIAL_SIZE;

#endif							/* DB_DOUBLING */

struct macrotable *macrotop;
extern char *alloc_string(const char *);
int number(const char *s);
int ifloat(const char *s);
void putproperties(FILE * f, int obj);
void getproperties(FILE * f, int obj, const char *pdir);


dbref
getparent_logic(dbref obj)
{
        if (obj == NOTHING) return NOTHING;
	if (Typeof(obj) == TYPE_THING && (FLAGS(obj) & VEHICLE)) {
		obj = THING_HOME(obj);
		if (obj != NOTHING && Typeof(obj) == TYPE_PLAYER) {
			obj = PLAYER_HOME(obj);
		}
	} else {
		obj = getloc(obj);
	}
	return obj;
}

dbref
getparent(dbref obj)
{
#if SECURE_THING_MOVEMENT
	return getloc(obj);
#else
        dbref ptr, oldptr;

	ptr = getparent_logic(obj);
	do {
		obj = getparent_logic(obj);
	} while (obj != (oldptr = ptr = getparent_logic(ptr)) &&
		 obj != (ptr = getparent_logic(ptr)) &&
		 obj != NOTHING && Typeof(obj) == TYPE_THING);
	if (obj != NOTHING && (obj == oldptr || obj == ptr)) {
		obj = GLOBAL_ENVIRONMENT;
	}
	return obj;
#endif
}


void
free_line(struct line *l)
{
	if (l->this_line)
		free((void *) l->this_line);
	free((void *) l);
}

void
free_prog_text(struct line *l)
{
	struct line *next;

	while (l) {
		next = l->next;
		free_line(l);
		l = next;
	}
}

#ifdef DB_DOUBLING

static void
db_grow(dbref newtop)
{
	struct object *newdb;

	if (newtop > db_top) {
		db_top = newtop;
		if (!db) {
			/* make the initial one */
			db_size = DB_INITIAL_SIZE;
			while (db_top > db_size)
				db_size += 1000;
			if ((db = (struct object *) malloc(db_size * sizeof(struct object))) == 0) {
				abort();
			}
		}
		/* maybe grow it */
		if (db_top > db_size) {
			/* make sure it's big enough */
			while (db_top > db_size)
				db_size += 1000;
			if ((newdb = (struct object *) realloc((void *) db,
												   db_size * sizeof(struct object))) == 0) {
				abort();
			}
			db = newdb;
		}
	}
}

#else							/* DB_DOUBLING */

static void
db_grow(dbref newtop)
{
	struct object *newdb;

	if (newtop > db_top) {
		db_top = newtop;
		if (db) {
			if ((newdb = (struct object *)
				 realloc((void *) db, db_top * sizeof(struct object))) == 0) {
				abort();
			}
			db = newdb;
		} else {
			/* make the initial one */
			int startsize = (newtop >= DB_INITIAL_SIZE) ? newtop : DB_INITIAL_SIZE;

			if ((db = (struct object *)
				 malloc(startsize * sizeof(struct object))) == 0) {
				abort();
			}
		}
	}
}

#endif							/* DB_DOUBLING */

void
db_clear_object(dbref i)
{
	struct object *o = DBFETCH(i);

	memset(o, 0, sizeof(struct object));

	NAME(i) = 0;
	ts_newobject(o);
	o->location = NOTHING;
	o->contents = NOTHING;
	o->exits = NOTHING;
	o->next = NOTHING;
	o->properties = 0;

	/* DBDIRTY(i); */
	/* flags you must initialize yourself */
	/* type-specific fields you must also initialize */
}

dbref
new_object(void)
{
	dbref newobj;

	if (recyclable != NOTHING) {
		newobj = recyclable;
		recyclable = DBFETCH(newobj)->next;
		db_free_object(newobj);
	} else {
		newobj = db_top;
		db_grow(db_top + 1);
	}

	/* clear it out */
	db_clear_object(newobj);
	DBDIRTY(newobj);
	return newobj;
}

void
putref(FILE * f, dbref ref)
{
	if (fprintf(f, "%d\n", ref) < 0) {
		abort();
	}
}

static void
putstring(FILE * f, const char *s)
{
	if (s) {
		if (fputs(s, f) == EOF) {
			abort();
		}
	}
	if (putc('\n', f) == EOF) {
		abort();
	}
}

void
putproperties_rec(FILE * f, const char *dir, dbref obj)
{
	PropPtr pref;
	PropPtr p, pptr;
	char buf[BUFFER_LEN];
	char name[BUFFER_LEN];

	pref = first_prop_nofetch(obj, dir, &pptr, name, sizeof(name));
	while (pref) {
		p = pref;
		db_putprop(f, dir, p);
		strlcpy(buf, dir, sizeof(buf));
		strlcat(buf, name, sizeof(buf));
		if (PropDir(p)) {
			strlcat(buf, "/", sizeof(buf));
			putproperties_rec(f, buf, obj);
		}
		pref = next_prop(pptr, pref, name, sizeof(name));
	}
}

/*** CHANGED:
was: void putproperties(FILE *f, PropPtr p)
 is: void putproperties(FILE *f, dbref obj)
***/
void
putproperties(FILE * f, dbref obj)
{
	putstring(f, "*Props*");
	db_dump_props(f, obj);
	/* putproperties_rec(f, "/", obj); */
	putstring(f, "*End*");
}


extern FILE *input_file;
extern FILE *delta_infile;
extern FILE *delta_outfile;

void
macrodump(struct macrotable *node, FILE * f)
{
	if (!node)
		return;
	macrodump(node->left, f);
	putstring(f, node->name);
	putstring(f, node->definition);
	putref(f, node->implementor);
	macrodump(node->right, f);
}

char *
file_line(FILE * f)
{
	char buf[BUFFER_LEN];
	int len;

	if (!fgets(buf, BUFFER_LEN, f))
		return NULL;
	len = strlen(buf);
	if (buf[len-1] == '\n') {
		buf[--len] = '\0';
	}
	if (buf[len-1] == '\r') {
		buf[--len] = '\0';
	}
	return alloc_string(buf);
}

void
foldtree(struct macrotable *center)
{
	int count = 0;
	struct macrotable *nextcent = center;

	for (; nextcent; nextcent = nextcent->left)
		count++;
	if (count > 1) {
		for (nextcent = center, count /= 2; count--; nextcent = nextcent->left) ;
		if (center->left)
			center->left->right = NULL;
		center->left = nextcent;
		foldtree(center->left);
	}
	for (count = 0, nextcent = center; nextcent; nextcent = nextcent->right)
		count++;
	if (count > 1) {
		for (nextcent = center, count /= 2; count--; nextcent = nextcent->right) ;
		if (center->right)
			center->right->left = NULL;
		foldtree(center->right);
	}
}

void
write_program(struct line *first, dbref i)
{
	FILE *f;
	char fname[BUFFER_LEN];

	snprintf(fname, sizeof(fname), "muf/%d.m", (int) i);
	f = fopen(fname, "wb");
	if (!f) {
		warn("Couldn't open file %s!", fname);
		return;
	}
	while (first) {
		if (!first->this_line)
			continue;
		if (fputs(first->this_line, f) == EOF) {
			abort();
		}
		if (fputc('\n', f) == EOF) {
			abort();
		}
		first = first->next;
	}
	fclose(f);
}

int
db_write_object(FILE * f, dbref i)
{
	struct object *o = DBFETCH(i);
	int j;
	putstring(f, NAME(i));
	putref(f, o->location);
	putref(f, o->contents);
	putref(f, o->next);
	putref(f, (FLAGS(i) & ~DUMP_MASK));	/* write non-internal flags */

	putref(f, o->ts.created);
	putref(f, o->ts.lastused);
	putref(f, o->ts.usecount);
	putref(f, o->ts.modified);


	putproperties(f, i);


	switch (Typeof(i)) {
	case TYPE_THING:
		putref(f, THING_HOME(i));
		putref(f, o->exits);
		putref(f, OWNER(i));
		break;

	case TYPE_ROOM:
		putref(f, o->sp.room.dropto);
		putref(f, o->exits);
		putref(f, OWNER(i));
		break;

	case TYPE_EXIT:
		putref(f, o->sp.exit.ndest);
		for (j = 0; j < o->sp.exit.ndest; j++) {
			putref(f, (o->sp.exit.dest)[j]);
		}
		putref(f, OWNER(i));
		break;

	case TYPE_PLAYER:
		putref(f, PLAYER_HOME(i));
		putref(f, o->exits);
		putstring(f, PLAYER_PASSWORD(i));
		break;

	case TYPE_PROGRAM:
		putref(f, OWNER(i));
		break;
	}

	return 0;
}

int deltas_count = 0;

#ifndef CLUMP_LOAD_SIZE
#define CLUMP_LOAD_SIZE 20
#endif


/* mode == 1 for dumping all objects.  mode == 0 for deltas only.  */

void
db_write_list(FILE * f, int mode)
{
	dbref i;

	for (i = db_top; i-- > 0;) {
		if (mode == 1 || (FLAGS(i) & OBJECT_CHANGED)) {
			if (fprintf(f, "#%d\n", i) < 0)
				abort();
			db_write_object(f, i);
			FLAGS(i) &= ~OBJECT_CHANGED;	/* clear changed flag */
		}
	}
}


dbref
db_write(FILE * f)
{
	putstring(f, DB_VERSION_STRING );

	putref(f, db_top);

	db_write_list(f, 1);

	fseek(f, 0L, 2);
	putstring(f, "***END OF DUMP***");

	fflush(f);
	deltas_count = 0;
	return (db_top);
}



dbref
db_write_deltas(FILE * f)
{
	fseek(f, 0L, 2);			/* seek end of file */
	putstring(f, "***Foxen8 Deltas Dump Extention***");
	db_write_list(f, 0);

	fseek(f, 0L, 2);
	putstring(f, "***END OF DUMP***");
	fflush(f);
	return (db_top);
}



dbref
parse_dbref(const char *s)
{
	const char *p;
	long x;

	x = atol(s);
	if (x > 0) {
		return x;
	} else if (x == 0) {
		/* check for 0 */
		for (p = s; *p; p++) {
			if (*p == '0')
				return 0;
			if (!isspace(*p))
				break;
		}
	}
	/* else x < 0 or s != 0 */
	return NOTHING;
}

#define getstring(x) alloc_string(getstring_noalloc(x))

/* returns true for numbers of form [ + | - ] <series of digits> */
int
number(const char *s)
{
	if (!s)
		return 0;
	while (isspace(*s))
		s++;
	if (*s == '+' || *s == '-')
		s++;
	if (!*s) 
		return 0;
	for (; *s; s++)
		if (*s < '0' || *s > '9')
			return 0;
	return 1;
}

/* returns true for floats of form  [+|-]<digits>.<digits>[E[+|-]<digits>] */
int
ifloat(const char *s)
{
	const char *hold;

	if (!s)
		return 0;
	while (isspace(*s))
		s++;
	if (*s == '+' || *s == '-')
		s++;
	/* WORK: for when float parsing is improved.
	if (!string_compare(s, "inf")) {
		return 1;
	}
	if (!string_compare(s, "nan")) {
		return 1;
	}
	*/
	hold = s;
	while ((*s) && (*s >= '0' && *s <= '9'))
		s++;
	if ((!*s) || (s == hold))
		return 0;
	if (*s != '.')
		return 0;
	s++;
	hold = s;
	while ((*s) && (*s >= '0' && *s <= '9'))
		s++;
	if (hold == s)
		return 0;
	if (!*s)
		return 1;
	if ((*s != 'e') && (*s != 'E'))
		return 0;
	s++;
	if (*s == '+' || *s == '-')
		s++;
	hold = s;
	while ((*s) && (*s >= '0' && *s <= '9'))
		s++;
	if (s == hold)
		return 0;
	if (*s)
		return 0;
	return 1;
}

/*** CHANGED:
was: PropPtr getproperties(FILE *f)
now: void getproperties(FILE *f, dbref obj, const char *pdir)
***/
void
getproperties(FILE * f, dbref obj, const char *pdir)
{
	char buf[BUFFER_LEN * 3], *p;
	int datalen;

	/* get rid of first line */
	fgets(buf, sizeof(buf), f);

	if (strcmp(buf, "Props*\n")) {
		/* initialize first line stuff */
		fgets(buf, sizeof(buf), f);
		while (1) {
			/* fgets reads in \n too! */
			if (!strcmp(buf, "***Property list end ***\n") || !strcmp(buf, "*End*\n"))
				break;
			p = index(buf, PROP_DELIMITER);
			*(p++) = '\0';		/* Purrrrrrrrrr... */
			datalen = strlen(p);
			p[datalen - 1] = '\0';

			if ((p - buf) >= BUFFER_LEN)
				buf[BUFFER_LEN - 1] = '\0';
			if (datalen >= BUFFER_LEN)
				p[BUFFER_LEN - 1] = '\0';

			if ((*p == '^') && (number(p + 1))) {
				add_prop_nofetch(obj, buf, NULL, atol(p + 1));
			} else {
				if (*buf) {
					add_prop_nofetch(obj, buf, p, 0);
				}
			}
			fgets(buf, sizeof(buf), f);
		}
	} else {
		db_getprops(f, obj, pdir);
	}
}

void
db_free_object(dbref i)
{
	struct object *o;

	o = DBFETCH(i);
	if (NAME(i))
		free((void *) NAME(i));

	if (o->properties) {
		delete_proplist(o->properties);
	}

	if (Typeof(i) == TYPE_EXIT && o->sp.exit.dest) {
		free((void *) o->sp.exit.dest);
    } else if (Typeof(i) == TYPE_PLAYER) {
        if (PLAYER_PASSWORD(i)) {
			free((void*)PLAYER_PASSWORD(i));
        }
        if (PLAYER_DESCRS(i)){ 
			free(PLAYER_DESCRS(i));
			PLAYER_SET_DESCRS(i, NULL);
			PLAYER_SET_DESCRCOUNT(i, 0);
        }
    }
	if (Typeof(i) == TYPE_THING) {
		FREE_THING_SP(i);
	}
	if (Typeof(i) == TYPE_PLAYER) {
		FREE_PLAYER_SP(i);
	}
}

void
db_free(void)
{
	dbref i;

	if (db) {
		for (i = 0; i < db_top; i++)
			db_free_object(i);
		free((void *) db);
		db = 0;
		db_top = 0;
	}
	clear_players();
	recyclable = NOTHING;
}


struct line *
get_new_line(void)
{
	struct line *nu;

	nu = (struct line *) malloc(sizeof(struct line));

	if (!nu) {
		fprintf(stderr, "get_new_line(): Out of memory!\n");
		abort();
	}
	nu->this_line = NULL;
	nu->next = NULL;
	nu->prev = NULL;
	return nu;
}

struct line *
read_program(dbref i)
{
	char buf[BUFFER_LEN];
	struct line *first;
	struct line *prev = NULL;
	struct line *nu;
	FILE *f;
	int len;

	first = NULL;
	snprintf(buf, sizeof(buf), "muf/%d.m", (int) i);
	f = fopen(buf, "rb");
	if (!f)
		return 0;

	while (fgets(buf, BUFFER_LEN, f)) {
		nu = get_new_line();
		len = strlen(buf);
		if (len > 0 && buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
			len--;
		}
		if (len > 0 && buf[len - 1] == '\r') {
			buf[len - 1] = '\0';
			len--;
		}
		if (!*buf)
			strlcpy(buf, " ", sizeof(buf));
		nu->this_line = alloc_string(buf);
		if (!first) {
			prev = nu;
			first = nu;
		} else {
			prev->next = nu;
			nu->prev = prev;
			prev = nu;
		}
	}

	fclose(f);
	return first;
}

#define getstring_oldcomp_noalloc(foo) getstring_noalloc(foo)

void
db_read_object_old(FILE * f, struct object *o, dbref objno)
{
	dbref exits;
	int pennies;
	const char *password;

	db_clear_object(objno);
	FLAGS(objno) = 0;
	NAME(objno) = getstring(f);
	LOADDESC(objno, getstring_oldcomp_noalloc(f));
	o->location = getref(f);
	o->contents = getref(f);
	exits = getref(f);
	o->next = getref(f);
	LOADLOCK(objno, getboolexp(f));
	LOADFAIL(objno, getstring_oldcomp_noalloc(f));
	LOADSUCC(objno, getstring_oldcomp_noalloc(f));
	LOADOFAIL(objno, getstring_oldcomp_noalloc(f));
	LOADOSUCC(objno, getstring_oldcomp_noalloc(f));
	OWNER(objno) = getref(f);
	pennies = getref(f);

	/* timestamps mods */
	o->ts.created = time(NULL);
	o->ts.lastused = time(NULL);
	o->ts.usecount = 0;
	o->ts.modified = time(NULL);

	FLAGS(objno) |= getref(f);
	/*
	 * flags have to be checked for conflict --- if they happen to coincide
	 * with chown_ok flags and jump_ok flags, we bump them up to the
	 * corresponding HAVEN and ABODE flags
	 */
	if (FLAGS(objno) & CHOWN_OK) {
		FLAGS(objno) &= ~CHOWN_OK;
		FLAGS(objno) |= HAVEN;
	}
	if (FLAGS(objno) & JUMP_OK) {
		FLAGS(objno) &= ~JUMP_OK;
		FLAGS(objno) |= ABODE;
	}
	password = getstring(f);
	/* convert GENDER flag to property */
	switch ((FLAGS(objno) & OBSOLETE_GENDER_MASK) >> OBSOLETE_GENDER_SHIFT) {
	case OBSOLETE_GENDER_NEUTER:
		add_property(objno, "sex", "neuter", 0);
		break;
	case OBSOLETE_GENDER_FEMALE:
		add_property(objno, "sex", "female", 0);
		break;
	case OBSOLETE_GENDER_MALE:
		add_property(objno, "sex", "male", 0);
		break;
	default:
		break;
	}
	/* For downward compatibility with databases using the */
	/* obsolete ANTILOCK flag. */
	if (FLAGS(objno) & OBSOLETE_ANTILOCK) {
		LOADLOCK(objno, negate_boolexp(copy_bool(GETLOCK(objno))))
				FLAGS(objno) &= ~OBSOLETE_ANTILOCK;
	}
	switch (FLAGS(objno) & TYPE_MASK) {
	case TYPE_THING:
		ALLOC_THING_SP(objno);
		THING_SET_HOME(objno, exits);
		LOADVALUE(objno, pennies);
		o->exits = NOTHING;
		break;
	case TYPE_ROOM:
		o->sp.room.dropto = o->location;
		o->location = NOTHING;
		o->exits = exits;
		break;
	case TYPE_EXIT:
		if (o->location == NOTHING) {
			o->sp.exit.ndest = 0;
			o->sp.exit.dest = NULL;
		} else {
			o->sp.exit.ndest = 1;
			o->sp.exit.dest = (dbref *) malloc(sizeof(dbref));
			(o->sp.exit.dest)[0] = o->location;
		}
		o->location = NOTHING;
		break;
	case TYPE_PLAYER:
		ALLOC_PLAYER_SP(objno);
		PLAYER_SET_HOME(objno, exits);
		o->exits = NOTHING;
		LOADVALUE(objno, pennies);
		set_password_raw(objno, NULL);
		set_password(objno, password);
		if (password)
			free((void*) password);
		PLAYER_SET_CURR_PROG(objno, NOTHING);
		PLAYER_SET_INSERT_MODE(objno, 0);
		PLAYER_SET_DESCRS(objno, NULL);
		PLAYER_SET_DESCRCOUNT(objno, 0);
		PLAYER_SET_IGNORE_CACHE(objno, NULL);
		PLAYER_SET_IGNORE_COUNT(objno, 0);
		PLAYER_SET_IGNORE_LAST(objno, NOTHING);
		break;
	case TYPE_GARBAGE:
		OWNER(objno) = NOTHING;
		o->next = recyclable;
		recyclable = objno;

		free((void *) NAME(objno));
		NAME(objno) = "<garbage>";
		SETDESC(objno, "<recyclable>");
		break;
	}
}

void
db_read_object_new(FILE * f, struct object *o, dbref objno)
{
	int j;
	const char *password;

	db_clear_object(objno);
	FLAGS(objno) = 0;
	NAME(objno) = getstring(f);
	LOADDESC(objno, getstring_noalloc(f));
	o->location = getref(f);
	o->contents = getref(f);
	/* o->exits = getref(f); */
	o->next = getref(f);
	LOADLOCK(objno, getboolexp(f));
	LOADFAIL(objno, getstring_oldcomp_noalloc(f));
	LOADSUCC(objno, getstring_oldcomp_noalloc(f));
	LOADOFAIL(objno, getstring_oldcomp_noalloc(f));
	LOADOSUCC(objno, getstring_oldcomp_noalloc(f));

	/* timestamps mods */
	o->ts.created = time(NULL);
	o->ts.lastused = time(NULL);
	o->ts.usecount = 0;
	o->ts.modified = time(NULL);

	/* OWNER(objno) = getref(f); */
	/* o->pennies = getref(f); */
	FLAGS(objno) |= getref(f);

	/*
	 * flags have to be checked for conflict --- if they happen to coincide
	 * with chown_ok flags and jump_ok flags, we bump them up to the
	 * corresponding HAVEN and ABODE flags
	 */
	if (FLAGS(objno) & CHOWN_OK) {
		FLAGS(objno) &= ~CHOWN_OK;
		FLAGS(objno) |= HAVEN;
	}
	if (FLAGS(objno) & JUMP_OK) {
		FLAGS(objno) &= ~JUMP_OK;
		FLAGS(objno) |= ABODE;
	}
	/* convert GENDER flag to property */
	switch ((FLAGS(objno) & OBSOLETE_GENDER_MASK) >> OBSOLETE_GENDER_SHIFT) {
	case OBSOLETE_GENDER_NEUTER:
		add_property(objno, "sex", "neuter", 0);
		break;
	case OBSOLETE_GENDER_FEMALE:
		add_property(objno, "sex", "female", 0);
		break;
	case OBSOLETE_GENDER_MALE:
		add_property(objno, "sex", "male", 0);
		break;
	default:
		break;
	}

	/* o->password = getstring(f); */
	/* For downward compatibility with databases using the */
	/* obsolete ANTILOCK flag. */
	if (FLAGS(objno) & OBSOLETE_ANTILOCK) {
		LOADLOCK(objno, negate_boolexp(copy_bool(GETLOCK(objno))))
				FLAGS(objno) &= ~OBSOLETE_ANTILOCK;
	}
	switch (FLAGS(objno) & TYPE_MASK) {
	case TYPE_THING:
		ALLOC_THING_SP(objno);
		THING_SET_HOME(objno, getref(f));
		o->exits = getref(f);
		OWNER(objno) = getref(f);
		LOADVALUE(objno, getref(f));
		break;
	case TYPE_ROOM:
		o->sp.room.dropto = getref(f);
		o->exits = getref(f);
		OWNER(objno) = getref(f);
		break;
	case TYPE_EXIT:
		o->sp.exit.ndest = getref(f);
		o->sp.exit.dest = (dbref *) malloc(sizeof(dbref)
										   * o->sp.exit.ndest);
		for (j = 0; j < o->sp.exit.ndest; j++) {
			(o->sp.exit.dest)[j] = getref(f);
		}
		OWNER(objno) = getref(f);
		break;
	case TYPE_PLAYER:
		ALLOC_PLAYER_SP(objno);
		PLAYER_SET_HOME(objno, getref(f));
		o->exits = getref(f);
		LOADVALUE(objno, getref(f));
		password = getstring(f);
		set_password_raw(objno, NULL);
		set_password(objno, password);
		if (password)
			free((void*) password);
		PLAYER_SET_CURR_PROG(objno, NOTHING);
		PLAYER_SET_INSERT_MODE(objno, 0);
		PLAYER_SET_DESCRS(objno, NULL);
		PLAYER_SET_DESCRCOUNT(objno, 0);
		PLAYER_SET_IGNORE_CACHE(objno, NULL);
		PLAYER_SET_IGNORE_COUNT(objno, 0);
		PLAYER_SET_IGNORE_LAST(objno, NOTHING);
		break;
	}
}

/* Reads in Foxen, Foxen[2-8], WhiteFire, Mage or Lachesis DB Formats */
void
db_read_object_foxen(FILE * f, struct object *o, dbref objno, int dtype, int read_before)
{
	int tmp, c, prop_flag = 0;
	int j = 0;
	const char *password;

	if (read_before) {
		db_free_object(objno);
	}
	db_clear_object(objno);

	FLAGS(objno) = 0;
	NAME(objno) = getstring(f);
	if (dtype <= 3) {
		LOADDESC(objno, getstring_oldcomp_noalloc(f));
	}
	o->location = getref(f);
	o->contents = getref(f);
	o->next = getref(f);
	if (dtype < 6) {
		LOADLOCK(objno, getboolexp(f));
	}
	if (dtype == 3) {
		/* Mage timestamps */
		o->ts.created = getref(f);
		o->ts.modified = getref(f);
		o->ts.lastused = getref(f);
		o->ts.usecount = 0;
	}
	if (dtype <= 3) {
		/* Lachesis, WhiteFire, and Mage messages */
		LOADFAIL(objno, getstring_oldcomp_noalloc(f));
		LOADSUCC(objno, getstring_oldcomp_noalloc(f));
		LOADDROP(objno, getstring_oldcomp_noalloc(f));
		LOADOFAIL(objno, getstring_oldcomp_noalloc(f));
		LOADOSUCC(objno, getstring_oldcomp_noalloc(f));
		LOADODROP(objno, getstring_oldcomp_noalloc(f));
	}
	tmp = getref(f);			/* flags list */
	if (dtype >= 4)
		tmp &= ~DUMP_MASK;
	FLAGS(objno) |= tmp;

	FLAGS(objno) &= ~SAVED_DELTA;

	if (dtype != 3) {
		/* Foxen and WhiteFire timestamps */
		o->ts.created = getref(f);
		o->ts.lastused = getref(f);
		o->ts.usecount = getref(f);
		o->ts.modified = getref(f);
	}
	c = getc(f);
	if (c == '*') {

		getproperties(f, objno, NULL);

		prop_flag++;
	} else {
		/* do our own getref */
		int sign = 0;
		char buf[BUFFER_LEN];
		int i = 0;

		if (c == '-')
			sign = 1;
		else if (c != '+') {
			buf[i] = c;
			i++;
		}
		while ((c = getc(f)) != '\n') {
			buf[i] = c;
			i++;
		}
		buf[i] = '\0';
		j = atol(buf);
		if (sign)
			j = -j;

		if (dtype < 10) {
			/* set gender stuff */
			/* convert GENDER flag to property */
			switch ((FLAGS(objno) & OBSOLETE_GENDER_MASK) >> OBSOLETE_GENDER_SHIFT) {
			case OBSOLETE_GENDER_NEUTER:
				add_property(objno, "sex", "neuter", 0);
				break;
			case OBSOLETE_GENDER_FEMALE:
				add_property(objno, "sex", "female", 0);
				break;
			case OBSOLETE_GENDER_MALE:
				add_property(objno, "sex", "male", 0);
				break;
			default:
				break;
			}
		}
	}

        if (dtype < 10) {
		/* For downward compatibility with databases using the */
		/* obsolete ANTILOCK flag. */
		if (FLAGS(objno) & OBSOLETE_ANTILOCK) {
			LOADLOCK(objno, negate_boolexp(copy_bool(GETLOCK(objno))))
					FLAGS(objno) &= ~OBSOLETE_ANTILOCK;
		}
	}

	switch (FLAGS(objno) & TYPE_MASK) {
	case TYPE_THING:{
			dbref home;

			ALLOC_THING_SP(objno);
			home = prop_flag ? getref(f) : j;
			THING_SET_HOME(objno, home);
			o->exits = getref(f);
			OWNER(objno) = getref(f);
			if (dtype < 10)
				LOADVALUE(objno, getref(f));
			break;
		}
	case TYPE_ROOM:
		o->sp.room.dropto = prop_flag ? getref(f) : j;
		o->exits = getref(f);
		OWNER(objno) = getref(f);
		break;
	case TYPE_EXIT:
		o->sp.exit.ndest = prop_flag ? getref(f) : j;
		if (o->sp.exit.ndest > 0)	/* only allocate space for linked exits */
			o->sp.exit.dest = (dbref *) malloc(sizeof(dbref) * (o->sp.exit.ndest));
		for (j = 0; j < o->sp.exit.ndest; j++) {
			(o->sp.exit.dest)[j] = getref(f);
		}
		OWNER(objno) = getref(f);
		break;
	case TYPE_PLAYER:
		ALLOC_PLAYER_SP(objno);
		PLAYER_SET_HOME(objno, (prop_flag ? getref(f) : j));
		o->exits = getref(f);
		if (dtype < 10)
			LOADVALUE(objno, getref(f));
		password = getstring(f);
		if (dtype <= 8 && password) {
			set_password_raw(objno, NULL);
			set_password(objno, password);
			free((void*) password);
		} else {
			set_password_raw(objno, password);
		}
		PLAYER_SET_CURR_PROG(objno, NOTHING);
		PLAYER_SET_INSERT_MODE(objno, 0);
		PLAYER_SET_DESCRS(objno, NULL);
		PLAYER_SET_DESCRCOUNT(objno, 0);
		PLAYER_SET_IGNORE_CACHE(objno, NULL);
		PLAYER_SET_IGNORE_COUNT(objno, 0);
		PLAYER_SET_IGNORE_LAST(objno, NOTHING);
		break;
	case TYPE_PROGRAM:
		ALLOC_PROGRAM_SP(objno);
		OWNER(objno) = getref(f);
		FLAGS(objno) &= ~INTERNAL;
		PROGRAM_SET_CURR_LINE(objno, 0);
		PROGRAM_SET_FIRST(objno, 0);
		PROGRAM_SET_CODE(objno, 0);
		PROGRAM_SET_SIZ(objno, 0);
		PROGRAM_SET_START(objno, 0);
		PROGRAM_SET_PUBS(objno, 0);
		PROGRAM_SET_MCPBINDS(objno, 0);
		PROGRAM_SET_PROFTIME(objno, 0, 0);
		PROGRAM_SET_PROFSTART(objno, 0);
		PROGRAM_SET_PROF_USES(objno, 0);
		PROGRAM_SET_INSTANCES(objno, 0);

		if (dtype < 8 && (FLAGS(objno) & LINK_OK)) {
			/* set Viewable flag on Link_ok programs. */
			FLAGS(objno) |= VEHICLE;
		}
		if (dtype < 5 && MLevel(objno) == 0)
			SetMLevel(objno, 2);

		break;
	case TYPE_GARBAGE:
		break;
	}
}

dbref
db_read(FILE * f)
{
	int i;
	dbref grow, thisref;
	struct object *o;
	const char *special, *version;
	int doing_deltas;
	int main_db_format = 0;
	int parmcnt;
	int dbflags;
	char c;

	/* Parse the header */
	dbflags = db_read_header( f, &version, &db_load_format, &grow, &parmcnt );

	/* grow the db up front */
	if ( dbflags & DB_ID_GROW ) {
		db_grow( grow );
	}

	doing_deltas = dbflags & DB_ID_DELTAS;
	if( doing_deltas ) {
		if( !db ) {
			fprintf(stderr, "Can't read a deltas file without a dbfile.\n");
			return -1;
		}
	} else {
		main_db_format = db_load_format;
	}

	c = getc(f);			/* get next char */
	for (i = 0;; i++) {
		switch (c) {
		case NUMBER_TOKEN:
			/* another entry, yawn */
			thisref = getref(f);

			if (thisref < db_top) {
				if (doing_deltas && Typeof(thisref) == TYPE_PLAYER) {
					delete_player(thisref);
				}
			}

			/* make space */
			db_grow(thisref + 1);

			/* read it in */
			o = DBFETCH(thisref);
			switch (db_load_format) {
			case 0:
				db_read_object_old(f, o, thisref);
				break;
			case 1:
				db_read_object_new(f, o, thisref);
				break;
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
				db_read_object_foxen(f, o, thisref, db_load_format, doing_deltas);
				break;
			default:
				warn("got to end of case for db_load_format");
				abort();
				break;
			}
			if (Typeof(thisref) == TYPE_PLAYER) {
				OWNER(thisref) = thisref;
				add_player(thisref);
			}
			break;
		case LOOKUP_TOKEN:
			special = getstring(f);
			if (strcmp(special, "**END OF DUMP***")) {
				free((void *) special);
				return -1;
			} else {
				free((void *) special);
				special = getstring(f);
				if (special && !strcmp(special, "***Foxen Deltas Dump Extention***")) {
					free((void *) special);
					db_load_format = 4;
					doing_deltas = 1;
				} else if (special && !strcmp(special, "***Foxen2 Deltas Dump Extention***")) {
					free((void *) special);
					db_load_format = 5;
					doing_deltas = 1;
				} else if (special && !strcmp(special, "***Foxen4 Deltas Dump Extention***")) {
					free((void *) special);
					db_load_format = 6;
					doing_deltas = 1;
				} else if (special && !strcmp(special, "***Foxen5 Deltas Dump Extention***")) {
					free((void *) special);
					db_load_format = 7;
					doing_deltas = 1;
				} else if (special && !strcmp(special, "***Foxen6 Deltas Dump Extention***")) {
					free((void *) special);
					db_load_format = 8;
					doing_deltas = 1;
				} else if (special && !strcmp(special, "***Foxen7 Deltas Dump Extention***")) {
					free((void *) special);
					db_load_format = 9;
					doing_deltas = 1;
				} else if (special && !strcmp(special, "***Foxen8 Deltas Dump Extention***")) {
					free((void *) special);
					db_load_format = 10;
					doing_deltas = 1;
				} else {
					if (special)
						free((void *) special);
					for (i = 0; i < db_top; i++) {
						if (Typeof(i) == TYPE_GARBAGE) {
							DBFETCH(i)->next = recyclable;
							recyclable = i;
						}
					}
					return db_top;
				}
			}
			break;
		default:
			return -1;
			/* break; */
		}
		c = getc(f);
	}							/* for */
}								/* db_read */

void
copyobj(dbref player, dbref old, dbref nu)
{
	struct object *newp = DBFETCH(nu);

	NAME(nu) = alloc_string(NAME(old));
	if (Typeof(old) == TYPE_THING) {
		ALLOC_THING_SP(nu);
		THING_SET_HOME(nu, player);
		SETVALUE(nu, 1);
	}
	newp->properties = copy_prop(old);
	newp->exits = NOTHING;
	newp->contents = NOTHING;
	newp->next = NOTHING;
	newp->location = NOTHING;
	moveto(nu, player);

	DBDIRTY(nu);
}

static const char *db_c_version = "$RCSfile$ $Revision: 1.39 $";
const char *get_db_c_version(void) { return db_c_version; }
