/* $Header$ */

/*
 * $Log: move.c,v $
 * Revision 1.2  2000/03/29 12:21:02  revar
 * Reformatted all code into consistent format.
 * 	Tabs are 4 spaces.
 * 	Indents are one tab.
 * 	Braces are generally K&R style.
 * Added ARRAY_DIFF, ARRAY_INTERSECT and ARRAY_UNION to man.txt.
 * Rewrote restart script as a bourne shell script.
 *
 * Revision 1.1.1.1  2000/01/14 22:56:07  revar
 * Initial Sourceforge checkin, fb6.00a29
 *
 * Revision 1.2  2000/01/14 22:53:01  foxen
 * Added Points' SECURE_THING_MOVING @tune support.
 *
 * Revision 1.1.1.1  1999/12/12 07:27:43  foxen
 * Initial FB6 CVS checkin.
 *
 * Revision 1.2  1996/07/03 22:35:46  foxen
 * 5.55 checkpoint.
 *
 * Revision 1.1  1996/06/12 02:43:08  foxen
 * Initial revision
 *
 * Revision 5.18  1994/03/21  11:00:42  foxen
 * Autoconfiguration mods.
 *
 * Revision 5.17  1994/03/14  12:20:58  foxen
 * Fb5.20 release checkpoint.
 *
 * Revision 5.16  1994/02/09  02:58:48  foxen
 * Fixed _depart running order.
 *
 * Revision 5.15  1994/02/09  02:51:24  foxen
 * Fixed _depart AGAIN, this time to run from the right room.
 *
 * Revision 5.14  1994/02/09  02:04:44  foxen
 * Made player be moved before doing _depart's.
 *
 * Revision 5.13  1994/02/08  11:08:32  foxen
 * Changes to make general propqueues run immediately, instead of queueing up.
 *
 * Revision 5.12  1994/01/18  20:52:20  foxen
 * Version 5.15 release.
 *
 * Revision 5.11  1993/12/20  06:22:51  foxen
 * *** empty log message ***
 *
 * Revision 5.1  1993/12/17  00:07:33  foxen
 * initial revision.
 *
 * Revision 1.11  90/09/16  04:42:35  rearl
 * Preparation code added for disk-based MUCK.
 *
 * Revision 1.10  90/09/15  22:26:57  rearl
 * Fixed moveto bug from HOME.
 *
 * Revision 1.9  90/09/13  06:28:04  rearl
 * send_contents changed to be usable for do_toad() in wiz.c
 *
 * Revision 1.8  90/09/10  02:20:29  rearl
 * Put exec_or_notify in the drop messages of exits.
 *
 * Revision 1.7  90/09/04  18:39:43  rearl
 * Added some prototypes for externs.
 *
 * Revision 1.6  90/09/01  05:59:21  rearl
 * Took out drop code for rooms, setting parent depends on @tel now.
 *
 * Revision 1.5  90/08/27  03:31:58  rearl
 * Added environment support.
 *
 * Revision 1.4  90/08/15  03:06:01  rearl
 * Fixed 0 PENNY_RATE bug.  Took out #ifdef GENDER.
 *
 * Revision 1.3  90/08/05  03:19:47  rearl
 * Redid matching routines.
 *
 * Revision 1.2  90/07/29  17:41:43  rearl
 * Fixed moveto problems relating to ROOM programs, also programs
 * go to their owner rather than "home" -- programs have no home by
 * definition.
 *
 * Revision 1.1  90/07/19  23:03:55  casie
 * Initial revision
 *
 *
 */

#include "copyright.h"
#include "config.h"

#include "db.h"
#include "props.h"
#include "params.h"
#include "tune.h"
#include "interface.h"
#include "match.h"
#include "externs.h"

void
moveto(dbref what, dbref where)
{
	dbref loc;

	/* do NOT move garbage */
	if (what != NOTHING && Typeof(what) == TYPE_GARBAGE) {
		return;
	}

	/* remove what from old loc */
	if ((loc = DBFETCH(what)->location) != NOTHING) {
		DBSTORE(loc, contents, remove_first(DBFETCH(loc)->contents, what));
	}
	/* test for special cases */
	switch (where) {
	case NOTHING:
		DBSTORE(what, location, NOTHING);
		return;					/* NOTHING doesn't have contents */
	case HOME:
		switch (Typeof(what)) {
		case TYPE_PLAYER:
			where = PLAYER_HOME(what);
			break;
		case TYPE_THING:
			where = THING_HOME(what);
			if (parent_loop_check(what, where))
				where = PLAYER_HOME(OWNER(what));
			break;
		case TYPE_ROOM:
			where = GLOBAL_ENVIRONMENT;
			break;
		case TYPE_PROGRAM:
			where = OWNER(what);
			break;
		}
	}

	/* now put what in where */
	PUSH(what, DBFETCH(where)->contents);
	DBDIRTY(where);
	DBSTORE(what, location, where);
}

dbref reverse(dbref);
void
send_contents(int descr, dbref loc, dbref dest)
{
	dbref first;
	dbref rest;
	dbref where;

	first = DBFETCH(loc)->contents;
	DBSTORE(loc, contents, NOTHING);

	/* blast locations of everything in list */
	DOLIST(rest, first) {
		DBSTORE(rest, location, NOTHING);
	}

	while (first != NOTHING) {
		rest = DBFETCH(first)->next;
		if ((Typeof(first) != TYPE_THING)
			&& (Typeof(first) != TYPE_PROGRAM)) {
			moveto(first, loc);
		} else {
			where = FLAGS(first) & STICKY ? HOME : dest;
			if (tp_thing_movement && (Typeof(first) == TYPE_THING)) {
				enter_room(descr, first,
						   parent_loop_check(first, where) ? loc : where,
						   DBFETCH(first)->location);
			} else {
				moveto(first, parent_loop_check(first, where) ? loc : where);
			}
		}
		first = rest;
	}

	DBSTORE(loc, contents, reverse(DBFETCH(loc)->contents));
}

void
maybe_dropto(int descr, dbref loc, dbref dropto)
{
	dbref thing;

	if (loc == dropto)
		return;					/* bizarre special case */

	/* check for players */
	DOLIST(thing, DBFETCH(loc)->contents) {
		if (Typeof(thing) == TYPE_PLAYER)
			return;
	}

	/* no players, send everything to the dropto */
	send_contents(descr, loc, dropto);
}

int
parent_loop_check(dbref source, dbref dest)
{
	if (source == dest)
		return 1;				/* That's an easy one! */
	if (dest == NOTHING)
		return 0;
	if (dest == HOME)
		return 0;
	if (Typeof(dest) == TYPE_THING && parent_loop_check(source, THING_HOME(dest)))
		return 1;
	return parent_loop_check(source, DBFETCH(dest)->location);
}

static int donelook = 0;
void
enter_room(int descr, dbref player, dbref loc, dbref exit)
{
	dbref old;
	dbref dropto;
	char buf[BUFFER_LEN];

	/* check for room == HOME */
	if (loc == HOME)
		loc = PLAYER_HOME(player);	/* home */

	/* get old location */
	old = DBFETCH(player)->location;

	/* check for self-loop */
	/* self-loops don't do move or other player notification */
	/* but you still get autolook and penny check */
	if (loc != old) {

		/* go there */
		moveto(player, loc);

		if (old != NOTHING) {
			propqueue(descr, player, old, exit, player, NOTHING, "_depart", "Depart", 1, 1);
			envpropqueue(descr, player, old, exit, old, NOTHING, "_depart", "Depart", 1, 1);

			propqueue(descr, player, old, exit, player, NOTHING, "_odepart", "Odepart", 1, 0);
			envpropqueue(descr, player, old, exit, old, NOTHING, "_odepart", "Odepart", 1, 0);

			/* notify others unless DARK */
			if (!Dark(old) && !Dark(player) &&
				((Typeof(player) != TYPE_THING) ||
				 ((Typeof(player) == TYPE_THING) && (FLAGS(player) & (ZOMBIE | VEHICLE))))
				&& (Typeof(exit) != TYPE_EXIT || !Dark(exit))) {
#if !defined(QUIET_MOVES)
				sprintf(buf, "%s has left.", PNAME(player));
				notify_except(DBFETCH(old)->contents, player, buf, player);
#endif
			}
		}

		/* if old location has STICKY dropto, send stuff through it */
		if (old != NOTHING && Typeof(old) == TYPE_ROOM
			&& (dropto = DBFETCH(old)->sp.room.dropto) != NOTHING && (FLAGS(old) & STICKY)) {
			maybe_dropto(descr, old, dropto);
		}

		/* tell other folks in new location if not DARK */
		if (!Dark(loc) && !Dark(player) &&
			((Typeof(player) != TYPE_THING) ||
			 ((Typeof(player) == TYPE_THING) && (FLAGS(player) & (ZOMBIE | VEHICLE))))
			&& (Typeof(exit) != TYPE_EXIT || !Dark(exit))) {
#if !defined(QUIET_MOVES)
			sprintf(buf, "%s has arrived.", PNAME(player));
			notify_except(DBFETCH(loc)->contents, player, buf, player);
#endif
		}
	}
	/* autolook */
	if ((Typeof(player) != TYPE_THING) ||
		((Typeof(player) == TYPE_THING) && (FLAGS(player) & (ZOMBIE | VEHICLE)))) {
		if (donelook < 8) {
			donelook++;
			if (can_move(descr, player, tp_autolook_cmd, 1)) {
				do_move(descr, player, tp_autolook_cmd, 1);
			} else {
				do_look_around(descr, player);
			}
			donelook--;
		} else {
			notify(player, "Look aborted because of look action loop.");
		}
	}

	if (tp_penny_rate != 0) {
		/* check for pennies */
		if (!controls(player, loc)
			&& PLAYER_PENNIES(player) <= tp_max_pennies && RANDOM() % tp_penny_rate == 0) {
			notify_fmt(player, "You found a %s!", tp_penny);
			PLAYER_ADD_PENNIES(OWNER(player), 1);
			DBDIRTY(OWNER(player));
		}
	}

	if (loc != old) {
		envpropqueue(descr, player, loc, exit, player, NOTHING, "_arrive", "Arrive", 1, 1);
		envpropqueue(descr, player, loc, exit, player, NOTHING, "_oarrive", "Oarrive", 1, 0);
	}
}

void
send_home(int descr, dbref thing, int puppethome)
{
	switch (Typeof(thing)) {
	case TYPE_PLAYER:
		/* send his possessions home first! */
		/* that way he sees them when he arrives */
		send_contents(descr, thing, HOME);
		enter_room(descr, thing, PLAYER_HOME(thing), DBFETCH(thing)->location);
		break;
	case TYPE_THING:
		if (puppethome)
			send_contents(descr, thing, HOME);
		if (tp_thing_movement || (FLAGS(thing) & (ZOMBIE | LISTENER))) {
			enter_room(descr, thing, PLAYER_HOME(thing), DBFETCH(thing)->location);
			break;
		}
		moveto(thing, HOME);	/* home */
		break;
	case TYPE_PROGRAM:
		moveto(thing, OWNER(thing));
		break;
	default:
		/* no effect */
		break;
	}
	return;
}

int
can_move(int descr, dbref player, const char *direction, int lev)
{
	struct match_data md;

	if (tp_allow_home && !string_compare(direction, "home"))
		return 1;

	/* otherwise match on exits */
	init_match(descr, player, direction, TYPE_EXIT, &md);
	md.match_level = lev;
	match_all_exits(&md);
	return (last_match_result(&md) != NOTHING);
}

/*
 * trigger()
 *
 * This procedure triggers a series of actions, or meta-actions
 * which are contained in the 'dest' field of the exit.
 * Locks other than the first one are over-ridden.
 *
 * `player' is the player who triggered the exit
 * `exit' is the exit triggered
 * `pflag' is a flag which indicates whether player and room exits
 * are to be used (non-zero) or ignored (zero).  Note that
 * player/room destinations triggered via a meta-link are
 * ignored.
 *
 */

void
trigger(int descr, dbref player, dbref exit, int pflag)
{
	int i;
	dbref dest;
	int sobjact;				/* sticky object action flag, sends home

								   * source obj */
	int succ;
	struct frame *tmpfr;

	sobjact = 0;
	succ = 0;

	for (i = 0; i < DBFETCH(exit)->sp.exit.ndest; i++) {
		dest = (DBFETCH(exit)->sp.exit.dest)[i];
		if (dest == HOME)
			dest = PLAYER_HOME(player);
		switch (Typeof(dest)) {
		case TYPE_ROOM:
			if (pflag) {
				if (parent_loop_check(player, dest)) {
					notify(player, "That would cause a paradox.");
					break;
				}
				if (!Wizard(OWNER(player)) && Typeof(player) == TYPE_THING
					&& (FLAGS(dest) & ZOMBIE)) {
					notify(player, "You can't go that way.");
					break;
				}
				if ((FLAGS(player) & VEHICLE) && ((FLAGS(dest) | FLAGS(exit)) & VEHICLE)) {
					notify(player, "You can't go that way.");
					break;
				}
				if (GETDROP(exit))
					exec_or_notify(descr, player, exit, GETDROP(exit), "(@Drop)");
				if (GETODROP(exit) && !Dark(player)) {
					parse_omessage(descr, player, dest, exit, GETODROP(exit),
								   PNAME(player), "(@Odrop)");
				}
				enter_room(descr, player, dest, exit);
				succ = 1;
			}
			break;
		case TYPE_THING:
			if (dest == getloc(exit) && (FLAGS(dest) & VEHICLE)) {
				if (pflag) {
					if (parent_loop_check(player, dest)) {
						notify(player, "That would cause a paradox.");
						break;
					}
					if (GETDROP(exit))
						exec_or_notify(descr, player, exit, GETDROP(exit), "(@Drop)");
					if (GETODROP(exit) && !Dark(player)) {
						parse_omessage(descr, player, dest, exit, GETODROP(exit),
									   PNAME(player), "(@Odrop)");
					}
					enter_room(descr, player, dest, exit);
					succ = 1;
				}
			} else {
				if (Typeof(DBFETCH(exit)->location) == TYPE_THING) {
					if (parent_loop_check(dest, getloc(getloc(exit)))) {
						notify(player, "That would cause a paradox.");
						break;
					}
					if (tp_thing_movement) {
						enter_room(descr, dest, DBFETCH(DBFETCH(exit)->location)->location,
								   exit);
					} else {
						moveto(dest, DBFETCH(DBFETCH(exit)->location)->location);
					}
					if (!(FLAGS(exit) & STICKY)) {
						/* send home source object */
						sobjact = 1;
					}
				} else {
					if (parent_loop_check(dest, getloc(exit))) {
						notify(player, "That would cause a paradox.");
						break;
					}
					if (tp_thing_movement) {
						enter_room(descr, dest, DBFETCH(exit)->location, exit);
					} else {
						moveto(dest, DBFETCH(exit)->location);
					}
				}
				if (GETSUCC(exit))
					succ = 1;
			}
			break;
		case TYPE_EXIT:		/* It's a meta-link(tm)! */
			ts_useobject(dest);
			trigger(descr, player, (DBFETCH(exit)->sp.exit.dest)[i], 0);
			if (GETSUCC(exit))
				succ = 1;
			break;
		case TYPE_PLAYER:
			if (pflag && DBFETCH(dest)->location != NOTHING) {
				if (parent_loop_check(player, dest)) {
					notify(player, "That would cause a paradox.");
					break;
				}
				succ = 1;
				if (FLAGS(dest) & JUMP_OK) {
					if (GETDROP(exit)) {
						exec_or_notify(descr, player, exit, GETDROP(exit), "(@Drop)");
					}
					if (GETODROP(exit) && !Dark(player)) {
						parse_omessage(descr, player, getloc(dest), exit,
									   GETODROP(exit), PNAME(player), "(@Odrop)");
					}
					enter_room(descr, player, DBFETCH(dest)->location, exit);
				} else {
					notify(player, "That player does not wish to be disturbed.");
				}
			}
			break;
		case TYPE_PROGRAM:
			tmpfr = interp(descr, player, DBFETCH(player)->location, dest, exit,
						   FOREGROUND, STD_REGUID);
			if (tmpfr) {
				interp_loop(player, dest, tmpfr, 0);
			}
			return;
		}
	}
	if (sobjact)
		send_home(descr, DBFETCH(exit)->location, 0);
	if (!succ && pflag)
		notify(player, "Done.");
}

void
do_move(int descr, dbref player, const char *direction, int lev)
{
	dbref exit;
	dbref loc;
	char buf[BUFFER_LEN];
	struct match_data md;

	if (tp_allow_home && !string_compare(direction, "home")) {
		/* send him home */
		/* but steal all his possessions */
		if ((loc = DBFETCH(player)->location) != NOTHING) {
			/* tell everybody else */
			sprintf(buf, "%s goes home.", PNAME(player));
			notify_except(DBFETCH(loc)->contents, player, buf, player);
		}
		/* give the player the messages */
		notify(player, "There's no place like home...");
		notify(player, "There's no place like home...");
		notify(player, "There's no place like home...");
		notify(player, "You wake up back home, without your possessions.");
		send_home(descr, player, 1);
	} else {
		/* find the exit */
		init_match_check_keys(descr, player, direction, TYPE_EXIT, &md);
		md.match_level = lev;
		match_all_exits(&md);
		switch (exit = match_result(&md)) {
		case NOTHING:
			notify(player, "You can't go that way.");
			break;
		case AMBIGUOUS:
			notify(player, "I don't know which way you mean!");
			break;
		default:
			/* we got one */
			/* check to see if we got through */
			ts_useobject(exit);
			loc = DBFETCH(player)->location;
			if (can_doit(descr, player, exit, "You can't go that way.")) {
				trigger(descr, player, exit, 1);
			}
			break;
		}
	}
}


void
do_leave(int descr, dbref player)
{
	dbref loc, dest;

	loc = DBFETCH(player)->location;
	if (loc == NOTHING || Typeof(loc) == TYPE_ROOM) {
		notify(player, "You can't go that way.");
		return;
	}

	if (!(FLAGS(loc) & VEHICLE)) {
		notify(player, "You can only exit vehicles.");
		return;
	}

	dest = DBFETCH(loc)->location;
	if (Typeof(dest) != TYPE_ROOM && Typeof(dest) != TYPE_THING) {
		notify(player, "You can't exit a vehicle inside of a player.");
		return;
	}

/*
 *  if (Typeof(dest) == TYPE_ROOM && !controls(player, dest)
 *          && !(FLAGS(dest) | JUMP_OK)) {
 *      notify(player, "You can't go that way.");
 *      return;
 *  }
 */

	if (parent_loop_check(player, dest)) {
		notify(player, "You can't go that way.");
		return;
	}

	notify(player, "You exit the vehicle.");
	enter_room(descr, player, dest, loc);
}


void
do_get(int descr, dbref player, const char *what, const char *obj)
{
	dbref thing, cont;
	int cando;
	struct match_data md;

	init_match_check_keys(descr, player, what, TYPE_THING, &md);
	match_neighbor(&md);
	match_possession(&md);
	if (Wizard(OWNER(player)))
		match_absolute(&md);	/* the wizard has long fingers */

	if ((thing = noisy_match_result(&md)) != NOTHING) {
		cont = thing;
		if (obj && *obj) {
			init_match_check_keys(descr, player, obj, TYPE_THING, &md);
			match_rmatch(cont, &md);
			if (Wizard(OWNER(player)))
				match_absolute(&md);	/* the wizard has long fingers */
			if ((thing = noisy_match_result(&md)) == NOTHING) {
				return;
			}
			if (Typeof(cont) == TYPE_PLAYER) {
				notify(player, "You can't steal things from players.");
				return;
			}
			if (!test_lock_false_default(descr, player, cont, "_/clk")) {
				notify(player, "You can't open that container.");
				return;
			}
		}
		if (DBFETCH(thing)->location == player) {
			notify(player, "You already have that!");
			return;
		}
		if (Typeof(cont) == TYPE_PLAYER) {
			notify(player, "You can't steal stuff from players.");
			return;
		}
		if (parent_loop_check(thing, player)) {
			notify(player, "You can't pick yourself up by your bootstraps!");
			return;
		}
		switch (Typeof(thing)) {
		case TYPE_THING:
			ts_useobject(thing);
		case TYPE_PROGRAM:
			if (obj && *obj) {
				cando = could_doit(descr, player, thing);
				if (!cando)
					notify(player, "You can't get that.");
			} else {
				cando = can_doit(descr, player, thing, "You can't pick that up.");
			}
			if (cando) {
				if (tp_thing_movement && (Typeof(thing) == TYPE_THING)) {
					enter_room(descr, thing, player, DBFETCH(thing)->location);
				} else {
					moveto(thing, player);
				}
				notify(player, "Taken.");
			}
			break;
		default:
			notify(player, "You can't take that!");
			break;
		}
	}
}

void
do_drop(int descr, dbref player, const char *name, const char *obj)
{
	dbref loc, cont;
	dbref thing;
	char buf[BUFFER_LEN];
	struct match_data md;

	if ((loc = getloc(player)) == NOTHING)
		return;

	init_match(descr, player, name, NOTYPE, &md);
	match_possession(&md);
	if ((thing = noisy_match_result(&md)) == NOTHING || thing == AMBIGUOUS)
		return;

	cont = loc;
	if (obj && *obj) {
		init_match(descr, player, obj, NOTYPE, &md);
		match_possession(&md);
		match_neighbor(&md);
		if (Wizard(OWNER(player)))
			match_absolute(&md);	/* the wizard has long fingers */
		if ((cont = noisy_match_result(&md)) == NOTHING || thing == AMBIGUOUS) {
			return;
		}
	}
	switch (Typeof(thing)) {
	case TYPE_THING:
		ts_useobject(thing);
	case TYPE_PROGRAM:
		if (DBFETCH(thing)->location != player) {
			/* Shouldn't ever happen. */
			notify(player, "You can't drop that.");
			break;
		}
		if (Typeof(cont) != TYPE_ROOM && Typeof(cont) != TYPE_PLAYER &&
			Typeof(cont) != TYPE_THING) {
			notify(player, "You can't put anything in that.");
			break;
		}
		if (Typeof(cont) != TYPE_ROOM &&
			!test_lock_false_default(descr, player, cont, "_/clk")) {
			notify(player, "You don't have permission to put something in that.");
			break;
		}
		if (parent_loop_check(thing, cont)) {
			notify(player, "You can't put something inside of itself.");
			break;
		}
		if (Typeof(cont) == TYPE_ROOM && (FLAGS(thing) & STICKY) &&
			Typeof(thing) == TYPE_THING) {
			send_home(descr, thing, 0);
		} else {
			int immediate_dropto = (Typeof(cont) == TYPE_ROOM &&
									DBFETCH(cont)->sp.room.dropto != NOTHING

									&& !(FLAGS(cont) & STICKY));

			if (tp_thing_movement && (Typeof(thing) == TYPE_THING)) {
				enter_room(descr, thing,
						   immediate_dropto ? DBFETCH(cont)->sp.room.dropto : cont, player);
			} else {
				moveto(thing, immediate_dropto ? DBFETCH(cont)->sp.room.dropto : cont);
			}
		}
		if (Typeof(cont) == TYPE_THING) {
			notify(player, "Put away.");
			return;
		} else if (Typeof(cont) == TYPE_PLAYER) {
			notify_fmt(cont, "%s hands you %s", PNAME(player), PNAME(thing));
			notify_fmt(player, "You hand %s to %s", PNAME(thing), PNAME(cont));
			return;
		}

		if (GETDROP(thing))
			exec_or_notify(descr, player, thing, GETDROP(thing), "(@Drop)");
		else
			notify(player, "Dropped.");

		if (GETDROP(loc))
			exec_or_notify(descr, player, loc, GETDROP(loc), "(@Drop)");

		if (GETODROP(thing)) {
			parse_omessage(descr, player, loc, thing, GETODROP(thing),
						   PNAME(player), "(@Odrop)");
		} else {
			sprintf(buf, "%s drops %s.", PNAME(player), PNAME(thing));
			notify_except(DBFETCH(loc)->contents, player, buf, player);
		}

		if (GETODROP(loc)) {
			parse_omessage(descr, player, loc, loc, GETODROP(loc), PNAME(thing), "(@Odrop)");
		}
		break;
	default:
		notify(player, "You can't drop that.");
		break;
	}
}

void
do_recycle(int descr, dbref player, const char *name)
{
	dbref thing;
	char buf[BUFFER_LEN];
	struct match_data md;

	init_match(descr, player, name, TYPE_THING, &md);
	match_all_exits(&md);
	match_neighbor(&md);
	match_possession(&md);
	match_registered(&md);
	match_here(&md);
	if (Wizard(OWNER(player))) {
		match_absolute(&md);
	}
	if ((thing = noisy_match_result(&md)) != NOTHING) {
		if (!controls(player, thing)) {
			notify(player, "Permission denied.");
		} else {
			switch (Typeof(thing)) {
			case TYPE_ROOM:
				if (OWNER(thing) != OWNER(player)) {
					notify(player, "Permission denied.");
					return;
				}
				if (thing == tp_player_start || thing == GLOBAL_ENVIRONMENT) {
					notify(player, "This room may not be recycled.");
					return;
				}
				break;
			case TYPE_THING:
				if (OWNER(thing) != OWNER(player)) {
					notify(player, "Permission denied.");
					return;
				}
				if (thing == player) {
					sprintf(buf,
							"%.512s's owner commands it to kill itself.  It blinks a few times in shock, and says, \"But.. but.. WHY?\"  It suddenly clutches it's heart, grimacing with pain..  Staggers a few steps before falling to it's knees, then plops down on it's face.  *thud*  It kicks it's legs a few times, with weakening force, as it suffers a seizure.  It's color slowly starts changing to purple, before it explodes with a fatal *POOF*!",
							PNAME(thing));
					notify_except(DBFETCH(getloc(thing))->contents, thing, buf, player);
					notify(OWNER(player), buf);
					notify(OWNER(player), "Now don't you feel guilty?");
				}
				break;
			case TYPE_EXIT:
				if (OWNER(thing) != OWNER(player)) {
					notify(player, "Permission denied.");
					return;
				}
				if (!unset_source(player, DBFETCH(player)->location, thing)) {
					notify(player, "You can't do that to an exit in another room.");
					return;
				}
				break;
			case TYPE_PLAYER:
				notify(player, "You can't recycle a player!");
				return;
				/* NOTREACHED */
				break;
			case TYPE_PROGRAM:
				if (OWNER(thing) != OWNER(player)) {
					notify(player, "Permission denied.");
					return;
				}
				break;
			case TYPE_GARBAGE:
				notify(player, "That's already garbage!");
				return;
				/* NOTREACHED */
				break;
			}
			recycle(descr, player, thing);
			notify(player, "Thank you for recycling.");
		}
	}
}

void
recycle(int descr, dbref player, dbref thing)
{
	extern dbref recyclable;
	static int depth = 0;
	dbref first;
	dbref rest;
	char buf[2048];
	int looplimit;

	depth++;
	/* dequeue any MUF or MPI events for the given object */
	dequeue_prog(thing, 0);
	switch (Typeof(thing)) {
	case TYPE_ROOM:
		if (!Wizard(OWNER(thing)))
			PLAYER_ADD_PENNIES(OWNER(thing), tp_room_cost);
		DBDIRTY(OWNER(thing));
		for (first = DBFETCH(thing)->exits; first != NOTHING; first = rest) {
			rest = DBFETCH(first)->next;
			if (DBFETCH(first)->location == NOTHING || DBFETCH(first)->location == thing)
				recycle(descr, player, first);
		}
		notify_except(DBFETCH(thing)->contents, NOTHING,
					  "You feel a wrenching sensation...", player);
		break;
	case TYPE_THING:
		if (!Wizard(OWNER(thing)))
			PLAYER_ADD_PENNIES(OWNER(thing), THING_VALUE(thing));
		DBDIRTY(OWNER(thing));
		for (first = DBFETCH(thing)->exits; first != NOTHING; first = rest) {
			rest = DBFETCH(first)->next;
			if (DBFETCH(first)->location == NOTHING || DBFETCH(first)->location == thing)
				recycle(descr, player, first);
		}
		break;
	case TYPE_EXIT:
		if (!Wizard(OWNER(thing)))
			PLAYER_ADD_PENNIES(OWNER(thing), tp_exit_cost);
		if (!Wizard(OWNER(thing)))
			if (DBFETCH(thing)->sp.exit.ndest != 0)
				PLAYER_ADD_PENNIES(OWNER(thing), tp_link_cost);
		DBDIRTY(OWNER(thing));
		break;
	case TYPE_PROGRAM:
		sprintf(buf, "muf/%d.m", (int) thing);
		unlink(buf);
		break;
	}

	for (rest = 0; rest < db_top; rest++) {
		switch (Typeof(rest)) {
		case TYPE_ROOM:
			if (DBFETCH(rest)->sp.room.dropto == thing) {
				DBFETCH(rest)->sp.room.dropto = NOTHING;
				DBDIRTY(rest);
			}
			if (DBFETCH(rest)->exits == thing) {
				DBFETCH(rest)->exits = DBFETCH(thing)->next;
				DBDIRTY(rest);
			}
			if (OWNER(rest) == thing) {
				OWNER(rest) = GOD;
				DBDIRTY(rest);
			}
			break;
		case TYPE_THING:
			if (THING_HOME(rest) == thing) {
				if (PLAYER_HOME(OWNER(rest)) == thing)
					PLAYER_SET_HOME(OWNER(rest), tp_player_start);
				THING_SET_HOME(rest, PLAYER_HOME(OWNER(rest)));
				DBDIRTY(rest);
			}
			if (DBFETCH(rest)->exits == thing) {
				DBFETCH(rest)->exits = DBFETCH(thing)->next;
				DBDIRTY(rest);
			}
			if (OWNER(rest) == thing) {
				OWNER(rest) = GOD;
				DBDIRTY(rest);
			}
			break;
		case TYPE_EXIT:
			{
				int i, j;

				for (i = j = 0; i < DBFETCH(rest)->sp.exit.ndest; i++) {
					if ((DBFETCH(rest)->sp.exit.dest)[i] != thing)
						(DBFETCH(rest)->sp.exit.dest)[j++] = (DBFETCH(rest)->sp.exit.dest)[i];
				}
				if (j < DBFETCH(rest)->sp.exit.ndest) {
					PLAYER_ADD_PENNIES(OWNER(rest), tp_link_cost);
					DBDIRTY(OWNER(rest));
					DBFETCH(rest)->sp.exit.ndest = j;
					DBDIRTY(rest);
				}
			}
			if (OWNER(rest) == thing) {
				OWNER(rest) = GOD;
				DBDIRTY(rest);
			}
			break;
		case TYPE_PLAYER:
			if (Typeof(thing) == TYPE_PROGRAM && (FLAGS(rest) & INTERACTIVE)
				&& (PLAYER_CURR_PROG(rest) == thing)) {
				if (FLAGS(rest) & READMODE) {
					notify(rest,
						   "The program you were running has been recycled.  Aborting program.");
				} else {
					free_prog_text(PROGRAM_FIRST(thing));
					PROGRAM_SET_FIRST(thing, NULL);
					PLAYER_SET_INSERT_MODE(rest, 0);
					FLAGS(thing) &= ~INTERNAL;
					FLAGS(rest) &= ~INTERACTIVE;
					PLAYER_SET_CURR_PROG(rest, NOTHING);
					notify(rest,
						   "The program you were editing has been recycled.  Exiting Editor.");
				}
			}
			if (PLAYER_HOME(rest) == thing) {
				PLAYER_SET_HOME(rest, tp_player_start);
				DBDIRTY(rest);
			}
			if (DBFETCH(rest)->exits == thing) {
				DBFETCH(rest)->exits = DBFETCH(thing)->next;
				DBDIRTY(rest);
			}
			if (PLAYER_CURR_PROG(rest) == thing)
				PLAYER_SET_CURR_PROG(rest, 0);
			break;
		case TYPE_PROGRAM:
			if (OWNER(rest) == thing) {
				OWNER(rest) = GOD;
				DBDIRTY(rest);
			}
		}
		/*
		 *if (DBFETCH(rest)->location == thing)
		 *    DBSTORE(rest, location, NOTHING);
		 */
		if (DBFETCH(rest)->contents == thing)
			DBSTORE(rest, contents, DBFETCH(thing)->next);
		if (DBFETCH(rest)->next == thing)
			DBSTORE(rest, next, DBFETCH(thing)->next);
	}

	looplimit = db_top;
	while ((looplimit-- > 0) && ((first = DBFETCH(thing)->contents) != NOTHING)) {
		if (Typeof(first) == TYPE_PLAYER ||
			(tp_thing_movement && (Typeof(first) == TYPE_THING))) {
			enter_room(descr, first, HOME, DBFETCH(thing)->location);
			/* If the room is set to drag players back, there'll be no
			 * reasoning with it.  DRAG the player out.
			 */
			if (DBFETCH(first)->location == thing) {
				notify_fmt(player, "Escaping teleport loop!");
				moveto(first, HOME);
			}
		} else {
			moveto(first, HOME);
		}
	}


	moveto(thing, NOTHING);

	depth--;

	db_free_object(thing);
	db_clear_object(thing);
	NAME(thing) = "<garbage>";
	SETDESC(thing, "<recyclable>");
	FLAGS(thing) = TYPE_GARBAGE;

	DBFETCH(thing)->next = recyclable;
	recyclable = thing;
	DBDIRTY(thing);
}