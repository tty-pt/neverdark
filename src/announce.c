/*
 *      announce - sits listening on a port, and whenever anyone connects
 *                 announces a message and disconnects them
 *
 *      Usage:  announce [port] < message_file
 *
 *      Author: Lawrence Brown <lpb@cs.adfa.oz.au>      Aug 90
 *
 *      Bits of code are adapted from the Berkeley telnetd sources
 */

#define PORT    4201

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>

#ifdef USE_IPV6
#include <netinet6/in6.h>
#endif

extern char **environ;
extern int errno;
char *Name;						/* name of this program for error messages */
char msg[32768];

int
main(argc, argv)
char *argv[];
{
	int s, ns, foo;

#ifdef USE_IPV6
	static struct sockaddr_in6 sin = { AF_INET6 };
	char host[128], *inet_ntoa();
#else
	static struct sockaddr_in sin = { AF_INET };
	char *host, *inet_ntoa();
#endif
	char tmp[32768];
	long ct;

	Name = argv[0];				/* save name of program for error messages  */

#ifdef USE_IPV6
	sin.sin6_port = htons((u_short) PORT);	/* Assume PORT */
	argc--, argv++;
	if (argc > 0) {				/* unless specified on command-line       */
		sin.sin6_port = atoi(*argv);
		sin.sin6_port = htons((u_short) sin.sin6_port);
	}
#else
	sin.sin_port = htons((u_short) PORT);	/* Assume PORT */
	argc--, argv++;
	if (argc > 0) {				/* unless specified on command-line       */
		sin.sin_port = atoi(*argv);
		sin.sin_port = htons((u_short) sin.sin_port);
	}
#endif

	strcpy(msg, "");
	strcpy(tmp, "");
	while (1) {
		if ((gets(tmp)) == NULL)
			break;
		strcat(tmp, "\r\n");
		strcat(msg, tmp);
	}
	msg[4095] = '\0';
	signal(SIGHUP, SIG_IGN);	/* get socket, bind port to it      */
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("announce: socket");
		exit(1);
	}
	if (bind(s, (struct sockaddr *) &sin, sizeof sin) < 0) {
		perror("bind");
		exit(1);
	}
	if ((foo = fork()) != 0) {
#ifdef USE_IPV6
		fprintf(stderr, "announce: pid %d running on port %d\n", foo,
				ntohs((u_short) sin.sin6_port));
#else
		fprintf(stderr, "announce: pid %d running on port %d\n", foo,
				ntohs((u_short) sin.sin_port));
#endif
		_exit(0);
	} else {
		setpriority(PRIO_PROCESS, getpid(), 10);
	}
	if (listen(s, 1) < 0) {		/* start listening on port */
		perror("announce: listen");
		_exit(1);
	}
	foo = sizeof sin;
	for (;;) {					/* loop forever, accepting requests & printing
								   * msg */
		ns = accept(s, (struct sockaddr *) &sin, &foo);
		if (ns < 0) {
			perror("announce: accept");
			_exit(1);
		}
#ifdef USE_IPV6
		inet_ntop(AF_INET6, sin.sin6_addr, host, 128);
#else
		host = inet_ntoa(sin.sin_addr);
#endif

		ct = time(0L);
		fprintf(stderr, "CONNECTION made from %s at %s", host, ctime(&ct));
		write(ns, msg, strlen(msg));
		sleep(5);
		close(ns);
	}
}								/* main */