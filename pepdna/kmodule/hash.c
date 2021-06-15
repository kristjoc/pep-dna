/*
 *  pep-dna/pepdna/kmodule/hash.c: PEP-DNA hash functions
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

#include "hash.h"

#include <linux/kernel.h>

/*
 * Generate hash key for (scr_ip, src_port)
 * ------------------------------------------------------------------------- */
__u32 pepdna_hash32_rjenkins1_2(__be32 src_ip, __be16 src_port)
{
        __u32 a   = be32_to_cpu(src_ip);
        __be32 sp = src_port;
        __u32 b   = be32_to_cpu(sp);

        __u32 hash = pepdna_hash_seed ^ a ^ b;
        __u32 x = 231232;
        __u32 y = 1232;
        pepdna_hashmix(a, b, hash);
        pepdna_hashmix(x, a, hash);
        pepdna_hashmix(b, y, hash);

        return hash;
}
