/* Some operating systems don't have strerror.
   This file provides a definition which David Mazieres <dm@lcs.mit.edu>
   assures me works. */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

char *
strerror(int errno)
{
  extern int sys_nerr;
  extern char *sys_errlist[];
  if (errno < 0 || errno >= sys_nerr)
    return (char *)"bad error number";
  else
    return sys_errlist[errno];
}

#ifdef __cplusplus
}
#endif
