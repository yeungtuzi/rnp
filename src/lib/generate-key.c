/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
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
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdbool.h>
#include <stdint.h>

#include <rekey/rnp_key_store.h>
#include <librekey/key_store_pgp.h>
#include <librekey/key_store_g10.h>

#include <librepgp/packet-show.h>
#include <librepgp/packet-parse.h>

#include "crypto/ec.h"
#include "readerwriter.h"
#include "pgp-key.h"
#include "memory.h"

static const pgp_symm_alg_t DEFAULT_SYMMETRIC_ALGS[] = {
  PGP_SA_AES_256, PGP_SA_AES_192, PGP_SA_AES_128, PGP_SA_TRIPLEDES};
static const pgp_hash_alg_t DEFAULT_HASH_ALGS[] = {
  PGP_HASH_SHA256, PGP_HASH_SHA384, PGP_HASH_SHA512, PGP_HASH_SHA224, PGP_HASH_SHA1};
static const pgp_compression_type_t DEFAULT_COMPRESS_ALGS[] = {
  PGP_C_ZLIB, PGP_C_BZIP2, PGP_C_ZIP, PGP_C_NONE};

/* Shortcut to load a single key from memory. */
static bool
load_generated_key(pgp_output_t **    output,
                   pgp_memory_t **    mem,
                   pgp_key_t *        dst,
                   key_store_format_t format,
                   pgp_key_t *        pubkey)
{
    bool             ok = false;
    pgp_io_t         io = {.errs = stderr, .res = stdout, .outs = stdout};
    rnp_key_store_t *pubring = NULL;

    // this would be better on the stack but the key store does not allow it
    rnp_key_store_t *key_store = calloc(1, sizeof(*key_store));

    if (!key_store) {
        return false;
    }
    switch (format) {
    case GPG_KEY_STORE:
    case KBX_KEY_STORE:
        if (!rnp_key_store_pgp_read_from_mem(&io, key_store, 0, *mem)) {
            RNP_LOG("failed to read back generated key");
            goto end;
        }
        break;
    case G10_KEY_STORE: {
        pubring = calloc(1, sizeof(*pubring));
        if (!pubring) {
            goto end;
        }
        if (!rnp_key_store_add_key(&io, pubring, pubkey)) {
            goto end;
        }
        if (!rnp_key_store_g10_from_mem(&io, pubring, key_store, *mem)) {
            goto end;
        }
    } break;
    default:
        RNP_LOG("invalid key format %d", format);
        break;
    }
    if (key_store->keyc != 1) {
        goto end;
    }
    memcpy(dst, &key_store->keys[0], sizeof(*dst));
    // we don't want the key store to free the internal key data
    rnp_key_store_remove_key(&io, key_store, &key_store->keys[0]);
    ok = true;

end:
    rnp_key_store_free(key_store);
    if (pubring) {
        rnp_key_store_remove_key(&io, pubring, &pubring->keys[0]);
        rnp_key_store_free(pubring);
    }
    pgp_teardown_memory_write(*output, *mem);
    *output = NULL;
    *mem = NULL;
    return ok;
}

static uint8_t
pk_alg_default_flags(pgp_pubkey_alg_t alg)
{
    // just use the full capabilities as the ultimate fallback
    return pgp_pk_alg_capabilities(alg);
}

static void
adjust_hash_to_curve(rnp_keygen_crypto_params_t *crypto)
{
    size_t digest_length = 0;
    if (!pgp_digest_length(crypto->hash_alg, &digest_length)) {
        return;
    }

    /*
     * Adjust hash to curve - see point 14 of RFC 4880 bis 01
     * and/or ECDSA spec.
     *
     * Minimal size of digest for curve:
     *    P-256  32 bytes
     *    P-384  48 bytes
     *    P-521  64 bytes
     */
    switch (crypto->ecc.curve) {
    case PGP_CURVE_NIST_P_256:
        if (digest_length < 32) {
            crypto->hash_alg = PGP_HASH_SHA256;
        }
        break;
    case PGP_CURVE_NIST_P_384:
        if (digest_length < 48) {
            crypto->hash_alg = PGP_HASH_SHA384;
        }
        break;
    case PGP_CURVE_NIST_P_521:
        if (digest_length < 64) {
            crypto->hash_alg = PGP_HASH_SHA512;
        }
        break;

    default:
        // TODO: if it's anything else, let the lower layers reject it?
        break;
    }
}

static void
keygen_merge_crypto_defaults(rnp_keygen_crypto_params_t *crypto)
{
    // default to RSA
    if (!crypto->key_alg) {
        crypto->key_alg = PGP_PKA_RSA;
    }

    switch (crypto->key_alg) {
    case PGP_PKA_RSA:
        if (!crypto->rsa.modulus_bit_len) {
            crypto->rsa.modulus_bit_len = DEFAULT_RSA_NUMBITS;
        }
        break;

    case PGP_PKA_SM2:
        if (!crypto->hash_alg) {
            crypto->hash_alg = PGP_HASH_SM3;
        }
        if (!crypto->ecc.curve) {
            crypto->ecc.curve = PGP_CURVE_SM2_P_256;
        }
        break;

    case PGP_PKA_ECDH:
    case PGP_PKA_ECDSA:
        if (!crypto->hash_alg) {
            crypto->hash_alg = DEFAULT_HASH_ALGS[0];
        }
        adjust_hash_to_curve(crypto);
        break;

    case PGP_PKA_EDDSA:
        if (!crypto->ecc.curve) {
            crypto->ecc.curve = PGP_CURVE_ED25519;
        }
        break;

    default:
        break;
    }
    if (!crypto->hash_alg) {
        crypto->hash_alg = DEFAULT_HASH_ALGS[0];
    }
}

static bool
validate_keygen_primary(const rnp_keygen_primary_desc_t *desc)
{
    /* Confirm that the specified pk alg can certify.
     * gpg requires this, though the RFC only says that a V4 primary
     * key SHOULD be a key capable of certification.
     */
    if (!(pgp_pk_alg_capabilities(desc->crypto.key_alg) & PGP_KF_CERTIFY)) {
        RNP_LOG("primary key alg (%d) must be able to sign", desc->crypto.key_alg);
        return false;
    }

    // check key flags
    if (!desc->cert.key_flags) {
        // these are probably not *technically* required
        RNP_LOG("key flags are required");
        return false;
    } else if (desc->cert.key_flags & ~pgp_pk_alg_capabilities(desc->crypto.key_alg)) {
        // check the flags against the alg capabilities
        RNP_LOG("usage not permitted for pk algorithm");
        return false;
    }

    // require a userid
    if (!desc->cert.userid[0]) {
        RNP_LOG("userid is required for primary key");
        return false;
    }
    return true;
}

static uint32_t
get_numbits(const rnp_keygen_crypto_params_t *crypto)
{
    switch (crypto->key_alg) {
    case PGP_PKA_RSA:
    case PGP_PKA_RSA_ENCRYPT_ONLY:
    case PGP_PKA_RSA_SIGN_ONLY:
        return crypto->rsa.modulus_bit_len;
    case PGP_PKA_ECDSA:
    case PGP_PKA_ECDH:
    case PGP_PKA_EDDSA:
    case PGP_PKA_SM2: {
        const ec_curve_desc_t *curve = get_curve_desc(crypto->ecc.curve);
        return curve ? curve->bitlen : 0;
    }
    default:
        return 0;
    }
}

static bool
set_default_user_prefs(pgp_user_prefs_t *prefs)
{
    if (!prefs->symm_algs) {
        for (size_t i = 0; i < ARRAY_SIZE(DEFAULT_SYMMETRIC_ALGS); i++) {
            EXPAND_ARRAY(prefs, symm_alg);
            if (!prefs->symm_algs) {
                return false;
            }
            prefs->symm_algs[i] = DEFAULT_SYMMETRIC_ALGS[i];
            prefs->symm_algc++;
        }
    }
    if (!prefs->hash_algs) {
        for (size_t i = 0; i < ARRAY_SIZE(DEFAULT_HASH_ALGS); i++) {
            EXPAND_ARRAY(prefs, hash_alg);
            if (!prefs->hash_algs) {
                return false;
            }
            prefs->hash_algs[i] = DEFAULT_HASH_ALGS[i];
            prefs->hash_algc++;
        }
    }
    if (!prefs->compress_algs) {
        for (size_t i = 0; i < ARRAY_SIZE(DEFAULT_COMPRESS_ALGS); i++) {
            EXPAND_ARRAY(prefs, compress_alg);
            if (!prefs->compress_algs) {
                return false;
            }
            prefs->compress_algs[i] = DEFAULT_COMPRESS_ALGS[i];
            prefs->compress_algc++;
        }
    }
    return true;
}

static void
keygen_primary_merge_defaults(rnp_keygen_primary_desc_t *desc)
{
    keygen_merge_crypto_defaults(&desc->crypto);
    set_default_user_prefs(&desc->cert.prefs);

    if (!desc->cert.key_flags) {
        // set some default key flags if none are provided
        desc->cert.key_flags = pk_alg_default_flags(desc->crypto.key_alg);
    }
    if (desc->cert.userid[0] == '\0') {
        snprintf((char *) desc->cert.userid,
                 sizeof(desc->cert.userid),
                 "%s %d-bit key <%s@localhost>",
                 pgp_show_pka(desc->crypto.key_alg),
                 get_numbits(&desc->crypto),
                 getenv("LOGNAME"));
    }
}

bool
pgp_generate_primary_key(rnp_keygen_primary_desc_t *desc,
                         bool                       merge_defaults,
                         pgp_key_t *                primary_sec,
                         pgp_key_t *                primary_pub,
                         key_store_format_t         secformat)
{
    bool          ok = false;
    pgp_output_t *output = NULL;
    pgp_memory_t *mem = NULL;
    pgp_seckey_t  seckey;

    memset(&seckey, 0, sizeof(seckey));

    // validate args
    if (!desc || !primary_pub || !primary_sec) {
        goto end;
    }
    if (primary_sec->type || primary_pub->type) {
        RNP_LOG("invalid parameters (should be zeroed)");
        goto end;
    }

    // merge some defaults in, if requested
    if (merge_defaults) {
        keygen_primary_merge_defaults(desc);
    }

    // now validate the keygen fields
    if (!validate_keygen_primary(desc)) {
        goto end;
    }

    // generate the raw key pair
    if (!pgp_generate_seckey(&desc->crypto, &seckey)) {
        goto end;
    }

    // write the public key, userid, and self-signature
    if (!pgp_setup_memory_write(NULL, &output, &mem, 4096)) {
        goto end;
    }
    if (!pgp_write_struct_pubkey(output, PGP_PTAG_CT_PUBLIC_KEY, &seckey.pubkey) ||
        !pgp_write_struct_userid(output, desc->cert.userid) ||
        !pgp_write_selfsig_cert(output, &seckey, desc->crypto.hash_alg, &desc->cert)) {
        RNP_LOG("failed to write out generated key+sigs");
        goto end;
    }
    // load the public key back in
    if (!load_generated_key(&output, &mem, primary_pub, GPG_KEY_STORE, NULL)) {
        goto end;
    }

    // write the secret key, userid, and self-signature
    if (!pgp_setup_memory_write(NULL, &output, &mem, 4096)) {
        goto end;
    }
    switch (secformat) {
    case GPG_KEY_STORE:
    case KBX_KEY_STORE:
        if (!pgp_write_struct_seckey(output, PGP_PTAG_CT_SECRET_KEY, &seckey, NULL) ||
            !pgp_write_struct_userid(output, desc->cert.userid) ||
            !pgp_write_selfsig_cert(output, &seckey, desc->crypto.hash_alg, &desc->cert)) {
            RNP_LOG("failed to write out generated key+sigs");
            goto end;
        }
        break;
    case G10_KEY_STORE:
        if (!g10_write_seckey(output, &seckey, NULL)) {
            RNP_LOG("failed to write generated seckey");
            goto end;
        }
        break;
    default:
        RNP_LOG("invalid format");
        goto end;
        break;
    }
    // load the secret key back in
    if (!load_generated_key(&output, &mem, primary_sec, secformat, primary_pub)) {
        RNP_LOG("failed to load back generated key");
        goto end;
    }

    ok = true;
end:
    // free any user preferences
    pgp_free_user_prefs(&desc->cert.prefs);

    if (output && mem) {
        pgp_teardown_memory_write(output, mem);
        output = NULL;
        mem = NULL;
    }
    // we don't need this as we have loaded the encrypted key
    // into primary_sec
    pgp_seckey_free(&seckey);
    if (!ok) {
        pgp_key_free_data(primary_pub);
        pgp_key_free_data(primary_sec);
    }
    return ok;
}

static bool
validate_keygen_subkey(rnp_keygen_subkey_desc_t *desc)
{
    if (!desc->binding.key_flags) {
        RNP_LOG("key flags are required");
        return false;
    } else if (desc->binding.key_flags & ~pgp_pk_alg_capabilities(desc->crypto.key_alg)) {
        // check the flags against the alg capabilities
        RNP_LOG("usage not permitted for pk algorithm");
        return false;
    }
    return true;
}

static void
keygen_subkey_merge_defaults(rnp_keygen_subkey_desc_t *desc)
{
    keygen_merge_crypto_defaults(&desc->crypto);
    if (!desc->binding.key_flags) {
        // set some default key flags if none are provided
        desc->binding.key_flags = pk_alg_default_flags(desc->crypto.key_alg);
    }
}

bool
pgp_generate_subkey(rnp_keygen_subkey_desc_t *     desc,
                    bool                           merge_defaults,
                    pgp_key_t *                    primary_sec,
                    pgp_key_t *                    primary_pub,
                    pgp_key_t *                    subkey_sec,
                    pgp_key_t *                    subkey_pub,
                    const pgp_password_provider_t *password_provider,
                    key_store_format_t             secformat)
{
    bool                ok = false;
    pgp_output_t *      output = NULL;
    pgp_memory_t *      mem = NULL;
    const pgp_seckey_t *primary_seckey = NULL;
    pgp_seckey_t *      decrypted_primary_seckey = NULL;
    pgp_seckey_t        seckey = {{0}};

    // validate args
    if (!desc || !primary_sec || !primary_pub || !subkey_sec || !subkey_pub) {
        RNP_LOG("NULL args");
        goto end;
    }
    if (!pgp_key_is_primary_key(primary_sec) || !pgp_key_is_primary_key(primary_pub) ||
        !pgp_is_key_secret(primary_sec) || !pgp_is_key_public(primary_pub)) {
        RNP_LOG("invalid parameters");
        goto end;
    }
    if (subkey_sec->type || subkey_pub->type) {
        RNP_LOG("invalid parameters (should be zeroed)");
        goto end;
    }

    // merge some defaults in, if requested
    if (merge_defaults) {
        keygen_subkey_merge_defaults(desc);
    }

    // now validate the keygen fields
    if (!validate_keygen_subkey(desc)) {
        goto end;
    }

    // decrypt the primary seckey if needed (for signatures)
    if (primary_sec->key.seckey.encrypted) {
        decrypted_primary_seckey = pgp_decrypt_seckey(
          primary_sec,
          password_provider,
          &(pgp_password_ctx_t){.op = PGP_OP_ADD_SUBKEY, .key = primary_sec});
        if (!decrypted_primary_seckey) {
            goto end;
        }
        primary_seckey = decrypted_primary_seckey;
    } else {
        primary_seckey = pgp_get_seckey(primary_sec);
    }

    // prepare to add subkeys
    EXPAND_ARRAY(primary_sec, subkey);
    EXPAND_ARRAY(primary_pub, subkey);
    if (!primary_sec->subkeys || !primary_pub->subkeys) {
        goto end;
    }

    // generate the raw key pair
    if (!pgp_generate_seckey(&desc->crypto, &seckey)) {
        goto end;
    }

    // write the public subkey, userid, and binding self-signature
    if (!pgp_setup_memory_write(NULL, &output, &mem, 4096)) {
        goto end;
    }
    if (!pgp_write_struct_pubkey(output, PGP_PTAG_CT_PUBLIC_SUBKEY, &seckey.pubkey) ||
        !pgp_write_selfsig_binding(
          output, primary_seckey, desc->crypto.hash_alg, &seckey.pubkey, &desc->binding)) {
        RNP_LOG("failed to write out generated key+sigs");
        goto end;
    }
    // load the public key back in
    if (!load_generated_key(&output, &mem, subkey_pub, GPG_KEY_STORE, NULL)) {
        goto end;
    }
    primary_pub->subkeys[primary_pub->subkeyc++] = subkey_pub;

    // write the secret subkey, userid, and binding self-signature
    if (!pgp_setup_memory_write(NULL, &output, &mem, 4096)) {
        goto end;
    }
    switch (secformat) {
    case GPG_KEY_STORE:
    case KBX_KEY_STORE:
        if (!pgp_write_struct_seckey(output, PGP_PTAG_CT_SECRET_SUBKEY, &seckey, NULL) ||
            !pgp_write_selfsig_binding(
              output, primary_seckey, desc->crypto.hash_alg, &seckey.pubkey, &desc->binding)) {
            RNP_LOG("failed to write out generated key+sigs");
            goto end;
        }
        break;
    case G10_KEY_STORE:
        if (!g10_write_seckey(output, &seckey, NULL)) {
            RNP_LOG("failed to write generated seckey");
            goto end;
        }
        break;
    default:
        RNP_LOG("invalid format");
        goto end;
        break;
    }
    // load the secret key back in
    if (!load_generated_key(&output, &mem, subkey_sec, secformat, subkey_pub)) {
        goto end;
    }
    primary_sec->subkeys[primary_sec->subkeyc++] = subkey_sec;

    ok = true;
end:
    pgp_seckey_free(&seckey);
    if (decrypted_primary_seckey) {
        pgp_seckey_free(decrypted_primary_seckey);
        free(decrypted_primary_seckey);
    }
    if (!ok) {
        pgp_key_free_data(subkey_pub);
        pgp_key_free_data(subkey_sec);
    }
    return ok;
}

static void
keygen_merge_defaults(rnp_keygen_primary_desc_t *primary_desc,
                      rnp_keygen_subkey_desc_t * subkey_desc)
{
    if (!primary_desc->cert.key_flags && !subkey_desc->binding.key_flags) {
        // if no flags are set for either the primary key nor subkey,
        // we can set up some typical defaults here (these are validated
        // later against the alg capabilities)
        primary_desc->cert.key_flags = PGP_KF_SIGN | PGP_KF_CERTIFY;
        subkey_desc->binding.key_flags = PGP_KF_ENCRYPT;
    }
}

static void
print_keygen_crypto(const rnp_keygen_crypto_params_t *crypto)
{
    printf("key_alg: %s (%d)\n", pgp_show_pka(crypto->key_alg), crypto->key_alg);
    if (crypto->key_alg == PGP_PKA_RSA) {
        printf("bits: %u\n", crypto->rsa.modulus_bit_len);
    } else {
        printf("curve: %d\n", crypto->ecc.curve);
    }
    printf("hash_alg: %s (%d)\n", pgp_show_hash_alg(crypto->hash_alg), crypto->hash_alg);
}

static void
print_keygen_primary(const rnp_keygen_primary_desc_t *desc)
{
    printf("Keygen (primary)\n");
    print_keygen_crypto(&desc->crypto);
}

static void
print_keygen_subkey(const rnp_keygen_subkey_desc_t *desc)
{
    printf("Keygen (subkey)\n");
    print_keygen_crypto(&desc->crypto);
}

bool
pgp_generate_keypair(rng_t *                    rng,
                     rnp_keygen_primary_desc_t *primary_desc,
                     rnp_keygen_subkey_desc_t * subkey_desc,
                     bool                       merge_defaults,
                     pgp_key_t *                primary_sec,
                     pgp_key_t *                primary_pub,
                     pgp_key_t *                subkey_sec,
                     pgp_key_t *                subkey_pub,
                     key_store_format_t         secformat)
{
    bool ok = false;

    if (rnp_get_debug(__FILE__)) {
        print_keygen_primary(primary_desc);
        print_keygen_subkey(subkey_desc);
    }

    // validate args
    if (!primary_desc || !subkey_desc || !primary_sec || !primary_pub || !subkey_sec ||
        !subkey_pub) {
        RNP_LOG("NULL args");
        goto end;
    }

    // merge some defaults in, if requested
    if (merge_defaults) {
        keygen_merge_defaults(primary_desc, subkey_desc);
    }

    // generate the primary key
    primary_desc->crypto.rng = rng;
    if (!pgp_generate_primary_key(
          primary_desc, merge_defaults, primary_sec, primary_pub, secformat)) {
        RNP_LOG("failed to generate primary key");
        goto end;
    }

    // generate the subkey
    subkey_desc->crypto.rng = rng;
    if (!pgp_generate_subkey(subkey_desc,
                             merge_defaults,
                             primary_sec,
                             primary_pub,
                             subkey_sec,
                             subkey_pub,
                             NULL,
                             secformat)) {
        RNP_LOG("failed to generate subkey");
        goto end;
    }
    ok = true;
end:
    return ok;
}
