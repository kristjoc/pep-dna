/*
 *  pep-dna/pepdna/kmodule/hash.c: Header file for PEP-DNA hash functions
 *
 *  Copyright (C) 2020  Kristjon Ciko <kristjoc@ifi.uio.no>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _PEPDNA_HASH_H
#define _PEPDNA_HASH_H

#include <linux/types.h>        /* types __u32, _be32, etc. */

#define pepdna_hashmix(a, b, c) do {        \
        a = a-b;  a = a-c;  a = a^(c>>13);  \
        b = b-c;  b = b-a;  b = b^(a<<8);   \
        c = c-a;  c = c-b;  c = c^(b>>13);  \
        a = a-b;  a = a-c;  a = a^(c>>12);  \
        b = b-c;  b = b-a;  b = b^(a<<16);  \
        c = c-a;  c = c-b;  c = c^(b>>5);   \
        a = a-b;  a = a-c;  a = a^(c>>3);   \
        b = b-c;  b = b-a;  b = b^(a<<10);  \
        c = c-a;  c = c-b;  c = c^(b>>15);  \
} while (0)

#define pepdna_hash_seed 1315423911

__u32 pepdna_hash32_rjenkins1_2(__be32, __be16);

#endif /* _PEPDNA_HASH_H */
