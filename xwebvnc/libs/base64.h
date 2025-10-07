#ifndef _BASE64_C
#define _BASE64_C

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef unsigned char BYTE;

// ----------------- BASE64 CHARSET -----------------
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// ----------------- UTILITIES -----------------
static inline int is_base64(BYTE c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

// ----------------- ENCODE -----------------
char *base64_encode(const char *buf, unsigned int bufLen);
char *base64_encode(const char *buf, unsigned int bufLen) {
    int i = 0, j = 0, k = 0;
    BYTE char_array_3[3], char_array_4[4];
    int out_len = 4 * ((bufLen + 2) / 3);
    char *ret = (char *)malloc(out_len + 1);
    if (!ret) return NULL;
    int idx = 0;

    while (bufLen--) {
        char_array_3[i++] = buf[k++];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret[idx++] = base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = 0;

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            ret[idx++] = base64_chars[char_array_4[j]];

        while (i++ < 3)
            ret[idx++] = '=';
    }
    ret[idx] = 0;
    return ret;
}

// ----------------- DECODE -----------------
typedef struct {
    BYTE *data;
    size_t size;
} BYTE_ARRAY;

BYTE_ARRAY base64_decode(const char *encoded_string);
BYTE_ARRAY base64_decode(const char *encoded_string) {
    int in_len = strlen(encoded_string);
    int i = 0, j = 0, in_ = 0;
    BYTE char_array_4[4], char_array_3[3];
    BYTE_ARRAY ret = {0};
    ret.data = (BYTE *)malloc(in_len * 3 / 4 + 1); // max possible
    ret.size = 0;
    if (!ret.data) return ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_++];
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                const char *p = strchr(base64_chars, char_array_4[i]);
                char_array_4[i] = p ? (BYTE)(p - base64_chars) : 0;
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++)
                ret.data[ret.size++] = char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        for (j = 0; j < 4; j++) {
            const char *p = strchr(base64_chars, char_array_4[j]);
            char_array_4[j] = p ? (BYTE)(p - base64_chars) : 0;
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; j < i - 1; j++)
            ret.data[ret.size++] = char_array_3[j];
    }

    return ret;
}

// ----------------- STRING TO HEX -----------------
char *string_to_hex(const char *input);
char *string_to_hex(const char *input) {
    static const char hexchars[] = "0123456789ABCDEF";
    size_t len = strlen(input);
    char *result = (char *)malloc(len * 2 + 1);
    if (!result) return NULL;

    for (size_t i = 0; i < len; i++) {
        result[i * 2] = hexchars[(unsigned char)input[i] >> 4];
        result[i * 2 + 1] = hexchars[(unsigned char)input[i] & 0x0F];
    }
    result[len * 2] = 0;
    return result;
}

#endif
