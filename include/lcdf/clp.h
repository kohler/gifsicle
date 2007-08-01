#ifndef CLP_H
#define CLP_H
#ifdef __cplusplus
extern "C" {
#endif

/* clp.h - Public interface to CLP.
 * This file is part of CLP, the command line parser package.
 *
 * Copyright (c) 1997-2006 Eddie Kohler, kohler@icir.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file, which is available in full at
 * http://www.pdos.lcs.mit.edu/click/license.html. The conditions include: you
 * must preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding. */


/* Argument types */
#define Clp_NoArg		0
#define Clp_ArgString		1
#define Clp_ArgStringNotOption	2
#define Clp_ArgBool		3
#define Clp_ArgInt		4
#define Clp_ArgUnsigned		5
#define Clp_ArgDouble		6

#define Clp_MaxDefaultType	Clp_ArgDouble

/* Argument type flags */
#define Clp_DisallowOptions	(1<<0)

/* Flags for individual Clp_Options */
#define Clp_Mandatory		(1<<0)
#define Clp_Optional		(1<<1)
#define Clp_Negate		(1<<2)
#define Clp_AllowDash		(1<<3)
#define Clp_OnlyNegated		(1<<4)

/* Option types for Clp_SetOptionChar */
/*	Clp_NotOption		0 */
#define Clp_Short		(1<<0)
#define Clp_Long		(1<<1)
#define Clp_ShortNegated	(1<<2)
#define Clp_LongNegated		(1<<3)
#define Clp_LongImplicit	(1<<4)

/* Flags for Clp_AddStringListType */
#define Clp_AllowNumbers	(1<<0)

/* Return values from Clp_Next */
#define Clp_NotOption		0
#define Clp_Done		-1
#define Clp_BadOption		-2
#define Clp_Error		-3

/* Sizes of clp->val */
#define Clp_ValSize		40
#define Clp_ValIntSize		10


typedef struct Clp_Option Clp_Option;
typedef struct Clp_Parser Clp_Parser;
typedef struct Clp_Internal Clp_Internal;
typedef struct Clp_ParserState Clp_ParserState;

typedef int (*Clp_ArgParseFunc)(Clp_Parser *, const char *, int, void *);
typedef void (*Clp_ErrorHandler)(const char *);


struct Clp_Option {
  
  const char *long_name;
  int short_name;
  
  int option_id;
  
  int arg_type;
  int flags;
  
};


struct Clp_Parser {
  
  int negated;
  
  int have_arg;
  const char *arg;
  
  union {
    int i;
    unsigned u;
    double d;
    const char *s;
    void *pv;
#ifdef HAVE_INT64_TYPES
    int64_t i64;
    uint64_t u64;
#endif
    char cs[Clp_ValSize];
    unsigned char ucs[Clp_ValSize];
    int is[Clp_ValIntSize];
    unsigned us[Clp_ValIntSize];
  } val;
  
  Clp_Internal *internal;
  
};


Clp_Parser *	Clp_NewParser(int argc, char * const argv[],
			      int nopt, Clp_Option *opt);
void		Clp_DeleteParser(Clp_Parser *);

Clp_ErrorHandler Clp_SetErrorHandler(Clp_Parser *, Clp_ErrorHandler);
int		Clp_SetOptionChar(Clp_Parser *, int c, int option_type);

int		Clp_AddType
			(Clp_Parser *, int type_id, int flags,
			 Clp_ArgParseFunc func, void *user_data);
int		Clp_AddStringListType
			(Clp_Parser *, int type_id, int flags, ...);
int		Clp_AddStringListTypeVec
			(Clp_Parser *, int type_id, int flags,
			 int n, char **str, int *val);

const char *	Clp_ProgramName(Clp_Parser *);

int		Clp_Next(Clp_Parser *);
const char *	Clp_Shift(Clp_Parser *, int allow_dashes);
int		Clp_SetOptionProcessing(Clp_Parser *, int option_processing);

Clp_ParserState *Clp_NewParserState(void);
void		Clp_DeleteParserState(Clp_ParserState *);
void		Clp_SaveParser(Clp_Parser *, Clp_ParserState *);
void		Clp_RestoreParser(Clp_Parser *, Clp_ParserState *);

int		Clp_OptionError(Clp_Parser *, const char *, ...);
int		Clp_CurOptionNameBuf(Clp_Parser *, char *buf, int buflen);
const char *	Clp_CurOptionName(Clp_Parser *); /* uses static memory */

#ifdef __cplusplus
}
#endif
#endif
