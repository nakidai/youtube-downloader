#if 0
/usr/bin/env FILE="$0" sh -c '                      \
filename="/tmp/temp-$(date +%s%N)" &&               \
${CC:-/usr/bin/env cc} -w -o "$filename" $FILE &&   \
"$filename";                                        \
rm -f "$filename"'
exit
#endif

/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
  JSMN_UNDEFINED = 0,
  JSMN_OBJECT = 1 << 0,
  JSMN_ARRAY = 1 << 1,
  JSMN_STRING = 1 << 2,
  JSMN_PRIMITIVE = 1 << 3
} jsmntype_t;

enum jsmnerr {
  /* Not enough tokens were provided */
  JSMN_ERROR_NOMEM = -1,
  /* Invalid character inside JSON string */
  JSMN_ERROR_INVAL = -2,
  /* The string is not a full JSON packet, more bytes expected */
  JSMN_ERROR_PART = -3
};

/**
 * JSON token description.
 * type		type (object, array, string etc.)
 * start	start position in JSON data string
 * end		end position in JSON data string
 */
typedef struct jsmntok {
  jsmntype_t type;
  int start;
  int end;
  int size;
#ifdef JSMN_PARENT_LINKS
  int parent;
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string.
 */
typedef struct jsmn_parser {
  unsigned int pos;     /* offset in the JSON string */
  unsigned int toknext; /* next token to allocate */
  int toksuper;         /* superior token node, e.g. parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
JSMN_API void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each
 * describing
 * a single JSON object.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens);

#ifndef JSMN_HEADER
/**
 * Allocates a fresh unused token from the token pool.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
                                   const size_t num_tokens) {
  jsmntok_t *tok;
  if (parser->toknext >= num_tokens) {
    return NULL;
  }
  tok = &tokens[parser->toknext++];
  tok->start = tok->end = -1;
  tok->size = 0;
#ifdef JSMN_PARENT_LINKS
  tok->parent = -1;
#endif
  return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type,
                            const int start, const int end) {
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
                                const size_t len, jsmntok_t *tokens,
                                const size_t num_tokens) {
  jsmntok_t *token;
  int start;

  start = parser->pos;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    switch (js[parser->pos]) {
#ifndef JSMN_STRICT
    /* In strict mode primitive must be followed by "," or "}" or "]" */
    case ':':
#endif
    case '\t':
    case '\r':
    case '\n':
    case ' ':
    case ',':
    case ']':
    case '}':
      goto found;
    default:
                   /* to quiet a warning from gcc*/
      break;
    }
    if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
      parser->pos = start;
      return JSMN_ERROR_INVAL;
    }
  }
#ifdef JSMN_STRICT
  /* In strict mode primitive must be followed by a comma/object/array */
  parser->pos = start;
  return JSMN_ERROR_PART;
#endif

found:
  if (tokens == NULL) {
    parser->pos--;
    return 0;
  }
  token = jsmn_alloc_token(parser, tokens, num_tokens);
  if (token == NULL) {
    parser->pos = start;
    return JSMN_ERROR_NOMEM;
  }
  jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
  token->parent = parser->toksuper;
#endif
  parser->pos--;
  return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
                             const size_t len, jsmntok_t *tokens,
                             const size_t num_tokens) {
  jsmntok_t *token;

  int start = parser->pos;
  
  /* Skip starting quote */
  parser->pos++;
  
  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c = js[parser->pos];

    /* Quote: end of string */
    if (c == '\"') {
      if (tokens == NULL) {
        return 0;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
      }
      jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
      token->parent = parser->toksuper;
#endif
      return 0;
    }

    /* Backslash: Quoted symbol expected */
    if (c == '\\' && parser->pos + 1 < len) {
      int i;
      parser->pos++;
      switch (js[parser->pos]) {
      /* Allowed escaped symbols */
      case '\"':
      case '/':
      case '\\':
      case 'b':
      case 'f':
      case 'r':
      case 'n':
      case 't':
        break;
      /* Allows escaped symbol \uXXXX */
      case 'u':
        parser->pos++;
        for (i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0';
             i++) {
          /* If it isn't a hex character we have an error */
          if (!((js[parser->pos] >= 48 && js[parser->pos] <= 57) ||   /* 0-9 */
                (js[parser->pos] >= 65 && js[parser->pos] <= 70) ||   /* A-F */
                (js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
            parser->pos = start;
            return JSMN_ERROR_INVAL;
          }
          parser->pos++;
        }
        parser->pos--;
        break;
      /* Unexpected symbol */
      default:
        parser->pos = start;
        return JSMN_ERROR_INVAL;
      }
    }
  }
  parser->pos = start;
  return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens) {
  int r;
  int i;
  jsmntok_t *token;
  int count = parser->toknext;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c;
    jsmntype_t type;

    c = js[parser->pos];
    switch (c) {
    case '{':
    case '[':
      count++;
      if (tokens == NULL) {
        break;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) {
        return JSMN_ERROR_NOMEM;
      }
      if (parser->toksuper != -1) {
        jsmntok_t *t = &tokens[parser->toksuper];
#ifdef JSMN_STRICT
        /* In strict mode an object or array can't become a key */
        if (t->type == JSMN_OBJECT) {
          return JSMN_ERROR_INVAL;
        }
#endif
        t->size++;
#ifdef JSMN_PARENT_LINKS
        token->parent = parser->toksuper;
#endif
      }
      token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
      token->start = parser->pos;
      parser->toksuper = parser->toknext - 1;
      break;
    case '}':
    case ']':
      if (tokens == NULL) {
        break;
      }
      type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
      if (parser->toknext < 1) {
        return JSMN_ERROR_INVAL;
      }
      token = &tokens[parser->toknext - 1];
      for (;;) {
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) {
            return JSMN_ERROR_INVAL;
          }
          token->end = parser->pos + 1;
          parser->toksuper = token->parent;
          break;
        }
        if (token->parent == -1) {
          if (token->type != type || parser->toksuper == -1) {
            return JSMN_ERROR_INVAL;
          }
          break;
        }
        token = &tokens[token->parent];
      }
#else
      for (i = parser->toknext - 1; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) {
            return JSMN_ERROR_INVAL;
          }
          parser->toksuper = -1;
          token->end = parser->pos + 1;
          break;
        }
      }
      /* Error if unmatched closing bracket */
      if (i == -1) {
        return JSMN_ERROR_INVAL;
      }
      for (; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) {
          parser->toksuper = i;
          break;
        }
      }
#endif
      break;
    case '\"':
      r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
      if (r < 0) {
        return r;
      }
      count++;
      if (parser->toksuper != -1 && tokens != NULL) {
        tokens[parser->toksuper].size++;
      }
      break;
    case '\t':
    case '\r':
    case '\n':
    case ' ':
      break;
    case ':':
      parser->toksuper = parser->toknext - 1;
      break;
    case ',':
      if (tokens != NULL && parser->toksuper != -1 &&
          tokens[parser->toksuper].type != JSMN_ARRAY &&
          tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
        parser->toksuper = tokens[parser->toksuper].parent;
#else
        for (i = parser->toknext - 1; i >= 0; i--) {
          if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
            if (tokens[i].start != -1 && tokens[i].end == -1) {
              parser->toksuper = i;
              break;
            }
          }
        }
#endif
      }
      break;
#ifdef JSMN_STRICT
    /* In strict mode primitives are: numbers and booleans */
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case 't':
    case 'f':
    case 'n':
      /* And they must not be keys of the object */
      if (tokens != NULL && parser->toksuper != -1) {
        const jsmntok_t *t = &tokens[parser->toksuper];
        if (t->type == JSMN_OBJECT ||
            (t->type == JSMN_STRING && t->size != 0)) {
          return JSMN_ERROR_INVAL;
        }
      }
#else
    /* In non-strict mode every unquoted value is a primitive */
    default:
#endif
      r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
      if (r < 0) {
        return r;
      }
      count++;
      if (parser->toksuper != -1 && tokens != NULL) {
        tokens[parser->toksuper].size++;
      }
      break;

#ifdef JSMN_STRICT
    /* Unexpected char in strict mode */
    default:
      return JSMN_ERROR_INVAL;
#endif
    }
  }

  if (tokens != NULL) {
    for (i = parser->toknext - 1; i >= 0; i--) {
      /* Unmatched opened object or array */
      if (tokens[i].start != -1 && tokens[i].end == -1) {
        return JSMN_ERROR_PART;
      }
    }
  }

  return count;
}

/**
 * Creates a new parser based over a given buffer with an array of tokens
 * available.
 */
JSMN_API void jsmn_init(jsmn_parser *parser) {
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}

#endif /* JSMN_HEADER */

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */

/*
 * MIT License
 * 
 * Copyright (c) 2022 Lucas Müller
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef JSMN_FIND_H
#define JSMN_FIND_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JSMN_H
#error "jsmn-find.h should be included after jsmn.h"
#else
/** @brief JSON token description */
struct jsmnftok {
    /** start position in JSON data string */
    int pos;
    /** length of token in JSON data string */
    size_t len;
};

/** @brief JSON object */
typedef struct jsmnf_pair {
    /** JSON type @see `jsmntype_t` at jsmn.h */
    jsmntype_t type;
    /** amount of children currently filled in */
    int size;
    /** children threshold capacity */
    int capacity;
    /** this pair's fields */
    struct jsmnf_pair *fields;
    /** the key of the pair */
    struct jsmnftok k;
    /** the value of the pair */
    struct jsmnftok v;
    /** current state of this pair */
    int state;
} jsmnf_pair;

/** @brief Bucket (@ref jsmnf_pair) loader, keeps track of pair array
 *      position */
typedef struct jsmnf_loader {
    /** next pair to allocate */
    unsigned pairnext;
} jsmnf_loader;

/**
 * @brief Initialize a @ref jsmnf_loader
 *
 * @param[out] loader jsmnf_loader to be initialized
 */
JSMN_API void jsmnf_init(jsmnf_loader *loader);

/**
 * @brief Populate the @ref jsmnf_pair pairs from jsmn tokens
 *
 * @param[in,out] loader the @ref jsmnf_loader initialized with jsmnf_init()
 * @param[in] js the JSON data string
 * @param[in] tokens jsmn tokens initialized with jsmn_parse() /
 *      jsmn_parse_auto()
 * @param[in] num_tokens amount of tokens initialized with jsmn_parse() /
 *      jsmn_parse_auto()
 * @param[out] pairs jsmnf_pair pairs array
 * @param[in] num_pairs maximum amount of pairs provided
 * @return a `enum jsmnerr` value for error or the amount of `pairs` used
 */
JSMN_API int jsmnf_load(jsmnf_loader *loader,
                        const char *js,
                        const jsmntok_t tokens[],
                        unsigned num_tokens,
                        jsmnf_pair pairs[],
                        unsigned num_pairs);

/**
 * @brief Find a @ref jsmnf_pair token by its associated key
 *
 * @param[in] head a @ref jsmnf_pair object or array loaded at jsmnf_start()
 * @param[in] js the JSON data string
 * @param[in] key the key too be matched
 * @param[in] length length of the key too be matched
 * @return the @ref jsmnf_pair `head`'s field matched to `key`, or NULL if
 * not encountered
 */
JSMN_API jsmnf_pair *jsmnf_find(const jsmnf_pair *head,
                                const char *js,
                                const char key[],
                                int length);

/**
 * @brief Find a @ref jsmnf_pair token by its full key path
 *
 * @param[in] head a @ref jsmnf_pair object or array loaded at jsmnf_start()
 * @param[in] js the JSON data string
 * @param[in] path an array of key path strings, from least to highest depth
 * @param[in] depth the depth level of the last `path` key
 * @return the @ref jsmnf_pair `head`'s field matched to `path`, or NULL if
 * not encountered
 */
JSMN_API jsmnf_pair *jsmnf_find_path(const jsmnf_pair *head,
                                     const char *js,
                                     char *const path[],
                                     unsigned depth);

/**
 * @brief Populate and automatically allocate the @ref jsmnf_pair pairs from
 *      jsmn tokens
 * @brief jsmnf_load() counterpart that automatically allocates the necessary
 *      amount of pairs necessary for sorting the JSON tokens
 *
 * @param[in,out] loader the @ref jsmnf_loader initialized with jsmnf_init()
 * @param[in] js the JSON data string
 * @param[in] tokens jsmn tokens initialized with jsmn_parse() /
 *      jsmn_parse_auto()
 * @param[in] num_tokens amount of tokens initialized with jsmn_parse() /
 *      jsmn_parse_auto()
 * @param[out] p_pairs pointer to @ref jsmnf_pair to be dynamically increased
 *      @note must be `free()`'d once done being used
 * @param[in,out] num_pairs initial amount of pairs provided
 * @return a `enum jsmnerr` value for error or the amount of `pairs` used
 */
JSMN_API int jsmnf_load_auto(jsmnf_loader *loader,
                             const char *js,
                             const jsmntok_t tokens[],
                             unsigned num_tokens,
                             jsmnf_pair **p_pairs,
                             unsigned *num_pairs);

/**
 * @brief `jsmn_parse()` counterpart that automatically allocates the necessary
 *      amount of tokens necessary for parsing the JSON string
 *
 * @param[in,out] parser the `jsmn_parser` initialized with `jsmn_init()`
 * @param[in] js the JSON data string
 * @param[in] length the raw JSON string length
 * @param[out] p_tokens pointer to `jsmntok_t` to be dynamically increased
 *      @note must be `free()`'d once done being used
 * @param[in,out] num_tokens amount of tokens
 * @return a `enum jsmnerr` value for error or the amount of `tokens` used
 */
JSMN_API int jsmn_parse_auto(jsmn_parser *parser,
                             const char *js,
                             size_t length,
                             jsmntok_t **p_tokens,
                             unsigned *num_tokens);

/**
 * @brief Utility function for unescaping a Unicode string
 *
 * @param[out] buf destination buffer
 * @param[in] bufsize destination buffer size
 * @param[in] src source string to be unescaped
 * @param[in] length source string length
 * @return length of unescaped string if successful or a negative jsmn error
 *      code on failure
 */
JSMN_API long jsmnf_unescape(char buf[],
                             size_t bufsize,
                             const char src[],
                             size_t length);

#ifndef JSMN_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* key */
#define CHASH_KEY_FIELD     k
/* value */
#define CHASH_VALUE_FIELD   v
/* fields */
#define CHASH_BUCKETS_FIELD fields
/* members count */
#define CHASH_LENGTH_FIELD  size

/*
 * C-Ware License
 * 
 * Copyright (c) 2022, C-Ware
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Redistributions of modified source code must append a copyright notice in
 *    the form of 'Copyright <YEAR> <NAME>' to each modified source file's
 *    copyright notice, and the standalone license file if one exists.
 *
 * A "redistribution" can be constituted as any version of the source code
 * that is intended to comprise some other derivative work of this code. A
 * fork created for the purpose of contributing to any version of the source
 * does not constitute a truly "derivative work" and does not require listing.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CWARE_LIBCHASH_H
#define CWARE_LIBCHASH_H

#define CWARE_LIBCHASH_VERSION  "2.0.0"

/* How big heap-allocated hashtables are by default */
#ifndef CHASH_INITIAL_SIZE
#define CHASH_INITIAL_SIZE 10
#elif CHASH_INITIAL_SIZE <= 0 
        "chash_init: default length must be greater than 0"
#endif

/* Calculates the next size of the hashtable. */
#ifndef CHASH_RESIZE
#define CHASH_RESIZE(size) \
    ((size) * 1.3)
#endif

/* The threshold that, when passed, will cause a resize */
#ifndef CHASH_LOAD_THRESHOLD
#define CHASH_LOAD_THRESHOLD 0.8
#endif

/* The type that is used for counters; useful for aligning hashtable
 * length and capacity fields so type casting warnings do not appear */
#ifndef CHASH_COUNTER_TYPE
#define CHASH_COUNTER_TYPE  int
#endif

/* The name of the key field */
#ifndef CHASH_KEY_FIELD
#define CHASH_KEY_FIELD key
#endif

/* The name of the value field */
#ifndef CHASH_VALUE_FIELD
#define CHASH_VALUE_FIELD value
#endif

/* The name of the state field */
#ifndef CHASH_STATE_FIELD
#define CHASH_STATE_FIELD state
#endif

/* The name of the buckets field */
#ifndef CHASH_BUCKETS_FIELD
#define CHASH_BUCKETS_FIELD buckets
#endif

/* The name of the length field */
#ifndef CHASH_LENGTH_FIELD
#define CHASH_LENGTH_FIELD length
#endif

/* The name of the capacity field */
#ifndef CHASH_CAPACITY_FIELD
#define CHASH_CAPACITY_FIELD capacity
#endif

/* State enums */
#define CHASH_UNFILLED  0
#define CHASH_FILLED    1
#define CHASH_TOMBSTONE 2

/* Built-ins */

#define chash_string_hash(key, hash)                                          \
    5031;                                                                     \
    do {                                                                      \
        int __CHASH_HINDEX = 0;                                               \
                                                                              \
        for(__CHASH_HINDEX = 0; (key)[__CHASH_HINDEX] != '\0';                \
                                                          __CHASH_HINDEX++) { \
            (hash) = (((hash) << 1) + (hash)) + (key)[__CHASH_HINDEX];        \
        }                                                                     \
    } while(0)

#define chash_string_compare(cmp_a, cmp_b) \
    (strcmp((cmp_a), (cmp_b)) == 0)

#define chash_default_init(bucket, _key, _value) \
    (bucket).CHASH_KEY_FIELD = (_key);           \
    (bucket).CHASH_VALUE_FIELD = _value




















/* utility macros */

#define __chash_abs(x) \
    ((x) < 0 ? (x) * - 1 : (x))

#define __chash_hash(mod, _key, namespace)                   \
    __CHASH_HASH = namespace ## _HASH((_key), __CHASH_HASH); \
    __CHASH_HASH = __CHASH_HASH % (mod);                     \
    __CHASH_HASH = __chash_abs(__CHASH_HASH);

#define __chash_probe(hashtable, _key, namespace)                             \
  while(__CHASH_INDEX < (hashtable)->CHASH_CAPACITY_FIELD) {                  \
    if((hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_STATE_FIELD ==    \
                                                      CHASH_UNFILLED)         \
      break;                                                                  \
                                                                              \
    if((namespace ## _COMPARE((_key),                                         \
     (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_KEY_FIELD)) == 1) { \
                                                                              \
      __CHASH_INDEX = -1;                                                     \
      break;                                                                  \
    }                                                                         \
                                                                              \
    __CHASH_HASH = (__CHASH_HASH + 1) % (hashtable)->CHASH_CAPACITY_FIELD;    \
    __CHASH_INDEX++;                                                          \
  }                                                                           \

#define __chash_probe_to_unfilled(mod, _key, buffer, namespace)              \
  while(1) {                                                                 \
    if(buffer[__CHASH_HASH].CHASH_STATE_FIELD != CHASH_FILLED)               \
      break;                                                                 \
                                                                             \
    if((namespace ## _COMPARE((_key), buffer[__CHASH_HASH].CHASH_KEY_FIELD)) \
                                                                      == 1)  \
      break;                                                                 \
                                                                             \
    __CHASH_HASH = (__CHASH_HASH + 1) % mod;                                 \
  }                                                                          \

#define __chash_resize(hashtable, namespace)                                  \
do {                                                                          \
  CHASH_COUNTER_TYPE __CHASH_INDEX = 0;                                       \
  namespace ## _BUCKET *__CHASH_BUCKETS = NULL;                               \
  CHASH_COUNTER_TYPE __CHASH_NEXT_SIZE = (CHASH_COUNTER_TYPE)                 \
                          CHASH_RESIZE((hashtable)->CHASH_CAPACITY_FIELD);    \
                                                                              \
  if((namespace ## _HEAP) == 0) {                                             \
    if((hashtable)->CHASH_LENGTH_FIELD !=                                     \
                                       (hashtable)->CHASH_CAPACITY_FIELD) {   \
        break;                                                                \
    }                                                                         \
                                                                              \
    fprintf(stderr, "__chash_resize: hashtable is full. could not resize"     \
                    " (%s:%i)\n", __FILE__, __LINE__);                        \
    abort();                                                                  \
  }                                                                           \
                                                                              \
  if((double) (hashtable)->CHASH_LENGTH_FIELD /                               \
     (double) (hashtable)->CHASH_CAPACITY_FIELD  < CHASH_LOAD_THRESHOLD)      \
    break;                                                                    \
                                                                              \
  __CHASH_BUCKETS = malloc((size_t) (__CHASH_NEXT_SIZE                        \
                           * ((CHASH_COUNTER_TYPE)                            \
                               sizeof(namespace ## _BUCKET))));               \
  memset(__CHASH_BUCKETS, 0, ((size_t) (__CHASH_NEXT_SIZE                     \
                           * ((CHASH_COUNTER_TYPE)                            \
                           sizeof(namespace ## _BUCKET)))));                  \
                                                                              \
  for(__CHASH_INDEX = 0; __CHASH_INDEX < (hashtable)->CHASH_CAPACITY_FIELD;   \
                                                         __CHASH_INDEX++) {   \
    namespace ## _BUCKET __CHASH_NEW_KEY_BUCKET;                              \
    memset(&__CHASH_NEW_KEY_BUCKET, 0, sizeof(namespace ## _BUCKET));         \
    namespace ## _INIT(__CHASH_NEW_KEY_BUCKET,                                \
        (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_INDEX].CHASH_KEY_FIELD,      \
        (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_INDEX].CHASH_VALUE_FIELD);   \
                                                                              \
    if((hashtable)->CHASH_BUCKETS_FIELD[__CHASH_INDEX].CHASH_STATE_FIELD      \
                                                             != CHASH_FILLED) \
      continue;                                                               \
                                                                              \
    __chash_hash(__CHASH_NEXT_SIZE, __CHASH_NEW_KEY_BUCKET.CHASH_KEY_FIELD,   \
                 namespace);                                                  \
    __chash_probe_to_unfilled(__CHASH_NEXT_SIZE,                              \
           (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_INDEX].CHASH_KEY_FIELD,   \
           __CHASH_BUCKETS, namespace)                                        \
                                                                              \
    __CHASH_BUCKETS[__CHASH_HASH] = __CHASH_NEW_KEY_BUCKET;                   \
    __CHASH_BUCKETS[__CHASH_HASH].CHASH_STATE_FIELD = CHASH_FILLED;           \
    __CHASH_HASH = 0;                                                         \
  }                                                                           \
                                                                              \
  free((hashtable)->CHASH_BUCKETS_FIELD);                                     \
  (hashtable)->CHASH_BUCKETS_FIELD = __CHASH_BUCKETS;                         \
  (hashtable)->CHASH_CAPACITY_FIELD = __CHASH_NEXT_SIZE;                      \
  __CHASH_HASH = 0;                                                           \
} while(0)

#define __chash_assert_nonnull(func, ptr)                            \
do {                                                                 \
    if((ptr) == NULL) {                                              \
        fprintf(stderr, #func ": " #ptr " cannot be null (%s:%i)\n", \
                __FILE__, __LINE__);                                 \
        abort();                                                     \
    }                                                                \
} while(0)

















/* operations */
#define chash_init(hashtable, namespace)                              \
    NULL;                                                             \
                                                                      \
    (hashtable) = malloc(sizeof((*(hashtable))));                     \
    (hashtable)->CHASH_LENGTH_FIELD = 0;                              \
    (hashtable)->CHASH_CAPACITY_FIELD = CHASH_INITIAL_SIZE;           \
    (hashtable)->CHASH_BUCKETS_FIELD = malloc(CHASH_INITIAL_SIZE      \
                      * sizeof(*((hashtable)->CHASH_BUCKETS_FIELD))); \
    memset((hashtable)->CHASH_BUCKETS_FIELD, 0,                       \
    sizeof(*((hashtable)->CHASH_BUCKETS_FIELD)) * CHASH_INITIAL_SIZE)

#define chash_init_stack(hashtable, buffer, _length, namespace)               \
    (*(hashtable));                                                           \
                                                                              \
    if((_length) <= 0) {                                                      \
        fprintf(stderr, "chash_init_stack: hashtable cannot have a maximum "  \
                        "length of 0 or less (%s:%i)\n", __FILE__, __LINE__); \
        abort();                                                              \
    }                                                                         \
                                                                              \
    __chash_assert_nonnull(chash_init_stack, buffer);                         \
                                                                              \
    (hashtable)->CHASH_LENGTH_FIELD = 0;                                      \
    (hashtable)->CHASH_CAPACITY_FIELD = _length;                              \
    (hashtable)->CHASH_BUCKETS_FIELD  = buffer 

#define chash_assign(hashtable, _key, _value, namespace)                     \
do {                                                                         \
  long __CHASH_HASH = 0;                                                     \
  namespace ## _BUCKET __CHASH_KEY_BUCKET;                                   \
  memset(&__CHASH_KEY_BUCKET, 0, sizeof(namespace ## _BUCKET));              \
  namespace ## _INIT(__CHASH_KEY_BUCKET, _key, _value);                      \
                                                                             \
  __chash_assert_nonnull(chash_assign, hashtable);                           \
  __chash_assert_nonnull(chash_assign, (hashtable)->CHASH_BUCKETS_FIELD);    \
  __chash_resize(hashtable, namespace);                                      \
  __chash_hash((hashtable)->CHASH_CAPACITY_FIELD, _key, namespace);          \
  __chash_probe_to_unfilled((hashtable)->CHASH_CAPACITY_FIELD,               \
                (_key), (hashtable)->CHASH_BUCKETS_FIELD, namespace)         \
                                                                             \
  if((hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_STATE_FIELD ==     \
                                                             CHASH_FILLED) { \
     namespace ## _FREE_VALUE(                                               \
          (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_VALUE_FIELD); \
  } else {                                                                   \
     (hashtable)->CHASH_LENGTH_FIELD++;                                      \
  }                                                                          \
                                                                             \
  (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH] = __CHASH_KEY_BUCKET;       \
  (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_STATE_FIELD =         \
                                                               CHASH_FILLED; \
} while(0)

#define chash_lookup(hashtable, _key, storage, namespace)                     \
storage;                                                                      \
                                                                              \
do {                                                                          \
  int __CHASH_INDEX = 0;                                                      \
  long __CHASH_HASH = 0;                                                      \
  namespace ## _BUCKET __CHASH_KEY_BUCKET;                                    \
  memset(&__CHASH_KEY_BUCKET, 0, sizeof(namespace ## _BUCKET));               \
  namespace ## _INIT(__CHASH_KEY_BUCKET, _key,                                \
                     __CHASH_KEY_BUCKET.CHASH_VALUE_FIELD);                   \
                                                                              \
  (void) __CHASH_KEY_BUCKET;                                                  \
                                                                              \
  __chash_assert_nonnull(chash_lookup, hashtable);                            \
  __chash_assert_nonnull(chash_lookup, (hashtable)->CHASH_BUCKETS_FIELD);     \
  __chash_hash((hashtable)->CHASH_CAPACITY_FIELD, _key, namespace);           \
  __chash_probe(hashtable, _key, namespace)                                   \
                                                                              \
  if(((hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_STATE_FIELD !=     \
              CHASH_FILLED) || __CHASH_INDEX != -1) {                         \
    fprintf(stderr, "chash_lookup: failed to find key in hashtable (%s:%i)"   \
                    "\n", __FILE__, __LINE__);                                \
    abort();                                                                  \
  }                                                                           \
                                                                              \
  storage = (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_VALUE_FIELD; \
} while(0)

#define chash_delete(hashtable, _key, namespace)                             \
do {                                                                         \
  int __CHASH_INDEX = 0;                                                     \
  long __CHASH_HASH = 0;                                                     \
                                                                             \
  __chash_assert_nonnull(chash_delete, hashtable);                           \
  __chash_assert_nonnull(chash_delete, (hashtable)->CHASH_BUCKETS_FIELD);    \
  __chash_hash((hashtable)->CHASH_CAPACITY_FIELD, _key, namespace);          \
  __chash_probe(hashtable, _key, namespace)                                  \
                                                                             \
  if(((hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_STATE_FIELD !=    \
                                    CHASH_FILLED) || __CHASH_INDEX != -1) {  \
    fprintf(stderr, "chash_delete: failed to find key in hashtable (%s:%i)"  \
                    "\n", __FILE__, __LINE__);                               \
    abort();                                                                 \
  }                                                                          \
                                                                             \
  namespace ## _FREE_KEY((hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH]      \
                         .CHASH_KEY_FIELD);                                  \
  namespace ## _FREE_VALUE(                                                  \
          (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_VALUE_FIELD); \
  (hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_STATE_FIELD =         \
                                                        CHASH_TOMBSTONE;     \
  (hashtable)->CHASH_LENGTH_FIELD--;                                         \
} while(0)

#define chash_contains(hashtable, _key, storage, namespace)                 \
1;                                                                          \
                                                                            \
do {                                                                        \
  int __CHASH_INDEX = 0;                                                    \
  long __CHASH_HASH = 0;                                                    \
                                                                            \
  __chash_assert_nonnull(chash_contents, hashtable);                        \
  __chash_assert_nonnull(chash_contents, (hashtable)->CHASH_BUCKETS_FIELD); \
  __chash_hash((hashtable)->CHASH_CAPACITY_FIELD, _key, namespace);         \
  __chash_probe(hashtable, _key, namespace)                                 \
                                                                            \
  if(((hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_STATE_FIELD !=   \
                                    CHASH_FILLED) || __CHASH_INDEX != -1) { \
    storage = 0;                                                            \
  }                                                                         \
} while(0)

#define chash_lookup_bucket(hashtable, _key, storage, namespace)           \
storage;                                                                   \
                                                                           \
do {                                                                       \
  CHASH_COUNTER_TYPE __CHASH_INDEX = 0;                                    \
  long __CHASH_HASH = 0;                                                   \
  namespace ## _BUCKET __CHASH_KEY_BUCKET;                                 \
  memset(&__CHASH_KEY_BUCKET, 0, sizeof(namespace ## _BUCKET));            \
  namespace ## _INIT(__CHASH_KEY_BUCKET, _key,                             \
                                  __CHASH_KEY_BUCKET.CHASH_VALUE_FIELD);   \
                                                                           \
  (void) __CHASH_KEY_BUCKET;                                               \
                                                                           \
  __chash_assert_nonnull(chash_lookup_bucket, hashtable);                  \
  __chash_assert_nonnull(chash_lookup_bucket,                              \
                                  (hashtable)->CHASH_BUCKETS_FIELD);       \
  __chash_hash((hashtable)->CHASH_CAPACITY_FIELD, _key, namespace);        \
  __chash_probe(hashtable, _key, namespace)                                \
                                                                           \
  if(((hashtable)->CHASH_BUCKETS_FIELD[__CHASH_HASH].CHASH_STATE_FIELD !=  \
                                   CHASH_FILLED) || __CHASH_INDEX != -1) { \
    fprintf(stderr, "chash_lookup_bucket: failed to find key in hashtable" \
                    "(%s:%i) \n", __FILE__, __LINE__);                     \
    abort();                                                               \
  }                                                                        \
                                                                           \
  storage = ((hashtable)->CHASH_BUCKETS_FIELD + __CHASH_HASH);             \
} while(0)

#define chash_free(hashtable, namespace)                                    \
do {                                                                        \
  __chash_assert_nonnull(chash_free, hashtable);                            \
  __chash_assert_nonnull(chash_free, (hashtable)->CHASH_BUCKETS_FIELD);     \
  (hashtable)->CHASH_CAPACITY_FIELD--;                                      \
                                                                            \
  while((hashtable)->CHASH_CAPACITY_FIELD != -1) {                          \
    if((hashtable)->CHASH_BUCKETS_FIELD[(hashtable)->CHASH_CAPACITY_FIELD]  \
                                      .CHASH_STATE_FIELD != CHASH_FILLED) { \
      (hashtable)->CHASH_CAPACITY_FIELD--;                                  \
      continue;                                                             \
    }                                                                       \
                                                                            \
    namespace ##_FREE_KEY(                                                  \
      (hashtable)->CHASH_BUCKETS_FIELD[(hashtable)->CHASH_CAPACITY_FIELD]   \
            .CHASH_KEY_FIELD);                                              \
    namespace ##_FREE_VALUE(                                                \
      (hashtable)->CHASH_BUCKETS_FIELD[(hashtable)->CHASH_CAPACITY_FIELD]   \
            .CHASH_VALUE_FIELD);                                            \
    (hashtable)->CHASH_CAPACITY_FIELD--;                                    \
    (hashtable)->CHASH_LENGTH_FIELD--;                                      \
  }                                                                         \
                                                                            \
  if((namespace ## _HEAP) == 1) {                                           \
    free((hashtable)->CHASH_BUCKETS_FIELD);                                 \
    free((hashtable));                                                      \
  }                                                                         \
} while(0);

#define chash_is_full(hashtable, namespace) \
    (((hashtable)->CHASH_LENGTH_FIELD) == ((hashtable)->CHASH_CAPACITY_FIELD))









/* Iterator logic */
#define chash_iter(hashtable, index, _key, _value)                           \
	for((index) = 0, (_key) = (hashtable)->CHASH_BUCKETS_FIELD[index].       \
                                                        CHASH_KEY_FIELD,     \
       (_value) = (hashtable)->CHASH_BUCKETS_FIELD[index].CHASH_VALUE_FIELD; \
       (index) < (hashtable)->CHASH_CAPACITY_FIELD;                          \
       (index) = ((index) < (hashtable)->CHASH_CAPACITY_FIELD)               \
                          ? ((index) + 1) : index,                           \
       (_key) = (hashtable)->CHASH_BUCKETS_FIELD[index].CHASH_KEY_FIELD,     \
       (_value) = (hashtable)->CHASH_BUCKETS_FIELD[index].CHASH_VALUE_FIELD, \
       (index) = (hashtable)->CHASH_CAPACITY_FIELD)

#define chash_skip(hashtable, index)                                        \
    if((hashtable)->CHASH_BUCKETS_FIELD[index].                             \
                                         CHASH_STATE_FIELD != CHASH_FILLED) \
        continue;

#endif

#define _jsmnf_key_hash(key, hash)                                            \
    5031;                                                                     \
    do {                                                                      \
        unsigned __CHASH_HINDEX;                                              \
        for (__CHASH_HINDEX = 0; __CHASH_HINDEX < (key).len;                  \
             ++__CHASH_HINDEX) {                                              \
            (hash) = (((hash) << 1) + (hash))                                 \
                     + _JSMNF_STRING_B[(key).pos + __CHASH_HINDEX];           \
        }                                                                     \
    } while (0)

/* compare jsmnf keys */
#define _jsmnf_key_compare(cmp_a, cmp_b)                                      \
    ((cmp_a).len == (cmp_b).len                                               \
     && !strncmp(_JSMNF_STRING_B + (cmp_a).pos,                               \
                 _JSMNF_STRING_A + (cmp_b).pos, (cmp_a).len))

#define _JSMNF_TABLE_HEAP   0
#define _JSMNF_TABLE_BUCKET struct jsmnf_pair
#define _JSMNF_TABLE_FREE_KEY(_key)
#define _JSMNF_TABLE_HASH(_key, _hash) _jsmnf_key_hash(_key, _hash)
#define _JSMNF_TABLE_FREE_VALUE(_value)
#define _JSMNF_TABLE_COMPARE(_cmp_a, _cmp_b) _jsmnf_key_compare(_cmp_a, _cmp_b)
#define _JSMNF_TABLE_INIT(_bucket, _key, _value)                              \
    chash_default_init(_bucket, _key, _value)

JSMN_API void
jsmnf_init(jsmnf_loader *loader)
{
    loader->pairnext = 0;
}

#define _JSMNF_STRING_A js
#define _JSMNF_STRING_B js

static int
_jsmnf_load_pairs(struct jsmnf_loader *loader,
                  const char *js,
                  struct jsmnf_pair *curr,
                  const struct jsmntok *tok,
                  unsigned num_tokens,
                  struct jsmnf_pair *pairs,
                  unsigned num_pairs)
{
    int offset = 0;

    if (!num_tokens) return 0;

    switch (tok->type) {
    case JSMN_STRING:
    case JSMN_PRIMITIVE:
        break;
    case JSMN_OBJECT:
    case JSMN_ARRAY: {
        const unsigned top_idx = loader->pairnext + (1 + tok->size),
                       bottom_idx = loader->pairnext;
        int ret;

        if (tok->size > (int)(num_pairs - bottom_idx)
            || top_idx > (num_pairs - bottom_idx))
        {
            return JSMN_ERROR_NOMEM;
        }

        loader->pairnext = top_idx;

        (void)chash_init_stack(curr, &pairs[bottom_idx], top_idx - bottom_idx,
                               _JSMNF_TABLE);

        if (JSMN_OBJECT == tok->type) {
            while (curr->size < tok->size) {
                const struct jsmntok *_key = tok + 1 + offset;
                struct jsmnftok key, value = { 0 };

                key.pos = _key->start;
                key.len = _key->end - _key->start;

                /* skip Key token */
                offset += 1;

                /* _key->size > 0 means either an Object or Array */
                if (_key->size > 0) {
                    const struct jsmntok *_value = tok + 1 + offset;
                    struct jsmnf_pair *found = NULL;

                    value.pos = _value->start;
                    value.len = _value->end - _value->start;

                    chash_assign(curr, key, value, _JSMNF_TABLE);
                    (void)chash_lookup_bucket(curr, key, found, _JSMNF_TABLE);

                    ret = _jsmnf_load_pairs(loader, js, found, _value,
                                            num_tokens - offset, pairs,
                                            num_pairs);
                    if (ret < 0) return ret;

                    offset += ret;
                }
                else {
                    chash_assign(curr, key, value, _JSMNF_TABLE);
                }
            }
        }
        else if (JSMN_ARRAY == tok->type) {
            for (; curr->size < tok->size; ++curr->size) {
                const struct jsmntok *_value = tok + 1 + offset;
                struct jsmnf_pair *element = curr->fields + curr->size;
                struct jsmnftok value;

                value.pos = _value->start;
                value.len = _value->end - _value->start;

                /* assign array element */
                element->v = value;
                element->state = CHASH_FILLED;
                /* unused for array elements */
                element->k.pos = 0;
                element->k.len = 0;

                ret = _jsmnf_load_pairs(loader, js, element, _value,
                                        num_tokens - offset, pairs, num_pairs);
                if (ret < 0) return ret;

                offset += ret;
            }
        }
        break;
    }
    default:
    case JSMN_UNDEFINED:
        fputs("Error: JSMN_UNDEFINED token detected, jsmn_parse() failure\n",
              stderr);
        return JSMN_ERROR_INVAL;
    }

    curr->type = tok->type;

    return offset + 1;
}

#undef _JSMNF_STRING_A
#undef _JSMNF_STRING_B

JSMN_API int
jsmnf_load(struct jsmnf_loader *loader,
           const char *js,
           const struct jsmntok tokens[],
           unsigned num_tokens,
           struct jsmnf_pair pairs[],
           unsigned num_pairs)
{
    int ret;

    if (!loader->pairnext) { /* first run, initialize pairs */
        static const struct jsmnf_pair blank_pair = { 0 };
        unsigned i = 0;

        for (; i < num_pairs; ++i)
            pairs[i] = blank_pair;
        /* root */
        pairs[0].v.pos = tokens->start;
        pairs[0].v.len = tokens->end - tokens->start;

        ++loader->pairnext;
    }

    ret = _jsmnf_load_pairs(loader, js, pairs, tokens, num_tokens, pairs,
                            num_pairs);

    /* TODO: rather than reseting pairnext keep the last 'bucket' ptr stored,
     *      so it can continue from the in the next try */
    if (ret < 0) loader->pairnext = 0;
    return ret;
}

#define _JSMNF_STRING_A js
#define _JSMNF_STRING_B key

JSMN_API struct jsmnf_pair *
jsmnf_find(const struct jsmnf_pair *head,
           const char *js,
           const char key[],
           int length)
{
    struct jsmnf_pair *found = NULL;

    if (!key || !head) return NULL;

    if (JSMN_OBJECT == head->type) {
        struct jsmnftok _key;
        int contains;

        _key.pos = 0;
        _key.len = length;

        contains = chash_contains(head, _key, contains, _JSMNF_TABLE);
        if (contains) {
            (void)chash_lookup_bucket(head, _key, found, _JSMNF_TABLE);
        }
    }
    else if (JSMN_ARRAY == head->type) {
        char *endptr;
        int idx = (int)strtol(key, &endptr, 10);
        if (endptr != key && idx < head->size) found = head->fields + idx;
    }
    return found;
}

#undef _JSMNF_STRING_A
#undef _JSMNF_STRING_B

JSMN_API struct jsmnf_pair *
jsmnf_find_path(const struct jsmnf_pair *head,
                const char *js,
                char *const path[],
                unsigned depth)
{
    const struct jsmnf_pair *iter = head;
    struct jsmnf_pair *found = NULL;
    unsigned i;

    for (i = 0; i < depth; ++i) {
        if (!iter) continue;
        found = jsmnf_find(iter, js, path[i], strlen(path[i]));
        if (!found) break;
        iter = found;
    }
    return found;
}

#define RECALLOC_OR_ERROR(ptr, prev_size)                                     \
    do {                                                                      \
        const unsigned new_size = *(prev_size)*2;                             \
        void *tmp = realloc((ptr), new_size * sizeof *(ptr));                 \
        if (!tmp) return JSMN_ERROR_NOMEM;                                    \
        (ptr) = tmp;                                                          \
        memset((ptr) + *(prev_size), 0,                                       \
               (new_size - *(prev_size)) * sizeof *(ptr));                    \
        *(prev_size) = new_size;                                              \
    } while (0)

JSMN_API int
jsmn_parse_auto(struct jsmn_parser *parser,
                const char *js,
                size_t length,
                struct jsmntok **p_tokens,
                unsigned *num_tokens)
{
    int ret;

    if (NULL == *p_tokens || 0 == *num_tokens) {
        *p_tokens = calloc(1, sizeof **p_tokens);
        *num_tokens = 1;
    }
    while (JSMN_ERROR_NOMEM
           == (ret = jsmn_parse(parser, js, length, *p_tokens, *num_tokens)))
    {
        RECALLOC_OR_ERROR(*p_tokens, num_tokens);
    }
    return ret;
}

JSMN_API int
jsmnf_load_auto(struct jsmnf_loader *loader,
                const char *js,
                const struct jsmntok tokens[],
                unsigned num_tokens,
                struct jsmnf_pair **p_pairs,
                unsigned *num_pairs)
{
    int ret;

    if (NULL == *p_pairs || 0 == *num_pairs) {
        *p_pairs = calloc(1, sizeof **p_pairs);
        *num_pairs = 1;
    }
    while (JSMN_ERROR_NOMEM
           == (ret = jsmnf_load(loader, js, tokens, num_tokens, *p_pairs,
                                *num_pairs)))
    {
        RECALLOC_OR_ERROR(*p_pairs, num_pairs);
    }
    return ret;
}

#undef RECALLOC_OR_ERROR

static int
_jsmnf_read_4_digits(char *s, const char *end, unsigned *p_hex)
{
    char buf[5] = { 0 };
    int i;

    if (end - s < 4) return JSMN_ERROR_PART;

    for (i = 0; i < 4; i++) {
        buf[i] = s[i];
        if (('0' <= s[i] && s[i] <= '9') || ('A' <= s[i] && s[i] <= 'F')
            || ('a' <= s[i] && s[i] <= 'f'))
        {
            continue;
        }
        return JSMN_ERROR_INVAL;
    }

    *p_hex = (unsigned)strtoul(buf, NULL, 16);

    return 4;
}

#define _JSMNF_UTF16_IS_FIRST_SURROGATE(c)                                    \
    (0xD800 <= (unsigned)c && (unsigned)c <= 0xDBFF)
#define _JSMNF_UTF16_IS_SECOND_SURROGATE(c)                                   \
    (0xDC00 <= (unsigned)c && (unsigned)c <= 0xDFFF)
#define _JSMNF_UTF16_JOIN_SURROGATE(c1, c2)                                   \
    (((((unsigned long)c1 & 0x3FF) << 10) | ((unsigned)c2 & 0x3FF)) + 0x10000)
#define _JSMNF_UTF8_IS_VALID(c)                                               \
    (((unsigned long)c <= 0x10FFFF)                                           \
     && ((unsigned long)c < 0xD800 || (unsigned long)c > 0xDFFF))
#define _JSMNF_UTF8_IS_TRAIL(c) (((unsigned char)c & 0xC0) == 0x80)
#define _JSMNF_UTF_ILLEGAL      0xFFFFFFFFu

static int
_jsmnf_utf8_trail_length(unsigned char c)
{
    if (c < 128) return 0;
    if (c < 194) return -1;
    if (c < 224) return 1;
    if (c < 240) return 2;
    if (c <= 244) return 3;
    return -1;
}

static int
_jsmnf_utf8_width(unsigned long value)
{
    if (value <= 0x7F) return 1;
    if (value <= 0x7FF) return 2;
    if (value <= 0xFFFF) return 3;
    return 4;
}

/* See RFC 3629
   Based on: http://www.w3.org/International/questions/qa-forms-utf-8 */
static unsigned long
_jsmnf_utf8_next(char **p, const char *end)
{
    unsigned char lead, tmp;
    int trail_size;
    unsigned long c;

    if (*p == end) return _JSMNF_UTF_ILLEGAL;

    lead = **p;
    (*p)++;

    /* First byte is fully validated here */
    trail_size = _jsmnf_utf8_trail_length(lead);

    if (trail_size < 0) return _JSMNF_UTF_ILLEGAL;

    /* Ok as only ASCII may be of size = 0 also optimize for ASCII text */
    if (trail_size == 0) return lead;

    c = lead & ((1 << (6 - trail_size)) - 1);

    /* Read the rest */
    switch (trail_size) {
    case 3:
        if (*p == end) return _JSMNF_UTF_ILLEGAL;
        tmp = **p;
        (*p)++;
        if (!_JSMNF_UTF8_IS_TRAIL(tmp)) return _JSMNF_UTF_ILLEGAL;
        c = (c << 6) | (tmp & 0x3F);
    /* fall-through */
    case 2:
        if (*p == end) return _JSMNF_UTF_ILLEGAL;
        tmp = **p;
        (*p)++;
        if (!_JSMNF_UTF8_IS_TRAIL(tmp)) return _JSMNF_UTF_ILLEGAL;
        c = (c << 6) | (tmp & 0x3F);
    /* fall-through */
    case 1:
        if (*p == end) return _JSMNF_UTF_ILLEGAL;
        tmp = **p;
        (*p)++;
        if (!_JSMNF_UTF8_IS_TRAIL(tmp)) return _JSMNF_UTF_ILLEGAL;
        c = (c << 6) | (tmp & 0x3F);
    }

    /* Check code point validity: no surrogates and valid range */
    if (!_JSMNF_UTF8_IS_VALID(c)) return _JSMNF_UTF_ILLEGAL;

    /* make sure it is the most compact representation */
    if (_jsmnf_utf8_width(c) != trail_size + 1) return _JSMNF_UTF_ILLEGAL;

    return c;
}

static long
_jsmnf_utf8_validate(char *p, const char *end)
{
    const char *start = p;
    while (p != end) {
        if (_jsmnf_utf8_next(&p, end) == _JSMNF_UTF_ILLEGAL)
            return JSMN_ERROR_INVAL;
    }
    return (long)(end - start);
}

static unsigned
_jsmnf_utf8_encode(unsigned long value, char utf8_seq[4])
{
    if (value <= 0x7F) {
        utf8_seq[0] = value;
        return 1;
    }
    if (value <= 0x7FF) {
        utf8_seq[0] = (value >> 6) | 0xC0;
        utf8_seq[1] = (value & 0x3F) | 0x80;
        return 2;
    }
    if (value <= 0xFFFF) {
        utf8_seq[0] = (value >> 12) | 0xE0;
        utf8_seq[1] = ((value >> 6) & 0x3F) | 0x80;
        utf8_seq[2] = (value & 0x3F) | 0x80;
        return 3;
    }
    utf8_seq[0] = (value >> 18) | 0xF0;
    utf8_seq[1] = ((value >> 12) & 0x3F) | 0x80;
    utf8_seq[2] = ((value >> 6) & 0x3F) | 0x80;
    utf8_seq[3] = (value & 0x3F) | 0x80;
    return 4;
}

static int
_jsmnf_utf8_append(unsigned long hex, char *buf_tok, const char *buf_end)
{
    char utf8_seq[4];
    unsigned utf8_seqlen = _jsmnf_utf8_encode(hex, utf8_seq);
    unsigned i;

    if ((buf_tok + utf8_seqlen) >= buf_end) return JSMN_ERROR_NOMEM;

    for (i = 0; i < utf8_seqlen; ++i)
        buf_tok[i] = utf8_seq[i];
    return utf8_seqlen;
}

#define BUF_PUSH(buf_tok, c, buf_end)                                         \
    do {                                                                      \
        if (buf_tok >= buf_end) return JSMN_ERROR_NOMEM;                      \
        *buf_tok++ = c;                                                       \
    } while (0)

JSMN_API long
jsmnf_unescape(char buf[], size_t bufsize, const char src[], size_t len)
{
    char *src_tok = (char *)src, *const src_end = src_tok + len;
    char *buf_tok = buf, *const buf_end = buf + bufsize;
    int second_surrogate_expected = 0;
    unsigned first_surrogate = 0;

    while (*src_tok && src_tok < src_end) {
        char c = *src_tok++;

        if (0 <= c && c <= 0x1F) return JSMN_ERROR_INVAL;

        if (c != '\\') {
            if (second_surrogate_expected) return JSMN_ERROR_INVAL;
            BUF_PUSH(buf_tok, c, buf_end);
            continue;
        }

        /* expects escaping but src is a well-formed string */
        if (!*src_tok || src_tok >= src_end) return JSMN_ERROR_PART;

        c = *src_tok++;

        if (second_surrogate_expected && c != 'u') return JSMN_ERROR_INVAL;

        switch (c) {
        case '"':
        case '\\':
        case '/':
            BUF_PUSH(buf_tok, c, buf_end);
            break;
        case 'b':
            BUF_PUSH(buf_tok, '\b', buf_end);
            break;
        case 'f':
            BUF_PUSH(buf_tok, '\f', buf_end);
            break;
        case 'n':
            BUF_PUSH(buf_tok, '\n', buf_end);
            break;
        case 'r':
            BUF_PUSH(buf_tok, '\r', buf_end);
            break;
        case 't':
            BUF_PUSH(buf_tok, '\t', buf_end);
            break;
        case 'u': {
            unsigned hex;
            int ret = _jsmnf_read_4_digits(src_tok, src_end, &hex);

            if (ret != 4) return ret;

            src_tok += ret;

            if (second_surrogate_expected) {
                if (!_JSMNF_UTF16_IS_SECOND_SURROGATE(hex))
                    return JSMN_ERROR_INVAL;

                ret = _jsmnf_utf8_append(
                    _JSMNF_UTF16_JOIN_SURROGATE(first_surrogate, hex), buf_tok,
                    buf_end);
                if (ret < 0) return ret;

                buf_tok += ret;

                second_surrogate_expected = 0;
            }
            else if (_JSMNF_UTF16_IS_FIRST_SURROGATE(hex)) {
                second_surrogate_expected = 1;
                first_surrogate = hex;
            }
            else {
                ret = _jsmnf_utf8_append(hex, buf_tok, buf_end);
                if (ret < 0) return ret;

                buf_tok += ret;
            }
        } break;
        default:
            return JSMN_ERROR_INVAL;
        }
    }
    return _jsmnf_utf8_validate(buf, buf_tok);
}

#undef BUF_PUSH

#endif /* JSMN_HEADER */
#endif /* JSMN_H */

#ifdef __cplusplus
}
#endif

#endif /* JSMN_FIND_H */



/* script source code */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lengthof(array) (sizeof(array) / sizeof(*array))

int main(int argc, char **argv)
{
    FILE *fp;
    int size;
    char *file, *config;

    jsmn_parser parser;
    jsmnf_loader loader;
    jsmntok_t *toks = NULL;
    jsmnf_pair *pairs = NULL;
    unsigned int num_tokens = 0, num_pairs = 0;

    jsmnf_pair *pair, *url_pair, *filename_pair;
    char *command;

    if (argc != 2)
    {
        fprintf(stderr, "usage: downloader <config>\n");
        exit(EXIT_FAILURE);
    }
    config = argv[1];

    if (!(fp = fopen(config, "r")))
    {
        fprintf(stderr, "cannot open %s: %s\n", config, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if ((fseek(fp, 0, SEEK_END)) < 0)
    {
        fprintf(stderr, "cannot read %s: %s\n", config, strerror(errno));
        exit(EXIT_FAILURE);
    }
    size = ftell(fp);
    rewind(fp);

    file = malloc(size);
    fread(file, 1, size, fp);
    if (ferror(fp))
    {
        fprintf(stderr, "cannot read %s: %s\n", config, strerror(errno));
        exit(EXIT_FAILURE);
    }
    fclose(fp);


    jsmn_init(&parser);
    if ((jsmn_parse_auto(&parser, file, strlen(file), &toks, &num_tokens)) <= 0)
    {
        fprintf(stderr, "cannot parse %s\n", config);
        exit(EXIT_FAILURE);
    }

    jsmnf_init(&loader);
    if ((jsmnf_load_auto(&loader, file, toks, num_tokens, &pairs, &num_pairs)) <= 0)
    {
        fprintf(stderr, "cannot parse %s\n", config);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < pairs->size; ++i)
    {
        pair = &pairs->fields[i];
        url_pair = &pair->fields[0];
        filename_pair = &pair->fields[1];

        asprintf(
            &command, 
            "yt-dlp %.*s -o %.*s.mp4 -f \"bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best\"",
            (int)url_pair->v.len, file + url_pair->v.pos,
            (int)filename_pair->v.len, file + filename_pair->v.pos
        );
        printf("%s\n", command);
        system(command);
        free(command);
    }

    free(toks);
    free(pairs);
}
