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
/*
 * POSIX 1003.2 wordexp implementation
 */

#include	<ast.h>
#include	<wordexp.h>
#include	<stk.h>

struct list
{
	struct list *next;
};

/*
 * eliminates shell quoting as inserted with sh_fmtq
 * result replaces <string>
 * length of resulting string is returned.
 */
static int	sh_unquote(char* string)
{
	char *sp=string, *dp;
	int c;
	while((c= *sp) && c!='\'')
		sp++;
	if(c==0)
		return sp-string;
	if((dp=sp) > string && sp[-1]=='$')
	{
		int n=stresc(sp+1);
		/* copy all but trailing ' */
		while(--n>0)
			*dp++ = *++sp;
	}
	else
	{
		while((c= *++sp) && c!='\'')
			*dp++ = c;
	}
	*dp=0;
	return dp-string;
}

int	wordexp(const char *string, wordexp_t *wdarg, int flags)
{
	Sfio_t *iop;
	char *cp=(char*)string;
	int c,quoted=0,literal=0,ac=0;
	int offset;
	char *savebase,**av;
	if(offset=stktell(stkstd))
		savebase = stkfreeze(stkstd,0);
	if(flags&WRDE_REUSE)
		wordfree(wdarg);
	else if(!(flags&WRDE_APPEND))
	{
		wdarg->we_wordv = 0;
		wdarg->we_wordc = 0;
	}
	if(flags&WRDE_UNDEF)
		sfwrite(stkstd,"set -u\n",7);
	if(!(flags&WRDE_SHOWERR))
		sfwrite(stkstd,"exec 2> /dev/null\n",18);
	sfwrite(stkstd,"print -f \"%q\\n\" ",16);
	if(*cp=='#')
		sfputc(stkstd,'\\');
	while(c = *cp++)
	{
		if(c=='\'' && !quoted)
			literal = !literal;
		else if(!literal)
		{
			if(c=='\\' && (!quoted || strchr("\\\"`\n$",c)))
			{
				sfputc(stkstd,'\\');
				if(c= *cp)
					cp++;
				else
					c = '\\';
			}
			else if(c=='"')
				quoted = !quoted;
			else if(c=='`' || (c=='$' && *cp=='('))
			{
				if(flags&WRDE_NOCMD)
				{
					c=WRDE_CMDSUB;
					goto err;
				}
				/* only the shell can parse the rest */
				sfputr(stkstd,cp-1,-1);
				break;
			}
			else if(!quoted && strchr("|&\n;<>"+ac,c))
			{
				c=WRDE_BADCHAR;
				goto err;
			}
			else if(c=='(') /* allow | and & inside pattern */
				ac=2;
		}
		sfputc(stkstd,c);
	}
	sfputc(stkstd,0);
	if(!(iop = sfpopen(NULL,stkptr(stkstd,0),"r")))
	{
		c = WRDE_NOSHELL;
		goto err;
	}
	stkseek(stkstd,0);
	ac = 0;
	while((c=sfgetc(iop)) != EOF)
	{
		if(c=='\'')
			quoted = ! quoted;
		else if(!quoted && (c==' ' || c=='\n'))
		{
			ac++;
			c = 0;
		}
		sfputc(stkstd,c);
	}
	if(c=sfclose(iop))
	{
		if(c==3 || !(flags&WRDE_UNDEF))
			c=WRDE_SYNTAX;
		else
			c=WRDE_BADVAL;
		goto err;
	}
	c = ac+2;
	if(flags&WRDE_DOOFFS)
		c += wdarg->we_offs;
	if(flags&WRDE_APPEND)
		av = (char**)realloc(&wdarg->we_wordv[-1], (wdarg->we_wordc+c)*sizeof(char*));
	else if(av = (char**)malloc(c*sizeof(char*)))
	{
		if(flags&WRDE_DOOFFS)
			memset(av,0,(wdarg->we_offs+1)*sizeof(char*));
		else
			av[0] = 0;
	}
	if(!av)
		return WRDE_NOSPACE;
	c = stktell(stkstd);
	if(!(cp = (char*)malloc(sizeof(char*)+c)))
	{
		c=WRDE_NOSPACE;
		goto err;
	}
	((struct list*)cp)->next = (struct list*)(*av);
	*av++ = (char*)cp;
	cp += sizeof(char*);
	wdarg->we_wordv = av;
	if(flags&WRDE_APPEND)
		av += wdarg->we_wordc;
	wdarg->we_wordc += ac;
	if(flags&WRDE_DOOFFS)
		av += wdarg->we_offs;
	memcpy(cp,stkptr(stkstd,offset),c);
	while(ac-- > 0)
	{
		*av++ = cp;
		sh_unquote(cp);
		while(c= *cp++);
	}
	*av = 0;
	c=0;
err:
	if(offset)
		stkset(stkstd,savebase,offset);
	else
		stkseek(stkstd,0);
	return c;
}

/*
 * free fields in <wdarg>
 */
int wordfree(wordexp_t *wdarg)
{
	struct list *arg, *argnext;
	if(wdarg->we_wordv)
	{
		argnext = (struct list*)wdarg->we_wordv[-1];
		while(arg=argnext)
		{
			argnext = arg->next;
			free(arg);
		}
		free(&wdarg->we_wordv[-1]);
		wdarg->we_wordv = 0;
	}
	wdarg->we_wordc=0;
	return 0;
}
