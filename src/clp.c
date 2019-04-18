/* -*- related-file-name: "../include/lcdf/clp.h" -*- */
/* clp.c - Complete source code for CLP.
 * This file is part of CLP, the command line parser package.
 *
 * Copyright (c) 1997-2019 Eddie Kohler, ekohler@gmail.com
 *
 * CLP is free software. It is distributed under the GNU General Public
 * License, Version 2, or, alternatively and at your discretion, under the
 * more permissive (BSD-like) Click LICENSE file as described below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, subject to the
 * conditions listed in the Click LICENSE file, which is available in full at
 * http://github.com/kohler/click/blob/master/LICENSE. The conditions
 * include: you must preserve this copyright notice, and you cannot mention
 * the copyright holders in advertising related to the Software without
 * their permission. The Software is provided WITHOUT ANY WARRANTY, EXPRESS
 * OR IMPLIED. This notice is a summary of the Click LICENSE file; the
 * license in that file is binding. */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <lcdf/clp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_INTTYPES_H || !defined(HAVE_CONFIG_H)
# include <inttypes.h>
#endif

/* By default, assume we have inttypes.h, strtoul, and uintptr_t. */
#if !defined(HAVE_STRTOUL) && !defined(HAVE_CONFIG_H)
# define HAVE_STRTOUL 1
#endif
#if defined(HAVE_INTTYPES_H) || !defined(HAVE_CONFIG_H)
# include <inttypes.h>
#endif
#if !defined(HAVE_UINTPTR_T) && defined(HAVE_CONFIG_H)
typedef unsigned long uintptr_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif


/** @file clp.h
 * @brief Functions for parsing command line options.
 *
 * The CLP functions are used to parse command line arugments into options.
 * It automatically handles value parsing, error messages, long options with
 * minimum prefix matching, short options, and negated options.
 *
 * The CLP model works like this.
 *
 * <ol>
 * <li>The user declares an array of Clp_Option structures that define the
 * options their program accepts.</li>
 * <li>The user creates a Clp_Parser object using Clp_NewParser(), passing in
 * the command line arguments to parse and the Clp_Option structures.</li>
 * <li>A loop repeatedly calls Clp_Next() to parse the arguments.</li>
 * </ol>
 *
 * Unlike many command line parsing libraries, CLP steps through all arguments
 * one at a time, rather than slurping up all options at once.  This makes it
 * meaningful to give an option more than once.
 *
 * Here's an example.
 *
 * @code
 * #define ANIMAL_OPT 1
 * #define VEGETABLE_OPT 2
 * #define MINERALS_OPT 3
 * #define USAGE_OPT 4
 *
 * static const Clp_Option options[] = {
 *     { "animal", 'a', ANIMAL_OPT, Clp_ValString, 0 },
 *     { "vegetable", 'v', VEGETABLE_OPT, Clp_ValString, Clp_Negate | Clp_Optional },
 *     { "minerals", 'm', MINERALS_OPT, Clp_ValInt, 0 },
 *     { "usage", 0, USAGE_OPT, 0, 0 }
 * };
 *
 * int main(int argc, char *argv[]) {
 *     Clp_Parser *clp = Clp_NewParser(argc, argv,
 *               sizeof(options) / sizeof(options[0]), options);
 *     int opt;
 *     while ((opt = Clp_Next(clp)) != Clp_Done)
 *         switch (opt) {
 *         case ANIMAL_OPT:
 *             fprintf(stderr, "animal is %s\n", clp->val.s);
 *             break;
 *         case VEGETABLE_OPT:
 *             if (clp->negated)
 *                 fprintf(stderr, "no vegetables!\n");
 *             else if (clp->have_val)
 *                 fprintf(stderr, "vegetable is %s\n", clp->val.s);
 *             else
 *                 fprintf(stderr, "vegetables OK\n");
 *             break;
 *         case MINERALS_OPT:
 *             fprintf(stderr, "%d minerals\n", clp->val.i);
 *             break;
 *         case USAGE_OPT:
 *             fprintf(stderr, "Usage: 20q [--animal=ANIMAL] [--vegetable[=VEGETABLE]] [--minerals=N]\n");
 *             break;
 *         case Clp_NotOption:
 *             fprintf(stderr, "non-option %s\n", clp->vstr);
 *             break;
 *         }
 *     }
 * }
 * @endcode
 *
 * Here are a couple of executions.
 *
 * <pre>
 * % ./20q --animal=cat
 * animal is cat
 * % ./20q --animal=cat -a dog -afish --animal bird --an=snake
 * animal is cat
 * animal is dog
 * animal is fish
 * animal is bird
 * animal is snake
 * % ./20q --no-vegetables
 * no vegetables!
 * % ./20q -v
 * vegetables OK
 * % ./20q -vkale
 * vegetable is kale
 * % ./20q -m10
 * 10 minerals
 * % ./20q -m foo
 * '-m' expects an integer, not 'foo'
 * </pre>
 */


/* Option types for Clp_SetOptionChar */
#define Clp_DoubledLong		(Clp_LongImplicit * 2)

#define Clp_InitialValType	8
#define MAX_AMBIGUOUS_VALUES	4

typedef struct {
    int val_type;
    Clp_ValParseFunc func;
    int flags;
    void *user_data;
} Clp_ValType;

typedef struct {
    unsigned ilong : 1;
    unsigned ishort : 1;
    unsigned imandatory : 1;
    unsigned ioptional : 1;
    unsigned ipos : 1;
    unsigned ineg : 1;
    unsigned iprefmatch : 1;
    unsigned lmmpos_short : 1;
    unsigned lmmneg_short : 1;
    unsigned char ilongoff;
    int lmmpos;
    int lmmneg;
} Clp_InternOption;


#define Clp_OptionCharsSize	5

typedef struct {
    int c;
    int type;
} Clp_Oclass;
#define Clp_OclassSize		10

typedef struct Clp_Internal {
    const Clp_Option *opt;
    Clp_InternOption *iopt;
    int nopt;
    unsigned opt_generation;

    Clp_ValType *valtype;
    int nvaltype;

    const char * const *argv;
    int argc;

    Clp_Oclass oclass[Clp_OclassSize];
    int noclass;
    int long1pos;
    int long1neg;
    int utf8;

    char option_chars[Clp_OptionCharsSize];
    const char *xtext;

    const char *program_name;
    void (*error_handler)(Clp_Parser *, const char *);

    int option_processing;
    int current_option;

    unsigned char is_short;
    unsigned char whole_negated; /* true if negated by an option character */
    unsigned char could_be_short;
    unsigned char current_short;
    unsigned char negated_by_no;

    int ambiguous;
    int ambiguous_values[MAX_AMBIGUOUS_VALUES];
} Clp_Internal;


struct Clp_ParserState {
    const char * const *argv;
    int argc;

    char option_chars[Clp_OptionCharsSize];
    const char *xtext;

    int option_processing;

    unsigned opt_generation;
    int current_option;
    unsigned char is_short;
    unsigned char whole_negated;
    unsigned char current_short;
    unsigned char negated_by_no;
};


typedef struct Clp_StringList {
    Clp_Option *items;
    Clp_InternOption *iopt;
    int nitems;

    unsigned char allow_int;
    unsigned char val_long;
    int nitems_invalid_report;
} Clp_StringList;


static const Clp_Option clp_option_sentinel[] = {
    {"", 0, Clp_NotOption, 0, 0},
    {"", 0, Clp_Done, 0, 0},
    {"", 0, Clp_BadOption, 0, 0},
    {"", 0, Clp_Error, 0, 0}
};


static int parse_string(Clp_Parser *, const char *, int, void *);
static int parse_int(Clp_Parser *, const char *, int, void *);
static int parse_bool(Clp_Parser *, const char *, int, void *);
static int parse_double(Clp_Parser *, const char *, int, void *);
static int parse_string_list(Clp_Parser *, const char *, int, void *);

static int ambiguity_error(Clp_Parser *, int, int *, const Clp_Option *,
			   const Clp_InternOption *, const char *, const char *,
			   ...);


/*******
 * utf8
 **/

#define U_REPLACEMENT 0xFFFD

static char *
encode_utf8(char *s, int n, int c)
{
    if (c < 0 || c >= 0x110000 || (c >= 0xD800 && c <= 0xDFFF))
	c = U_REPLACEMENT;
    if (c <= 0x7F && n >= 1)
	*s++ = c;
    else if (c <= 0x7FF && n >= 2) {
	*s++ = 0xC0 | (c >> 6);
	goto char1;
    } else if (c <= 0xFFFF && n >= 3) {
	*s++ = 0xE0 | (c >> 12);
	goto char2;
    } else if (n >= 4) {
	*s++ = 0xF0 | (c >> 18);
	*s++ = 0x80 | ((c >> 12) & 0x3F);
      char2:
	*s++ = 0x80 | ((c >> 6) & 0x3F);
      char1:
	*s++ = 0x80 | (c & 0x3F);
    }
    return s;
}

static int
decode_utf8(const char *s, const char **cp)
{
    int c;
    if ((unsigned char) *s <= 0x7F)		/* 1 byte:  0x000000-0x00007F */
	c = *s++;
    else if ((unsigned char) *s <= 0xC1)	/*   bad/overlong encoding */
	goto replacement;
    else if ((unsigned char) *s <= 0xDF) {	/* 2 bytes: 0x000080-0x0007FF */
	if ((s[1] & 0xC0) != 0x80)		/*   bad encoding */
	    goto replacement;
	c = (*s++ & 0x1F) << 6;
	goto char1;
    } else if ((unsigned char) *s <= 0xEF) {	/* 3 bytes: 0x000800-0x00FFFF */
	if ((s[1] & 0xC0) != 0x80		/*   bad encoding */
	    || (s[2] & 0xC0) != 0x80		/*   bad encoding */
	    || ((unsigned char) *s == 0xE0	/*   overlong encoding */
		&& (s[1] & 0xE0) == 0x80)
	    || ((unsigned char) *s == 0xED	/*   encoded surrogate */
		&& (s[1] & 0xE0) == 0xA0))
	    goto replacement;
	c = (*s++ & 0x0F) << 12;
	goto char2;
    } else if ((unsigned char) *s <= 0xF4) {	/* 4 bytes: 0x010000-0x10FFFF */
	if ((s[1] & 0xC0) != 0x80		/*   bad encoding */
	    || (s[2] & 0xC0) != 0x80		/*   bad encoding */
	    || (s[3] & 0xC0) != 0x80		/*   bad encoding */
	    || ((unsigned char) *s == 0xF0	/*   overlong encoding */
		&& (s[1] & 0xF0) == 0x80)
	    || ((unsigned char) *s == 0xF4	/*   encoded value > 0x10FFFF */
		&& (unsigned char) s[1] >= 0x90))
	    goto replacement;
	c = (*s++ & 0x07) << 18;
	c += (*s++ & 0x3F) << 12;
      char2:
	c += (*s++ & 0x3F) << 6;
      char1:
	c += (*s++ & 0x3F);
    } else {
      replacement:
	c = U_REPLACEMENT;
	for (s++; (*s & 0xC0) == 0x80; s++)
	    /* nothing */;
    }
    if (cp)
	*cp = s;
    return c;
}

static int
utf8_charlen(const char *s)
{
    const char *sout;
    (void) decode_utf8(s, &sout);
    return sout - s;
}

static int
clp_utf8_charlen(const Clp_Internal *cli, const char *s)
{
    return (cli->utf8 ? utf8_charlen(s) : 1);
}


/*******
 * Clp_NewParser, etc.
 **/

static int
min_different_chars(const char *s, const char *t)
     /* Returns the minimum number of bytes required to distinguish
	s from t.
	If s is shorter than t, returns strlen(s). */
{
    const char *sfirst = s;
    while (*s && *t && *s == *t)
	s++, t++;
    if (!*s)
	return s - sfirst;
    else
	return s - sfirst + 1;
}

static int
long_as_short(const Clp_Internal *cli, const Clp_Option *o,
	      Clp_InternOption *io, int failure)
{
    if ((cli->long1pos || cli->long1neg) && io->ilong) {
	const char *name = o->long_name + io->ilongoff;
	if (cli->utf8) {
	    int c = decode_utf8(name, &name);
	    if (!*name && c && c != U_REPLACEMENT)
		return c;
	} else if (name[0] && !name[1])
	    return (unsigned char) name[0];
    }
    return failure;
}

static void
compare_options(Clp_Parser *clp, const Clp_Option *o1, Clp_InternOption *io1,
		const Clp_Option *o2, Clp_InternOption *io2)
{
    Clp_Internal *cli = clp->internal;
    int short1, shortx1;

    /* ignore meaningless combinations */
    if ((!io1->ishort && !io1->ilong) || (!io2->ishort && !io2->ilong)
	|| !((io1->ipos && io2->ipos) || (io1->ineg && io2->ineg))
	|| o1->option_id == o2->option_id)
	return;

    /* look for duplication of short options */
    short1 = (io1->ishort ? o1->short_name : -1);
    shortx1 = long_as_short(cli, o1, io1, -2);
    if (short1 >= 0 || shortx1 >= 0) {
	int short2 = (io2->ishort ? o2->short_name : -3);
	int shortx2 = long_as_short(cli, o2, io2, -4);
	if (short1 == short2)
	    Clp_OptionError(clp, "CLP internal error: more than 1 option has short name %<%c%>", short1);
	else if ((short1 == shortx2 || shortx1 == short2 || shortx1 == shortx2)
		 && ((io1->ipos && io2->ipos && cli->long1pos)
		     || (io1->ineg && io2->ineg && cli->long1neg)))
	    Clp_OptionError(clp, "CLP internal error: 1-char long name conflicts with short name %<%c%>", (short1 == shortx2 ? shortx2 : shortx1));
    }

    /* analyze longest minimum match */
    if (io1->ilong) {
	const char *name1 = o1->long_name + io1->ilongoff;

	/* long name's first character matches short name */
	if (io2->ishort && !io1->iprefmatch) {
	    int name1char = (cli->utf8 ? decode_utf8(name1, 0) : (unsigned char) *name1);
	    if (name1char == o2->short_name) {
		if (io1->ipos && io2->ipos)
		    io1->lmmpos_short = 1;
		if (io1->ineg && io2->ineg)
		    io1->lmmneg_short = 1;
	    }
	}

	/* match long name to long name */
	if (io2->ilong) {
	    const char *name2 = o2->long_name + io2->ilongoff;
	    if (strcmp(name1, name2) == 0)
		Clp_OptionError(clp, "CLP internal error: duplicate long name %<%s%>", name1);
	    if (io1->ipos && io2->ipos && !strncmp(name1, name2, io1->lmmpos)
		&& (!io1->iprefmatch || strncmp(name1, name2, strlen(name1))))
		io1->lmmpos = min_different_chars(name1, name2);
	    if (io1->ineg && io2->ineg && !strncmp(name1, name2, io1->lmmneg)
		&& (!io1->iprefmatch || strncmp(name1, name2, strlen(name1))))
		io1->lmmneg = min_different_chars(name1, name2);
	}
    }
}

static void
calculate_lmm(Clp_Parser *clp, const Clp_Option *opt, Clp_InternOption *iopt, int nopt)
{
    int i, j;
    for (i = 0; i < nopt; ++i) {
	iopt[i].lmmpos = iopt[i].lmmneg = 1;
	iopt[i].lmmpos_short = iopt[i].lmmneg_short = 0;
	for (j = 0; j < nopt; ++j)
	    compare_options(clp, &opt[i], &iopt[i], &opt[j], &iopt[j]);
    }
}

/** @param argc number of arguments
 * @param argv argument array
 * @param nopt number of option definitions
 * @param opt option definition array
 * @return the parser
 *
 * The new Clp_Parser that will parse the arguments in @a argv according to
 * the option definitions in @a opt.
 *
 * The Clp_Parser is created with the following characteristics:
 *
 * <ul>
 * <li>The "-" character introduces short options (<tt>Clp_SetOptionChar(clp,
 * '-', Clp_Short)</tt>).</li>
 * <li>Clp_ProgramName is set from the first argument in @a argv, if any.  The
 * first argument returned by Clp_Next() will be the second argument in @a
 * argv.  Note that this behavior differs from Clp_SetArguments.</li>
 * <li>UTF-8 support is on iff the <tt>LANG</tt> environment variable contains
 * one of the substrings "UTF-8", "UTF8", or "utf8".  Override this with
 * Clp_SetUTF8().</li>
 * <li>The Clp_ValString, Clp_ValStringNotOption, Clp_ValInt, Clp_ValUnsigned,
 * Clp_ValLong, Clp_ValUnsignedLong, Clp_ValBool, and Clp_ValDouble types are
 * installed.</li>
 * <li>Errors are reported to standard error.</li>
 * </ul>
 *
 * You may also create a Clp_Parser with no arguments or options
 * (<tt>Clp_NewParser(0, 0, 0, 0)</tt>) and set the arguments and options
 * later.
 *
 * Returns NULL if there isn't enough memory to construct the parser.
 *
 * @note The CLP library will not modify the contents of @a argv or @a opt.
 * The calling program must not modify @a opt.  It may modify @a argv in
 * limited cases.
 */
Clp_Parser *
Clp_NewParser(int argc, const char * const *argv, int nopt, const Clp_Option *opt)
{
    Clp_Parser *clp = (Clp_Parser *)malloc(sizeof(Clp_Parser));
    Clp_Internal *cli = (Clp_Internal *)malloc(sizeof(Clp_Internal));
    Clp_InternOption *iopt = (Clp_InternOption *)malloc(sizeof(Clp_InternOption) * nopt);
    if (cli)
	cli->valtype = (Clp_ValType *)malloc(sizeof(Clp_ValType) * Clp_InitialValType);
    if (!clp || !cli || !iopt || !cli->valtype)
	goto failed;

    clp->option = &clp_option_sentinel[-Clp_Done];
    clp->negated = 0;
    clp->have_val = 0;
    clp->vstr = 0;
    clp->user_data = 0;
    clp->internal = cli;

    cli->opt = opt;
    cli->nopt = nopt;
    cli->iopt = iopt;
    cli->opt_generation = 0;
    cli->error_handler = 0;

    /* Assign program name (now so we can call Clp_OptionError) */
    if (argc > 0) {
	const char *slash = strrchr(argv[0], '/');
	cli->program_name = slash ? slash + 1 : argv[0];
    } else
	cli->program_name = 0;

    /* Assign arguments, skipping program name */
    Clp_SetArguments(clp, argc - 1, argv + 1);

    /* Initialize UTF-8 status and option classes */
    {
	char *s = getenv("LANG");
	cli->utf8 = (s && (strstr(s, "UTF-8") != 0 || strstr(s, "UTF8") != 0
			   || strstr(s, "utf8") != 0));
    }
    cli->oclass[0].c = '-';
    cli->oclass[0].type = Clp_Short;
    cli->noclass = 1;
    cli->long1pos = cli->long1neg = 0;

    /* Add default type parsers */
    cli->nvaltype = 0;
    Clp_AddType(clp, Clp_ValString, 0, parse_string, 0);
    Clp_AddType(clp, Clp_ValStringNotOption, Clp_DisallowOptions, parse_string, 0);
    Clp_AddType(clp, Clp_ValInt, 0, parse_int, (void*) (uintptr_t) 0);
    Clp_AddType(clp, Clp_ValUnsigned, 0, parse_int, (void*) (uintptr_t) 1);
    Clp_AddType(clp, Clp_ValLong, 0, parse_int, (void*) (uintptr_t) 2);
    Clp_AddType(clp, Clp_ValUnsignedLong, 0, parse_int, (void*) (uintptr_t) 3);
    Clp_AddType(clp, Clp_ValBool, 0, parse_bool, 0);
    Clp_AddType(clp, Clp_ValDouble, 0, parse_double, 0);

    /* Set options */
    Clp_SetOptions(clp, nopt, opt);

    return clp;

  failed:
    if (cli && cli->valtype)
	free(cli->valtype);
    if (cli)
	free(cli);
    if (clp)
	free(clp);
    if (iopt)
	free(iopt);
    return 0;
}

/** @param clp the parser
 *
 * All memory associated with @a clp is freed. */
void
Clp_DeleteParser(Clp_Parser *clp)
{
    int i;
    Clp_Internal *cli;
    if (!clp)
	return;

    cli = clp->internal;

    /* get rid of any string list types */
    for (i = 0; i < cli->nvaltype; i++)
	if (cli->valtype[i].func == parse_string_list) {
	    Clp_StringList *clsl = (Clp_StringList *)cli->valtype[i].user_data;
	    free(clsl->items);
	    free(clsl->iopt);
	    free(clsl);
	}

    free(cli->valtype);
    free(cli->iopt);
    free(cli);
    free(clp);
}


/** @param clp the parser
 * @param errh error handler function
 * @return previous error handler function
 *
 * The error handler function is called when CLP encounters an error while
 * parsing the command line.  It is called with the arguments "<tt>(*errh)(@a
 * clp, s)</tt>", where <tt>s</tt> is a description of the error terminated by
 * a newline.  The <tt>s</tt> descriptions produced by CLP itself are prefixed
 * by the program name, if any. */
Clp_ErrorHandler
Clp_SetErrorHandler(Clp_Parser *clp, void (*errh)(Clp_Parser *, const char *))
{
    Clp_Internal *cli = clp->internal;
    Clp_ErrorHandler old = cli->error_handler;
    cli->error_handler = errh;
    return old;
}

/** @param clp the parser
 * @param utf8 does the parser support UTF-8?
 * @return previous UTF-8 mode
 *
 * In UTF-8 mode, all input strings (arguments and long names for options) are
 * assumed to be encoded via UTF-8, and all character names
 * (Clp_SetOptionChar() and short names for options) may cover the whole
 * Unicode range.  Out of UTF-8 mode, all input strings are treated as binary,
 * and all character names must be unsigned char values.
 *
 * Furthermore, error messages in UTF-8 mode may contain Unicode quote
 * characters. */
int
Clp_SetUTF8(Clp_Parser *clp, int utf8)
{
    Clp_Internal *cli = clp->internal;
    int old_utf8 = cli->utf8;
    cli->utf8 = utf8;
    calculate_lmm(clp, cli->opt, cli->iopt, cli->nopt);
    return old_utf8;
}

/** @param clp the parser
 * @param c character
 * @return option character treatment
 *
 * Returns an integer specifying how CLP treats arguments that begin
 * with character @a c.  See Clp_SetOptionChar for possibilities.
 */
int
Clp_OptionChar(Clp_Parser *clp, int c)
{
    Clp_Internal *cli = clp->internal;
    int i, oclass = 0;
    if (cli->noclass > 0 && cli->oclass[0].c == 0)
	oclass = cli->oclass[0].type;
    for (i = 0; i < cli->noclass; ++i)
	if (cli->oclass[i].c == c)
	    oclass = cli->oclass[i].type;
    return oclass;
}

/** @param clp the parser
 * @param c character
 * @param type option character treatment
 * @return previous option character treatment, or -1 on error
 *
 * @a type specifies how CLP treats arguments that begin with character @a c.
 * Possibilities are:
 *
 * <dl>
 * <dt>Clp_NotOption (or 0)</dt>
 * <dd>The argument cannot be an option.</dd>
 * <dt>Clp_Long</dt>
 * <dd>The argument is a long option.</dd>
 * <dt>Clp_Short</dt>
 * <dd>The argument is a set of short options.</dd>
 * <dt>Clp_Short|Clp_Long</dt>
 * <dd>The argument is either a long option or, if no matching long option is
 * found, a set of short options.</dd>
 * <dt>Clp_LongNegated</dt>
 * <dd>The argument is a negated long option.  For example, after
 * Clp_SetOptionChar(@a clp, '^', Clp_LongNegated), the argument "^foo" is
 * equivalent to "--no-foo".</dd>
 * <dt>Clp_ShortNegated</dt>
 * <dd>The argument is a set of negated short options.</dd>
 * <dt>Clp_ShortNegated|Clp_LongNegated</dt>
 * <dd>The argument is either a negated long option or, if no matching long
 * option is found, a set of negated short options.</dd>
 * <dt>Clp_LongImplicit</dt>
 * <dd>The argument may be a long option, where the character @a c is actually
 * part of the long option name.  For example, after Clp_SetOptionChar(@a clp,
 * 'f', Clp_LongImplicit), the argument "foo" may be equivalent to
 * "--foo".</dd>
 * </dl>
 *
 * In UTF-8 mode, @a c may be any Unicode character.  Otherwise, @a c must be
 * an unsigned char value.  The special character 0 assigns @a type to @em
 * every character.
 *
 * It is an error if @a c is out of range, @a type is illegal, or there are
 * too many character definitions stored in @a clp already.  The function
 * returns -1 on error.
 *
 * A double hyphen "--" always introduces a long option.  This behavior cannot
 * currently be changed with Clp_SetOptionChar().
 */
int
Clp_SetOptionChar(Clp_Parser *clp, int c, int type)
{
    int i, long1pos, long1neg;
    int old = Clp_OptionChar(clp, c);
    Clp_Internal *cli = clp->internal;

    if (type != Clp_NotOption && type != Clp_Short && type != Clp_Long
	&& type != Clp_ShortNegated && type != Clp_LongNegated
	&& type != Clp_LongImplicit && type != (Clp_Short | Clp_Long)
	&& type != (Clp_ShortNegated | Clp_LongNegated))
	return -1;
    if (c < 0 || c >= (cli->utf8 ? 0x110000 : 256))
	return -1;

    if (c == 0)
	cli->noclass = 0;
    for (i = 0; i < cli->noclass; ++i)
	if (cli->oclass[i].c == c)
	    break;
    if (i == Clp_OclassSize)
	return -1;

    cli->oclass[i].c = c;
    cli->oclass[i].type = type;
    if (cli->noclass == i)
	cli->noclass = i + 1;

    long1pos = long1neg = 0;
    for (i = 0; i < cli->noclass; ++i) {
	if ((cli->oclass[i].type & Clp_Short)
	    && (cli->oclass[i].type & Clp_Long))
	    long1pos = 1;
	if ((cli->oclass[i].type & Clp_ShortNegated)
	    && (cli->oclass[i].type & Clp_LongNegated))
	    long1neg = 1;
    }

    if (long1pos != cli->long1pos || long1neg != cli->long1neg) {
	/* Must recheck option set */
	cli->long1pos = long1pos;
	cli->long1neg = long1neg;
	calculate_lmm(clp, cli->opt, cli->iopt, cli->nopt);
    }

    return old;
}

/** @param clp the parser
 * @param nopt number of option definitions
 * @param opt option definition array
 * @return 0 on success, -1 on failure
 *
 * Installs the option definitions in @a opt.  Future option parsing will
 * use @a opt to search for options.
 *
 * Also checks @a opt's option definitions for validity.  "CLP internal
 * errors" are reported via Clp_OptionError() if:
 *
 * <ul>
 * <li>An option has a negative ID.</li>
 * <li>Two different short options have the same name.</li>
 * <li>Two different long options have the same name.</li>
 * <li>A short and a long option are ambiguous, in that some option character
 * might introduce either a short or a long option (e.g., Clp_SetOptionChar(@a
 * clp, '-', Clp_Long|Clp_Short)), and a short name equals a long name.</li>
 * </ul>
 *
 * If necessary memory cannot be allocated, this function returns -1 without
 * modifying the parser.
 *
 * @note The CLP library will not modify the contents of @a argv or @a opt.
 * The calling program must not modify @a opt either until another call to
 * Clp_SetOptions() or the parser is destroyed.
 */
int
Clp_SetOptions(Clp_Parser *clp, int nopt, const Clp_Option *opt)
{
    Clp_Internal *cli = clp->internal;
    Clp_InternOption *iopt;
    int i;
    static unsigned opt_generation = 0;

    if (nopt > cli->nopt) {
	iopt = (Clp_InternOption *)malloc(sizeof(Clp_InternOption) * nopt);
	if (!iopt)
	    return -1;
	free(cli->iopt);
	cli->iopt = iopt;
    }

    cli->opt = opt;
    cli->nopt = nopt;
    cli->opt_generation = ++opt_generation;
    iopt = cli->iopt;
    cli->current_option = -1;

    /* Massage the options to make them usable */
    for (i = 0; i < nopt; ++i) {
	memset(&iopt[i], 0, sizeof(iopt[i]));

	/* Ignore negative option_ids, which are internal to CLP */
	if (opt[i].option_id < 0) {
	    Clp_OptionError(clp, "CLP internal error: option %d has negative option_id", i);
	    iopt[i].ilong = iopt[i].ishort = iopt[i].ipos = iopt[i].ineg = 0;
	    continue;
	}

	/* Set flags based on input flags */
	iopt[i].ilong = (opt[i].long_name != 0 && opt[i].long_name[0] != 0);
	iopt[i].ishort = (opt[i].short_name > 0
			  && opt[i].short_name < (cli->utf8 ? 0x110000 : 256));
	iopt[i].ipos = 1;
	iopt[i].ineg = (opt[i].flags & Clp_Negate) != 0;
	iopt[i].imandatory = (opt[i].flags & Clp_Mandatory) != 0;
	iopt[i].ioptional = (opt[i].flags & Clp_Optional) != 0;
	iopt[i].iprefmatch = (opt[i].flags & Clp_PreferredMatch) != 0;
	iopt[i].ilongoff = 0;

	/* Enforce invariants */
	if (opt[i].val_type <= 0)
	    iopt[i].imandatory = iopt[i].ioptional = 0;
	if (opt[i].val_type > 0 && !iopt[i].ioptional)
	    iopt[i].imandatory = 1;

	/* Options that start with 'no-' should be changed to OnlyNegated */
	if (iopt[i].ilong && strncmp(opt[i].long_name, "no-", 3) == 0) {
	    iopt[i].ipos = 0;
	    iopt[i].ineg = 1;
	    iopt[i].ilongoff = 3;
	    if (strncmp(opt[i].long_name + 3, "no-", 3) == 0)
		Clp_OptionError(clp, "CLP internal error: option %d begins with \"no-no-\"", i);
	} else if (opt[i].flags & Clp_OnlyNegated) {
	    iopt[i].ipos = 0;
	    iopt[i].ineg = 1;
	}
    }

    /* Check option set */
    calculate_lmm(clp, opt, iopt, nopt);

    return 0;
}

/** @param clp the parser
 * @param argc number of arguments
 * @param argv argument array
 *
 * Installs the arguments in @a argv for parsing.  Future option parsing will
 * analyze @a argv.
 *
 * Unlike Clp_NewParser(), this function does not treat @a argv[0] specially.
 * The first subsequent call to Clp_Next() will analyze @a argv[0].
 *
 * This function also sets option processing to on, as by
 * Clp_SetOptionProcessing(@a clp, 1).
 *
 * @note The CLP library will not modify the contents of @a argv.  The calling
 * program should not generally modify the element of @a argv that CLP is
 * currently analyzing.
 */
void
Clp_SetArguments(Clp_Parser *clp, int argc, const char * const *argv)
{
    Clp_Internal *cli = clp->internal;

    cli->argc = argc + 1;
    cli->argv = argv - 1;

    cli->is_short = 0;
    cli->whole_negated = 0;
    cli->option_processing = 1;
    cli->current_option = -1;
}


/** @param clp the parser
 * @param on whether to search for options
 * @return previous option processing setting
 *
 * When option processing is off, every call to Clp_Next() returns
 * Clp_NotOption.  By default the option <tt>"--"</tt> turns off option
 * processing and is otherwise ignored.
 */
int
Clp_SetOptionProcessing(Clp_Parser *clp, int on)
{
    Clp_Internal *cli = clp->internal;
    int old = cli->option_processing;
    cli->option_processing = on;
    return old;
}


/*******
 * functions for Clp_Option lists
 **/

/* the ever-glorious argcmp */

static int
argcmp(const char *ref, const char *arg, int min_match, int fewer_dashes)
     /* Returns 0 if ref and arg don't match.
	Returns -1 if ref and arg match, but fewer than min_match characters.
	Returns len if ref and arg match min_match or more characters;
	len is the number of characters that matched in arg.
	Allows arg to contain fewer dashes than ref iff fewer_dashes != 0.

	Examples:
	argcmp("x", "y", 1, 0)	-->  0	/ just plain wrong
	argcmp("a", "ax", 1, 0)	-->  0	/ ...even though min_match == 1
					and the 1st chars match
	argcmp("box", "bo", 3, 0) --> -1	/ ambiguous
	argcmp("cat", "c=3", 1, 0) -->  1	/ handles = arguments
	*/
{
    const char *refstart = ref;
    const char *argstart = arg;
    assert(min_match > 0);

  compare:
    while (*ref && *arg && *arg != '=' && *ref == *arg)
	ref++, arg++;

    /* Allow arg to contain fewer dashes than ref */
    if (fewer_dashes && *ref == '-' && ref[1] && ref[1] == *arg) {
	ref++;
	goto compare;
    }

    if (*arg && *arg != '=')
	return 0;
    else if (ref - refstart < min_match)
	return -1;
    else
	return arg - argstart;
}

static int
find_prefix_opt(Clp_Parser *clp, const char *arg,
		int nopt, const Clp_Option *opt,
		const Clp_InternOption *iopt,
		int *ambiguous, int *ambiguous_values)
     /* Looks for an unambiguous match of 'arg' against one of the long
        options in 'opt'. Returns positive if it finds one; otherwise, returns
        -1 and possibly changes 'ambiguous' and 'ambiguous_values' to keep
        track of at most MAX_AMBIGUOUS_VALUES possibilities. */
{
    int i, fewer_dashes = 0, first_ambiguous = *ambiguous;
    int negated = clp && clp->negated;
    int first_charlen = (clp ? clp_utf8_charlen(clp->internal, arg) : 1);

  retry:
    for (i = 0; i < nopt; i++) {
	int len, lmm;
	if (!iopt[i].ilong || (negated ? !iopt[i].ineg : !iopt[i].ipos))
	    continue;

	lmm = (negated ? iopt[i].lmmneg : iopt[i].lmmpos);
	if (clp && clp->internal->could_be_short
	    && (negated ? iopt[i].lmmneg_short : iopt[i].lmmpos_short))
	    lmm = (first_charlen >= lmm ? first_charlen + 1 : lmm);
	len = argcmp(opt[i].long_name + iopt[i].ilongoff, arg, lmm, fewer_dashes);
	if (len > 0)
	    return i;
	else if (len < 0) {
	    if (*ambiguous < MAX_AMBIGUOUS_VALUES)
		ambiguous_values[*ambiguous] = i;
	    (*ambiguous)++;
	}
    }

    /* If there were no partial matches, try again with fewer_dashes true */
    if (*ambiguous == first_ambiguous && !fewer_dashes) {
	fewer_dashes = 1;
	goto retry;
    }

    return -1;
}


/*****
 * Argument parsing
 **/

static int
val_type_binsearch(Clp_Internal *cli, int val_type)
{
    unsigned l = 0, r = cli->nvaltype;
    while (l < r) {
	unsigned m = l + (r - l) / 2;
	if (cli->valtype[m].val_type == val_type)
	    return m;
	else if (cli->valtype[m].val_type < val_type)
	    l = m + 1;
	else
	    r = m;
    }
    return l;
}

/** @param clp the parser
 * @param val_type value type ID
 * @param flags value type flags
 * @param parser parser function
 * @param user_data user data for @a parser function
 * @return 0 on success, -1 on failure
 *
 * Defines argument type @a val_type in parser @a clp.  The parsing function
 * @a parser will be passed argument values for type @a val_type.  It should
 * parse the argument into values (usually in @a clp->val, but sometimes
 * elsewhere), report errors if necessary, and return whether the parse was
 * successful.
 *
 * Any prior argument parser match @a val_type is removed.  @a val_type must
 * be greater than zero.
 *
 * @a flags specifies additional parsing flags.  At the moment the only
 * relevant flag is Clp_DisallowOptions, which means that separated values
 * must not look like options.  For example, assume argument
 * <tt>--a</tt>/<tt>-a</tt> has mandatory value type Clp_ValStringNotOption
 * (which has Clp_DisallowOptions).  Then:
 *
 * <ul>
 * <li><tt>--a=--b</tt> will parse with value <tt>--b</tt>.</li>
 * <li><tt>-a--b</tt> will parse with value <tt>--b</tt>.</li>
 * <li><tt>--a --b</tt> will not parse, since the mandatory value looks like
 * an option.</li>
 * <li><tt>-a --b</tt> will not parse, since the mandatory value looks like
 * an option.</li>
 * </ul>
 */
int
Clp_AddType(Clp_Parser *clp, int val_type, int flags,
	    Clp_ValParseFunc parser, void *user_data)
{
    Clp_Internal *cli = clp->internal;
    int vtpos;

    if (val_type <= 0 || !parser)
	return -1;

    vtpos = val_type_binsearch(cli, val_type);

    if (vtpos == cli->nvaltype || cli->valtype[vtpos].val_type != val_type) {
	if (cli->nvaltype != 0 && (cli->nvaltype % Clp_InitialValType) == 0) {
	    Clp_ValType *new_valtype =
		(Clp_ValType *) realloc(cli->valtype, sizeof(Clp_ValType) * (cli->nvaltype + Clp_InitialValType));
	    if (!new_valtype)
		return -1;
	    cli->valtype = new_valtype;
	}
	memmove(&cli->valtype[vtpos + 1], &cli->valtype[vtpos],
		sizeof(Clp_ValType) * (cli->nvaltype - vtpos));
	cli->nvaltype++;
	cli->valtype[vtpos].func = 0;
    }

    if (cli->valtype[vtpos].func == parse_string_list) {
	Clp_StringList *clsl = (Clp_StringList *) cli->valtype[vtpos].user_data;
	free(clsl->items);
	free(clsl->iopt);
	free(clsl);
    }

    cli->valtype[vtpos].val_type = val_type;
    cli->valtype[vtpos].func = parser;
    cli->valtype[vtpos].flags = flags;
    cli->valtype[vtpos].user_data = user_data;
    return 0;
}


/*******
 * Default argument parsers
 **/

static int
parse_string(Clp_Parser *clp, const char *arg, int complain, void *user_data)
{
    (void)complain, (void)user_data;
    clp->val.s = arg;
    return 1;
}

static int
parse_int(Clp_Parser* clp, const char* arg, int complain, void* user_data)
{
    const char *val;
    uintptr_t type = (uintptr_t) user_data;
    if (*arg == 0 || isspace((unsigned char) *arg)
	|| ((type & 1) && *arg == '-'))
	val = arg;
    else if (type & 1) { /* unsigned */
#if HAVE_STRTOUL
	clp->val.ul = strtoul(arg, (char **) &val, 0);
#else
	/* don't bother really trying to do it right */
	if (arg[0] == '-')
	    val = arg;
	else
	    clp->val.l = strtol(arg, (char **) &val, 0);
#endif
    } else
	clp->val.l = strtol(arg, (char **) &val, 0);
    if (type <= 1)
        clp->val.u = (unsigned) clp->val.ul;
    if (*arg != 0 && *val == 0)
	return 1;
    else {
        if (complain) {
            const char *message = (type & 1)
                ? "%<%O%> expects a nonnegative integer, not %<%s%>"
                : "%<%O%> expects an integer, not %<%s%>";
            Clp_OptionError(clp, message, arg);
        }
        return 0;
    }
}

static int
parse_double(Clp_Parser *clp, const char *arg, int complain, void *user_data)
{
    const char *val;
    (void)user_data;
    if (*arg == 0 || isspace((unsigned char) *arg))
	val = arg;
    else
	clp->val.d = strtod(arg, (char **) &val);
    if (*arg != 0 && *val == 0)
	return 1;
    else {
        if (complain)
            Clp_OptionError(clp, "%<%O%> expects a real number, not %<%s%>", arg);
	return 0;
    }
}

static int
parse_bool(Clp_Parser *clp, const char *arg, int complain, void *user_data)
{
    int i;
    char lcarg[6];
    (void)user_data;
    if (strlen(arg) > 5 || strchr(arg, '=') != 0)
	goto error;

    for (i = 0; arg[i] != 0; i++)
	lcarg[i] = tolower((unsigned char) arg[i]);
    lcarg[i] = 0;

    if (argcmp("yes", lcarg, 1, 0) > 0
	|| argcmp("true", lcarg, 1, 0) > 0
	|| argcmp("1", lcarg, 1, 0) > 0) {
	clp->val.i = 1;
	return 1;
    } else if (argcmp("no", lcarg, 1, 0) > 0
	       || argcmp("false", lcarg, 1, 0) > 0
	       || argcmp("1", lcarg, 1, 0) > 0) {
	clp->val.i = 0;
	return 1;
    }

  error:
    if (complain)
	Clp_OptionError(clp, "%<%O%> expects a true-or-false value, not %<%s%>", arg);
    return 0;
}


/*****
 * Clp_AddStringListType
 **/

static int
parse_string_list(Clp_Parser *clp, const char *arg, int complain, void *user_data)
{
    Clp_StringList *sl = (Clp_StringList *)user_data;
    int idx, ambiguous = 0;
    int ambiguous_values[MAX_AMBIGUOUS_VALUES + 1];

    /* actually look for a string value */
    idx = find_prefix_opt
	(0, arg, sl->nitems, sl->items, sl->iopt,
	 &ambiguous, ambiguous_values);
    if (idx >= 0) {
	clp->val.i = sl->items[idx].option_id;
        if (sl->val_long)
            clp->val.l = clp->val.i;
        return 1;
    }

    if (sl->allow_int) {
	if (parse_int(clp, arg, 0, (void*) (uintptr_t) (sl->val_long ? 2 : 0)))
	    return 1;
    }

    if (complain) {
	const char *complaint = (ambiguous ? "ambiguous" : "invalid");
	if (!ambiguous) {
	    ambiguous = sl->nitems_invalid_report;
	    for (idx = 0; idx < ambiguous; idx++)
		ambiguous_values[idx] = idx;
	}
	return ambiguity_error
	    (clp, ambiguous, ambiguous_values, sl->items, sl->iopt,
	     "", "option %<%V%> is %s", complaint);
    } else
	return 0;
}


static int
finish_string_list(Clp_Parser *clp, int val_type, int flags,
		   Clp_Option *items, int nitems, int itemscap)
{
    int i;
    Clp_StringList *clsl = (Clp_StringList *)malloc(sizeof(Clp_StringList));
    Clp_InternOption *iopt = (Clp_InternOption *)malloc(sizeof(Clp_InternOption) * nitems);
    if (!clsl || !iopt)
	goto error;

    clsl->items = items;
    clsl->iopt = iopt;
    clsl->nitems = nitems;
    clsl->allow_int = (flags & Clp_AllowNumbers) != 0;
    clsl->val_long = (flags & Clp_StringListLong) != 0;

    if (nitems < MAX_AMBIGUOUS_VALUES && nitems < itemscap && clsl->allow_int) {
	items[nitems].long_name = "any integer";
	clsl->nitems_invalid_report = nitems + 1;
    } else if (nitems > MAX_AMBIGUOUS_VALUES + 1)
	clsl->nitems_invalid_report = MAX_AMBIGUOUS_VALUES + 1;
    else
	clsl->nitems_invalid_report = nitems;

    for (i = 0; i < nitems; i++) {
	iopt[i].ilong = iopt[i].ipos = 1;
	iopt[i].ishort = iopt[i].ineg = iopt[i].ilongoff = iopt[i].iprefmatch = 0;
    }
    calculate_lmm(clp, items, iopt, nitems);

    if (Clp_AddType(clp, val_type, 0, parse_string_list, clsl) >= 0)
	return 0;

  error:
    if (clsl)
	free(clsl);
    if (iopt)
	free(iopt);
    return -1;
}

/** @param clp the parser
 * @param val_type value type ID
 * @param flags string list flags
 * @return 0 on success, -1 on failure
 *
 * Defines argument type @a val_type in parser @a clp.  The parsing function
 * sets @a clp->val.i to an integer.  The value string is matched against
 * strings provided in the ellipsis arguments.  For example, the
 * Clp_AddStringListType() call below has the same effect as the
 * Clp_AddStringListTypeVec() call:
 *
 * For example:
 * @code
 * Clp_AddStringListType(clp, 100, Clp_AllowNumbers, "cat", 1,
 *                       "cattle", 2, "dog", 3, (const char *) NULL);
 *
 * const char * const strs[] = { "cat", "cattle", "dog" };
 * const int vals[]          = { 1,     2,        3     };
 * Clp_AddStringListTypeVec(clp, 100, Clp_AllowNumbers, 3, strs, vals);
 * @endcode
 *
 * @note The CLP library will not modify any of the passed-in strings.  The
 * calling program must not modify or free them either until the parser is
 * destroyed.
 */
int
Clp_AddStringListType(Clp_Parser *clp, int val_type, int flags, ...)
{
    int nitems = 0;
    int itemscap = 5;
    Clp_Option *items = (Clp_Option *)malloc(sizeof(Clp_Option) * itemscap);

    va_list val;
    va_start(val, flags);

    if (!items)
	goto error;

    /* slurp up the arguments */
    while (1) {
	int value;
	char *name = va_arg(val, char *);
	if (!name)
	    break;
        if (flags & Clp_StringListLong) {
            long lvalue = va_arg(val, long);
            value = (int) lvalue;
            assert(value == lvalue);
        } else
            value = va_arg(val, int);

	if (nitems >= itemscap) {
	    Clp_Option *new_items;
	    itemscap *= 2;
	    new_items = (Clp_Option *)realloc(items, sizeof(Clp_Option) * itemscap);
	    if (!new_items)
		goto error;
	    items = new_items;
	}

	items[nitems].long_name = name;
	items[nitems].option_id = value;
	items[nitems].flags = 0;
	nitems++;
    }

    va_end(val);
    if (finish_string_list(clp, val_type, flags, items, nitems, itemscap) >= 0)
	return 0;

  error:
    va_end(val);
    if (items)
	free(items);
    return -1;
}

/** @param clp the parser
 * @param val_type value type ID
 * @param flags string list flags
 * @param nstrs number of strings in list
 * @param strs array of strings
 * @param vals array of values
 * @return 0 on success, -1 on failure
 *
 * Defines argument type @a val_type in parser @a clp.  The parsing function
 * sets @a clp->val.i to an integer.  The value string is matched against the
 * @a strs.  If there's a unique match, the corresponding entry from @a vals
 * is returned.  Unique prefix matches also work.  Finally, if @a flags
 * contains the Clp_AllowNumbers flag, then integers are also accepted.
 *
 * For example:
 * @code
 * const char * const strs[] = { "cat", "cattle", "dog" };
 * const int vals[]          = { 1,     2,        3     };
 * Clp_AddStringListTypeVec(clp, 100, Clp_AllowNumbers, 3, strs, vals);
 * @endcode
 *
 * Say that option <tt>--animal</tt> takes value type 100.  Then:
 *
 * <ul>
 * <li><tt>--animal=cat</tt> will succeed and set @a clp->val.i = 1.</li>
 * <li><tt>--animal=cattle</tt> will succeed and set @a clp->val.i = 2.</li>
 * <li><tt>--animal=dog</tt> will succeed and set @a clp->val.i = 3.</li>
 * <li><tt>--animal=d</tt> will succeed and set @a clp->val.i = 3.</li>
 * <li><tt>--animal=c</tt> will fail, since <tt>c</tt> is ambiguous.</li>
 * <li><tt>--animal=4</tt> will succeed and set @a clp->val.i = 4.</li>
 * </ul>
 *
 * @note The CLP library will not modify the contents of @a strs or @a vals.
 * The calling program can modify the @a strs array, but the actual strings
 * (for instance, @a strs[0] and @a strs[1]) must not be modified or freed
 * until the parser is destroyed.
 */
int
Clp_AddStringListTypeVec(Clp_Parser *clp, int val_type, int flags,
			 int nstrs, const char * const *strs,
			 const int *vals)
     /* An alternate way to make a string list type. See Clp_AddStringListType
	for the basics; this coalesces the strings and values into two arrays,
	rather than spreading them out into a variable argument list. */
{
    int i;
    int itemscap = (nstrs < 5 ? 5 : nstrs);
    Clp_Option *items = (Clp_Option *)malloc(sizeof(Clp_Option) * itemscap);
    if (!items)
	return -1;

    /* copy over items */
    for (i = 0; i < nstrs; i++) {
	items[i].long_name = strs[i];
	items[i].option_id = vals[i];
	items[i].flags = 0;
    }

    if (finish_string_list(clp, val_type, flags, items, nstrs, itemscap) >= 0)
	return 0;
    else {
	free(items);
	return -1;
    }
}


/*******
 * Returning information
 **/

const char *
Clp_ProgramName(Clp_Parser *clp)
{
    return clp->internal->program_name;
}

/** @param clp the parser
 * @param name new program name
 * @return previous program name
 *
 * The calling program should not modify or free @a name until @a clp itself
 * is destroyed. */
const char *
Clp_SetProgramName(Clp_Parser *clp, const char *name)
{
    const char *old = clp->internal->program_name;
    clp->internal->program_name = name;
    return old;
}


/******
 * Clp_ParserStates
 **/

/** @return the parser state
 *
 * A Clp_ParserState object can store a parsing state of a Clp_Parser object.
 * This state specifies exactly how far the Clp_Parser has gotten in parsing
 * an argument list.  The Clp_SaveParser() and Clp_RestoreParser() functions
 * can be used to save this state and then restore it later, allowing a
 * Clp_Parser to switch among argument lists.
 *
 * The initial state is empty, in that after Clp_RestoreParser(clp, state),
 * Clp_Next(clp) would return Clp_Done.
 *
 * Parser states can be saved and restored among different parser objects.
 *
 * @sa Clp_DeleteParserState, Clp_SaveParser, Clp_RestoreParser
 */
Clp_ParserState *
Clp_NewParserState(void)
{
    Clp_ParserState *state = (Clp_ParserState *)malloc(sizeof(Clp_ParserState));
    if (state) {
	state->argv = 0;
	state->argc = 0;
	state->option_chars[0] = 0;
	state->xtext = 0;
	state->option_processing = 0;
	state->opt_generation = 0;
	state->current_option = -1;
	state->is_short = 0;
	state->whole_negated = 0;
	state->current_short = 0;
	state->negated_by_no = 0;
    }
    return state;
}

/** @param state parser state
 *
 * The memory associated with @a state is freed.
 */
void
Clp_DeleteParserState(Clp_ParserState *state)
{
    free(state);
}

/** @param clp the parser
 * @param state parser state
 * @sa Clp_NewParserState, Clp_RestoreParser
 */
void
Clp_SaveParser(const Clp_Parser *clp, Clp_ParserState *state)
{
    Clp_Internal *cli = clp->internal;
    state->argv = cli->argv;
    state->argc = cli->argc;
    memcpy(state->option_chars, cli->option_chars, Clp_OptionCharsSize);
    state->xtext = cli->xtext;

    state->option_processing = cli->option_processing;
    state->opt_generation = cli->opt_generation;
    state->current_option = cli->current_option;
    state->is_short = cli->is_short;
    state->whole_negated = cli->whole_negated;
    state->current_short = cli->current_short;
    state->negated_by_no = cli->negated_by_no;
}


/** @param clp the parser
 * @param state parser state
 *
 * The parser state in @a state is restored into @a clp.  The next call to
 * Clp_Next() will return the same result as it would have at the time @a
 * state was saved (probably by Clp_SaveParser(@a clp, @a state)).
 *
 * A parser state contains information about arguments (argc and argv; see
 * Clp_SetArguments()) and option processing (Clp_SetOptionProcessing()), but
 * not about options (Clp_SetOptions()).  Changes to options and value types
 * are preserved across Clp_RestoreParser().
 *
 * @sa Clp_NewParserState, Clp_SaveParser
 */
void
Clp_RestoreParser(Clp_Parser *clp, const Clp_ParserState *state)
{
    Clp_Internal *cli = clp->internal;
    cli->argv = state->argv;
    cli->argc = state->argc;
    memcpy(cli->option_chars, state->option_chars, Clp_OptionCharsSize);
    cli->xtext = state->xtext;
    cli->option_processing = state->option_processing;
    cli->is_short = state->is_short;
    cli->whole_negated = state->whole_negated;
    cli->current_short = state->current_short;
    cli->negated_by_no = state->negated_by_no;
    if (cli->opt_generation == state->opt_generation)
	cli->current_option = state->current_option;
    else
	cli->current_option = -1;
}


/*******
 * Clp_Next and its helpers
 **/

static void
set_option_text(Clp_Internal *cli, const char *text, int n_option_chars)
{
    assert(n_option_chars < Clp_OptionCharsSize);
    memcpy(cli->option_chars, text, n_option_chars);
    cli->option_chars[n_option_chars] = 0;
    cli->xtext = text + n_option_chars;
}

static int
get_oclass(Clp_Parser *clp, const char *text, int *ocharskip)
{
    int c;
    if (clp->internal->utf8) {
	const char *s;
	c = decode_utf8(text, &s);
	*ocharskip = s - text;
    } else {
	c = (unsigned char) text[0];
	*ocharskip = 1;
    }
    return Clp_OptionChar(clp, c);
}

static int
next_argument(Clp_Parser *clp, int want_argument)
     /* Moves clp to the next argument.
	Returns 1 if it finds another option.
	Returns 0 if there aren't any more arguments.
	Returns 0, sets clp->have_val = 1, and sets clp->vstr to the argument
	if the next argument isn't an option.
	If want_argument > 0, it'll look for an argument.
	want_argument == 1: Accept arguments that start with Clp_NotOption
		or Clp_LongImplicit.
	want_argument == 2: Accept ALL arguments.

	Where is the option stored when this returns?
	Well, cli->argv[0] holds the whole of the next command line argument.
	cli->option_chars holds a string: what characters began the option?
	It is generally "-" or "--".
	cli->text holds the text of the option:
	for short options, cli->text[0] is the relevant character;
	for long options, cli->text holds the rest of the option. */
{
    Clp_Internal *cli = clp->internal;
    const char *text;
    int oclass, ocharskip;

    /* clear relevant flags */
    clp->have_val = 0;
    clp->vstr = 0;
    cli->could_be_short = 0;

    /* if we're in a string of short options, move up one char in the string */
    if (cli->is_short) {
	cli->xtext += clp_utf8_charlen(cli, cli->xtext);
	if (cli->xtext[0] == 0)
	    cli->is_short = 0;
	else if (want_argument > 0) {
	    /* handle -O[=]argument case */
	    clp->have_val = 1;
	    if (cli->xtext[0] == '=')
		clp->vstr = cli->xtext + 1;
	    else
		clp->vstr = cli->xtext;
	    cli->is_short = 0;
	    return 0;
	}
    }

    /* if in short options, we're all set */
    if (cli->is_short)
	return 1;

    /** if not in short options, move to the next argument **/
    cli->whole_negated = 0;
    cli->xtext = 0;

    if (cli->argc <= 1)
	return 0;

    cli->argc--;
    cli->argv++;
    text = cli->argv[0];

    if (want_argument > 1)
	goto not_option;

    if (text[0] == '-' && text[1] == '-') {
	oclass = Clp_DoubledLong;
	ocharskip = 2;
    } else
	oclass = get_oclass(clp, text, &ocharskip);

    /* If this character could introduce either a short or a long option,
       try a long option first, but remember that short's a possibility for
       later. */
    if ((oclass & (Clp_Short | Clp_ShortNegated))
	&& (oclass & (Clp_Long | Clp_LongNegated))) {
	oclass &= ~(Clp_Short | Clp_ShortNegated);
	if (text[ocharskip])
	    cli->could_be_short = 1;
    }

    switch (oclass) {

      case Clp_Short:
	cli->is_short = 1;
	goto check_singleton;

      case Clp_ShortNegated:
	cli->is_short = 1;
	cli->whole_negated = 1;
	goto check_singleton;

      case Clp_Long:
	goto check_singleton;

      case Clp_LongNegated:
	cli->whole_negated = 1;
	goto check_singleton;

      check_singleton:
	/* For options introduced with one character, option-char,
	   '[option-char]' alone is NOT an option. */
	if (!text[ocharskip])
	    goto not_option;
	set_option_text(cli, text, ocharskip);
	break;

      case Clp_LongImplicit:
	/* LongImplict: option_chars == "" (since all chars are part of the
	   option); restore head -> text of option */
	if (want_argument > 0)
	    goto not_option;
	set_option_text(cli, text, 0);
	break;

      case Clp_DoubledLong:
	set_option_text(cli, text, ocharskip);
	break;

      not_option:
      case Clp_NotOption:
	cli->is_short = 0;
	clp->have_val = 1;
	clp->vstr = text;
	return 0;

      default:
	assert(0 /* CLP misconfiguration: bad option type */);

    }

    return 1;
}


static void
switch_to_short_argument(Clp_Parser *clp)
{
    Clp_Internal *cli = clp->internal;
    const char *text = cli->argv[0];
    int ocharskip, oclass = get_oclass(clp, text, &ocharskip);
    assert(cli->could_be_short);
    cli->is_short = 1;
    cli->whole_negated = !!(oclass & Clp_ShortNegated);
    set_option_text(cli, cli->argv[0], ocharskip);
}


static int
find_long(Clp_Parser *clp, const char *arg)
     /* If arg corresponds to one of clp's options, finds that option &
	returns it. If any argument is given after an = sign in arg, sets
	clp->have_val = 1 and clp->vstr to that argument. Sets cli->ambiguous
	to 1 iff there was no match because the argument was ambiguous. */
{
    Clp_Internal *cli = clp->internal;
    int optno, len, lmm;
    const Clp_Option *opt = cli->opt;
    const Clp_InternOption *iopt;
    int first_negative_ambiguous;

    /* Look for a normal option. */
    optno = find_prefix_opt
	(clp, arg, cli->nopt, opt, cli->iopt,
	 &cli->ambiguous, cli->ambiguous_values);
    if (optno >= 0)
	goto worked;

    /* If we can't find it, look for a negated option. */
    /* I know this is silly, but it makes me happy to accept
       --no-no-option as a double negative synonym for --option. :) */
    first_negative_ambiguous = cli->ambiguous;
    while (arg[0] == 'n' && arg[1] == 'o' && arg[2] == '-') {
	arg += 3;
	clp->negated = !clp->negated;
	optno = find_prefix_opt
	    (clp, arg, cli->nopt, opt, cli->iopt,
	     &cli->ambiguous, cli->ambiguous_values);
	if (optno >= 0)
	    goto worked;
    }

    /* No valid option was found; return 0. Mark the ambiguous values found
       through '--no' by making them negative. */
    {
	int i, max = cli->ambiguous;
	if (max > MAX_AMBIGUOUS_VALUES) max = MAX_AMBIGUOUS_VALUES;
	for (i = first_negative_ambiguous; i < max; i++)
	    cli->ambiguous_values[i] = -cli->ambiguous_values[i] - 1;
    }
    return -1;

  worked:
    iopt = &cli->iopt[optno];
    lmm = (clp->negated ? iopt->lmmneg : iopt->lmmpos);
    if (cli->could_be_short
	&& (clp->negated ? iopt->lmmneg_short : iopt->lmmpos_short)) {
	int first_charlen = clp_utf8_charlen(cli, arg);
	lmm = (first_charlen >= lmm ? first_charlen + 1 : lmm);
    }
    len = argcmp(opt[optno].long_name + iopt->ilongoff, arg, lmm, 1);
    assert(len > 0);
    if (arg[len] == '=') {
	clp->have_val = 1;
	clp->vstr = arg + len + 1;
    }
    return optno;
}


static int
find_short(Clp_Parser *clp, const char *text)
     /* If short_name corresponds to one of clp's options, returns it. */
{
    Clp_Internal *cli = clp->internal;
    const Clp_Option *opt = cli->opt;
    const Clp_InternOption *iopt = cli->iopt;
    int i, c;
    if (clp->internal->utf8)
	c = decode_utf8(text, 0);
    else
	c = (unsigned char) *text;

    for (i = 0; i < cli->nopt; i++)
	if (iopt[i].ishort && opt[i].short_name == c
            && (!clp->negated || iopt[i].ineg)) {
            clp->negated = clp->negated || !iopt[i].ipos;
	    return i;
        }

    return -1;
}


/** @param clp the parser
 * @return option ID of next option
 *
 * Parse the next argument from the argument list, store information about
 * that argument in the fields of @a clp, and return the option's ID.
 *
 * If an argument was successfully parsed, that option's ID is returned.
 * Other possible return values are:
 *
 * <dl>
 * <dt>Clp_Done</dt>
 * <dd>There are no more arguments.</dd>
 * <dt>Clp_NotOption</dt>
 * <dd>The next argument was not an option.  The argument's text is @a
 * clp->vstr (and @a clp->val.s).</dd>
 * <dt>Clp_BadOption</dt>
 * <dd>The next argument was a bad option: either an option that wasn't
 * understood, or an option lacking a required value, or an option whose value
 * couldn't be parsed.  The option has been skipped.</dd>
 * <dt>Clp_Error</dt>
 * <dd>There was an internal error.  This should never occur unless a user
 * messes with, for example, a Clp_Option array.</dd>
 * </dl>
 *
 * The fields of @a clp are set as follows.
 *
 * <dl>
 * <dt><tt>negated</tt></dt>
 * <dd>1 if the option was negated, 0 if it wasn't.</dd>
 * <dt><tt>have_val</tt></dt>
 * <dd>1 if the option had a value, 0 if it didn't.  Note that negated options
 * are not allowed to have values.</dd>
 * <dt><tt>vstr</tt></dt>
 * <dd>The value string, if any.  NULL if there was no value.</dd>
 * <dt><tt>val</tt></dt>
 * <dd>An option's value type will parse the value string into this
 * union.</dd>
 * </dl>
 *
 * The parsed argument is shifted off the argument list, so that sequential
 * calls to Clp_Next() step through the arugment list.
 */
int
Clp_Next(Clp_Parser *clp)
{
    Clp_Internal *cli = clp->internal;
    int optno;
    const Clp_Option *opt;
    Clp_ParserState clpsave;
    int vtpos, complain;

    /* Set up clp */
    cli->current_option = -1;
    cli->ambiguous = 0;

    /* Get the next argument or option */
    if (!next_argument(clp, cli->option_processing ? 0 : 2)) {
	clp->val.s = clp->vstr;
	optno = clp->have_val ? Clp_NotOption : Clp_Done;
	clp->option = &clp_option_sentinel[-optno];
	return optno;
    }

    clp->negated = cli->whole_negated;
    if (cli->is_short)
	optno = find_short(clp, cli->xtext);
    else
	optno = find_long(clp, cli->xtext);

    /* If there's ambiguity between long & short options, and we couldn't
       find a long option, look for a short option */
    if (optno < 0 && cli->could_be_short) {
	switch_to_short_argument(clp);
	optno = find_short(clp, cli->xtext);
    }

    /* If we didn't find an option... */
    if (optno < 0 || (clp->negated && !cli->iopt[optno].ineg)) {
	/* default processing for the "--" option: turn off option processing
	   and return the next argument */
	if (strcmp(cli->argv[0], "--") == 0) {
	    Clp_SetOptionProcessing(clp, 0);
	    return Clp_Next(clp);
	}

	/* otherwise, report some error or other */
	if (cli->ambiguous)
	    ambiguity_error(clp, cli->ambiguous, cli->ambiguous_values,
			    cli->opt, cli->iopt, cli->option_chars,
			    "option %<%s%s%> is ambiguous",
			    cli->option_chars, cli->xtext);
	else if (cli->is_short && !cli->could_be_short)
	    Clp_OptionError(clp, "unrecognized option %<%s%C%>",
			    cli->option_chars, cli->xtext);
	else
	    Clp_OptionError(clp, "unrecognized option %<%s%s%>",
			    cli->option_chars, cli->xtext);

	clp->option = &clp_option_sentinel[-Clp_BadOption];
	return Clp_BadOption;
    }

    /* Set the current option */
    cli->current_option = optno;
    cli->current_short = cli->is_short;
    cli->negated_by_no = clp->negated && !cli->whole_negated;

    /* The no-argument (or should-have-no-argument) case */
    if (clp->negated
	|| (!cli->iopt[optno].imandatory && !cli->iopt[optno].ioptional)) {
	if (clp->have_val) {
	    Clp_OptionError(clp, "%<%O%> can%,t take an argument");
	    clp->option = &clp_option_sentinel[-Clp_BadOption];
	    return Clp_BadOption;
	} else {
	    clp->option = &cli->opt[optno];
	    return cli->opt[optno].option_id;
	}
    }

    /* Get an argument if we need one, or if it's optional */
    /* Sanity-check the argument type. */
    opt = &cli->opt[optno];
    if (opt->val_type <= 0) {
	clp->option = &clp_option_sentinel[-Clp_Error];
	return Clp_Error;
    }
    vtpos = val_type_binsearch(cli, opt->val_type);
    if (vtpos == cli->nvaltype
	|| cli->valtype[vtpos].val_type != opt->val_type) {
	clp->option = &clp_option_sentinel[-Clp_Error];
	return Clp_Error;
    }

    /* complain == 1 only if the argument was explicitly given,
       or it is mandatory. */
    complain = (clp->have_val != 0) || cli->iopt[optno].imandatory;
    Clp_SaveParser(clp, &clpsave);

    if (cli->iopt[optno].imandatory && !clp->have_val) {
	/* Mandatory argument case */
	/* Allow arguments to options to start with a dash, but only if the
	   argument type allows it by not setting Clp_DisallowOptions */
	int disallow = (cli->valtype[vtpos].flags & Clp_DisallowOptions) != 0;
	next_argument(clp, disallow ? 1 : 2);
	if (!clp->have_val) {
	    int got_option = cli->xtext != 0;
	    Clp_RestoreParser(clp, &clpsave);
	    if (got_option)
		Clp_OptionError(clp, "%<%O%> requires a non-option argument");
	    else
		Clp_OptionError(clp, "%<%O%> requires an argument");
	    clp->option = &clp_option_sentinel[-Clp_BadOption];
	    return Clp_BadOption;
	}

    } else if (cli->is_short && !clp->have_val
	       && cli->xtext[clp_utf8_charlen(cli, cli->xtext)])
	/* The -[option]argument case:
	   Assume that the rest of the current string is the argument. */
	next_argument(clp, 1);

    /* Parse the argument */
    clp->option = opt;
    if (clp->have_val) {
	Clp_ValType *atr = &cli->valtype[vtpos];
	if (atr->func(clp, clp->vstr, complain, atr->user_data) <= 0) {
	    /* parser failed */
	    clp->have_val = 0;
	    if (complain) {
		clp->option = &clp_option_sentinel[-Clp_BadOption];
		return Clp_BadOption;
	    } else {
		Clp_RestoreParser(clp, &clpsave);
                clp->option = opt;
            }
	}
    }

    return opt->option_id;
}


/** @param clp the parser
 * @param allow_options whether options will be allowed
 *
 * Remove and return the next argument from @a clp's argument array.  If there
 * are no arguments left, or if the next argument is an option and @a
 * allow_options != 0, then returns null.
 */
const char *
Clp_Shift(Clp_Parser *clp, int allow_options)
     /* Returns the next argument from the argument list without parsing it.
        If there are no more arguments, returns 0. */
{
    Clp_ParserState clpsave;
    Clp_SaveParser(clp, &clpsave);
    next_argument(clp, allow_options ? 2 : 1);
    if (!clp->have_val)
	Clp_RestoreParser(clp, &clpsave);
    return clp->vstr;
}


/*******
 * Clp_OptionError
 **/

typedef struct Clp_BuildString {
    char* data;
    char* pos;
    char* end_data;
    char buf[256];
} Clp_BuildString;

static void build_string_program_prefix(Clp_BuildString* bs,
                                        const Clp_Parser* clp);

static void build_string_init(Clp_BuildString* bs, Clp_Parser* clp) {
    bs->data = bs->pos = bs->buf;
    bs->end_data = &bs->buf[sizeof(bs->buf)];
    if (clp)
        build_string_program_prefix(bs, clp);
}

static void build_string_cleanup(Clp_BuildString* bs) {
    if (bs->data != bs->buf)
        free(bs->data);
}

static int build_string_grow(Clp_BuildString* bs, size_t want) {
    size_t ipos = bs->pos - bs->data, ncap;
    if (!bs->pos)
        return 0;
    for (ncap = (bs->end_data - bs->data) << 1; ncap < want; ncap *= 2)
        /* nada */;
    if (bs->data == bs->buf) {
        if ((bs->data = (char*) malloc(ncap)))
            memcpy(bs->data, bs->buf, bs->pos - bs->buf);
    } else
        bs->data = (char*) realloc(bs->data, ncap);
    if (!bs->data) {
        bs->pos = bs->end_data = bs->data;
        return 0;
    } else {
        bs->pos = bs->data + ipos;
        bs->end_data = bs->data + ncap;
        return 1;
    }
}

#define ENSURE_BUILD_STRING(bs, space)                                  \
    ((((bs)->end_data - (bs)->pos) >= (space))                          \
     || build_string_grow((bs), (bs)->pos - (bs)->data + (space)))

static void
append_build_string(Clp_BuildString *bs, const char *s, int l)
{
    if (l < 0)
	l = strlen(s);
    if (ENSURE_BUILD_STRING(bs, l)) {
	memcpy(bs->pos, s, l);
	bs->pos += l;
    }
}

static void
build_string_program_prefix(Clp_BuildString* bs, const Clp_Parser* clp)
{
    const Clp_Internal* cli = clp->internal;
    if (cli->program_name && cli->program_name[0]) {
	append_build_string(bs, cli->program_name, -1);
	append_build_string(bs, ": ", 2);
    }
}


static void
Clp_vbsprintf(Clp_Parser *clp, Clp_BuildString *bs,
              const char *fmt, va_list val)
{
    Clp_Internal *cli = clp->internal;
    const char *percent;
    int c;

    for (percent = strchr(fmt, '%'); percent; percent = strchr(fmt, '%')) {
	append_build_string(bs, fmt, percent - fmt);
	switch (*++percent) {

	  case 's': {
	      const char *s = va_arg(val, const char *);
              append_build_string(bs, s ? s : "(null)", -1);
	      break;
	  }

	  case 'C': {
	      const char *s = va_arg(val, const char *);
	      if (cli->utf8)
		  c = decode_utf8(s, 0);
	      else
		  c = (unsigned char) *s;
	      goto char_c;
	  }

	  case 'c':
	    c = va_arg(val, int);
	    goto char_c;

	  char_c:
	    if (ENSURE_BUILD_STRING(bs, 4)) {
		if (c >= 32 && c <= 126)
		    *bs->pos++ = c;
		else if (c < 32) {
		    *bs->pos++ = '^';
		    *bs->pos++ = c + 64;
		} else if (cli->utf8 && c >= 127 && c < 0x110000) {
		    bs->pos = encode_utf8(bs->pos, 4, c);
		} else if (c >= 127 && c <= 255) {
		    sprintf(bs->pos, "\\%03o", c & 0xFF);
		    bs->pos += 4;
		} else {
		    *bs->pos++ = '\\';
		    *bs->pos++ = '?';
		}
	    }
	    break;

	  case 'd': {
	      int d = va_arg(val, int);
	      if (ENSURE_BUILD_STRING(bs, 32)) {
		  sprintf(bs->pos, "%d", d);
		  bs->pos = strchr(bs->pos, 0);
	      }
	      break;
	  }

        case 'O':
        case 'V': {
            int optno = cli->current_option;
            const Clp_Option *opt = &cli->opt[optno];
            if (optno < 0)
                append_build_string(bs, "(no current option!)", -1);
            else if (cli->current_short) {
                append_build_string(bs, cli->option_chars, -1);
                if (ENSURE_BUILD_STRING(bs, 5)) {
                    if (cli->utf8)
                        bs->pos = encode_utf8(bs->pos, 5, opt->short_name);
                    else
                        *bs->pos++ = opt->short_name;
                }
            } else if (cli->negated_by_no) {
                append_build_string(bs, cli->option_chars, -1);
                append_build_string(bs, "no-", 3);
                append_build_string(bs, opt->long_name + cli->iopt[optno].ilongoff, -1);
            } else {
                append_build_string(bs, cli->option_chars, -1);
                append_build_string(bs, opt->long_name + cli->iopt[optno].ilongoff, -1);
            }
            if (optno >= 0 && clp->have_val && *percent == 'V') {
                if (cli->current_short && !cli->iopt[optno].ioptional)
                    append_build_string(bs, " ", 1);
                else if (!cli->current_short)
                    append_build_string(bs, "=", 1);
                append_build_string(bs, clp->vstr, -1);
            }
            break;
        }

	  case '%':
	    if (ENSURE_BUILD_STRING(bs, 1))
		*bs->pos++ = '%';
	    break;

	  case '<':
	    append_build_string(bs, (cli->utf8 ? "\342\200\230" : "'"), -1);
	    break;

	  case ',':
	  case '>':
	    append_build_string(bs, (cli->utf8 ? "\342\200\231" : "'"), -1);
	    break;

        case 0:
            append_build_string(bs, "%", 1);
            goto done;

	  default:
	    if (ENSURE_BUILD_STRING(bs, 2)) {
		*bs->pos++ = '%';
		*bs->pos++ = *percent;
	    }
	    break;

	}
	fmt = ++percent;
    }

 done:
    append_build_string(bs, fmt, -1);
}

static const char* build_string_text(Clp_BuildString* bs, int report_oom) {
    if (bs->pos) {
        *bs->pos = 0;
        return bs->data;
    } else if (report_oom)
        return "out of memory\n";
    else
        return NULL;
}

static void
do_error(Clp_Parser *clp, Clp_BuildString *bs)
{
    const char *text = build_string_text(bs, 1);
    if (clp->internal->error_handler != 0)
	(*clp->internal->error_handler)(clp, text);
    else
	fputs(text, stderr);
}

/** @param clp the parser
 * @param format error format
 *
 * Format an error message from @a format and any additional arguments in
 * the ellipsis. The resulting error string is then printed to standard
 * error (or passed to the error handler specified by Clp_SetErrorHandler).
 * Returns the number of characters printed.
 *
 * The following format characters are accepted:
 *
 * <dl>
 * <dt><tt>%</tt><tt>c</tt></dt>
 * <dd>A character (type <tt>int</tt>).  Control characters are printed in
 * caret notation.  If the parser is in UTF-8 mode, the character is formatted
 * in UTF-8.  Otherwise, special characters are printed with backslashes and
 * octal notation.</dd>
 * <dt><tt>%</tt><tt>s</tt></dt>
 * <dd>A string (type <tt>const char *</tt>).</dd>
 * <dt><tt>%</tt><tt>C</tt></dt>
 * <dd>The argument is a string (type <tt>const char *</tt>).  The first
 * character in this string is printed.  If the parser is in UTF-8 mode, this
 * may involve multiple bytes.</dd>
 * <dt><tt>%</tt><tt>d</tt></dt>
 * <dd>An integer (type <tt>int</tt>).  Printed in decimal.</dd>
 * <dt><tt>%</tt><tt>O</tt></dt>
 * <dd>The current option.  No values are read from the argument list; the
 * current option is defined in the Clp_Parser object itself.</dd>
 * <dt><tt>%</tt><tt>V</tt></dt>
 * <dd>Like <tt>%</tt><tt>O</tt>, but also includes the current value,
 * if any.</dd>
 * <dt><tt>%%</tt></dt>
 * <dd>Prints a percent character.</dd>
 * <dt><tt>%</tt><tt>&lt;</tt></dt>
 * <dd>Prints an open quote string.  In UTF-8 mode, prints a left single
 * quote.  Otherwise prints a single quote.</dd>
 * <dt><tt>%</tt><tt>&gt;</tt></dt>
 * <dd>Prints a closing quote string.  In UTF-8 mode, prints a right single
 * quote.  Otherwise prints a single quote.</dd>
 * <dt><tt>%</tt><tt>,</tt></dt>
 * <dd>Prints an apostrophe.  In UTF-8 mode, prints a right single quote.
 * Otherwise prints a single quote.</dd>
 * </dl>
 *
 * Note that no flag characters, precision, or field width characters are
 * currently supported.
 *
 * @sa Clp_SetErrorHandler
 */
int
Clp_OptionError(Clp_Parser *clp, const char *format, ...)
{
    Clp_BuildString bs;
    va_list val;
    va_start(val, format);
    build_string_init(&bs, clp);
    Clp_vbsprintf(clp, &bs, format, val);
    append_build_string(&bs, "\n", 1);
    va_end(val);
    do_error(clp, &bs);
    build_string_cleanup(&bs);
    return bs.pos - bs.data;
}

/** @param clp the parser
 * @param f output file
 * @param format error format
 *
 * Format an error message using @a format and additional arguments in the
 * ellipsis, according to the Clp_OptionError formatting conventions. The
 * resulting message is written to @a f.
 *
 * @sa Clp_OptionError */
int
Clp_fprintf(Clp_Parser* clp, FILE* f, const char* format, ...)
{
    Clp_BuildString bs;
    va_list val;
    va_start(val, format);
    build_string_init(&bs, NULL);
    Clp_vbsprintf(clp, &bs, format, val);
    va_end(val);
    if (bs.pos != bs.data)
        fwrite(bs.data, 1, bs.pos - bs.data, f);
    build_string_cleanup(&bs);
    return bs.pos - bs.data;
}

/** @param clp the parser
 * @param f output file
 * @param format error format
 * @param val arguments
 *
 * Format an error message using @a format and @a val, according to the
 * Clp_OptionError formatting conventions. The resulting message is written
 * to @a f.
 *
 * @sa Clp_OptionError */
int
Clp_vfprintf(Clp_Parser* clp, FILE* f, const char* format, va_list val)
{
    Clp_BuildString bs;
    build_string_init(&bs, NULL);
    Clp_vbsprintf(clp, &bs, format, val);
    if (bs.pos != bs.data)
        fwrite(bs.data, 1, bs.pos - bs.data, f);
    build_string_cleanup(&bs);
    return bs.pos - bs.data;
}

/** @param clp the parser
 * @param str output string
 * @param size size of output string
 * @param format error format
 *
 * Format an error message from @a format and any additional arguments in
 * the ellipsis, according to the Clp_OptionError formatting conventions.
 * The resulting string is written to @a str. At most @a size characters are
 * written to @a str, including a terminating null byte. The return value is
 * the number of characters that would have been written (excluding the
 * terminating null byte) if @a size were large enough to contain the entire
 * string.
 *
 * @sa Clp_OptionError */
int
Clp_vsnprintf(Clp_Parser* clp, char* str, size_t size,
              const char* format, va_list val)
{
    Clp_BuildString bs;
    build_string_init(&bs, NULL);
    Clp_vbsprintf(clp, &bs, format, val);
    if ((size_t) (bs.pos - bs.data) < size) {
        memcpy(str, bs.data, bs.pos - bs.data);
        str[bs.pos - bs.data] = 0;
    } else {
        memcpy(str, bs.data, size - 1);
        str[size - 1] = 0;
    }
    build_string_cleanup(&bs);
    return bs.pos - bs.data;
}

static int
ambiguity_error(Clp_Parser *clp, int ambiguous, int *ambiguous_values,
		const Clp_Option *opt, const Clp_InternOption *iopt,
		const char *prefix, const char *fmt, ...)
{
    Clp_Internal *cli = clp->internal;
    Clp_BuildString bs;
    int i;
    va_list val;

    va_start(val, fmt);
    build_string_init(&bs, clp);
    Clp_vbsprintf(clp, &bs, fmt, val);
    append_build_string(&bs, "\n", 1);

    build_string_program_prefix(&bs, clp);
    append_build_string(&bs, "(Possibilities are", -1);

    for (i = 0; i < ambiguous && i < MAX_AMBIGUOUS_VALUES; i++) {
	int value = ambiguous_values[i];
	const char *no_dash = "";
	if (value < 0)
	    value = -(value + 1), no_dash = "no-";
	if (i == 0)
	    append_build_string(&bs, " ", 1);
	else if (i == ambiguous - 1)
	    append_build_string(&bs, (i == 1 ? " and " : ", and "), -1);
	else
	    append_build_string(&bs, ", ", 2);
	append_build_string(&bs, (cli->utf8 ? "\342\200\230" : "'"), -1);
	append_build_string(&bs, prefix, -1);
	append_build_string(&bs, no_dash, -1);
	append_build_string(&bs, opt[value].long_name + iopt[value].ilongoff, -1);
	append_build_string(&bs, (cli->utf8 ? "\342\200\231" : "'"), -1);
    }

    if (ambiguous > MAX_AMBIGUOUS_VALUES)
	append_build_string(&bs, ", and others", -1);
    append_build_string(&bs, ".)\n", -1);
    va_end(val);

    do_error(clp, &bs);
    build_string_cleanup(&bs);
    return 0;
}

static int
copy_string(char *buf, int buflen, int bufpos, const char *what)
{
    int l = strlen(what);
    if (l > buflen - bufpos - 1)
	l = buflen - bufpos - 1;
    memcpy(buf + bufpos, what, l);
    return l;
}

/** @param clp the parser
 * @param buf output buffer
 * @param len length of output buffer
 * @return number of characters written to the buffer, not including the terminating NUL
 *
 * A string that looks like the last option parsed by @a clp is extracted into
 * @a buf.  The correct option characters are put into the string first,
 * followed by the option text.  The output buffer is null-terminated unless
 * @a len == 0.
 *
 * @sa Clp_CurOptionName
 */
int
Clp_CurOptionNameBuf(Clp_Parser *clp, char *buf, int len)
{
    Clp_Internal *cli = clp->internal;
    int optno = cli->current_option;
    int pos = 0;
    if (optno < 0)
	pos += copy_string(buf, len, pos, "(no current option!)");
    else if (cli->current_short) {
	pos += copy_string(buf, len, pos, cli->option_chars);
	if (cli->utf8)
	    pos = (encode_utf8(buf + pos, len - pos - 1, cli->opt[optno].short_name) - buf);
	else if (pos < len - 1)
	    buf[pos++] = cli->opt[optno].short_name;
    } else if (cli->negated_by_no) {
	pos += copy_string(buf, len, pos, cli->option_chars);
	pos += copy_string(buf, len, pos, "no-");
	pos += copy_string(buf, len, pos, cli->opt[optno].long_name + cli->iopt[optno].ilongoff);
    } else {
	pos += copy_string(buf, len, pos, cli->option_chars);
	pos += copy_string(buf, len, pos, cli->opt[optno].long_name + cli->iopt[optno].ilongoff);
    }
    if (pos < len)
	buf[pos] = 0;
    return pos;
}

/** @param clp the parser
 * @return string describing the current option
 *
 * This function acts like Clp_CurOptionNameBuf(), but returns a pointer into
 * a static buffer that will be rewritten on the next call to
 * Clp_CurOptionName().
 *
 * @note This function is not thread safe.
 *
 * @sa Clp_CurOptionName
 */
const char *
Clp_CurOptionName(Clp_Parser *clp)
{
    static char buf[256];
    Clp_CurOptionNameBuf(clp, buf, 256);
    return buf;
}

int
Clp_IsLong(Clp_Parser *clp, const char *long_name)
{
    Clp_Internal *cli = clp->internal;
    int optno = cli->current_option;
    return optno >= 0 && strcmp(cli->opt[optno].long_name, long_name) == 0;
}

int
Clp_IsShort(Clp_Parser *clp, int short_name)
{
    Clp_Internal *cli = clp->internal;
    int optno = cli->current_option;
    return optno >= 0 && cli->opt[optno].short_name == short_name;
}

#ifdef __cplusplus
}
#endif
