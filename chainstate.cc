#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <leveldb/db.h>

#include "hex.hh"
#include "pubkey.hh"
#include "varint.hh"

using namespace std;

typedef struct prefix {
    const char *name;
    unsigned char pubkey_prefix;
    unsigned char script_prefix;
} prefix_t;

int main(int argc, char **argv)
{
    leveldb::DB* db;
    leveldb::Options options;

    uint32_t code, fCoinBase, nHeight;
    uint64_t amount;

    unsigned char pub[PUBLIC_KEY_SIZE];
    size_t publen = PUBLIC_KEY_SIZE;

    string addr;
    bool dump = false;
    unsigned char script_type;
    string old_value;

    options.create_if_missing = false;

    // Can be found in src/chainparams.cpp, base58Prefixes
    prefix_t prefixes[] = {
        { "bitcoin", 0, 5 },
        { "bitcoin-testnet", 111, 196 },
        { "litecoin", 48, 50 }, // Also uses 48, 50
        { "litecoin-testnet", 111, 58 }, // Also uses 111, 58
        { "dashcore", 76, 16 },
        { "dashcore-testnet", 140, 19 },
        { NULL, 255, 255 }
    };

    prefix_t current_prefix = prefixes[0];

    if (argc == 2) {
        int idx = 0;

        do {
            if (0 == strcmp(argv[1], current_prefix.name)) {
                break;
            }

            idx ++;
            current_prefix = prefixes[idx];

        } while (current_prefix.name != NULL);

        if (current_prefix.name == NULL) {
            cerr << "Could not find coin named " << argv[1] << endl;
            exit(1);
        }
    }

    leveldb::Status status = leveldb::DB::Open(options, "state", &db);
    assert(status.ok());

    // Getting obfuscation key
    const string OBFUSCATE_KEY_KEY("\016\000obfuscate_key\79", 15);
    const unsigned int OBFUSCATE_KEY_NUM_BYTES = 8;
    string obfuscate_key_str;
    vector<unsigned char>obfuscate_key = vector<unsigned char>(OBFUSCATE_KEY_NUM_BYTES, '\000');

    if (dump) {
        cout << "Obfuscation key: " << string_to_hex(OBFUSCATE_KEY_KEY) << endl;
    }

    leveldb::Status s = db->Get(leveldb::ReadOptions(), OBFUSCATE_KEY_KEY, &obfuscate_key_str);
    if (s.ok()) {
        // First byte: key size.
        obfuscate_key_str = obfuscate_key_str.substr(1);
        cerr << "using obfuscation key " << string_to_hex(obfuscate_key_str) << endl;
        obfuscate_key = vector<unsigned char>(obfuscate_key_str.begin(), obfuscate_key_str.end());
    } else {
        if (s.IsNotFound()) {
            cerr << "obfuscation key not found... Please check bitcoin's log and report if bitcoin's obfuscation key not 0000000000000000." << endl;
        }
    }

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        string idx = it->key().ToString();
        string value = it->value().ToString();
        string tx;
        int txn = 0;

        if (idx[0] == 'B') {
            reverse(value.begin(), value.end());
            cerr << "last block: " << string_to_hex(value) << endl << endl;

            continue;
        }

        if (idx[0] != 'C') {
            cerr << "Record type " << int(idx[0]) << " is not handled." << endl;
            cerr << "Index: " << string_to_hex(idx) << endl;
            cerr << "Value: " << string_to_hex(value) << endl << endl;
            continue;
        }

        assert(idx[0] == 'C');

        // Parsing key/idx.

        tx = idx.substr(1, 32);
        reverse(tx.begin(), tx.end());

        if (idx.length() > 32) {
            string tx_num = idx.substr(33);
            txn = get_next_varint(tx_num);
        }

        // Parsing value.
        for (size_t i = 0; i < value.length(); i ++) {
            value[i] ^= obfuscate_key[i % obfuscate_key.size()];
        }

        code = get_next_varint(value);
        nHeight = code >> 1;
        fCoinBase = code & 1;

        amount = decompress_amount(get_next_varint(value));

        if (dump) {
            cout << "TX: " << string_to_hex(tx) << ":" << txn << endl;
            cout << "VA: " << string_to_hex(value) << endl;

            cout <<
                "IsCoinBase: " << fCoinBase <<
                " | " <<
                "Height: " << nHeight <<
                " | " <<
                "Amount: " << amount <<
                endl;
        }

        // Uncompress Script
        // see src/compressor.cpp:88
        script_type = value[0];

        if (dump) {
            cout << "Script type: " << (int)script_type << endl;
        }

        // Do not assert as there is a few invalid scripts out there..
        // assert(script_type >= 0x00 && script_type <= 0x05);
        old_value = value;
        value = value.substr(1);

        switch(script_type) {
            case 0x00:
                assert(value.size() == 20);
                addr = get_addr(current_prefix.pubkey_prefix, value);
                if (dump) {
                    cout << "DUP HASH160 " << value.size() << " " << addr << " EQUALVERIFY CHECKSIG" << endl; // P2PKH
                }
                break;

            case 0x01:
                assert(value.size() == 20);
                addr = get_addr(current_prefix.script_prefix, value);

                if (dump) {
                    cout << "HASH160 " << value.size() << " " << addr << " EQUAL" << endl; // P2SH
                }
                break;

            case 0x02:
            case 0x03:
                assert(value.size() == 32);
                addr = get_addr(current_prefix.pubkey_prefix, str_to_ripesha(old_value));

                if (dump) {
                    cout << "PUSHDATA(33) " << addr << " CHECKSIG" << endl; // P2PK
                }
                break;

            case 0x04:
            case 0x05:
                memset(pub, 0, PUBLIC_KEY_SIZE);
                pubkey_decompress(script_type, value.c_str(), (unsigned char*) &pub, &publen);
                addr = get_addr(current_prefix.pubkey_prefix, str_to_ripesha(string((const char*)pub, PUBLIC_KEY_SIZE)));

                if (dump) {
                    cout << "PUSHDATA(65) " << addr << " CHECKSIG" << endl; // P2PK
                }
                break;

            case 0x1c:
            case 0x28:
                // P2WPKH / P2WSH
                addr = rebuild_bech32(value);

                if (addr == string()) {
                    cout << "Invalid segwit ?" << endl;
                    cout << "TX: " << string_to_hex(tx) << endl;
                }

                if (dump) {
                    cout << "P2WSH "<< addr << endl;
                }

                break;

            default:
                cerr << "Invalid output: " << string_to_hex(tx) << " " << string_to_hex(old_value) << " " << amount << endl;
        }

        if (dump) {
            cout << endl;
        }

        if (addr != string()) {
            cout << string_to_hex(tx) << ";" << txn << ";" << addr << ";" << amount << endl;
        } else {
            cout << string_to_hex(tx) << ";Invalid address or lost;" << amount << endl;
        }
    }

    delete it;
    delete db;
}
