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
// @unsafe - Checkpoint serialization for Masstree data persistence
// Traverses tree nodes and serializes key-value pairs to binary msgpack format
// SAFETY: Uses raw row pointers and binary serialization without bounds checking
// EXCLUDED FROM BORROW CHECK: Uses kvthread allocator (void* return limitation)
//
// External safety annotations for circular_int and string operations
// @external_unsafe: circular_int::*
// @external_unsafe: lcdf::String_base::*
// @external_unsafe: lcdf::String::*
// @external_unsafe: lcdf::String_generic::*
// @external_unsafe: msgpack::*
// @external_unsafe: threadinfo::*

#include "checkpoint.hh"

// add one key/value to a checkpoint.
// called by checkpoint_tree() for each node.
// @unsafe - serializes raw row pointers into checkpoint stream
bool ckstate::visit_value(Str key, const row_type* value, threadinfo&) {
    if (endkey && key >= endkey)
        return false;
    if (!row_is_marker(value)) {
        msgpack::unparser<kvout> up(*vals);
        up.write(key).write_wide(value->timestamp());
        value->checkpoint_write(up);
        ++count;
    }
    return true;
}
