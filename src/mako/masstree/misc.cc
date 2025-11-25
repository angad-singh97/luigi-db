/* Masstree
 * Eddie Kohler, Yandong Mao, Robert Morris
 * Copyright (c) 2012-2013 President and Fellows of Harvard College
 * Copyright (c) 2012-2013 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Masstree LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Masstree LICENSE file; the license in that file
 * is legally binding.
 */
// @unsafe - Miscellaneous utility functions for argument parsing
// Provides suffix-based double parsing for command line arguments
// SAFETY: Raw string parsing with suffix handling (k/m/g multipliers)
// EXCLUDED FROM BORROW CHECK: Uses kvthread allocator (void* return limitation)
//
// External safety annotations for string operations
// @external_unsafe: lcdf::String_base::*
// @external_unsafe: lcdf::String::*
// @external_unsafe: threadinfo::*

#include "misc.hh"
#include <unistd.h>
#include "kvthread.hh"

// @unsafe - parser uses raw string conversion
int clp_parse_suffixdouble(Clp_Parser *clp, const char *vstr,
			   int complain, void *)
{
    const char *post;
    if (*vstr == 0 || isspace((unsigned char) *vstr))
	post = vstr;
    else
	clp->val.d = strtod(vstr, (char **) &post);
    if (vstr != post && (*post == 'K' || *post == 'k'))
	clp->val.d *= 1000, ++post;
    else if (vstr != post && (*post == 'M' || *post == 'm'))
	clp->val.d *= 1000000, ++post;
    else if (vstr != post && (*post == 'B' || *post == 'b' || *post == 'G' || *post == 'g'))
	clp->val.d *= 1000000000, ++post;
    if (*vstr != 0 && *post == 0)
	return 1;
    else if (complain)
	return Clp_OptionError(clp, "%<%O%> expects a real number, not %<%s%>", vstr);
    else
	return 0;
}
