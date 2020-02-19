#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <leveldb/db.h>

#include "hex.hh"
#include "pubkey.hh"
#include "varint.hh"

#define DEFAULT_ADDRESS "Unknown Address"

using namespace std;

typedef struct prefix {
    const char *name;
    unsigned char pubkey_prefix[4];
    size_t pubkey_prefix_size;
    unsigned char script_prefix[4];
    size_t script_prefix_size;
} prefix_t;

int main(int argc, char **argv)
{
    leveldb::DB* db;
    leveldb::Options options;

    uint32_t code, fCoinBase, nHeight;
    uint64_t amount;

    unsigned char pub[PUBLIC_KEY_SIZE];
    size_t publen = PUBLIC_KEY_SIZE;

    string addr = DEFAULT_ADDRESS;
    bool dump = false;
    unsigned char script_type;
    string old_value;

    bool ret;

    options.create_if_missing = false;
    options.compression = leveldb::kNoCompression;

    // Can be found in src/chainparams.cpp, base58Prefixes
    prefix_t prefixes[] = {
        { "bitcoin",                { 0x00 }, 1, { 0x05 }, 1 },
        { "bitcoin-testnet",        { 0x6f }, 1, { 0xc4 }, 1 },
        { "bitcoin-abc",            { 0x00 }, 1, { 0x05 }, 1 },
        { "bitcoin-abc-testnet",    { 0x6f }, 1, { 0xc4 }, 1 },
        { "bitcoin-sv",             { 0x00 }, 1, { 0x05 }, 1 },
        { "bitcoin-sv-testnet",     { 0x6f }, 1, { 0xc4 }, 1 },
        { "litecoin",               { 0x30 }, 1, { 0x32 }, 1 }, // Also uses 48, 50
        { "litecoin-testnet",       { 0x6f }, 1, { 0x3a }, 1 }, // Also uses 111, 58
        { "dashcore",               { 0x4c }, 1, { 0x10 }, 1 },
        { "dashcore-testnet",       { 0x8c }, 1, { 0x13 }, 1 },
        { "dogecoin",               { 0x1e }, 1, { 0x16 }, 1 },
        { "bitcoin-private",        { 0x13, 0x25 }, 2, { 0x13, 0xaf }, 2 }, // b1 & bx
        { "bitcoin-private-testnet",        { 0x19, 0x57 }, 2, { 0x19, 0xe0 }, 2 }, // n1 & nx
        { "zcash",                  { 0x1c, 0xb8 }, 2, { 0x1c, 0xbd }, 2 }, // t1 & t3
        { "zcash-testnet",          { 0x1d, 0x25 }, 2, { 0x1c, 0xba }, 2 }, // tm & t2
        { "bitcoinz",               { 0x1c, 0xb8 }, 2, { 0x1c, 0xbd }, 2 },
        { NULL, {}, 0 , {}, 0 }
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

        if (idx[0] == 65 || idx[0] == 97 || idx[0] == 115) {
            // BitcoinPrivate
            continue;
        }

        if (idx[0] != 'C' && idx[0] != 'c') {
            cerr << "Record type " << int(idx[0]) << " is not handled." << endl;
            cerr << "Index: " << string_to_hex(idx) << endl;
            cerr << "Value: " << string_to_hex(value) << endl << endl;
            continue;
        }

        assert(idx[0] == 'C' || idx[0] == 'c');

        if(idx[0] == 'c') {
            std::vector<bool> vAvail(2, false);

            // old fashion parsing coins (dev for bitcoin-private or dogecoin)
            // Parsing value.
            for (size_t i = 0; i < value.length(); i ++) {
                value[i] ^= obfuscate_key[i % obfuscate_key.size()];
            }

            tx = idx.substr(1, 32);
            reverse(tx.begin(), tx.end());

            string orig_value = value;

            uint64_t version = get_next_varint(value);
            uint64_t code = get_next_varint(value);

            bool isCoinbase = code & 0x01;
            bool isVout0NotSpent = code & 0x02;
            bool isVout1NotSpent = code & 0x04;

            vAvail[0] = isVout0NotSpent;
            vAvail[1] = isVout1NotSpent;

            bool has_error = false;

            unsigned int nMaskCode = (code / 8) + ((code & 6) != 0 ? 0 : 1);
            uint64_t unspentnessbytes = code >> 3;

            // In case both bit 1 & bit 2 are unset, they encode N - 1.
            if (!isVout0NotSpent && !isVout1NotSpent) {
                unspentnessbytes ++;
            }

            if (dump) {
                cout << "========================================================================================" << endl;
                cout << "TX: " << string_to_hex(tx) << " | Version: " << (int)version << " | code: " << (int)code << " | maskcode: " << (int)nMaskCode << endl;
                cout << "coinbase: " << isCoinbase << " / " <<  isVout0NotSpent << "|" << isVout1NotSpent << " / N=" << (int)unspentnessbytes << endl;
                cout << "Value: " << string_to_hex(orig_value) << endl;
            }

            while (nMaskCode > 0) {
                unsigned char chAvail = 0;
                chAvail = value[0];
                value = value.substr(1);

                for (unsigned int p = 0; p < 8; p++) {
                    bool f = (chAvail & (1 << p)) != 0;
                    vAvail.push_back(f);
                }

                if (chAvail != 0)
                    nMaskCode --;
            }

            uint64_t vout_idx = 0;

            while(value.size() > 21 && false == has_error) {
                unsigned char type;
                // read vout (compact amount representation / special txout type / address uint160)

                for(; vAvail[vout_idx] == 0; ++vout_idx);

                amount = decompress_amount(get_next_varint(value));

                if(value.size() < 1) {
                    cerr << "ERROR: Can't read type. String too small while reading TX " << string_to_hex(tx) << endl;
                    break;
                }

                // special txout type pay-to-pubkey-hash
                type = value[0];

                if (dump) {
                    cout << "Amount: " << amount << " | Type: " << (int)type << endl;
                    // cout << string_to_hex(value) << endl;
                }

                switch(type) {
                    case 0x00:
                        assert(value.size() >= 20);
                        value = value.substr(1);
                        addr = get_addr(current_prefix.pubkey_prefix, current_prefix.pubkey_prefix_size, value.substr(0, 20));
                        value = value.substr(20);
                        break;
                    case 0x01:
                        assert(value.size() >= 20);
                        value = value.substr(1);
                        addr = get_addr(current_prefix.script_prefix, current_prefix.script_prefix_size, value.substr(0, 20));
                        value = value.substr(20);
                        break;
                    case 0x02:
                    case 0x03:
                        assert(value.size() >= 33);
                        addr = get_addr(current_prefix.pubkey_prefix, current_prefix.pubkey_prefix_size, str_to_ripesha(value, 33));
                        value = value.substr(33);
                        break;
                    case 0x04:
                    case 0x05:
                        assert(value.size() >= 33);
                        value = value.substr(1);
                        memset(pub, 0, PUBLIC_KEY_SIZE);
                        ret = pubkey_decompress(type, value.c_str(), (unsigned char*) &pub, &publen);
                        if(ret == false) {
                            cerr << "Could not parse compressed public key." << endl;
                        }

                        addr = get_addr(current_prefix.pubkey_prefix, current_prefix.pubkey_prefix_size, str_to_ripesha(string((const char*)pub, PUBLIC_KEY_SIZE)));
                        value = value.substr(33);
                        break;
                    case 0x4d:
                        // we have something like 4d 51 21 03 addr 21 20 ??? 52 ae
                        // lets grab the address and skip the remaining
                        assert(value.size() >= 72);
                        assert(value[0x03] == 0x02 || value[0x03] == 0x03);

                        value = value.substr(3);
                        addr = get_addr(current_prefix.pubkey_prefix, current_prefix.pubkey_prefix_size, str_to_ripesha(value, 33));
                        value = value.substr(33 + 36);
                        break;

                    case 0x6d:
                        assert(value.size() >= 72);
                        assert(value[0x03] == 0x04 || value[0x03] == 0x05);
                        value = value.substr(3);

                        memset(pub, 0, PUBLIC_KEY_SIZE);
//                        ret = pubkey_decompress(type, value.c_str(), (unsigned char*) &pub, &publen);
//                        if(ret == false) {
//                            cerr << "Could not parse compressed public key." << endl;
//                        }
                        addr = get_addr(current_prefix.pubkey_prefix, current_prefix.pubkey_prefix_size, str_to_ripesha(string(value.c_str(), PUBLIC_KEY_SIZE)));

                        value = value.substr(101);
                        break;

                    default:
                        cerr << "ERROR type: " << (int)type << " on TX: " << string_to_hex(tx) << " | Version: " << (int)version << endl;
                        cerr << "Value: " << string_to_hex(orig_value) << endl;
                        cerr << "Remaining: " << string_to_hex(value) << endl;

                        has_error = true;
                        break;
                }

                cout << string_to_hex(tx) << ";" << vout_idx << ";" << addr << ";" << amount << endl;
                vout_idx ++;
                addr = DEFAULT_ADDRESS;
            }
            continue;
        }

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
                addr = get_addr(current_prefix.pubkey_prefix, current_prefix.pubkey_prefix_size, value);
                if (dump) {
                    cout << "DUP HASH160 " << value.size() << " " << addr << " EQUALVERIFY CHECKSIG" << endl; // P2PKH
                }
                break;

            case 0x01:
                assert(value.size() == 20);
                addr = get_addr(current_prefix.script_prefix, current_prefix.script_prefix_size, value);

                if (dump) {
                    cout << "HASH160 " << value.size() << " " << addr << " EQUAL" << endl; // P2SH
                }
                break;

            case 0x02:
            case 0x03:
                assert(value.size() == 32);
                addr = get_addr(current_prefix.pubkey_prefix, current_prefix.pubkey_prefix_size, str_to_ripesha(old_value));

                if (dump) {
                    cout << "PUSHDATA(33) " << addr << " CHECKSIG" << endl; // P2PK
                }
                break;

            case 0x04:
            case 0x05:
                memset(pub, 0, PUBLIC_KEY_SIZE);
                pubkey_decompress(script_type, value.c_str(), (unsigned char*) &pub, &publen);
                addr = get_addr(current_prefix.pubkey_prefix, current_prefix.pubkey_prefix_size, str_to_ripesha(string((const char*)pub, PUBLIC_KEY_SIZE)));

                if (dump) {
                    cout << "PUSHDATA(65) " << addr << " CHECKSIG" << endl; // P2PK
                }
                break;

            case 0x1c:
            case 0x28:
                addr = string();

                // P2WPKH / P2WSH
                if(value[0] == 0x00 && ((script_type == 0x1c && value.size() == 22) || (script_type == 0x28 && value.size() == 34))) {
                    addr = rebuild_bech32(value);
                }

                if (addr == string()) {
                    cerr << "Invalid segwit ?" << endl;
                    cerr << "TX: " << string_to_hex(tx) << endl;
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
            cerr << string_to_hex(tx) << ";Invalid address or lost;" << amount << endl;
        }
    }

    delete it;
    delete db;
}
