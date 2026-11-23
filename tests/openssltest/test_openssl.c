/**
 * @file     test_openssl.c
 * @brief    Ring 3 QEMU test for cross-compiled OpenSSL (libcrypto/libssl)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0031 */

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int check_sha256_and_rand(void) {
    unsigned char rnd[32];
    memset(rnd, 0, sizeof(rnd));
    if (RAND_bytes(rnd, (int)sizeof(rnd)) != 1) {
        printf("[FAIL] openssltest RAND_bytes failed\n");
        return -1;
    }
    int nonzero = 0;
    for (size_t i = 0u; i < sizeof(rnd); i++) {
        if (rnd[i] != 0u) {
            nonzero = 1;
            break;
        }
    }
    if (!nonzero) {
        printf("[FAIL] openssltest RAND_bytes returned all zeros\n");
        return -1;
    }

    static const unsigned char k_expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
        0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
        0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    unsigned char digest[32];
    unsigned int dlen = 0u;
    if (EVP_Digest("abc", 3u, digest, &dlen, EVP_sha256(), NULL) != 1 ||
        dlen != 32u || memcmp(digest, k_expected, 32u) != 0) {
        printf("[FAIL] openssltest SHA-256 mismatch\n");
        return -1;
    }
    return 0;
}

static int check_aes_gcm(void) {
    static const unsigned char k_key[32] = {
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    static const unsigned char k_iv[12] = {1, 2, 3, 4,  5,  6,
                                           7, 8, 9, 10, 11, 12};
    static const unsigned char k_pt[] = "Microkernel OpenSSL AES-256-GCM test!";
    unsigned char ct[64];
    unsigned char out[64];
    unsigned char tag[16];
    int len = 0;
    int flen = 0;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return -1;
    }
    int ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, k_key, k_iv) == 1 &&
        EVP_EncryptUpdate(ctx, ct, &len, k_pt, (int)sizeof(k_pt)) == 1 &&
        EVP_EncryptFinal_ex(ctx, ct + len, &flen) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
        printf("[FAIL] openssltest AES-256-GCM encrypt failed\n");
        return -1;
    }
    int ct_len = len + flen;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return -1;
    }
    memset(out, 0, sizeof(out));
    ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, k_key, k_iv) == 1 &&
         EVP_DecryptUpdate(ctx, out, &len, ct, ct_len) == 1 &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) == 1 &&
         EVP_DecryptFinal_ex(ctx, out + len, &flen) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok || len + flen != (int)sizeof(k_pt) ||
        memcmp(out, k_pt, sizeof(k_pt)) != 0) {
        printf("[FAIL] openssltest AES-256-GCM decrypt mismatch\n");
        return -1;
    }
    return 0;
}

static int sign_and_verify(EVP_PKEY *pkey, const EVP_MD *md, const char *name) {
    static const unsigned char k_msg[] = "Microkernel OpenSSL signature check";
    unsigned char sig[512];
    size_t siglen = sizeof(sig);

    EVP_MD_CTX *mctx = EVP_MD_CTX_new();
    if (!mctx) {
        return -1;
    }
    int ok = EVP_DigestSignInit(mctx, NULL, md, NULL, pkey) == 1 &&
             EVP_DigestSign(mctx, sig, &siglen, k_msg, sizeof(k_msg)) == 1;
    EVP_MD_CTX_free(mctx);
    if (!ok) {
        printf("[FAIL] openssltest %s sign failed\n", name);
        return -1;
    }

    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        return -1;
    }
    ok = EVP_DigestVerifyInit(mctx, NULL, md, NULL, pkey) == 1 &&
         EVP_DigestVerify(mctx, sig, siglen, k_msg, sizeof(k_msg)) == 1;
    EVP_MD_CTX_free(mctx);
    if (!ok) {
        printf("[FAIL] openssltest %s verify failed\n", name);
        return -1;
    }

    sig[siglen / 2u] ^= 0xFFu;
    mctx = EVP_MD_CTX_new();
    if (!mctx) {
        return -1;
    }
    int bad = EVP_DigestVerifyInit(mctx, NULL, md, NULL, pkey) == 1 &&
              EVP_DigestVerify(mctx, sig, siglen, k_msg, sizeof(k_msg)) == 1;
    EVP_MD_CTX_free(mctx);
    if (bad) {
        printf("[FAIL] openssltest %s accepted tampered signature\n", name);
        return -1;
    }
    return 0;
}

static X509 *make_self_signed_cert(EVP_PKEY *pkey) {
    X509 *cert = X509_new();
    if (!cert) {
        return NULL;
    }
    if (X509_set_version(cert, 2L) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(cert), 1L) != 1 ||
        !X509_gmtime_adj(X509_getm_notBefore(cert), -60L) ||
        !X509_gmtime_adj(X509_getm_notAfter(cert), 3600L) ||
        X509_set_pubkey(cert, pkey) != 1) {
        X509_free(cert);
        return NULL;
    }
    X509_NAME *name = X509_get_subject_name(cert);
    if (!name ||
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   (const unsigned char *)"microkernel.local",
                                   -1, -1, 0) != 1 ||
        X509_set_issuer_name(cert, name) != 1 ||
        X509_sign(cert, pkey, EVP_sha256()) <= 0) {
        X509_free(cert);
        return NULL;
    }
    return cert;
}

static int check_tls_handshake(EVP_PKEY *ec_key) {
    X509 *cert = make_self_signed_cert(ec_key);
    if (!cert) {
        printf("[FAIL] openssltest X509 self-sign failed\n");
        return -1;
    }

    SSL_CTX *sctx = SSL_CTX_new(TLS_server_method());
    SSL_CTX *cctx = SSL_CTX_new(TLS_client_method());
    if (!sctx || !cctx || SSL_CTX_use_certificate(sctx, cert) != 1 ||
        SSL_CTX_use_PrivateKey(sctx, ec_key) != 1 ||
        SSL_CTX_check_private_key(sctx) != 1) {
        printf("[FAIL] openssltest SSL_CTX setup failed\n");
        SSL_CTX_free(sctx);
        SSL_CTX_free(cctx);
        X509_free(cert);
        return -1;
    }

    SSL *srv = SSL_new(sctx);
    SSL *cli = SSL_new(cctx);
    BIO *sbio = NULL;
    BIO *cbio = NULL;
    if (!srv || !cli || BIO_new_bio_pair(&sbio, 0u, &cbio, 0u) != 1) {
        printf("[FAIL] openssltest SSL/BIO setup failed\n");
        SSL_free(srv);
        SSL_free(cli);
        SSL_CTX_free(sctx);
        SSL_CTX_free(cctx);
        X509_free(cert);
        return -1;
    }
    SSL_set_bio(srv, sbio, sbio);
    SSL_set_bio(cli, cbio, cbio);

    int cli_done = 0;
    int srv_done = 0;
    for (int iter = 0; iter < 64 && (!cli_done || !srv_done); iter++) {
        if (!cli_done) {
            int r = SSL_connect(cli);
            if (r == 1) {
                cli_done = 1;
            } else {
                int err = SSL_get_error(cli, r);
                if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                    printf("[FAIL] openssltest SSL_connect err=%d\n", err);
                    break;
                }
            }
        }
        if (!srv_done) {
            int r = SSL_accept(srv);
            if (r == 1) {
                srv_done = 1;
            } else {
                int err = SSL_get_error(srv, r);
                if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                    printf("[FAIL] openssltest SSL_accept err=%d\n", err);
                    break;
                }
            }
        }
    }

    int rc = -1;
    if (cli_done && srv_done && SSL_version(cli) == TLS1_3_VERSION) {
        static const char k_ping[] = "TLS 1.3 on Microkernel Ring 3!";
        char rbuf[64];
        memset(rbuf, 0, sizeof(rbuf));
        int nw = SSL_write(cli, k_ping, (int)strlen(k_ping));
        int nr = SSL_read(srv, rbuf, (int)sizeof(rbuf) - 1);
        if (nw == (int)strlen(k_ping) && nr == nw &&
            strcmp(rbuf, k_ping) == 0) {
            rc = 0;
        } else {
            printf("[FAIL] openssltest TLS I/O mismatch nw=%d nr=%d\n", nw, nr);
        }
    } else {
        printf("[FAIL] openssltest TLS handshake failed cli=%d srv=%d "
               "ver=0x%x\n",
               cli_done, srv_done, SSL_version(cli));
    }

    SSL_free(srv);
    SSL_free(cli);
    SSL_CTX_free(sctx);
    SSL_CTX_free(cctx);
    X509_free(cert);
    return rc;
}

int main(void) {
    const char *ver = OpenSSL_version(OPENSSL_VERSION);
    if (!ver || strncmp(ver, "OpenSSL 3.0.15", 14u) != 0) {
        printf("[FAIL] openssltest version mismatch: %s\n",
               ver ? ver : "(null)");
        return 1;
    }
    if (check_sha256_and_rand() != 0 || check_aes_gcm() != 0) {
        return 1;
    }

    EVP_PKEY *rsa = EVP_RSA_gen(2048u);
    if (!rsa || sign_and_verify(rsa, EVP_sha256(), "RSA-2048") != 0) {
        EVP_PKEY_free(rsa);
        return 1;
    }
    EVP_PKEY_free(rsa);

    EVP_PKEY *ec = EVP_EC_gen("P-256");
    if (!ec || sign_and_verify(ec, EVP_sha256(), "ECDSA-P256") != 0) {
        EVP_PKEY_free(ec);
        return 1;
    }

    EVP_PKEY *ed = EVP_PKEY_Q_keygen(NULL, NULL, "ED25519");
    if (!ed || sign_and_verify(ed, NULL, "Ed25519") != 0) {
        EVP_PKEY_free(ed);
        EVP_PKEY_free(ec);
        return 1;
    }
    EVP_PKEY_free(ed);

    if (check_tls_handshake(ec) != 0) {
        EVP_PKEY_free(ec);
        return 1;
    }
    EVP_PKEY_free(ec);

    printf("[OK] OpenSSL 3.0.15 sysroot SHA-256, AES-GCM, RSA-2048, ECDSA-P256,"
           " Ed25519, and TLS 1.3 verified\n");
    return 0;
}
