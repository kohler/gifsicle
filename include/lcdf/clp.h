#ifndef CLP_H
#define CLP_H
#ifdef __cplusplus
extern "C" {
#endif

/* clp.h - Public interface to CLP.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of CLP, the command line parser package.

   CLP is free software. It is distributed under the GNU Public License,
   version 2 or later; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */


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
#define Clp_LongMinMatch	(1<<3)
#define Clp_AllowDash		(1<<4)

/* Option types for Clp_SetOptionChar */
/*	Clp_NotOption		0 */
#define Clp_Short		1
#define Clp_Long		2
#define Clp_ShortNegated	4
#define Clp_LongNegated		8
#define Clp_LongImplicit	16

/* Flags for Clp_AddStringListType */
#define Clp_AllowNumbers	(1<<0)
  
/* Return values from Clp_Next */
#define Clp_NotOption		0
#define Clp_Done		-1
#define Clp_BadOption		-2
#define Clp_Error		-3


typedef struct Clp_Option Clp_Option;
typedef struct Clp_Parser Clp_Parser;
typedef struct Clp_Internal Clp_Internal;
typedef struct Clp_ParserState Clp_ParserState;

typedef int (*Clp_ArgParseFunc)(Clp_Parser *, const char *, int, void *);


struct Clp_Option {
  
  char *long_name;
  int short_name;
  
  int option_id;
  
  int arg_type;
  int flags;
  
  int long_min_match;
  
};


struct Clp_Parser {
  
  Clp_Internal *internal;
  
  int negated;
  
  int hadarg;
  char *arg;
  
  union {
    int i;
    unsigned u;
    double d;
    char *s;
    void *pv;
  } val;
  
};


Clp_Parser *	Clp_NewParser(int ac, char **av, int, Clp_Option *);

void		Clp_SetOptionProcessing(Clp_Parser *, int option_processing);
void		Clp_SetErrorHook(Clp_Parser *, void (*hook)(void));
void		Clp_SetOptionChar(Clp_Parser *, int c, int option_type);

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
char *		Clp_Shift(Clp_Parser *, int allow_dashes);

Clp_ParserState *Clp_NewParserState(void);
void		Clp_DeleteParserState(Clp_ParserState *);
void		Clp_SaveParser(Clp_Parser *, Clp_ParserState *);
void		Clp_RestoreParser(Clp_Parser *, Clp_ParserState *);
  
int		Clp_OptionError(Clp_Parser *, const char *, ...);

#ifdef __cplusplus
}
#endif
#endif
