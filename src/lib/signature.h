/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file
 */

#ifndef SIGNATURE_H_
#define SIGNATURE_H_

#include <sys/types.h>
#include <stdbool.h>

#include <inttypes.h>

#include "packet-create.h"
#include "memory.h"

typedef struct pgp_create_sig_t pgp_create_sig_t;

pgp_create_sig_t *pgp_create_sig_new(void);
void              pgp_create_sig_delete(pgp_create_sig_t *);

bool pgp_check_useridcert_sig(rnp_ctx_t *,
                              const pgp_pubkey_t *,
                              const uint8_t *,
                              const pgp_sig_t *,
                              const pgp_pubkey_t *,
                              const uint8_t *);
bool pgp_check_userattrcert_sig(rnp_ctx_t *,
                                const pgp_pubkey_t *,
                                const pgp_data_t *,
                                const pgp_sig_t *,
                                const pgp_pubkey_t *,
                                const uint8_t *);
bool pgp_check_subkey_sig(rnp_ctx_t *,
                          const pgp_pubkey_t *,
                          const pgp_pubkey_t *,
                          const pgp_sig_t *,
                          const pgp_pubkey_t *,
                          const uint8_t *);
bool pgp_check_direct_sig(
  rnp_ctx_t *, const pgp_pubkey_t *, const pgp_sig_t *, const pgp_pubkey_t *, const uint8_t *);

bool pgp_sig_start_key_sig(
  pgp_create_sig_t *, const pgp_pubkey_t *, const uint8_t *, pgp_sig_type_t, pgp_hash_alg_t);
bool pgp_sig_start_subkey_sig(pgp_create_sig_t *,
                              const pgp_pubkey_t *,
                              const pgp_pubkey_t *,
                              pgp_sig_type_t,
                              pgp_hash_alg_t);
void pgp_sig_start(pgp_create_sig_t *,
                   const pgp_seckey_t *,
                   const pgp_hash_alg_t,
                   const pgp_sig_type_t);

void        pgp_sig_add_data(pgp_create_sig_t *, const void *, size_t);
pgp_hash_t *pgp_sig_get_hash(pgp_create_sig_t *);
unsigned    pgp_sig_end_hashed_subpkts(pgp_create_sig_t *);
bool        pgp_sig_write(
  rng_t *, pgp_output_t *, pgp_create_sig_t *, const pgp_pubkey_t *, const pgp_seckey_t *);
unsigned pgp_sig_add_time(pgp_create_sig_t *, int64_t, pgp_content_enum);
unsigned pgp_sig_add_issuer_keyid(pgp_create_sig_t *, const uint8_t *);
void     pgp_sig_add_primary_userid(pgp_create_sig_t *, unsigned);
unsigned pgp_sig_add_key_flags(pgp_create_sig_t *sig,
                               const uint8_t *   key_flags,
                               size_t            octet_count);
unsigned pgp_sig_add_pref_symm_algs(pgp_create_sig_t *sig,
                                    const uint8_t *   algs,
                                    size_t            octet_count);
unsigned pgp_sig_add_pref_hash_algs(pgp_create_sig_t *sig,
                                    const uint8_t *   algs,
                                    size_t            octet_count);
unsigned pgp_sig_add_pref_compress_algs(pgp_create_sig_t *sig,
                                        const uint8_t *   algs,
                                        size_t            octet_count);
unsigned pgp_sig_add_key_server_prefs(pgp_create_sig_t *sig,
                                      const uint8_t *   flags,
                                      size_t            octet_count);
unsigned pgp_sig_add_preferred_key_server(pgp_create_sig_t *sig, const uint8_t *uri);

/* Standard Interface */
bool pgp_sign_file(
  rnp_ctx_t *, pgp_io_t *, const char *, const char *, const pgp_seckey_t *, bool cleartext);

int pgp_sign_detached(
  rnp_ctx_t *, pgp_io_t *, const char *, const char *, const pgp_seckey_t *);

rnp_result_t pgp_sign_memory_detached(rnp_ctx_t *         ctx,
                                      const pgp_seckey_t *seckey,
                                      const uint8_t       membuf[],
                                      size_t              membuf_len,
                                      uint8_t **          sig_output,
                                      size_t *            sig_output_len);

bool pgp_check_sig(
  rng_t *, const uint8_t *, unsigned, const pgp_sig_t *, const pgp_pubkey_t *);

/* armored stuff */
unsigned pgp_crc24(unsigned, uint8_t);

// TODO: This should endup in reader.h or armor.h
void pgp_reader_push_dearmor(pgp_stream_t *);
void pgp_reader_pop_dearmor(pgp_stream_t *);

bool pgp_writer_push_clearsigned(pgp_output_t *, pgp_create_sig_t *);
void pgp_reader_pop_dearmor(pgp_stream_t *);

typedef enum {
    PGP_PGP_MESSAGE = 1,
    PGP_PGP_PUBLIC_KEY_BLOCK,
    PGP_PGP_PRIVATE_KEY_BLOCK,
    PGP_PGP_MULTIPART_MESSAGE_PART_X_OF_Y,
    PGP_PGP_MULTIPART_MESSAGE_PART_X,
    PGP_PGP_SIGNATURE,
    PGP_PGP_CLEARTEXT_SIGNATURE
} pgp_armor_type_t;

#define CRC24_INIT 0xb704ceL

bool pgp_writer_push_armored(pgp_output_t *, pgp_armor_type_t);

pgp_memory_t *pgp_sign_buf(
  rnp_ctx_t *, pgp_io_t *, const void *, const size_t, const pgp_seckey_t *, const bool);

#endif /* SIGNATURE_H_ */
