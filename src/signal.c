/*
 * signal.c -- Curently included into interface.c
 *
 * Seperates the signal handlers and such into a seperate file
 * for maintainability.
 *
 * Broken off from interface.c, and restructured for POSIX
 * compatible systems by Peter A. Torkelson, aka WhiteFire.
 */

#ifdef SOLARIS
#  ifndef _POSIX_SOURCE
#    define _POSIX_SOURCE		/* Solaris needs this */
#  endif
#endif

#include "config.h"
#include "interface.h"
#include "externs.h"
#include "version.h"

#include <signal.h>
#include <sys/wait.h>

/*
 * SunOS can't include signal.h and sys/signal.h, stupid broken OS.
 */
#if defined(HAVE_SYS_SIGNAL_H) && !defined(SUN_OS)
# include <sys/signal.h>
#endif

#if defined(ULTRIX) || defined(_POSIX_VERSION)
#undef RETSIGTYPE
#define RETSIGTYPE void
#endif

/*
 * Function prototypes
 */
void set_signals(void);
RETSIGTYPE bailout(int);
RETSIGTYPE sig_dump_status(int i);
RETSIGTYPE sig_shutdown(int i);
RETSIGTYPE sig_reap(int i);

#ifdef _POSIX_VERSION
void our_signal(int signo, void (*sighandler) (int));
#else
# define our_signal(s,f) signal((s),(f))
#endif

/*
 * our_signal(signo, sighandler)
 *
 * signo      - Signal #, see defines in signal.h
 * sighandler - The handler function desired.
 *
 * Calls sigaction() to set a signal, if we are posix.
 */
#ifdef _POSIX_VERSION
void
our_signal(int signo, void (*sighandler) (int))
{
	struct sigaction act, oact;

	act.sa_handler = sighandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	/* Restart long system calls if a signal is caught. */
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;
#endif

	/* Make it so */
	sigaction(signo, &act, &oact);
}

#endif							/* _POSIX_VERSION */

void
set_dumper_signals(void)
{
	our_signal(SIGPIPE, SIG_IGN); /* Ignore Blocked Pipe */
	our_signal(SIGHUP,  SIG_IGN); /* Ignore Terminal Hangup */
	our_signal(SIGCHLD, SIG_IGN); /* Ignore Child termination */
	our_signal(SIGFPE,  SIG_IGN); /* Ignore FP exceptions */
	our_signal(SIGUSR1, SIG_IGN); /* Ignore SIGUSR1 */
	our_signal(SIGUSR2, SIG_IGN); /* Ignore SIGUSR2 */
	our_signal(SIGINT,  SIG_DFL); /* Take Interrupt signal and die! */
	our_signal(SIGTERM, SIG_DFL); /* Take Terminate signal and die! */
	our_signal(SIGSEGV, SIG_DFL); /* Take Segfault and die! */
#ifdef SIGTRAP
	our_signal(SIGTRAP, SIG_DFL);
#endif
#ifdef SIGIOT
	our_signal(SIGIOT, SIG_DFL);
#endif
#ifdef SIGEMT
	our_signal(SIGEMT, SIG_DFL);
#endif
#ifdef SIGBUS
	our_signal(SIGBUS, SIG_DFL);
#endif
#ifdef SIGSYS
	our_signal(SIGSYS, SIG_DFL);
#endif
#ifdef SIGXCPU
	our_signal(SIGXCPU, SIG_IGN);  /* CPU usage limit exceeded */
#endif
#ifdef SIGXFSZ
	our_signal(SIGXFSZ, SIG_IGN);  /* Exceeded file size limit */
#endif
#ifdef SIGVTALRM
	our_signal(SIGVTALRM, SIG_DFL);
#endif
}


/*
 * set_signals()
 * set_sigs_intern(bail)
 *
 * Traps a bunch of signals and reroutes them to various
 * handlers. Mostly bailout.
 *
 * If called from bailout, then reset all to default.
 *
 * Called from main() and bailout()
 */
#define SET_BAIL (bail ? SIG_DFL : bailout)
#define SET_IGN  (bail ? SIG_DFL : SIG_IGN)

static void
set_sigs_intern(int bail)
{
	/* we don't care about SIGPIPE, we notice it in select() and write() */
	our_signal(SIGPIPE, SET_IGN);

	/* didn't manage to lose that control tty, did we? Ignore it anyway. */
	our_signal(SIGHUP, SET_IGN);

	/* resolver's exited. Better clean up the mess our child leaves */
	our_signal(SIGCHLD, bail ? SIG_DFL : sig_reap);

	/* standard termination signals */
	our_signal(SIGINT, SET_BAIL);
	our_signal(SIGTERM, SET_BAIL);

	/* catch these because we might as well */
/*  our_signal(SIGQUIT, SET_BAIL);  */
#ifdef SIGTRAP
	our_signal(SIGTRAP, SET_IGN);
#endif
#ifdef SIGIOT
	our_signal(SIGIOT, SET_BAIL);
#endif
#ifdef SIGEMT
	our_signal(SIGEMT, SET_BAIL);
#endif
#ifdef SIGBUS
	our_signal(SIGBUS, SET_BAIL);
#endif
#ifdef SIGSYS
	our_signal(SIGSYS, SET_BAIL);
#endif
	our_signal(SIGFPE, SET_BAIL);
	our_signal(SIGSEGV, SET_BAIL);
	our_signal(SIGTERM, bail ? SET_BAIL : sig_shutdown);
#ifdef SIGXCPU
	our_signal(SIGXCPU, SET_BAIL);
#endif
#ifdef SIGXFSZ
	our_signal(SIGXFSZ, SET_BAIL);
#endif
#ifdef SIGVTALRM
	our_signal(SIGVTALRM, SET_BAIL);
#endif
	our_signal(SIGUSR2, SET_BAIL);

	/* status dumper (predates "WHO" command) */
	our_signal(SIGUSR1, bail ? SIG_DFL : sig_dump_status);
}

void
set_signals(void)
{
	set_sigs_intern(FALSE);
	our_signal(SIGTERM, sig_shutdown);
}

/*
 * Signal handlers
 */

/*
 * BAIL!
 */
RETSIGTYPE bailout(int sig)
{
	char message[1024];

	/* turn off signals */
	set_sigs_intern(TRUE);

	snprintf(message, sizeof(message), "BAILOUT: caught signal %d", sig);

	panic(message);
	exit(7);

#if !defined(SYSV) && !defined(_POSIX_VERSION) && !defined(ULTRIX)
	return 0;
#endif
}

/*
 * Spew WHO to file
 */
RETSIGTYPE sig_dump_status(int i)
{
	dump_status();
#if !defined(SYSV) && !defined(_POSIX_VERSION) && !defined(ULTRIX)
	return 0;
#endif
}

/*
 * Gracefully shut the server down.
 */
RETSIGTYPE sig_shutdown(int i)
{
	warn("SHUTDOWN: via SIGNAL");
	shutdown_flag = 1;
#if !defined(SYSV) && !defined(_POSIX_VERSION) && !defined(ULTRIX)
	return 0;
#endif
}

/*
 * Clean out Zombie Resolver Process.
 */
#if !defined(SYSV) && !defined(_POSIX_VERSION) && !defined(ULTRIX)
#define RETSIGVAL 0
#else
#define RETSIGVAL 
#endif

RETSIGTYPE sig_reap(int i)
{
	/* If DISKBASE is not defined, then there is one type of
	 * child that can die.
	 * The database dumper. */

	/* The fix for SSL connections getting closed
	 * required closing all sockets 
	 * when the server fork()ed.  This made it impossible for that
	 * process to spit out the "save done" message.  However, because
	 * that process dies as soon as it finishes dumping the database,
	 * can detect that the child died, and broadcast the "save done"
	 * message anyway. */

	int status = 0;
	int reapedpid = 0;

	reapedpid = waitpid(-1, &status, WNOHANG);
	if(!reapedpid)
	{
		warn("SIG_CHILD signal handler called with no pid!");
	} else {
		if (reapedpid == global_dumper_pid) {
			int warnflag = 0;

			warn("forked DB dump task exited with status %d", status);

			if (WIFSIGNALED(status)) {
				warnflag = 1;
			} else if (WIFEXITED(status)) {
				/* In case NOCOREDUMP is defined, check for panic()s exit codes. */
				int statres = WEXITSTATUS(status);
				if (statres == 135 || statres == 136) {
					warnflag = 1;
				}
			}
			if (warnflag) {
				wall_wizards("# WARNING: The forked DB save process crashed while saving the database.");
				wall_wizards("# This is probably due to memory corruption, which can crash this server.");
				wall_wizards("# Unless you have a REALLY good unix programmer around who can try to fix");
				wall_wizards("# this process live with a debugger, you should try to restart this Muck");
				wall_wizards("# as soon as possible, and accept the data lost since the previous DB save.");
			}
			global_dumpdone = 1;
			global_dumper_pid = 0;
		} else {
			fprintf(stderr, "unknown child process (pid %d) exited with status %d\n", reapedpid, status);
		}
	}
	return RETSIGVAL;
}

static const char *signal_c_version = "$RCSfile$ $Revision: 1.21 $";
const char *get_signal_c_version(void) { return signal_c_version; }
