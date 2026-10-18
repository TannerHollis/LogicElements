/**
 * @file le_crypto.c
 * @brief Self-contained, zero-allocation Ed25519 and SHA-512 implementation for LogicElements.
 *
 * Implements deterministic Ed25519 signature generation and verification according
 * to RFC 8032. Tailored for ultra-portable embedded runtime verification with zero heap
 * usage and constant-time modular arithmetic.
 */

#include "le_crypto.h"
#include <string.h>

/* ========================================================================== */
/* SHA-512 Implementation (RFC 6234)                                          */
/* ========================================================================== */

typedef struct {
    uint64_t state[8];
    uint64_t count[2];
    uint8_t  buffer[128];
} le_sha512_ctx_t;

#define ROR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define Ch(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define Sigma0(x)    (ROR64(x, 28) ^ ROR64(x, 34) ^ ROR64(x, 39))
#define Sigma1(x)    (ROR64(x, 14) ^ ROR64(x, 18) ^ ROR64(x, 41))
#define sigma0(x)    (ROR64(x, 1)  ^ ROR64(x, 8)  ^ ((x) >> 7))
#define sigma1(x)    (ROR64(x, 19) ^ ROR64(x, 61) ^ ((x) >> 6))

static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7bee0ee30ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47867d1bULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc2478794f746eeaaULL, 0xc597914eac9c60d0ULL,
    0xd044a157450e75dcULL, 0xd499c0de7361a296ULL, 0xe99f530457f72fdeULL, 0xf40e35855771202aULL,
    0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
    0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL,
    0x8cc702081a6439ecULL, 0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL,
    0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL,
    0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL
};

static void sha512_transform(le_sha512_ctx_t* ctx, const uint8_t data[128])
{
    uint64_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint64_t)data[i * 8 + 0] << 56) |
               ((uint64_t)data[i * 8 + 1] << 48) |
               ((uint64_t)data[i * 8 + 2] << 40) |
               ((uint64_t)data[i * 8 + 3] << 32) |
               ((uint64_t)data[i * 8 + 4] << 24) |
               ((uint64_t)data[i * 8 + 5] << 16) |
               ((uint64_t)data[i * 8 + 6] << 8)  |
               ((uint64_t)data[i * 8 + 7]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];
    }

    uint64_t a = ctx->state[0];
    uint64_t b = ctx->state[1];
    uint64_t c = ctx->state[2];
    uint64_t d = ctx->state[3];
    uint64_t e = ctx->state[4];
    uint64_t f = ctx->state[5];
    uint64_t g = ctx->state[6];
    uint64_t h = ctx->state[7];

    for (int i = 0; i < 80; i++) {
        uint64_t t1 = h + Sigma1(e) + Ch(e, f, g) + K512[i] + w[i];
        uint64_t t2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha512_init(le_sha512_ctx_t* ctx)
{
    ctx->state[0] = 0x6a09e667f3bcc908ULL;
    ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL;
    ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL;
    ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL;
    ctx->state[7] = 0x5be0cd19137e2179ULL;
    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

static void sha512_update(le_sha512_ctx_t* ctx, const uint8_t* data, size_t len)
{
    size_t left = (size_t)(ctx->count[0] & 127);
    ctx->count[0] += (uint64_t)len;
    if (ctx->count[0] < (uint64_t)len) {
        ctx->count[1]++;
    }

    if (left > 0) {
        size_t to_copy = 128 - left;
        if (len < to_copy) to_copy = len;
        memcpy(ctx->buffer + left, data, to_copy);
        left += to_copy;
        data += to_copy;
        len -= to_copy;
        if (left == 128) {
            sha512_transform(ctx, ctx->buffer);
        }
    }

    while (len >= 128) {
        sha512_transform(ctx, data);
        data += 128;
        len -= 128;
    }

    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }
}

static void sha512_final(le_sha512_ctx_t* ctx, uint8_t out[64])
{
    uint8_t pad[256];
    size_t left = (size_t)(ctx->count[0] & 127);
    size_t pad_len = (left < 112) ? (112 - left) : (240 - left);

    memset(pad, 0, pad_len + 16);
    pad[0] = 0x80;

    uint64_t bits_lo = ctx->count[0] << 3;
    uint64_t bits_hi = (ctx->count[1] << 3) | (ctx->count[0] >> 61);

    for (int i = 0; i < 8; i++) {
        pad[pad_len + i]     = (uint8_t)(bits_hi >> (56 - i * 8));
        pad[pad_len + 8 + i] = (uint8_t)(bits_lo >> (56 - i * 8));
    }

    sha512_update(ctx, pad, pad_len + 16);

    for (int i = 0; i < 8; i++) {
        out[i * 8 + 0] = (uint8_t)(ctx->state[i] >> 56);
        out[i * 8 + 1] = (uint8_t)(ctx->state[i] >> 48);
        out[i * 8 + 2] = (uint8_t)(ctx->state[i] >> 40);
        out[i * 8 + 3] = (uint8_t)(ctx->state[i] >> 32);
        out[i * 8 + 4] = (uint8_t)(ctx->state[i] >> 24);
        out[i * 8 + 5] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 8 + 6] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 8 + 7] = (uint8_t)(ctx->state[i]);
    }
}

void le_crypto_sha512(uint8_t out_hash[64], const uint8_t* in, size_t in_len)
{
    le_sha512_ctx_t ctx;
    sha512_init(&ctx);
    sha512_update(&ctx, in, in_len);
    sha512_final(&ctx, out_hash);
}

/* ========================================================================== */
/* Ed25519 Curve and Scalar Operations (TweetNaCl-based)                       */
/* ========================================================================== */

typedef int64_t gf[16];

static const gf gf0 = {0};
static const gf gf1 = {1};
static const gf D = {
    0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
    0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203
};
static const gf D2 = {
    0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
    0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406
};
static const gf X = {
    0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
    0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169
};
static const gf Y = {
    0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
    0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666
};
static const gf I = {
    0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
    0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83
};

static const uint64_t L[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
    0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10
};

static void car25519(gf o)
{
    for (int i = 0; i < 16; i++) {
        o[i] += (1LL << 16);
        int64_t c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}

static void sel25519(gf p, gf q, int b)
{
    int64_t mask = ~(b - 1);
    for (int i = 0; i < 16; i++) {
        int64_t t = mask & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void pack25519(uint8_t* o, const gf n)
{
    gf m, t;
    memcpy(t, n, sizeof(gf));
    car25519(t);
    car25519(t);
    car25519(t);
    for (int j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (int i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        int b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }
    for (int i = 0; i < 16; i++) {
        o[2 * i] = (uint8_t)t[i];
        o[2 * i + 1] = (uint8_t)(t[i] >> 8);
    }
}

static int par25519(const gf a)
{
    uint8_t d_bytes[32];
    pack25519(d_bytes, a);
    return d_bytes[0] & 1;
}

static void unpack25519(gf o, const uint8_t* n)
{
    for (int i = 0; i < 16; i++) {
        o[i] = (int64_t)n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
    }
    o[15] &= 0x7fff;
}

static void A(gf o, const gf a, const gf b)
{
    for (int i = 0; i < 16; i++) o[i] = a[i] + b[i];
}

static void Z(gf o, const gf a, const gf b)
{
    for (int i = 0; i < 16; i++) o[i] = a[i] - b[i];
}

static void M(gf o, const gf a, const gf b)
{
    int64_t t[31] = {0};
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            t[i + j] += a[i] * b[j];
        }
    }
    for (int i = 0; i < 15; i++) {
        t[i] += 38 * t[i + 16];
    }
    for (int i = 0; i < 16; i++) {
        o[i] = t[i];
    }
    car25519(o);
    car25519(o);
}

static void S(gf o, const gf a)
{
    M(o, a, a);
}

static void inv25519(gf o, const gf i)
{
    gf c;
    memcpy(c, i, sizeof(gf));
    for (int a = 253; a >= 0; a--) {
        S(c, c);
        if (a != 2 && a != 4) M(c, c, i);
    }
    memcpy(o, c, sizeof(gf));
}

static int vn(const uint8_t* x, const uint8_t* y, int n)
{
    uint32_t d_diff = 0;
    for (int i = 0; i < n; i++) d_diff |= x[i] ^ y[i];
    return (1 & ((d_diff - 1) >> 8)) - 1;
}

static void add(gf p[4], gf q[4])
{
    gf a, b, c, dd, t, u, v, w;
    Z(a, p[1], p[0]);
    Z(t, q[1], q[0]);
    M(a, a, t);
    A(b, p[1], p[0]);
    A(t, q[1], q[0]);
    M(b, b, t);
    M(c, p[3], q[3]);
    M(c, c, D2);
    M(dd, p[2], q[2]);
    A(dd, dd, dd);
    Z(u, b, a);
    Z(v, dd, c);
    A(w, dd, c);
    A(t, b, a);
    M(p[0], u, v);
    M(p[1], t, w);
    M(p[2], v, w);
    M(p[3], u, t);
}

static void cswap(gf p[4], gf q[4], int b)
{
    for (int i = 0; i < 4; i++) {
        sel25519(p[i], q[i], b);
    }
}

static void scalarmult(gf p[4], gf q[4], const uint8_t* s)
{
    memcpy(p[0], gf0, sizeof(gf));
    memcpy(p[1], gf1, sizeof(gf));
    memcpy(p[2], gf1, sizeof(gf));
    memcpy(p[3], gf0, sizeof(gf));

    for (int i = 255; i >= 0; i--) {
        int b = (s[i / 8] >> (i & 7)) & 1;
        cswap(p, q, b);
        add(q, p);
        add(p, p);
        cswap(p, q, b);
    }
}

static void scalarbase(gf p[4], const uint8_t* s)
{
    gf q[4];
    memcpy(q[0], X, sizeof(gf));
    memcpy(q[1], Y, sizeof(gf));
    memcpy(q[2], gf1, sizeof(gf));
    M(q[3], X, Y);
    scalarmult(p, q, s);
}

static void modL(uint8_t* r, int64_t x[64])
{
    for (int i = 63; i >= 32; i--) {
        int64_t carry = 0;
        for (int j = i - 32; j < i - 12; j++) {
            x[j] += carry - 16 * x[i] * L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry << 8;
        }
        x[i - 12] += carry;
        x[i] = 0;
    }
    int64_t carry = 0;
    for (int j = 0; j < 32; j++) {
        x[j] += carry - (x[31] >> 4) * L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (int j = 0; j < 32; j++) {
        x[j] -= carry * L[j];
    }
    for (int i = 0; i < 32; i++) {
        x[i + 1] += x[i] >> 8;
        r[i] = (uint8_t)(x[i] & 255);
    }
}

static void reduce(uint8_t* r)
{
    int64_t x[64];
    for (int i = 0; i < 64; i++) x[i] = (uint64_t)r[i];
    memset(r, 0, 64);
    modL(r, x);
}

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

static int neq25519(const gf a, const gf b)
{
    uint8_t c[32], d_bytes[32];
    pack25519(c, a);
    pack25519(d_bytes, b);
    return vn(c, d_bytes, 32);
}

static void pow2523(gf o, const gf i)
{
    gf c;
    for (int a = 0; a < 16; a++) c[a] = i[a];
    for (int a = 250; a >= 0; a--) {
        S(c, c);
        if (a != 1) M(c, c, i);
    }
    for (int a = 0; a < 16; a++) o[a] = c[a];
}

static void pack(uint8_t* r, gf p[4])
{
    gf tx, ty, zi;
    inv25519(zi, p[2]);
    M(tx, p[0], zi);
    M(ty, p[1], zi);
    pack25519(r, ty);
    r[31] ^= (uint8_t)(par25519(tx) << 7);
}

static int unpackneg(gf r[4], const uint8_t p[32])
{
    gf t, chk, num, den, den2, den4, den6;
    memcpy(r[2], gf1, sizeof(gf));
    unpack25519(r[1], p);
    S(num, r[1]);
    M(den, num, D);
    Z(num, num, r[2]);
    A(den, r[2], den);

    S(den2, den);
    S(den4, den2);
    M(den6, den4, den2);
    M(t, den6, num);
    M(t, t, den);

    pow2523(t, t);
    M(t, t, num);
    M(t, t, den);
    M(t, t, den);
    M(r[0], t, den);

    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num)) M(r[0], r[0], I);

    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num)) return -1;

    if (par25519(r[0]) == (p[31] >> 7)) Z(r[0], gf0, r[0]);

    M(r[3], r[0], r[1]);
    return 0;
}

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

le_status_t le_crypto_ed25519_keypair_from_seed(uint8_t out_pk[LE_CRYPTO_PUBLIC_KEY_BYTES],
                                                uint8_t out_sk[LE_CRYPTO_SECRET_KEY_BYTES],
                                                const uint8_t seed[LE_CRYPTO_SEED_BYTES])
{
    if (!out_pk || !out_sk || !seed) return LE_ERR_NULL_PTR;

    uint8_t az[64];
    le_crypto_sha512(az, seed, 32);
    az[0] &= 248;
    az[31] &= 127;
    az[31] |= 64;

    gf p[4];
    scalarbase(p, az);
    pack(out_pk, p);

    memcpy(out_sk, seed, 32);
    memcpy(out_sk + 32, out_pk, 32);
    return LE_OK;
}

le_status_t le_crypto_ed25519_sign(uint8_t out_sig[LE_CRYPTO_SIGNATURE_BYTES],
                                   const uint8_t* msg,
                                   size_t msg_len,
                                   const uint8_t sk[LE_CRYPTO_SECRET_KEY_BYTES])
{
    if (!out_sig || !msg || !sk) return LE_ERR_NULL_PTR;

    uint8_t az[64];
    le_crypto_sha512(az, sk, 32);
    az[0] &= 248;
    az[31] &= 127;
    az[31] |= 64;

    le_sha512_ctx_t ctx;
    sha512_init(&ctx);
    sha512_update(&ctx, az + 32, 32);
    sha512_update(&ctx, msg, msg_len);
    uint8_t nonce[64];
    sha512_final(&ctx, nonce);
    reduce(nonce);

    gf r[4];
    scalarbase(r, nonce);
    pack(out_sig, r);

    sha512_init(&ctx);
    sha512_update(&ctx, out_sig, 32);
    sha512_update(&ctx, sk + 32, 32);
    sha512_update(&ctx, msg, msg_len);
    uint8_t hram[64];
    sha512_final(&ctx, hram);
    reduce(hram);

    int64_t x[64] = {0};
    for (int i = 0; i < 32; i++) x[i] = (uint64_t)nonce[i];
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            x[i + j] += (uint64_t)hram[i] * (uint64_t)az[j];
        }
    }
    modL(out_sig + 32, x);
    return LE_OK;
}

le_status_t le_crypto_ed25519_verify(const uint8_t sig[LE_CRYPTO_SIGNATURE_BYTES],
                                     const uint8_t* msg,
                                     size_t msg_len,
                                     const uint8_t pk[LE_CRYPTO_PUBLIC_KEY_BYTES])
{
    if (!sig || !msg || !pk) return LE_ERR_NULL_PTR;

    gf p[4], q[4];
    if (unpackneg(q, pk) != 0) return LE_ERR_SIGNATURE_INVALID;

    /* Compute h = SHA512(R, A, M) */
    le_sha512_ctx_t ctx;
    sha512_init(&ctx);
    sha512_update(&ctx, sig, 32);
    sha512_update(&ctx, pk, 32);
    sha512_update(&ctx, msg, msg_len);
    uint8_t hram[64];
    sha512_final(&ctx, hram);
    reduce(hram);

    /* Compute p = [h](-A) */
    scalarmult(p, q, hram);

    /* Compute q = [s]B */
    scalarbase(q, sig + 32);

    /* Add: p = [s]B + [h](-A) = [s]B - [h]A */
    add(p, q);

    /* Pack result to 32 bytes (R') */
    uint8_t rcheck[32];
    pack(rcheck, p);

    return (vn(rcheck, sig, 32) == 0) ? LE_OK : LE_ERR_SIGNATURE_INVALID;
}
