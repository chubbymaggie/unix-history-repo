/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)slave.c	1.3 (Berkeley) %G%";
#endif not lint

#include "globals.h"
#include <protocols/timed.h>
#include <setjmp.h>

extern long delay1;
extern long delay2;
extern int trace;
extern int sock;
extern struct sockaddr_in from;
extern struct sockaddr_in server;
extern char hostname[];
extern char *fj;
extern FILE *fd;

slave()
{
	int length;
	int senddateack;
	long electiontime, refusetime;
	u_short seq;
	char candidate[MAXHOSTNAMELEN];
	struct tsp *msg, *readmsg();
	extern int backoff;
	struct sockaddr_in saveaddr, msaveaddr;
	extern jmp_buf jmpenv;
	struct timeval wait;
	struct timeval time;
	struct tsp *answer, *acksend();
	int timeout();
	char *date(), *strcpy();
	long casual();
	int bytenetorder();
	char *olddate;

	syslog(LOG_NOTICE, "THIS MACHINE IS A SLAVE");
	if (trace) {
		fprintf(fd, "THIS MACHINE IS A SLAVE\n");
	}

	seq = 0;
	senddateack = OFF;
	refusetime = 0;

	(void)gettimeofday(&time, (struct timezone *)0);
	electiontime = time.tv_sec + delay2;

loop:
	length = sizeof(struct sockaddr_in);
	(void)gettimeofday(&time, (struct timezone *)0);
	if (time.tv_sec > electiontime) {
		if (trace) 
			fprintf(fd, "election timer expired\n");
		longjmp(jmpenv, 1);
	}
	wait.tv_sec = electiontime - time.tv_sec + 10;
	wait.tv_usec = 0;
	msg = readmsg(TSP_ANY, (char *)ANYADDR, &wait);
	if (msg != NULL) {
		switch (msg->tsp_type) {

		case TSP_ADJTIME:
			(void)gettimeofday(&time, (struct timezone *)0);
			electiontime = time.tv_sec + delay2;
			if (seq != msg->tsp_seq) {
				seq = msg->tsp_seq;
				adjclock(&(msg->tsp_time));
			}
			break;
		case TSP_SETTIME:
			if (seq == msg->tsp_seq)
				break;

			seq = msg->tsp_seq;

			olddate = date();
			(void)settimeofday(&msg->tsp_time,
				(struct timezone *)0);
			syslog(LOG_NOTICE, "date changed by %s from: %s",
				msg->tsp_name, olddate);
			electiontime = time.tv_sec + delay2;

			if (senddateack == ON) {
				senddateack = OFF;
				msg->tsp_type = TSP_DATEACK;
				(void)strcpy(msg->tsp_name, hostname);
				bytenetorder(msg);
				length = sizeof(struct sockaddr_in);
				if (sendto(sock, (char *)msg, 
						sizeof(struct tsp), 0,
						&saveaddr, length) < 0) {
					syslog(LOG_ERR, "sendto: %m");
					exit(1);
				}
			}
			break;
		case TSP_MASTERUP:
			msg->tsp_type = TSP_SLAVEUP;
			msg->tsp_vers = TSPVERSION;
			(void)strcpy(msg->tsp_name, hostname);
			bytenetorder(msg);
			answerdelay();
			length = sizeof(struct sockaddr_in);
			if (sendto(sock, (char *)msg, sizeof(struct tsp), 0, 
						&from, length) < 0) {
				syslog(LOG_ERR, "sendto: %m");
				exit(1);
			}
			backoff = 1;
			delay2 = casual((long)MINTOUT, (long)MAXTOUT);
			(void)gettimeofday(&time, (struct timezone *)0);
			electiontime = time.tv_sec + delay2;
			refusetime = 0;
			break;
		case TSP_MASTERREQ:
			(void)gettimeofday(&time, (struct timezone *)0);
			electiontime = time.tv_sec + delay2;
			break;
		case TSP_DATE:
			saveaddr = from;
			msg->tsp_time.tv_usec = 0;
			msg->tsp_type = TSP_DATEREQ;
			msg->tsp_vers = TSPVERSION;
			(void)strcpy(msg->tsp_name, hostname);
			answer = acksend(msg, (char *)ANYADDR, TSP_DATEACK);
			if (answer != NULL) {
				msg->tsp_type = TSP_ACK;
				bytenetorder(msg);
				length = sizeof(struct sockaddr_in);
				if (sendto(sock, (char *)msg, 
						sizeof(struct tsp), 0,
						&saveaddr, length) < 0) {
					syslog(LOG_ERR, "sendto: %m");
					exit(1);
				}
				senddateack = ON;
			}
			break;
		case TSP_TRACEON:
			if (!(trace)) {
				fd = fopen(fj, "w");
				setlinebuf(fd);
				fprintf(fd, "Tracing started on: %s\n\n", 
								date());
			}
			trace = ON;
			break;
		case TSP_TRACEOFF:
			if (trace) {
				fprintf(fd, "Tracing ended on: %s\n", date());
				(void)fclose(fd);
			}
			trace = OFF;
			break;
		case TSP_ELECTION:
			(void)gettimeofday(&time, (struct timezone *)0);
			electiontime = time.tv_sec + delay2;
			seq = 0;            /* reset sequence number */
			if (time.tv_sec < refusetime)
				msg->tsp_type = TSP_REFUSE;
			else {
				msg->tsp_type = TSP_ACCEPT;
				refusetime = time.tv_sec + 30;
			}
			(void)strcpy(candidate, msg->tsp_name);
			(void)strcpy(msg->tsp_name, hostname);
			answerdelay();
			server = from;
			answer = acksend(msg, candidate, TSP_ACK);
			if (answer == NULL) {
				syslog(LOG_ERR, "problem in election\n");
			}
			break;
		case TSP_MSITE:
			msaveaddr = from;
			msg->tsp_type = TSP_MSITEREQ;
			msg->tsp_vers = TSPVERSION;
			(void)strcpy(msg->tsp_name, hostname);
			answer = acksend(msg, (char *)ANYADDR, TSP_ACK);
			if (answer != NULL) {
				msg->tsp_type = TSP_ACK;
				length = sizeof(struct sockaddr_in);
				bytenetorder(msg);
				if (sendto(sock, (char *)msg, 
						sizeof(struct tsp), 0,
						&msaveaddr, length) < 0) {
					syslog(LOG_ERR, "sendto: %m");
					exit(1);
				}
			}
			break;
		case TSP_ACCEPT:
		case TSP_REFUSE:
		case TSP_SLAVEUP:
		case TSP_DATEREQ:
			break;
#ifdef TESTING
		case TSP_TEST:
			electiontime = 0;
			break;
#endif
		default:
			if (trace) {
				fprintf(fd, "garbage: ");
				print(msg);
			}
			break;
		}
	}
	goto loop;
}

/*
 * Used before answering a broadcast message to avoid network
 * contention and likely collisions.
 */
answerdelay()
{
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = delay1;

	(void)select(0, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL,
	    &timeout);
	return;
}
