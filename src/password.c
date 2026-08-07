#include "header.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#define ATM_GETPID _getpid
#else
#include <unistd.h>
#define ATM_GETPID getpid
#endif

#define PBKDF2_ITERATIONS 100000U
#define PBKDF2_SALT_LEN 16U
#define SHA256_LEN 32U

typedef struct {
    uint8_t data[64];
    uint32_t data_len;
    uint64_t bit_len;
    uint32_t state[8];
} Sha256;

static const uint32_t k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t rotr(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32U - count));
}

static void transform(Sha256 *ctx, const uint8_t data[64]) {
    uint32_t m[64];
    for (uint32_t i = 0U; i < 16U; ++i) {
        uint32_t j = i * 4U;
        m[i] = ((uint32_t)data[j] << 24U) | ((uint32_t)data[j + 1U] << 16U) |
               ((uint32_t)data[j + 2U] << 8U) | (uint32_t)data[j + 3U];
    }
    for (uint32_t i = 16U; i < 64U; ++i) {
        uint32_t s0 = rotr(m[i - 15U], 7U) ^ rotr(m[i - 15U], 18U) ^ (m[i - 15U] >> 3U);
        uint32_t s1 = rotr(m[i - 2U], 17U) ^ rotr(m[i - 2U], 19U) ^ (m[i - 2U] >> 10U);
        m[i] = m[i - 16U] + s0 + m[i - 7U] + s1;
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    for (uint32_t i = 0U; i < 64U; ++i) {
        uint32_t s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + k[i] + m[i];
        uint32_t s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
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

static void sha256_init(Sha256 *ctx) {
    ctx->data_len = 0U;
    ctx->bit_len = 0U;
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
}

static void sha256_update(Sha256 *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0U; i < len; ++i) {
        ctx->data[ctx->data_len++] = data[i];
        if (ctx->data_len == 64U) {
            transform(ctx, ctx->data);
            ctx->bit_len += 512U;
            ctx->data_len = 0U;
        }
    }
}

static void sha256_final(Sha256 *ctx, uint8_t hash[SHA256_LEN]) {
    uint32_t i = ctx->data_len;
    ctx->data[i++] = 0x80U;

    if (i > 56U) {
        while (i < 64U) {
            ctx->data[i++] = 0U;
        }
        transform(ctx, ctx->data);
        i = 0U;
    }

    while (i < 56U) {
        ctx->data[i++] = 0U;
    }

    ctx->bit_len += (uint64_t)ctx->data_len * 8U;
    for (uint32_t shift = 0U; shift < 8U; ++shift) {
        ctx->data[63U - shift] = (uint8_t)(ctx->bit_len >> (shift * 8U));
    }
    transform(ctx, ctx->data);

    for (uint32_t word = 0U; word < 8U; ++word) {
        hash[word * 4U] = (uint8_t)(ctx->state[word] >> 24U);
        hash[word * 4U + 1U] = (uint8_t)(ctx->state[word] >> 16U);
        hash[word * 4U + 2U] = (uint8_t)(ctx->state[word] >> 8U);
        hash[word * 4U + 3U] = (uint8_t)ctx->state[word];
    }
}

static void sha256_bytes(const uint8_t *data, size_t len, uint8_t hash[SHA256_LEN]) {
    Sha256 ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

static void hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
                        uint8_t output[SHA256_LEN]) {
    uint8_t key_block[64] = {0};
    if (key_len > sizeof(key_block)) {
        sha256_bytes(key, key_len, key_block);
    } else {
        memcpy(key_block, key, key_len);
    }

    uint8_t inner_pad[64];
    uint8_t outer_pad[64];
    for (size_t i = 0U; i < 64U; ++i) {
        inner_pad[i] = (uint8_t)(key_block[i] ^ 0x36U);
        outer_pad[i] = (uint8_t)(key_block[i] ^ 0x5cU);
    }

    Sha256 ctx;
    uint8_t inner[SHA256_LEN];
    sha256_init(&ctx);
    sha256_update(&ctx, inner_pad, sizeof(inner_pad));
    sha256_update(&ctx, data, data_len);
    sha256_final(&ctx, inner);

    sha256_init(&ctx);
    sha256_update(&ctx, outer_pad, sizeof(outer_pad));
    sha256_update(&ctx, inner, sizeof(inner));
    sha256_final(&ctx, output);
}

static void pbkdf2_sha256(const char *password, const uint8_t salt[PBKDF2_SALT_LEN], uint32_t iterations,
                          uint8_t output[SHA256_LEN]) {
    uint8_t block[PBKDF2_SALT_LEN + 4U];
    memcpy(block, salt, PBKDF2_SALT_LEN);
    block[PBKDF2_SALT_LEN] = 0U;
    block[PBKDF2_SALT_LEN + 1U] = 0U;
    block[PBKDF2_SALT_LEN + 2U] = 0U;
    block[PBKDF2_SALT_LEN + 3U] = 1U;

    uint8_t u[SHA256_LEN];
    hmac_sha256((const uint8_t *)password, strlen(password), block, sizeof(block), u);
    memcpy(output, u, SHA256_LEN);

    for (uint32_t round = 1U; round < iterations; ++round) {
        uint8_t next[SHA256_LEN];
        hmac_sha256((const uint8_t *)password, strlen(password), u, sizeof(u), next);
        memcpy(u, next, sizeof(u));
        for (size_t i = 0U; i < SHA256_LEN; ++i) {
            output[i] ^= u[i];
        }
    }
}

static void hex_encode(const uint8_t *input, size_t len, char *output) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0U; i < len; ++i) {
        output[i * 2U] = digits[input[i] >> 4U];
        output[i * 2U + 1U] = digits[input[i] & 0x0fU];
    }
    output[len * 2U] = '\0';
}

static int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static bool hex_decode(const char *input, size_t bytes, uint8_t *output) {
    for (size_t i = 0U; i < bytes; ++i) {
        int hi = hex_value(input[i * 2U]);
        int lo = hex_value(input[i * 2U + 1U]);
        if (hi < 0 || lo < 0) return false;
        output[i] = (uint8_t)((hi << 4) | lo);
    }
    return input[bytes * 2U] == '\0';
}

static bool secure_equal(const uint8_t *left, const uint8_t *right, size_t len) {
    uint8_t diff = 0U;
    for (size_t i = 0U; i < len; ++i) {
        diff |= (uint8_t)(left[i] ^ right[i]);
    }
    return diff == 0U;
}

static void generate_salt(uint8_t salt[PBKDF2_SALT_LEN]) {
    static uint64_t counter = 0U;
    struct {
        time_t wall;
        clock_t cpu;
        long pid;
        uintptr_t address;
        uint64_t sequence;
    } seed;
    seed.wall = time(NULL);
    seed.cpu = clock();
    seed.pid = (long)ATM_GETPID();
    seed.address = (uintptr_t)(void *)salt;
    seed.sequence = ++counter;

    uint8_t digest[SHA256_LEN];
    sha256_bytes((const uint8_t *)&seed, sizeof(seed), digest);
    memcpy(salt, digest, PBKDF2_SALT_LEN);
}

static void legacy_sha256_string(const char *password, char output[ATM_PASSWORD_LEN]) {
    uint8_t hash[SHA256_LEN];
    char hex[SHA256_LEN * 2U + 1U];
    sha256_bytes((const uint8_t *)password, strlen(password), hash);
    hex_encode(hash, sizeof(hash), hex);
    (void)snprintf(output, ATM_PASSWORD_LEN, "sha256:%s", hex);
}

void hash_password(const char *password, char output[ATM_PASSWORD_LEN]) {
    uint8_t salt[PBKDF2_SALT_LEN];
    uint8_t derived[SHA256_LEN];
    char salt_hex[PBKDF2_SALT_LEN * 2U + 1U];
    char hash_hex[SHA256_LEN * 2U + 1U];

    generate_salt(salt);
    pbkdf2_sha256(password, salt, PBKDF2_ITERATIONS, derived);
    hex_encode(salt, sizeof(salt), salt_hex);
    hex_encode(derived, sizeof(derived), hash_hex);
    (void)snprintf(output, ATM_PASSWORD_LEN, "pbkdf2-sha256$%u$%s$%s", PBKDF2_ITERATIONS, salt_hex, hash_hex);
}

bool password_needs_upgrade(const char *stored) {
    return strncmp(stored, "pbkdf2-sha256$", 14U) != 0;
}

bool password_matches(const char *password, const char *stored) {
    if (strncmp(stored, "pbkdf2-sha256$", 14U) == 0) {
        unsigned int iterations = 0U;
        char salt_hex[PBKDF2_SALT_LEN * 2U + 1U];
        char hash_hex[SHA256_LEN * 2U + 1U];
        char extra;
        int parsed = sscanf(stored, "pbkdf2-sha256$%u$%32[0-9a-fA-F]$%64[0-9a-fA-F]%c",
                            &iterations, salt_hex, hash_hex, &extra);
        if (parsed != 3 || iterations == 0U || iterations > 1000000U) {
            return false;
        }

        uint8_t salt[PBKDF2_SALT_LEN];
        uint8_t expected[SHA256_LEN];
        uint8_t actual[SHA256_LEN];
        if (!hex_decode(salt_hex, sizeof(salt), salt) || !hex_decode(hash_hex, sizeof(expected), expected)) {
            return false;
        }
        pbkdf2_sha256(password, salt, iterations, actual);
        return secure_equal(actual, expected, sizeof(actual));
    }

    if (strncmp(stored, "sha256:", 7U) == 0) {
        char legacy[ATM_PASSWORD_LEN];
        legacy_sha256_string(password, legacy);
        size_t len = strlen(stored);
        return len == strlen(legacy) && secure_equal((const uint8_t *)stored, (const uint8_t *)legacy, len);
    }

    size_t password_len = strlen(password);
    size_t stored_len = strlen(stored);
    return password_len == stored_len && secure_equal((const uint8_t *)password, (const uint8_t *)stored, password_len);
}
