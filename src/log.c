/***
 * A set of logging macro and functions that can be used.
 */

#include "log.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 


/* Prints `len` bytes starting from `bytes` to stderr */
void dump(const uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len;) {
        fprintf(stderr, "%04x:  ", (int) i);

        for (size_t j = 0; j < 16 && i + j < len; j++) {
            fprintf(stderr, "%02x ", bytes[i + j]);
        }
        fprintf(stderr, "\t");
        for (size_t j = 0; j < 16 && i < len; j++, i++) {
            fprintf(stderr,"%c ", bytes[i]);
        }
        fprintf(stderr,"\n");
    }
}
void dump_bits(const uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        fprintf(stderr, BYTE_TO_BINARY_PATTERN" ", BYTE_TO_BINARY(bytes[i]));
        
    }
    fprintf(stderr,"\n");
}