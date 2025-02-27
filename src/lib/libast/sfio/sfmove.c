/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2011 AT&T Intellectual Property          *
*          Copyright (c) 2020-2024 Contributors to ksh 93u+m           *
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
*            Johnothan King <johnothanking@protonmail.com>             *
*                                                                      *
***********************************************************************/
#include	"sfhdr.h"

/*	Move data from one stream to another.
**	This code is written so that it'll work even in the presence
**	of stacking streams, pool, and discipline.
**	If you must change it, be gentle.
**
**	Written by Kiem-Phong Vo.
*/
#define MAX_SSIZE	((ssize_t)((~((size_t)0)) >> 1))

Sfoff_t sfmove(Sfio_t*	fr,	/* moving data from this stream */
	       Sfio_t*	fw,	/* moving data to this stream */
	       Sfoff_t	n,	/* number of bytes/records to move. <0 for unbounded move */
	       int	rc)	/* record separator */
{
	uchar		*cp, *next;
	ssize_t		r, w;
	uchar		*endb;
	int		direct;
	Sfoff_t		n_move, sk, cur;
	uchar		*rbuf = NULL;
	ssize_t		rsize = 0;

	if(!(fr)) return 0;

	for(n_move = 0; n != 0; )
	{
		if(rc >= 0) /* moving records, let sfgetr() deal with record reading */
		{	if(!(cp = (uchar*)sfgetr(fr,rc,0)) )
				n = 0;
			else
			{	r = sfvalue(fr);
				if(fw && (w = SFWRITE(fw, cp, r)) != r)
				{	if(fr->extent >= 0 )
						(void)SFSEEK(fr,(Sfoff_t)(-r),SEEK_CUR);
					if(fw->extent >= 0 && w > 0)
						(void)SFSEEK(fw,(Sfoff_t)(-w),SEEK_CUR);
					n = 0;
				}
				else
				{	n_move += 1;
					if(n > 0)
						n -= 1;
				}
			}
			continue;
		}

		/* get the streams into the right mode */
		if(fr->mode != SFIO_READ && _sfmode(fr,SFIO_READ,0) < 0)
			break;

		SFLOCK(fr,0);

		/* flush the write buffer as necessary to make room */
		if(fw)
		{	if(fw->mode != SFIO_WRITE && _sfmode(fw,SFIO_WRITE,0) < 0 )
				break;
			SFLOCK(fw,0);
			if(fw->next >= fw->endb ||
			   (fw->next > fw->data && fr->extent < 0 &&
			    (fw->extent < 0 || (fw->flags&SFIO_SHARE)) ) )
				if(SFFLSBUF(fw,-1) < 0 )
					break;
		}
		else if((cur = SFSEEK(fr, 0, SEEK_CUR)) >= 0 )
		{	sk = n > 0 ? SFSEEK(fr, n, SEEK_CUR) : SFSEEK(fr, 0, SEEK_END);
			if(sk > cur) /* safe to skip over data in current stream */
			{	n_move += sk - cur;
				if(n > 0)
					n -= sk - cur;
				continue;
			}
			/* else: stream unstacking may happen below */
		}

		/* about to move all, set map to a large amount */
		if(n < 0 && (fr->bits&SFIO_MMAP) && !(fr->bits&SFIO_MVSIZE) )
		{	SFMVSET(fr);
			fr->bits |= SFIO_SEQUENTIAL; /* sequentially access data */
		}

		/* try reading a block of data */
		direct = 0;
		if(fr->rsrv && (r = -fr->rsrv->slen) > 0)
		{	fr->rsrv->slen = 0;
			next = fr->rsrv->data;
		}
		else if((r = fr->endb - (next = fr->next)) <= 0)
		{	/* amount of data remained to be read */
			if((w = n > MAX_SSIZE ? MAX_SSIZE : (ssize_t)n) < 0)
			{	if(fr->extent < 0)
					w = fr->data == fr->tiny ? SFIO_GRAIN : fr->size;
				else if((fr->extent-fr->here) > SFIO_NMAP*SFIO_PAGE)
					w = SFIO_NMAP*SFIO_PAGE;
				else	w = (ssize_t)(fr->extent-fr->here);
			}

			/* use a decent buffer for data transfer but make sure
			   that if we overread, the left over can be retrieved
			*/
			if(!(fr->flags&SFIO_STRING) && !(fr->bits&SFIO_MMAP) &&
			   (n < 0 || fr->extent >= 0) )
			{	ssize_t maxw = 4*(_Sfpage > 0 ? _Sfpage : SFIO_PAGE);

				/* direct transfer to a seekable write stream */
				if(fw && fw->extent >= 0 && w <= (fw->endb-fw->next) )
				{	w = fw->endb - (next = fw->next);
					direct = SFIO_WRITE;
				}
				else if(w > fr->size && maxw > fr->size)
				{	/* making our own buffer */
					if(w >= maxw)
						w = maxw;
					else	w = ((w+fr->size-1)/fr->size)*fr->size;
					if(rsize <= 0 && (rbuf = (uchar*)malloc(w)) )
						rsize = w;
					if(rbuf)
					{	next = rbuf;
						w = rsize;
						direct = SFIO_STRING;
					}
				}
			}

			if(!direct)
			{	/* make sure we don't read too far ahead */
				if(n > 0 && fr->extent < 0 && (fr->flags&SFIO_SHARE) )
				{	if((Sfoff_t)(r = fr->size) > n)
						r = (ssize_t)n;
				}
				else	r = -1;
				if((r = SFFILBUF(fr,r)) <= 0)
					break;
				next = fr->next;
			}
			else
			{	/* actual amount to be read */
				if(n > 0 && n < w)
					w = (ssize_t)n;

				if((r = SFRD(fr,next,w,fr->disc)) > 0)
					fr->next = fr->endb = fr->endr = fr->data;
				else if(r == 0)
					break;		/* eof */
				else	goto again;	/* popped stack */
			}
		}

		/* compute the extent of data to be moved */
		endb = next+r;
		if(n > 0)
		{	if(r > n)
				r = (ssize_t)n;
			n -= r;
		}
		n_move += r;
		cp = next+r;

		if(!direct)
			fr->next += r;
		else if((w = endb-cp) > 0)
		{	/* move leftover to read stream */
			if(w > fr->size)
				w = fr->size;
			memmove(fr->data,cp,w);
			fr->endb = fr->data+w;
			if((w = endb - (cp+w)) > 0)
				(void)SFSK(fr,(Sfoff_t)(-w),SEEK_CUR,fr->disc);
		}

		if(fw)
		{	if(direct == SFIO_WRITE)
				fw->next += r;
			else if(r <= (fw->endb-fw->next) )
			{	memmove(fw->next,next,r);
				fw->next += r;
			}
			else if((w = SFWRITE(fw,next,r)) != r)
			{	/* a write error happened */
				if(w > 0)
				{	r -= w;
					n_move -= r;
				}
				if(fr->extent >= 0)
					(void)SFSEEK(fr,(Sfoff_t)(-r),SEEK_CUR);
				break;
			}
		}

	again:
		SFOPEN(fr,0);
		if(fw)
			SFOPEN(fw,0);
	}

	if(n < 0 && (fr->bits&SFIO_MMAP) && (fr->bits&SFIO_MVSIZE))
	{	/* back to normal access mode */
		SFMVUNSET(fr);
		if((fr->bits&SFIO_SEQUENTIAL) && (fr->data))
			SFMMSEQOFF(fr,fr->data,fr->endb-fr->data);
		fr->bits &= ~SFIO_SEQUENTIAL;
	}

	if(rbuf)
		free(rbuf);

	if(fw)
	{	SFOPEN(fw,0);
	}

	SFOPEN(fr,0);
	return n_move;
}
