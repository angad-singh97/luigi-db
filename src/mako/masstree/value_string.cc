/* Masstree
 * Eddie Kohler, Yandong Mao, Robert Morris
 * Copyright (c) 2012-2014 President and Fellows of Harvard College
 * Copyright (c) 2012-2014 Massachusetts Institute of Technology
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
// @unsafe - Single-string value type for Masstree rows
// Stores variable-length string data with inline length and timestamp
// SAFETY: Header-only implementation, relies on kvrow.hh for allocation
// EXCLUDED FROM BORROW CHECK: Uses kvthread allocator (void* return limitation)
//
// External safety annotations for circular_int and string operations
// @external_unsafe: circular_int::*
// @external_unsafe: lcdf::String_base::*
// @external_unsafe: lcdf::String::*
// @external_unsafe: threadinfo::*

#include "kvrow.hh"
#include "value_string.hh"
#include <string.h>
