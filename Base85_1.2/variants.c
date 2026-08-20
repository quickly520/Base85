#include "variants.h"
#include <string.h>
#include <stdint.h>
static const uint8_t B85_ASCII85[85] = {
    '!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',
    ':',';','<','=','>','?','@',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '[','\\',']','^','_','`',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u'
};
static const uint8_t B85_Z85[85] = {
    '0','1','2','3','4','5','6','7','8','9',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '.','-',':','+','=','^','!','/','*','?','&','<','>','(',')',
    '[',']','{','}','@','%','$','#'
};
static const uint8_t B85_RFC1924[85] = {
    '0','1','2','3','4','5','6','7','8','9',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '!','#','$','%','&','(',')','*','+','-',';','<','=','>','?',
    '@','^','_','`','{','|','}','~'
};
const uint8_t *base85_get_charset(base85_variant_t variant) {
    switch (variant) {
        case BASE85_VARIANT_Z85: return B85_Z85;
        case BASE85_VARIANT_RFC1924: return B85_RFC1924;
        default: return B85_ASCII85;
    }
}
static int charset_len(base85_variant_t variant) {
    (void)variant;
    return 85;
}
int base85_is_valid_char(base85_variant_t variant, unsigned char c) {
    const uint8_t *charset = base85_get_charset(variant);
    int len = charset_len(variant);
    for (int i = 0; i < len; i++) {
        if (charset[i] == c) return 1;
    }
    return 0;
}
int base85_char_to_value(base85_variant_t variant, unsigned char c) {
    const uint8_t *charset = base85_get_charset(variant);
    int len = charset_len(variant);
    for (int i = 0; i < len; i++) {
        if (charset[i] == c) return i;
    }
    return -1;
}
int base85_encode_block_var(base85_variant_t variant, const uint8_t *in, uint8_t *out) {
    const uint8_t *charset = base85_get_charset(variant);
    uint32_t n = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
                 ((uint32_t)in[2] << 8) | (uint32_t)in[3];
    out[0] = charset[n / 52200625u];
    n %= 52200625u;
    out[1] = charset[n / 614125u];
    n %= 614125u;
    out[2] = charset[n / 7225u];
    n %= 7225u;
    out[3] = charset[n / 85u];
    out[4] = charset[n % 85u];
    return 0;
}
int base85_decode_block_var(base85_variant_t variant, const uint8_t *in, uint8_t *out) {
    uint64_t n = 0;
    for (int i = 0; i < 5; i++) {
        int v = base85_char_to_value(variant, in[i]);
        if (v < 0) return -1;
        n = n * 85u + (uint8_t)v;
    }
    out[0] = (uint8_t)(n >> 24);
    out[1] = (uint8_t)(n >> 16);
    out[2] = (uint8_t)(n >> 8);
    out[3] = (uint8_t)n;
    return 0;
}
