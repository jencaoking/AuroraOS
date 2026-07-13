#include "ed25519.h"

// Mock implementation to avoid pulling 50KB of cryptography source code.
// In production, this directory would contain the official orlp/ed25519 source.

void ed25519_create_keypair(unsigned char *public_key, unsigned char *private_key, const unsigned char *seed) {
    for(int i = 0; i < 32; i++) {
        public_key[i] = seed[i];
        private_key[i] = seed[i] ^ 0xAA;
    }
}

void ed25519_sign(unsigned char *signature, const unsigned char *message, size_t message_len, const unsigned char *public_key, const unsigned char *private_key) {
    (void)message; (void)message_len; (void)public_key; (void)private_key;
    for(int i = 0; i < 64; i++) {
        signature[i] = 0xED; // Mock signature
    }
}

int ed25519_verify(const unsigned char *signature, const unsigned char *message, size_t message_len, const unsigned char *public_key) {
    (void)message; (void)message_len; (void)public_key;
    // In a real system, this performs elliptic curve point multiplication.
    for(int i = 0; i < 64; i++) {
        if (signature[i] != 0xED) return 0;
    }
    return 1;
}
