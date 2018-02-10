#include <iostream>

#include <string.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <libbase58.h>

#include <secp256k1.h>

#include "segwit_addr.h"

using namespace std;

void pubkey_decompress(unsigned int size, const char *input, unsigned char *pub, size_t *publen)
{
    secp256k1_context* secp256k1_context_verify = nullptr;
    secp256k1_pubkey pubkey;
    unsigned char vch[33] = {};
    vch[0] = size - 2;

    memcpy(&vch[1], input, 32);
    memset(&pubkey, 0, sizeof(pubkey));

    secp256k1_context_verify = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);

    if (!secp256k1_ec_pubkey_parse(secp256k1_context_verify, &pubkey, vch, 33)) {
        cerr << "Could not parse..." << endl;
    }

    secp256k1_ec_pubkey_serialize(secp256k1_context_verify, pub, publen, &pubkey, SECP256K1_EC_UNCOMPRESSED);

    secp256k1_context_destroy(secp256k1_context_verify);

    return;
}

void sha256(unsigned char *input, size_t s, unsigned char output[SHA256_DIGEST_LENGTH])
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input, s);
    SHA256_Final(output, &sha256);
}

void ripemd160(unsigned char *input, size_t s, unsigned char output[RIPEMD160_DIGEST_LENGTH])
{
    RIPEMD160_CTX ctx;
    RIPEMD160_Init(&ctx);
    RIPEMD160_Update(&ctx, input, s);
    RIPEMD160_Final(output, &ctx);
}

string get_addr(unsigned char networkbit, string value)
{
    char b58[1024];
    size_t b58len = 1024;
    unsigned char output[SHA256_DIGEST_LENGTH];

    value.insert(0, 1, networkbit);

    sha256((unsigned char*)value.c_str(), value.size(), output);
    sha256(output, SHA256_DIGEST_LENGTH, output);

    value.append((char *)output, 4);

    b58enc(b58, &b58len, value.c_str(), value.size());

    return string(b58);
}

string str_to_ripesha(string value)
{
    string v = value;

    unsigned char output[SHA256_DIGEST_LENGTH];
    unsigned char ripe_output[RIPEMD160_DIGEST_LENGTH];

    sha256((unsigned char*)v.c_str(), v.size(), output);
    ripemd160(output, SHA256_DIGEST_LENGTH, ripe_output);

    return string((const char*)ripe_output, RIPEMD160_DIGEST_LENGTH);
}

string rebuild_bech32(string value)
{
    int witver;
    size_t witprog_len;
    unsigned char witprog[256];
    unsigned char* scriptpubkey;
    int ret;
    char rebuild[93];

    witver = value[0];
    witprog_len = value[1];
    scriptpubkey = (unsigned char*)value.c_str();

    if(witver) {
        witver -= 0x50;
    }

    memcpy(witprog, scriptpubkey + 2, witprog_len);

    ret = segwit_addr_encode(rebuild, "bc", witver, witprog, witprog_len);
    if (ret != 1) {
        return string();
    }

    return string(rebuild);
}
