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
*                                                                      *
***********************************************************************/
/*
 * Glenn Fowler
 * AT&T Bell Laboratories
 *
 * time conversion support
 */

#include <ast.h>
#include <tm.h>
#include <ctype.h>

/*
 * return the tab table index that matches s ignoring case and .'s
 * tm_data.format checked if tminfo.format!=tm_data.format
 *
 * ntab and nsuf are the number of elements in tab and suf,
 * -1 for 0 sentinel
 *
 * all isalpha() chars in str must match
 * suf is a table of nsuf valid str suffixes
 * if e is non-null then it will point to first unmatched char in str
 * which will always be non-isalpha()
 */

int
tmlex(const char* s, char** e, char** tab, int ntab, char** suf, int nsuf)
{
	char**	p;
	char*	x;
	int	n;

	for (p = tab, n = ntab; n-- && (x = *p); p++)
		if (*x && *x != '%' && tmword(s, e, x, suf, nsuf))
			return p - tab;
	if (tm_info.format != tm_data.format && tab >= tm_info.format && tab < tm_info.format + TM_NFORM)
	{
		tab = tm_data.format + (tab - tm_info.format);
		if (suf && tab >= tm_info.format && tab < tm_info.format + TM_NFORM)
			suf = tm_data.format + (suf - tm_info.format);
		for (p = tab, n = ntab; n-- && (x = *p); p++)
			if (*x && *x != '%' && tmword(s, e, x, suf, nsuf))
				return p - tab;
	}
	return -1;
}
