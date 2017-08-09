#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"
#include "sqlite3ext.h"


SQLITE_EXTENSION_INIT1


typedef struct bf_t {
  size_t size;
  void *bits;
} bf_t;


uint32_t murmurhash(
    const unsigned char *key,
    ssize_t nlen,
    uint32_t seed
) {
  uint32_t m = 0x5bd1e995;
  int r = 24;
  const unsigned char *data = key;
  uint32_t h = seed ^ nlen;

  while (nlen >= 4) {
    uint32_t k = *(uint32_t *)data;
    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    nlen -= 4;
  }

  switch(nlen) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
            h *= m;
  }

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h;
}


bf_t *bf_create(size_t size)
{
  bf_t *bf = (bf_t *)calloc(1, sizeof(*bf));
  bf->size = size;
  bf->bits = malloc(size);
  return bf;
}


uint32_t bf_bitindex(bf_t *bf, unsigned char *key, uint32_t seed)
{
  uint32_t h = murmurhash(key, strlen((const char *)key), seed);
  return h % (bf->size * 8);
}


static uint32_t seeds[4] = {0, 1337, 37, 0xabcd};


void bf_add(bf_t *bf, unsigned char *key) {
  uint8_t *bits = (uint8_t *)(bf->bits);
  uint32_t h;
  int i, pos;

  for (i = 0; i < 4; i++) {
    h = bf_bitindex(bf, key, seeds[i]);
    pos = h / 8;
    bits[pos] |= 1 << (h % 8);
  }
}


int bf_contains(bf_t *bf, unsigned char *key) {
  uint8_t *bits = (uint8_t *)(bf->bits);
  uint32_t h;
  int i, pos;

  for (i = 0; i < 4; i++) {
    h = bf_bitindex(bf, key, seeds[i]);
    pos = h / 8;
    if (!(bits[pos] & (1 << (h % 8)))) return 0;
  }

  return 1;
}


void bf_free(bf_t *bf) {
  free(bf->bits);
  free(bf);
}


static void sqlite3_murmurhash(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
  const char *key = 0;
  uint32_t hash;
  uint32_t seed = 0;

  if (sqlite3_value_type(argv[0]) == SQLITE_NULL) {
    sqlite3_result_null(context);
    return;
  }

  if (argc == 2) {
    if (sqlite3_value_type(argv[1]) != SQLITE_INTEGER) {
      sqlite3_result_error(context, "Seed must be an integer", -1);
      return;
    } else {
      seed = (uint32_t)sqlite3_value_int(argv[1]);
    }
  }

  key = (char *)sqlite3_value_text(argv[0]);
  if (!key) {
    sqlite3_result_error_nomem(context);
    return;
  }

  hash = murmurhash((const unsigned char *)key, strlen(key), seed);
  sqlite3_result_int64(context, (sqlite3_int64)hash);
}


static void bloom_step(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
  if (sqlite3_value_type(argv[0]) == SQLITE_NULL) return;

  if (argc != 1 && argc != 2) {
    sqlite3_result_error(context, "Arguments: key [size]", -1);
    return;
  }

  size_t size;
  bf_t *bf;
  unsigned char *key;

  if (argc == 2) {
    size = (size_t)sqlite3_value_int(argv[1]);
  } else {
    size = 1024;
  }

  bf = sqlite3_aggregate_context(context, sizeof(*bf));
  if (!bf->bits) {
    bf->bits = malloc(size);
    if (!bf->bits) {
      sqlite3_result_error_nomem(context);
      return;
    }
    bf->size = size;
    memset(bf->bits, '\0', size);
  }

  key = (unsigned char *)sqlite3_value_text(argv[0]);
  bf_add(bf, key);
}


static void bloom_finalize(sqlite3_context *context) {
  bf_t *bf = sqlite3_aggregate_context(context, sizeof(*bf));
  void *out = malloc(bf->size);
  if (!out) {
    sqlite3_result_error_nomem(context);
    return;
  }

  memcpy(out, bf->bits, bf->size);
  sqlite3_result_blob(context, out, bf->size, free);
}


static void bloom_contains(
    sqlite3_context *context,
    int argc,
    sqlite3_value **argv
) {
  unsigned char *key;
  bf_t bf;

  key = (unsigned char *)sqlite3_value_text(argv[0]);
  bf.bits = (void *)sqlite3_value_blob(argv[1]);
  bf.size = sqlite3_value_bytes(argv[1]);

  sqlite3_result_int(context, bf_contains(&bf, key));
}


int sqlite3_extension_init(
    sqlite3 *db,
    char **pzErr,
    const sqlite3_api_routines *pApi
) {
  SQLITE_EXTENSION_INIT2(pApi);

  /* Basic implementation and variant that accepts seed. */
  sqlite3_create_function(db, "murmurhash", 1, SQLITE_ANY, 0,
      sqlite3_murmurhash, 0, 0);
  sqlite3_create_function(db, "murmurhash", 2, SQLITE_ANY, 0,
      sqlite3_murmurhash, 0, 0);

  sqlite3_create_function(db, "bloomfilter", 1, SQLITE_ANY, 0, 0, bloom_step,
                          bloom_finalize);
  sqlite3_create_function(db, "bloomfilter", 2, SQLITE_ANY, 0, 0, bloom_step,
                          bloom_finalize);

  sqlite3_create_function(db, "bloom_contains", 2, SQLITE_ANY, 0,
                          bloom_contains, 0, 0);

  return SQLITE_OK;
}
