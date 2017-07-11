/* -*- related-file-name: "../../liblcdf/clp.c" -*- */
#ifndef LCDF_CLP_H
#define LCDF_CLP_H
#ifdef __cplusplus
extern "C" {
#endif

/* clp.h - Public interface to CLP.
 * This file is part of CLP, the command line parser package.
 *
 * Copyright (c) 1997-2017 Eddie Kohler, ekohler@gmail.com
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

#include <stdio.h>
#include <stdarg.h>

typedef struct Clp_Option Clp_Option;
typedef struct Clp_Parser Clp_Parser;
typedef struct Clp_ParserState Clp_ParserState;


/** @brief Option description.
 *
 * CLP users declare arrays of Clp_Option structures to specify what options
 * should be parsed.
 * @sa Clp_NewParser, Clp_SetOptions */
struct Clp_Option {
    const char *long_name;	/**< Name of long option, or NULL if the option
				     has no long name. */
    int short_name;		/**< Character defining short option, or 0 if
				     the option has no short name. */
    int option_id;		/**< User-specified ID defining option,
				     returned by Clp_Next. */
    int val_type;		/**< ID of option's value type, or 0 if option
				     takes no value. */
    int flags;			/**< Option parsing flags. */
};

/** @name Value types
 * These values describe the type of an option's argument and are used in
 * the Clp_Option val_type field.  For example, if an option took integers, its
 * Clp_Option structure would have val_type set to Clp_ValInt. */
/**@{*/
#define Clp_NoVal		0	/**< @brief Option takes no value. */
#define Clp_ValString		1	/**< @brief Option value is an
					     arbitrary string. */
#define Clp_ValStringNotOption	2	/**< @brief Option value is a
					     non-option string.

		See Clp_DisallowOptions. */
#define Clp_ValBool		3	/**< @brief Option value is a
					     boolean.

		Accepts "true", "false", "yes", "no", "1", and "0", or any
		prefixes thereof.  The match is case-insensitive. */
#define Clp_ValInt		4	/**< @brief Option value is a
					     signed int.

		Accepts an optional "+" or "-" sign, followed by one or more
		digits.  The digits may be include a "0x" or "0X" prefix, for
		a hexadecimal number, or a "0" prefix, for an octal number;
		otherwise it is decimal. */
#define Clp_ValUnsigned		5	/**< @brief Option value is an
					     unsigned int.

		Accepts an optional "+" sign, followed by one or more
		digits.  The digits may be include a "0x" or "0X" prefix, for
		a hexadecimal number, or a "0" prefix, for an octal number;
		otherwise it is decimal. */
#define Clp_ValLong             6       /**< @brief Option value is a
                                             signed long. */
#define Clp_ValUnsignedLong     7       /**< @brief Option value is an
                                             unsigned long. */
#define Clp_ValDouble		8	/**< @brief Option value is a
					     double.
		Accepts a real number as defined by strtod(). */
#define Clp_ValFirstUser	10	/**< @brief Value types >=
					     Clp_ValFirstUser are available
					     for user types. */
/**@}*/

/** @name Option flags
 * These flags are used in the Clp_Option flags field. */
/**@{*/
#define Clp_Mandatory		(1<<0)	/**< @brief Option flag: value
					     is mandatory.

		It is an error if the option has no value.  This is the
		default if an option has arg_type != 0 and the Clp_Optional
		flag is not provided. */
#define Clp_Optional		(1<<1)	/**< @brief Option flag: value
					     is optional. */
#define Clp_Negate		(1<<2)	/**< @brief Option flag: option
					     may be negated.

		--no-[long_name] will be accepted in argument lists. */
#define Clp_OnlyNegated		(1<<3)	/**< @brief Option flag: option
					     <em>must</em> be negated.

		--no-[long_name] will be accepted in argument lists, but
		--[long_name] will not.  This is the default if long_name
		begins with "no-". */
#define Clp_PreferredMatch	(1<<4)	/**< @brief Option flag: prefer this
					     option when matching.

		Prefixes of --[long_name] should map to this option, even if
		other options begin with --[long_name]. */
/**@}*/

/** @name Option character types
 * These flags are used in to define character types in Clp_SetOptionChar(). */
/**@{*/
/*	Clp_NotOption		0 */
#define Clp_Short		(1<<0)	/**< @brief Option character begins
					     a set of short options. */
#define Clp_Long		(1<<1)	/**< @brief Option character begins
					     a long option. */
#define Clp_ShortNegated	(1<<2)	/**< @brief Option character begins
					     a set of negated short options. */
#define Clp_LongNegated		(1<<3)	/**< @brief Option character begins
					     a negated long option. */
#define Clp_LongImplicit	(1<<4)	/**< @brief Option character can begin
					     a long option, and is part of that
					     long option. */
/**@}*/

#define Clp_NotOption		0	/**< @brief Clp_Next value: argument
					     was not an option. */
#define Clp_Done		-1	/**< @brief Clp_Next value: there are
					     no more arguments. */
#define Clp_BadOption		-2	/**< @brief Clp_Next value: argument
					     was an erroneous option. */
#define Clp_Error		-3	/**< @brief Clp_Next value: internal
					     CLP error. */

#define Clp_ValSize		40	/**< @brief Minimum size of the
					     Clp_Parser val.cs field. */
#define Clp_ValIntSize		10	/**< @brief Minimum size of the
					     Clp_Parser val.is field. */


/** @brief A value parsing function.
 * @param clp the parser
 * @param vstr the value to be parsed
 * @param complain if nonzero, report error messages via Clp_OptionError
 * @param user_data user data passed to Clp_AddType()
 * @return 1 if parsing succeeded, 0 otherwise
 */
typedef int (*Clp_ValParseFunc)(Clp_Parser *clp, const char *vstr,
				int complain, void *user_data);

/** @brief A function for reporting option errors.
 * @param clp the parser
 * @param message error message
 */
typedef void (*Clp_ErrorHandler)(Clp_Parser *clp, const char *message);


/** @brief Command line parser.
 *
 * A Clp_Parser object defines an instance of CLP, including allowed options,
 * value types, and current arguments.
 * @sa Clp_NewParser, Clp_SetOptions, Clp_SetArguments */
struct Clp_Parser {
    const Clp_Option *option;	/**< The last option. */

    int negated;		/**< Whether the last option was negated. */

    int have_val;		/**< Whether the last option had a value. */
    const char *vstr;		/**< The string value provided with the last
				     option. */

    union {
	int i;
	unsigned u;
        long l;
        unsigned long ul;
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
    } val;			/**< The parsed value provided with the last
				     option. */

    void *user_data;		/**< Uninterpreted by CLP; users can set
				     arbitrarily. */

    struct Clp_Internal *internal;
};

/** @cond never */
#if __GNUC__ >= 4
# define CLP_SENTINEL		__attribute__((sentinel))
#else
# define CLP_SENTINEL		/* nothing */
#endif
/** @endcond never */


/** @brief Create a new Clp_Parser. */
Clp_Parser *Clp_NewParser(int argc, const char * const *argv,
			  int nopt, const Clp_Option *opt);

/** @brief Destroy a Clp_Parser object. */
void Clp_DeleteParser(Clp_Parser *clp);


/** @brief Return @a clp's program name. */
const char *Clp_ProgramName(Clp_Parser *clp);

/** @brief Set @a clp's program name. */
const char *Clp_SetProgramName(Clp_Parser *clp, const char *name);


/** @brief Set @a clp's error handler function. */
Clp_ErrorHandler Clp_SetErrorHandler(Clp_Parser *clp, Clp_ErrorHandler errh);

/** @brief Set @a clp's UTF-8 mode. */
int Clp_SetUTF8(Clp_Parser *clp, int utf8);

/** @brief Return @a clp's treatment of character @a c. */
int Clp_OptionChar(Clp_Parser *clp, int c);

/** @brief Set @a clp's treatment of character @a c. */
int Clp_SetOptionChar(Clp_Parser *clp, int c, int type);

/** @brief Set @a clp's option definitions. */
int Clp_SetOptions(Clp_Parser *clp, int nopt, const Clp_Option *opt);

/** @brief Set @a clp's arguments. */
void Clp_SetArguments(Clp_Parser *clp, int argc, const char * const *argv);

/** @brief Set whether @a clp is searching for options. */
int Clp_SetOptionProcessing(Clp_Parser *clp, int on);


#define Clp_DisallowOptions	(1<<0)	/**< @brief Value type flag: value
					     can't be an option string.

		See Clp_AddType(). */

/** @brief Define a new value type for @a clp. */
int Clp_AddType(Clp_Parser *clp, int val_type, int flags,
		Clp_ValParseFunc parser, void *user_data);


#define Clp_AllowNumbers	(1<<0)	/**< @brief String list flag: allow
					     explicit numbers.

		See Clp_AddStringListType() and Clp_AddStringListTypeVec(). */
#define Clp_StringListLong      (1<<1)  /**< @brief String list flag: values
                                             have long type. */

/** @brief Define a new string list value type for @a clp. */
int Clp_AddStringListTypeVec(Clp_Parser *clp, int val_type, int flags,
			     int nstrs, const char * const *strs,
			     const int *vals);

/** @brief Define a new string list value type for @a clp. */
int Clp_AddStringListType(Clp_Parser *clp, int val_type, int flags, ...)
			  CLP_SENTINEL;


/** @brief Parse and return the next argument from @a clp. */
int Clp_Next(Clp_Parser *clp);

/** @brief Return the next argument from @a clp without option parsing. */
const char *Clp_Shift(Clp_Parser *clp, int allow_options);


/** @brief Create a new Clp_ParserState. */
Clp_ParserState *Clp_NewParserState(void);

/** @brief Destroy a Clp_ParserState object. */
void Clp_DeleteParserState(Clp_ParserState *state);

/** @brief Save @a clp's current state in @a state. */
void Clp_SaveParser(const Clp_Parser *clp, Clp_ParserState *state);

/** @brief Restore parser state from @a state into @a clp. */
void Clp_RestoreParser(Clp_Parser *clp, const Clp_ParserState *state);


/** @brief Report a parser error. */
int Clp_OptionError(Clp_Parser *clp, const char *format, ...);

/** @brief Format a message. */
int Clp_vsnprintf(Clp_Parser* clp, char* str, size_t size,
                  const char* format, va_list val);

/** @brief Print a message. */
int Clp_fprintf(Clp_Parser* clp, FILE* f, const char* format, ...);

/** @brief Print a message. */
int Clp_vfprintf(Clp_Parser* clp, FILE* f, const char* format, va_list val);

/** @brief Extract the current option as a string. */
int Clp_CurOptionNameBuf(Clp_Parser *clp, char *buf, int len);

/** @brief Extract the current option as a string. */
const char *Clp_CurOptionName(Clp_Parser *clp);

/** @brief Test if the current option had long name @a name. */
int Clp_IsLong(Clp_Parser *clp, const char *long_name);

/** @brief Test if the current option had short name @a name. */
int Clp_IsShort(Clp_Parser *clp, int short_name);

#undef CLP_SENTINEL
#ifdef __cplusplus
}
#endif
#endif
