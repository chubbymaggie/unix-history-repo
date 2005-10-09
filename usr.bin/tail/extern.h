/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)extern.h	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 */

#define	WR(p, size) do { \
	if (write(STDOUT_FILENO, p, size) != (ssize_t)size) \
		oerr(); \
	} while(0)

#define TAILMAPLEN (4<<20)

struct mapinfo {
	off_t	mapoff;
	off_t	maxoff;
	size_t	maplen;
	char	*start;
	int	fd;
};

struct file_info {
	FILE *fp;
	char *file_name;
	struct stat st;
};

typedef struct file_info file_info_t;

enum STYLE { NOTSET = 0, FBYTES, FLINES, RBYTES, RLINES, REVERSE };

void follow(file_info_t *, enum STYLE, off_t);
void forward(FILE *, enum STYLE, off_t, struct stat *);
void reverse(FILE *, enum STYLE, off_t, struct stat *);

int bytes(FILE *, off_t);
int lines(FILE *, off_t);

void ierr(void);
void oerr(void);
int mapprint(struct mapinfo *, off_t, off_t);
int maparound(struct mapinfo *, off_t);

extern int Fflag, fflag, rflag, rval, no_files;
extern const char *fname;
