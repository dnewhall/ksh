/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2011 AT&T Intellectual Property          *
*          Copyright (c) 2020-2025 Contributors to ksh 93u+m           *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 2.0                  *
*                                                                      *
*                A copy of the License is available at                 *
*      https://www.eclipse.org/org/documents/epl-2.0/EPL-2.0.html      *
*         (with md5 checksum 84283fa8859daf213bdda5a9f8d1be1d)         *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                  Martijn Dekker <martijn@inlv.org>                   *
*                                                                      *
***********************************************************************/
#include	"sfdchdr.h"


/*	Make a sequence of streams act like a single stream.
**	This is for reading only.
**
**	Written by Kiem-Phong Vo, kpv@research.att.com, 03/18/1998.
*/

#define UNSEEKABLE	1

typedef struct _file_s
{	Sfio_t*	f;	/* the stream		*/
	Sfoff_t	lower;	/* its lowest end	*/
} File_t;

typedef struct _union_s
{
	Sfdisc_t	disc;	/* discipline structure */
	short		type;	/* type of streams	*/
	short		c;	/* current stream	*/
	short		n;	/* number of streams	*/
	Sfoff_t		here;	/* current location	*/
	File_t		f[1];	/* array of streams	*/
} Union_t;

static ssize_t unwrite(Sfio_t*		f,	/* stream involved */
		       const void*	buf,	/* buffer to read into */
		       size_t		n,	/* number of bytes to read */
		       Sfdisc_t*	disc)	/* discipline */
{
	NOT_USED(f);
	NOT_USED(buf);
	NOT_USED(n);
	NOT_USED(disc);
	return -1;
}

static ssize_t unread(Sfio_t*	f,	/* stream involved */
		      void*	buf,	/* buffer to read into */
		      size_t	n,	/* number of bytes to read */
		      Sfdisc_t*	disc)	/* discipline */
{
	Union_t*	un;
	ssize_t	r, m;

	un = (Union_t*)disc;
	m = n;
	f = un->f[un->c].f;
	while(1)
	{	if((r = sfread(f,buf,m)) < 0 || (r == 0 && un->c == un->n-1) )
			break;

		m -= r;
		un->here += r;

		if(m == 0)
			break;

		buf = (char*)buf + r;
		if(sfeof(f) && un->c < un->n-1)
			f = un->f[un->c += 1].f;
	}
	return n-m;
}

static Sfoff_t unseek(Sfio_t* f, Sfoff_t addr, int type, Sfdisc_t* disc)
{
	Union_t*	un;
	int		i;
	Sfoff_t	extent, s;

	NOT_USED(f);

	un = (Union_t*)disc;
	if(un->type&UNSEEKABLE)
		return -1L;

	if(type == 2)
	{	extent = 0;
		for(i = 0; i < un->n; ++i)
			extent += (sfsize(un->f[i].f) - un->f[i].lower);
		addr += extent;
	}
	else if(type == 1)
		addr += un->here;

	if(addr < 0)
		return -1;

	/* find the stream where the addr could be in */
	extent = 0;
	for(i = 0; i < un->n-1; ++i)
	{	s = sfsize(un->f[i].f) - un->f[i].lower;
		if(addr < extent + s)
			break;
		extent += s;
	}

	s = (addr-extent) + un->f[i].lower;
	if(sfseek(un->f[i].f,s,0) != s)
		return -1;

	un->c = i;
	un->here = addr;

	for(i += 1; i < un->n; ++i)
		sfseek(un->f[i].f,un->f[i].lower,0);

	return addr;
}

/* on close, remove the discipline */
static int unexcept(Sfio_t* f, int type, void* data, Sfdisc_t* disc)
{
	NOT_USED(f);
	NOT_USED(data);

	if(type == SFIO_FINAL || type == SFIO_DPOP)
		free(disc);

	return 0;
}

int sfdcunion(Sfio_t* f, Sfio_t** array, int n)
{
	Union_t*	un;
	int		i;

	if(n <= 0)
		return -1;

	if(!(un = (Union_t*)malloc(sizeof(Union_t)+(n-1)*sizeof(File_t))) )
		return -1;
	memset(un, 0, sizeof(*un));

	un->disc.readf = unread;
	un->disc.writef = unwrite;
	un->disc.seekf = unseek;
	un->disc.exceptf = unexcept;
	un->n = n;

	for(i = 0; i < n; ++i)
	{	un->f[i].f = array[i];
		if(!(un->type&UNSEEKABLE))
		{	un->f[i].lower = sfseek(array[i],0,1);
			if(un->f[i].lower < 0)
				un->type |= UNSEEKABLE;
		}
	}

	if(sfdisc(f,(Sfdisc_t*)un) != (Sfdisc_t*)un)
	{	free(un);
		return -1;
	}

	return 0;
}
