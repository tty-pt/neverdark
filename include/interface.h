
/* $Header$ */

#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "copyright.h"

#include "db.h"
#include "command.h"

/* these symbols must be defined by the interface */
extern int notify(dbref player, const char *msg);
extern int notify_nolisten(dbref player, const char *msg, int ispriv);
extern int notify_filtered(dbref from, dbref player, const char *msg, int ispriv);
extern void wall(const char *msg);
extern void wall_wizards(const char *msg);
extern int shutdown_flag;		/* if non-zero, interface should shut down */
extern int restart_flag;		/* if non-zero, should restart after shut down */
extern void emergency_shutdown(void);
extern int boot_off(dbref player);
extern void boot_player_off(dbref player);
extern int* get_player_descrs(dbref player, int*count);

/* the following symbols are provided by game.c */

extern dbref create_player(const char *name, const char *password);
extern dbref connect_player(const char *name, const char *password);

extern int init_game();
extern void dump_database(void);
extern void panic(const char *);

#endif /* _INTERFACE_H */

#ifdef DEFINE_HEADER_VERSIONS

#ifndef interfaceh_version
#define interfaceh_version
const char *interface_h_version = "$RCSfile$ $Revision: 1.12 $";
#endif
#else
extern const char *interface_h_version;
#endif

/* extern void art_print(dbref player, dbref what); */
void art(int descr, const char *art_str);
