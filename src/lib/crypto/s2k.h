/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 *
 * This code is originally derived from software contributed to
 * The NetBSD Foundation by Alistair Crooks (agc@netbsd.org), and
 * carried further by Ribose Inc (https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RNP_S2K_H_
#define RNP_S2K_H_

#include "hash.h"

#define PGP_S2K_DEFAULT_ITERATIONS 524288

int pgp_s2k_simple(pgp_hash_alg_t alg,
                   uint8_t *      out,
                   size_t         output_len,
                   const char *   passphrase);

int pgp_s2k_salted(pgp_hash_alg_t alg,
                   uint8_t *      out,
                   size_t         output_len,
                   const char *   passphrase,
                   const uint8_t *salt);

int pgp_s2k_iterated(pgp_hash_alg_t alg,
                     uint8_t *      out,
                     size_t         output_len,
                     const char *   passphrase,
                     const uint8_t *salt,
                     size_t         iterations);

size_t pgp_s2k_decode_iterations(uint8_t encoded_iter);

uint8_t pgp_s2k_encode_iterations(size_t iterations);

// Round iterations to nearest representable value
size_t pgp_s2k_round_iterations(size_t iterations);

#endif
