/* $Header$ */


#undef POINTERS

#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>

#include "mdb.h"
#include "defaults.h"
#include "externs.h"

#include "params.h"
#include "props.h"
#include "search.h"

#define TYPEOF(i)   (db[i].flags & TYPE_MASK)
#define LOCATION(x) (db[x].location)

#define CONTENTS(x) (db[x].contents)
#define NEXTOBJ(x)  (db[x].next)

#define unparse(x) ((char*)unparse_object(GOD, (x)))

int sanity_violated = 0;

void
SanPrint(dbref player, const char *format, ...)
{
	va_list args;
	char buf[16384];

	va_start(args, format);

	vsnprintf(buf, sizeof(buf), format, args);
	if (player == NOTHING) {
		fprintf(stdout, "%s\n", buf);
		fflush(stdout);
	} else if (player == AMBIGUOUS) {
		fprintf(stderr, "%s\n", buf);
	} else {
		notify(player, buf);
	}

	va_end(args);
}

void
violate(dbref player, dbref i, const char *s)
{
	SanPrint(player, "Object \"%s\" %s!", unparse(i), s);
	sanity_violated = 1;
}


static int
valid_ref(dbref obj)
{
	if (obj == NOTHING) {
		return 1;
	}
	if (obj < 0) {
		return 0;
	}
	if (obj >= db_top) {
		return 0;
	}
	return 1;
}


static int
valid_obj(dbref obj)
{
	if (obj == NOTHING) {
		return 0;
	}
	if (!valid_ref(obj)) {
		return 0;
	}
	switch (TYPEOF(obj)) {
	case TYPE_ROOM:
	case TYPE_EXIT:
	case TYPE_ENTITY:
	case TYPE_FOOD:
	case TYPE_DRINK:
	case TYPE_EQUIPMENT:
	case TYPE_THING:
		return 1;
		break;
	default:
		return 0;
	}
}


static void
check_next_chain(dbref player, dbref obj)
{
	dbref i;
	dbref orig;

	orig = obj;
	while (obj != NOTHING && valid_ref(obj)) {
		for (i = orig; i != NOTHING; i = NEXTOBJ(i)) {
			if (i == NEXTOBJ(obj)) {
				violate(player, obj,
						"has a 'next' field that forms an illegal loop in an object chain");
				return;
			}
			if (i == obj) {
				break;
			}
		}
		obj = NEXTOBJ(obj);
	}
	if (!valid_ref(obj)) {
		violate(player, obj, "has an invalid object in its 'next' chain");
	}
}


extern dbref recyclable;

static void
find_orphan_objects(dbref player)
{
	int i;

	SanPrint(player, "Searching for orphan objects...");

	for (i = 0; i < db_top; i++) {
		FLAGS(i) &= ~SANEBIT;
	}

	if (recyclable != NOTHING) {
		FLAGS(recyclable) |= SANEBIT;
	}
	FLAGS(GLOBAL_ENVIRONMENT) |= SANEBIT;

	for (i = 0; i < db_top; i++) {
		switch (Typeof(i)) {
		case TYPE_ROOM:
			if (db[i].sp.room.exits != NOTHING) {
				if (FLAGS(db[i].sp.room.exits) & SANEBIT) {
					violate(player, db[i].sp.room.exits,
							"is referred to by more than one object's Next, Contents, or Exits field");
				} else {
					FLAGS(db[i].sp.room.exits) |= SANEBIT;
				}
			}
			FLAGS(i) |= SANEBIT;
		}
		if (CONTENTS(i) != NOTHING) {
			if (FLAGS(CONTENTS(i)) & SANEBIT) {
				violate(player, CONTENTS(i),
						"is referred to by more than one object's Next, Contents, or Exits field");
			} else {
				FLAGS(CONTENTS(i)) |= SANEBIT;
			}
		}
		if (NEXTOBJ(i) != NOTHING) {
			if (FLAGS(NEXTOBJ(i)) & SANEBIT) {
				violate(player, NEXTOBJ(i),
						"is referred to by more than one object's Next, Contents, or Exits field");
			} else {
				FLAGS(NEXTOBJ(i)) |= SANEBIT;
			}
		}
	}

	for (i = 0; i < db_top; i++) {
		if (!(FLAGS(i) & SANEBIT)) {
			violate(player, i,
					"appears to be an orphan object, that is not referred to by any other object");
		}
	}

	for (i = 0; i < db_top; i++) {
		FLAGS(i) &= ~SANEBIT;
	}
}


void
check_room(dbref player, dbref obj)
{
	dbref i;

	i = db[obj].sp.room.dropto;

	if (!valid_ref(i) && i != HOME) {
		violate(player, obj, "has its dropto set to an invalid object");
	} else if (i >= 0 && !is_item(i) && TYPEOF(i) != TYPE_ROOM) {
		violate(player, obj, "has its dropto set to a non-room, non-item object");
	}
}

void
check_exit(dbref player, dbref obj)
{
	int i;

	if (db[obj].sp.exit.ndest < 0)
		violate(player, obj, "has a negative link count.");
	for (i = 0; i < db[obj].sp.exit.ndest; i++) {
		if (!valid_ref((db[obj].sp.exit.dest)[i]) &&
			(db[obj].sp.exit.dest)[i] != HOME) {
			violate(player, obj, "has an invalid object as one of its link destinations");
		}
	}
}


void
check_entity(dbref player, dbref obj)
{
	dbref i;

	i = ENTITY(obj)->home;

	if (!valid_obj(i)) {
		violate(player, obj, "has its home set to an invalid object");
	} else if (i >= 0 && TYPEOF(i) != TYPE_ROOM) {
		violate(player, obj, "has its home set to a non-room object");
	}
}


void
check_garbage(dbref player, dbref obj)
{
	if (NEXTOBJ(obj) != NOTHING && TYPEOF(NEXTOBJ(obj)) != TYPE_GARBAGE) {
		violate(player, obj,
				"has a non-garbage object as the 'next' object in the garbage chain");
	}
}


void
check_contents_list(dbref player, dbref obj)
{
	dbref i;
	int limit;

	if (TYPEOF(obj) != TYPE_EXIT && TYPEOF(obj) != TYPE_GARBAGE) {
		for (i = CONTENTS(obj), limit = db_top;
			 valid_obj(i) &&
			 --limit && LOCATION(i) == obj && TYPEOF(i) != TYPE_EXIT; i = NEXTOBJ(i)) ;
		if (i != NOTHING) {
			if (!limit) {
				check_next_chain(player, CONTENTS(obj));
				violate(player, obj,
						"is the containing object, and has the loop in its contents chain");
			} else {
				if (!valid_obj(i)) {
					violate(player, obj, "has an invalid object in its contents list");
				} else {
					if (TYPEOF(i) == TYPE_EXIT) {
						violate(player, obj,
								"has an exit in its contents list (it shoudln't)");
					}
					if (LOCATION(i) != obj) {
						violate(player, obj,
								"has an object in its contents lists that thinks it is located elsewhere");
					}
				}
			}
		}
	} else {
		if (CONTENTS(obj) != NOTHING) {
			if (TYPEOF(obj) == TYPE_EXIT) {
				violate(player, obj, "is an exit/action whose contents aren't #-1");
			} else if (TYPEOF(obj) == TYPE_GARBAGE) {
				violate(player, obj, "is a garbage object whose contents aren't #-1");
			} else {
				violate(player, obj, "is a program whose contents aren't #-1");
			}
		}
	}
}


void
check_exits_list(dbref player, dbref obj)
{
	dbref i;
	int limit;

	if (TYPEOF(obj) == TYPE_ROOM) {
		for (i = db[obj].sp.room.exits, limit = db_top;
			 valid_obj(i) &&
			 --limit && LOCATION(i) == obj && TYPEOF(i) == TYPE_EXIT; i = NEXTOBJ(i)) ;
		if (i != NOTHING) {
			if (!limit) {
				check_next_chain(player, CONTENTS(obj));
				violate(player, obj,
						"is the containing object, and has the loop in its exits chain");
			} else if (!valid_obj(i)) {
				violate(player, obj, "has an invalid object in it's exits list");
			} else {
				if (TYPEOF(i) != TYPE_EXIT) {
					violate(player, obj, "has a non-exit in it's exits list");
				}
				if (LOCATION(i) != obj) {
					violate(player, obj,
							"has an exit in its exits lists that thinks it is located elsewhere");
				}
			}
		}
	}
}


void
check_object(dbref player, dbref obj)
{
	/*
	 * Do we have a name?
	 */
	if (!NAME(obj))
		violate(player, obj, "doesn't have a name");

	/*
	 * Check the ownership
	 */
	if (TYPEOF(obj) != TYPE_GARBAGE) {
		if (!valid_obj(OWNER(obj))) {
			violate(player, obj, "has an invalid object as its owner.");
		} else if (TYPEOF(OWNER(obj)) != TYPE_ENTITY) {
			violate(player, obj, "has a non-player object as its owner.");
		}

		/* 
		 * check location 
		 */
		if (!valid_obj(LOCATION(obj)) &&
			!((obj == GLOBAL_ENVIRONMENT || Typeof(obj) == TYPE_ROOM) && LOCATION(obj) == NOTHING)) {
			violate(player, obj, "has an invalid object as it's location");
		}
	}

	if (LOCATION(obj) != NOTHING &&
		(TYPEOF(LOCATION(obj)) == TYPE_GARBAGE ||
		 TYPEOF(LOCATION(obj)) == TYPE_EXIT))
		violate(player, obj, "thinks it is located in a non-container object");

	if ((TYPEOF(obj) == TYPE_GARBAGE) && (LOCATION(obj) != NOTHING))
		violate(player, obj, "is a garbage object with a location that isn't #-1");

	check_contents_list(player, obj);
	check_exits_list(player, obj);

	switch (TYPEOF(obj)) {
	case TYPE_ROOM:
		check_room(player, obj);
		break;
	case TYPE_FOOD:
	case TYPE_DRINK:
	case TYPE_EQUIPMENT:
	case TYPE_THING:
		break;
	case TYPE_ENTITY:
		check_entity(player, obj);
		break;
	case TYPE_EXIT:
		check_exit(player, obj);
		break;
	case TYPE_GARBAGE:
		check_garbage(player, obj);
		break;
	default:
		violate(player, obj, "has an unknown object type, and its flags may also be corrupt");
		break;
	}
}


void
sanity(dbref player)
{
	const int increp = 10000;
	int i;
	int j;

#ifdef GOD_PRIV
	if (player > NOTHING && !God(player)) {
#else
	if (player > NOTHING && !Wizard(player)) {
#endif

		notify(player, "Permission Denied.");
		return;
	}

	sanity_violated = 0;

	for (i = 0; i < db_top; i++) {
		if (!(i % increp)) {
			j = i + increp - 1;
			j = (j >= db_top) ? (db_top - 1) : j;
			SanPrint(player, "Checking objects %d to %d...", i, j);
		}
		check_object(player, i);
	}

	find_orphan_objects(player);

	SanPrint(player, "Done.");
}

#define SanFixed(ref, fixed) san_fixed_log((fixed), 1, (ref), -1)
#define SanFixed2(ref, ref2, fixed) san_fixed_log((fixed), 1, (ref), (ref2))
#define SanFixedRef(ref, fixed) san_fixed_log((fixed), 0, (ref), -1)
void
san_fixed_log(char *format, int unparse, dbref ref1, dbref ref2)
{
	char buf1[4096];
	char buf2[4096];

	if (unparse) {
		if (ref1 >= 0) {
			strlcpy(buf1, unparse(ref1), sizeof(buf1));
		}
		if (ref2 >= 0) {
			strlcpy(buf2, unparse(ref2), sizeof(buf2));
		}
		warn(format, buf1, buf2);
	} else {
		warn(format, ref1, ref2);
	}
}

void
cut_all_chains(dbref obj)
{
	if (CONTENTS(obj) != NOTHING) {
		SanFixed(obj, "Cleared contents of %s");
		CONTENTS(obj) = NOTHING;
	}

	if (Typeof(obj) == TYPE_ROOM && db[obj].sp.room.exits != NOTHING) {
		SanFixed(obj, "Cleared exits of %s");
		db[obj].sp.room.exits = NOTHING;
	}
}

void
cut_bad_recyclable(void)
{
	dbref loop, prev;

	loop = recyclable;
	prev = NOTHING;
	while (loop != NOTHING) {
		if (!valid_ref(loop) || TYPEOF(loop) != TYPE_GARBAGE || FLAGS(loop) & SANEBIT) {
			SanFixed(loop, "Recyclable object %s is not TYPE_GARBAGE");
			if (prev != NOTHING)
				db[prev].next = NOTHING;
			else {
				recyclable = NOTHING;
			}
			return;
		}
		FLAGS(loop) |= SANEBIT;
		prev = loop;
		loop = db[loop].next;
	}
}

void
cut_bad_contents(dbref obj)
{
	dbref loop, prev;

	loop = CONTENTS(obj);
	prev = NOTHING;
	while (loop != NOTHING) {
		if (!valid_obj(loop) || FLAGS(loop) & SANEBIT ||
			TYPEOF(loop) == TYPE_EXIT || LOCATION(loop) != obj || loop == obj) {
			if (!valid_obj(loop)) {
				SanFixed(obj, "Contents chain for %s cut at invalid dbref");
			} else if (TYPEOF(loop) == TYPE_EXIT) {
				SanFixed2(obj, loop, "Contents chain for %s cut at exit %s");
			} else if (loop == obj) {
				SanFixed(obj, "Contents chain for %s cut at self-reference");
			} else if (LOCATION(loop) != obj) {
				SanFixed2(obj, loop, "Contents chain for %s cut at misplaced object %s");
			} else if (FLAGS(loop) & SANEBIT) {
				SanFixed2(obj, loop, "Contents chain for %s cut at already chained object %s");
			} else {
				SanFixed2(obj, loop, "Contents chain for %s cut at %s");
			}
			if (prev != NOTHING)
				db[prev].next = NOTHING;
			else
				CONTENTS(obj) = NOTHING;
			return;
		}
		FLAGS(loop) |= SANEBIT;
		prev = loop;
		loop = db[loop].next;
	}
}

void
cut_bad_exits(dbref obj)
{
	dbref loop, prev;

	CBUG(!(TYPEOF(obj) == TYPE_ROOM));
	loop = db[obj].sp.room.exits;
	prev = NOTHING;
	while (loop != NOTHING) {
		if (!valid_obj(loop) || FLAGS(loop) & SANEBIT ||
			TYPEOF(loop) != TYPE_EXIT || LOCATION(loop) != obj) {
			if (!valid_obj(loop)) {
				SanFixed(obj, "Exits chain for %s cut at invalid dbref");
			} else if (TYPEOF(loop) != TYPE_EXIT) {
				SanFixed2(obj, loop, "Exits chain for %s cut at non-exit %s");
			} else if (LOCATION(loop) != obj) {
				SanFixed2(obj, loop, "Exits chain for %s cut at misplaced exit %s");
			} else if (FLAGS(loop) & SANEBIT) {
				SanFixed2(obj, loop, "Exits chain for %s cut at already chained exit %s");
			} else {
				SanFixed2(obj, loop, "Exits chain for %s cut at %s");
			}
			if (prev != NOTHING)
				db[prev].next = NOTHING;
			else
				db[obj].sp.room.exits = NOTHING;
			return;
		}
		FLAGS(loop) |= SANEBIT;
		prev = loop;
		loop = db[loop].next;
	}
}

void
hacksaw_bad_chains(void)
{
	dbref loop;

	cut_bad_recyclable();
	for (loop = 0; loop < db_top; loop++) {
		if (TYPEOF(loop) != TYPE_ROOM && !is_item(loop) &&
			TYPEOF(loop) != TYPE_ENTITY) {
			cut_all_chains(loop);
		} else {
			cut_bad_contents(loop);
			if (TYPEOF(loop) == TYPE_ROOM)
				cut_bad_exits(loop);
		}
	}
}

char *
rand_password(void)
{
	int loop;
	char pwdchars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char password[17];
	int charslen = strlen(pwdchars);

	for (loop = 0; loop < 16; loop++) {
		password[loop] = pwdchars[(RANDOM() >> 8) % charslen];
	}
	password[16] = 0;
	return alloc_string(password);
}

void
create_lostandfound(dbref * player, dbref * room)
{
	char player_name[PLAYER_NAME_LIMIT + 2] = "lost+found";
	int temp = 0;

	*room = object_new();
	NAME(*room) = alloc_string("lost+found");
	LOCATION(*room) = GLOBAL_ENVIRONMENT;
	db[*room].sp.room.exits = NOTHING;
	db[*room].sp.room.dropto = NOTHING;
	FLAGS(*room) = TYPE_ROOM | SANEBIT;
	PUSH(*room, db[GLOBAL_ENVIRONMENT].contents);
	SanFixed(*room, "Using %s to resolve unknown location");

	while (lookup_player(player_name) != NOTHING && strlen(player_name) < PLAYER_NAME_LIMIT) {
		snprintf(player_name, sizeof(player_name), "lost+found%d", ++temp);
	}
	if (strlen(player_name) >= PLAYER_NAME_LIMIT) {
		warn("WARNING: Unable to get lost+found player, using %s",
		     unparse(GOD));
		*player = GOD;
	} else {
		const char *rpass;
		*player = object_new();
		NAME(*player) = alloc_string(player_name);
		LOCATION(*player) = *room;
		FLAGS(*player) = TYPE_ENTITY |  SANEBIT;
		OWNER(*player) = *player;
		ENTITY(*player)->home = *room;
		SETVALUE(*player, START_PENNIES);
		rpass = rand_password();
		PUSH(*player, db[*room].contents);
		add_player(*player);
		warn("Using %s (with password %s) to resolve "
				 "unknown owner", unparse(*player), rpass);
	}
	OWNER(*room) = *player;
}

void
fix_room(dbref obj)
{
	dbref i;

	i = db[obj].sp.room.dropto;

	if (!valid_ref(i) && i != HOME) {
		SanFixed(obj, "Removing invalid drop-to from %s");
		db[obj].sp.room.dropto = NOTHING;
	} else if (i >= 0 && !is_item(i) && TYPEOF(i) != TYPE_ROOM) {
		SanFixed2(obj, i, "Removing drop-to on %s to %s");
		db[obj].sp.room.dropto = NOTHING;
	}
}

void
fix_exit(dbref obj)
{
	int i, j;

	for (i = 0; i < db[obj].sp.exit.ndest;) {
		if (!valid_obj((db[obj].sp.exit.dest)[i]) &&
			(db[obj].sp.exit.dest)[i] != HOME) {
			SanFixed(obj, "Removing invalid destination from %s");
			db[obj].sp.exit.ndest--;
			for (j = i; j < db[obj].sp.exit.ndest; j++) {
				(db[obj].sp.exit.dest)[i] = (db[obj].sp.exit.dest)[i + 1];
			}
		} else {
			i++;
		}
	}
}

void
fix_entity(dbref obj)
{
	dbref i;

	i = ENTITY(obj)->home;

	if (!valid_obj(i) || TYPEOF(i) != TYPE_ROOM) {
		SanFixed2(obj, PLAYER_START, "Setting the home on %s to %s");
		ENTITY(obj)->home = PLAYER_START;
	}
}

void
fix_garbage(dbref obj)
{
	return;
}

void
find_misplaced_objects(void)
{
	dbref loop, player = NOTHING, room;

	for (loop = 0; loop < db_top; loop++) {
		if (TYPEOF(loop) != TYPE_ROOM &&
			!is_item(loop) &&
			TYPEOF(loop) != TYPE_ENTITY &&
			TYPEOF(loop) != TYPE_EXIT &&
			TYPEOF(loop) != TYPE_GARBAGE) {
			SanFixedRef(loop, "Object #%d is of unknown type");
			sanity_violated = 1;
			continue;
		}
		if (!NAME(loop) || !(*NAME(loop))) {
			switch TYPEOF
				(loop) {
			case TYPE_GARBAGE:
				NAME(loop) = "<garbage>";
				break;
			case TYPE_ENTITY:
				{
					char name[PLAYER_NAME_LIMIT + 1] = "Unnamed";
					int temp = 0;

					while (lookup_player(name) != NOTHING && strlen(name) < PLAYER_NAME_LIMIT) {
						snprintf(name, sizeof(name), "Unnamed%d", ++temp);
					}
					NAME(loop) = alloc_string(name);
					add_player(loop);
				}
				break;
			default:
				NAME(loop) = alloc_string("Unnamed");
				}
			SanFixed(loop, "Gave a name to %s");
		}
		if (TYPEOF(loop) != TYPE_GARBAGE) {
			if (!valid_obj(OWNER(loop)) || TYPEOF(OWNER(loop)) != TYPE_ENTITY) {
				if (player == NOTHING) {
					create_lostandfound(&player, &room);
				}
				SanFixed2(loop, player, "Set owner of %s to %s");
				OWNER(loop) = player;
			}
			if (loop != GLOBAL_ENVIRONMENT && (!valid_obj(LOCATION(loop)) ||
											   TYPEOF(LOCATION(loop)) == TYPE_GARBAGE ||
											   TYPEOF(LOCATION(loop)) == TYPE_EXIT ||
											   (TYPEOF(loop) == TYPE_ENTITY &&
												TYPEOF(LOCATION(loop)) == TYPE_ENTITY))) {
				if (TYPEOF(loop) == TYPE_ENTITY) {
					if (valid_obj(LOCATION(loop)) && TYPEOF(LOCATION(loop)) == TYPE_ENTITY) {
						dbref loop1;

						loop1 = LOCATION(loop);
						if (CONTENTS(loop1) == loop)
							CONTENTS(loop1) = db[loop].next;
						else
							for (loop1 = CONTENTS(loop1);
								 loop1 != NOTHING; loop1 = db[loop1].next) {
								if (db[loop1].next == loop) {
									db[loop1].next = db[loop].next;
									break;
								}
							}
					}
					LOCATION(loop) = PLAYER_START;
				} else {
					if (player == NOTHING) {
						create_lostandfound(&player, &room);
					}
					LOCATION(loop) = room;
				}
				PUSH(loop, CONTENTS(LOCATION(loop)));
				FLAGS(loop) |= SANEBIT;
				SanFixed2(loop, LOCATION(loop), "Set location of %s to %s");
			}
		} else {
			if (OWNER(loop) != NOTHING) {
				SanFixedRef(loop, "Set owner of recycled object #%d to NOTHING");
				OWNER(loop) = NOTHING;
			}
			if (LOCATION(loop) != NOTHING) {
				SanFixedRef(loop, "Set location of recycled object #%d to NOTHING");
				LOCATION(loop) = NOTHING;
			}
		}
		switch (TYPEOF(loop)) {
		case TYPE_ROOM:
			fix_room(loop);
			break;
		case TYPE_FOOD:
		case TYPE_DRINK:
		case TYPE_EQUIPMENT:
		case TYPE_THING:
			break;
		case TYPE_ENTITY:
			fix_entity(loop);
			break;
		case TYPE_EXIT:
			fix_exit(loop);
			break;
		case TYPE_GARBAGE:
			fix_garbage(loop);
			break;
		}
	}
}

void
adopt_orphans(void)
{
	dbref loop;

	for (loop = 0; loop < db_top; loop++) {
		if (!(FLAGS(loop) & SANEBIT)) {
			switch (TYPEOF(loop)) {
			case TYPE_ROOM:
			case TYPE_FOOD:
			case TYPE_DRINK:
			case TYPE_EQUIPMENT:
			case TYPE_THING:
			case TYPE_ENTITY:
				db[loop].next = db[LOCATION(loop)].contents;
				db[LOCATION(loop)].contents = loop;
				SanFixed2(loop, LOCATION(loop), "Orphaned object %s added to contents of %s");
				break;
			case TYPE_EXIT:
				db[loop].next = db[LOCATION(loop)].sp.room.exits;
				db[LOCATION(loop)].sp.room.exits = loop;
				SanFixed2(loop, LOCATION(loop), "Orphaned exit %s added to exits of %s");
				break;
			case TYPE_GARBAGE:
				db[loop].next = recyclable;
				recyclable = loop;
				SanFixedRef(loop, "Litter object %d moved to recycle bin");
				break;
			default:
				sanity_violated = 1;
				break;
			}
		}
	}
}

void
clean_global_environment(void)
{
	if (db[GLOBAL_ENVIRONMENT].next != NOTHING) {
		SanFixed(GLOBAL_ENVIRONMENT, "Removed the global environment %s from a chain");
		db[GLOBAL_ENVIRONMENT].next = NOTHING;
	}
	if (LOCATION(GLOBAL_ENVIRONMENT) != NOTHING) {
		SanFixed2(GLOBAL_ENVIRONMENT, LOCATION(GLOBAL_ENVIRONMENT),
				  "Removed the global environment %s from %s");
		LOCATION(GLOBAL_ENVIRONMENT) = NOTHING;
	}
}

void
sanfix(dbref player)
{
	dbref loop;

#ifdef GOD_PRIV
	if (player > NOTHING && !God(player)) {
#else
	if (player > NOTHING && !Wizard(player)) {
#endif

		notify(player, "Yeah right!  With a psyche like yours, you think "
			   "theres any hope of getting your sanity fixed?");
		return;
	}

	sanity_violated = 0;

	for (loop = 0; loop < db_top; loop++) {
		FLAGS(loop) &= ~SANEBIT;
	}
	FLAGS(GLOBAL_ENVIRONMENT) |= SANEBIT;

	hacksaw_bad_chains();
	find_misplaced_objects();
	adopt_orphans();
	clean_global_environment();

	for (loop = 0; loop < db_top; loop++) {
		FLAGS(loop) &= ~SANEBIT;
	}

	if (player > NOTHING) {
		if (!sanity_violated) {
			notify(player, "Database repair complete, please re-run"
			       " @sanity.  For details of repairs, check logs/sanfixed.");
		} else {
			notify(player, "Database repair complete, however the "
			       "database is still corrupt.  Please re-run @sanity.");
		}
	} else {
		fprintf(stderr, "Database repair complete, ");
		if (!sanity_violated)
			fprintf(stderr, "please re-run sanity check.\n");
		else
			fprintf(stderr, "however the database is still corrupt.\n"
					"Please re-run sanity check for details and fix it by hand.\n");
		fprintf(stderr, "For details of repairs made, check logs/sanfixed.\n");
	}
	if (sanity_violated)
		warn("WARNING: The database is still corrupted, please repair by hand");
}



char cbuf[1000];
char buf2[1000];

void
sanechange(dbref player, const char *command)
{
	dbref d, v;
	char field[50];
	char which[1000];
	char value[1000];
	int *ip;
	int results;

	if (player > NOTHING) {

#ifdef GOD_PRIV
		if (!God(player)) {
			notify(player, "Only GOD may alter the basic structure of the universe!");
#else
		if (!Wizard(player)) {
			notify(player, "Only Wizards may alter the basic structure of the universe!");
#endif
			return;
		}
		results = sscanf(command, "%s %s %s", which, field, value);
		sscanf(which, "#%d", &d);
		sscanf(value, "#%d", &v);
	} else {
		results = sscanf(command, "%s %s %s", which, field, value);
		sscanf(which, "%d", &d);
		sscanf(value, "%d", &v);
	}

	if (results != 3) {
		d = v = 0;
		strlcpy(field, "help", sizeof(field));
	}

	*buf2 = 0;

	if (!valid_ref(d) || d < 0) {
		SanPrint(player, "## %d is an invalid dbref.", d);
		return;
	}

	if (!strcmp(field, "next")) {
		strlcpy(buf2, unparse(NEXTOBJ(d)), sizeof(buf2));
		NEXTOBJ(d) = v;
		SanPrint(player, "## Setting #%d's next field to %s", d, unparse(v));

	} else if (!strcmp(field, "exits")) {
		strlcpy(buf2, unparse(db[d].sp.room.exits), sizeof(buf2));
		db[d].sp.room.exits = v;
		SanPrint(player, "## Setting #%d's Exits list start to %s", d, unparse(v));

	} else if (!strcmp(field, "contents")) {
		strlcpy(buf2, unparse(CONTENTS(d)), sizeof(buf2));
		CONTENTS(d) = v;
		SanPrint(player, "## Setting #%d's Contents list start to %s", d, unparse(v));

	} else if (!strcmp(field, "location")) {
		strlcpy(buf2, unparse(LOCATION(d)), sizeof(buf2));
		LOCATION(d) = v;
		SanPrint(player, "## Setting #%d's location to %s", d, unparse(v));

	} else if (!strcmp(field, "owner")) {
		strlcpy(buf2, unparse(OWNER(d)), sizeof(buf2));
		OWNER(d) = v;
		SanPrint(player, "## Setting #%d's owner to %s", d, unparse(v));

	} else if (!strcmp(field, "home")) {
		switch (TYPEOF(d)) {
		case TYPE_ENTITY:
			ip = &(ENTITY(d)->home);
			break;

		default:
			printf("%s has no home to set.\n", unparse(d));
			return;
		}

		strlcpy(buf2, unparse(*ip), sizeof(buf2));
		*ip = v;
		printf("Setting home to: %s\n", unparse(v));

	} else {
		if (player > NOTHING) {
			notify(player, "@sanchange <dbref> <field> <object>");
		} else {
			SanPrint(player, "change command help:");
			SanPrint(player, "c <dbref> <field> <object>");
		}
		SanPrint(player, "Fields are:     exits       Start of Exits list.");
		SanPrint(player, "                contents    Start of Contents list.");
		SanPrint(player, "                next        Next object in list.");
		SanPrint(player, "                location    Object's Location.");
		SanPrint(player, "                home        Object's Home.");
		SanPrint(player, "                owner       Object's Owner.");
		return;
	}

	if (*buf2) {
		SanPrint(player, "## Old value was %s", buf2);
	}
}
