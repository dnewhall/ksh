/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1992-2012 AT&T Intellectual Property          *
*          Copyright (c) 2020-2025 Contributors to ksh 93u+m           *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 2.0                  *
*                                                                      *
*                A copy of the License is available at                 *
*      https://www.eclipse.org/org/documents/epl-2.0/EPL-2.0.html      *
*         (with md5 checksum 84283fa8859daf213bdda5a9f8d1be1d)         *
*                                                                      *
*           Derek Newhall <dnewhall@users.noreply.github.com>          *
*                                                                      *
***********************************************************************/
/*
 * readlink [-fnz] file
 *
 * print the target of a symlink
 */

static const char usage[] =
"[-?\n@(#)$Id: readlink (ksh 93u+m) 2025-11-03 $\n]"
"[--catalog?" ERROR_CATALOG "]"
"[+NAME?readlink - print the target of a symbolic link]"
"[+DESCRIPTION?\breadlink\b prints the target of a symbolic link to "
"standard output. If \afile\a is not a symbolic link, a diagnositc message "
"is printed to standard error and a non-zero exit status is returned.]"
"[+?Prints the value of a symbolic link (its target path) to the standard output. If \afile\a is not a symbolic link, an error message is printed to standard error and a non-zero exit status is returned.]"
"[f:canonicalize?Canonicalize \afile\a by recursively following every component of the given path (removes reduncant . and /\a./\a, \a/..\a, etc.).]"
"[n:no-newline?Do not output a trailing newline character.]"
"[z:zero?Each line of output is terminated with a NUL character instead "
    "of a newline.]"
"\n"
"\n file ...\n"
"\n"
"[+EXIT STATUS?]"
    "{"
	"[+0?Successful completion.]"
	"[+>0?An error occurred.]"
    "}"
"[+SEE ALSO?\breadlink\b(3), \breadlpath\b(1), \brealpath\b(3)]"
;


#include <cmd.h>
#include <ast.h>
#include <sys/stat.h>
#include <stdlib.h>

static void linkname(Sfio_t *outfile, char *pathname, int canonicalize, int termch)
{
	struct stat statbuf;
	char *namebuf;
	if (canonicalize)
	{
		if ((namebuf = realpath(pathname, NULL)) == NULL)
		//if (pathcanon(pathname, PATH_DOTDOT | PATH_EXISTS | PATH_PHYSICAL) == NULL)
		{
			error(ERROR_SYSTEM|ERROR_PANIC, strerror(errno));
			UNREACHABLE();
		}
	}
	else
	{
		if (lstat(pathname, &statbuf) == -1)
		{
			error(ERROR_system(1), "%s: cannot stat", pathname);
			UNREACHABLE();
		}
		// A symlink's file size is the size of its target's pathname
		if ((namebuf = malloc(statbuf.st_size + 1)) == NULL)
		{
			error(ERROR_SYSTEM|ERROR_PANIC, "out of memory");
			UNREACHABLE();
		}
		// Use AST's pathgetlink instead of readlink directly to handle links between universes
		int len = pathgetlink(pathname, namebuf, statbuf.st_size + 1);
		if (len == -1) {
			if (errno == EINVAL)
			{
				error(ERROR_system(1), "%s is not a symbolic link", pathname);
				UNREACHABLE();
			} else {
				error(ERROR_system(1), "%s: readlink() failed", pathname);
				UNREACHABLE();
			}
		}
		namebuf[len] = '\0';
	}
        sfputr(outfile, namebuf, termch);
	free(namebuf);
}

int
b_readlink(int argc, char** argv, Shbltin_t* context)
{
	char*	string;
	int	canonicalize = 0;
	char    termch = '\n';

	cmdinit(argc, argv, context, ERROR_CATALOG, 0);
	for (;;)
	{
		switch (optget(argv, usage))
		{
		case 'f':
			canonicalize = 1;
			continue;
		case 'n':
			termch = -1;
			continue;
		case 'z':
			termch = '\0';
			continue;
		case ':':
			error(2, "%s", opt_info.arg);
			break;
		case '?':
			/* self-doc: write to standard output */
			error(ERROR_USAGE|ERROR_OUTPUT, STDOUT_FILENO, "%s", opt_info.arg);
			return 0;
		}
		break;
	}
	argv += opt_info.index;
	argc -= opt_info.index;
	if (error_info.errors || argc < 1)
	{
		error(ERROR_usage(2), "%s", optusage(NULL));
		UNREACHABLE();
	}

	while (string = *argv++)
		linkname(sfstdout, string, canonicalize, termch);
	return 0;
}
