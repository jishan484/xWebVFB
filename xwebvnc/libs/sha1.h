#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include "base64.h"   // already defines base64_encode

char *generate_websocket_accept(const char *client_key);
char *generate_websocket_accept(const char *client_key) {
    const char *GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];

    // Combine client key and GUID
    snprintf(combined, sizeof(combined), "%s%s", client_key, GUID);

    // SHA1 hash (fixed type cast)
    SHA1((const unsigned char *)combined, strlen(combined), sha1_hash);

    // Base64 encode using your existing function
    char *accept_key = base64_encode((const char *)sha1_hash, SHA_DIGEST_LENGTH);

    return accept_key; // free later if malloced inside base64_encode
}
