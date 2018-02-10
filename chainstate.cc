#include <algorithm>
#include <iostream>
#include <string>

#include <leveldb/db.h>

#include "hex.hh"
#include "pubkey.hh"
#include "varint.hh"

using namespace std;

int main(void)
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

    leveldb::Status status = leveldb::DB::Open(options, "state", &db);
    assert(status.ok());

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        string idx = it->key().ToString();
        string value = it->value().ToString();

        if (idx[0] == 'B') {
            reverse(value.begin(), value.end());
            cout << "last block: " << string_to_hex(value) << endl;
            continue;
        }

        assert(idx[0] == 'C');

        string tx = idx.substr(1, 32);
        // XXX: There may be stuff after the last TX char, need to find out why. TX output number ?
        reverse(tx.begin(), tx.end());

        code = get_next_varint(value);
        nHeight = code >> 1;
        fCoinBase = code & 1;

        amount = decompress_amount(get_next_varint(value));

        if (dump) {
            cout << "TX: " << string_to_hex(tx) << endl;
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

        // Do not assert as there is a few invalid scripts out there..
        // assert(script_type >= 0x00 && script_type <= 0x05);
        old_value = value;
        value = value.substr(1);

        switch(script_type) {
            case 0x00:
                assert(value.size() == 20);
                addr = get_addr('\000', value);
                if (dump) {
                    cout << "DUP HASH160 " << value.size() << " " << addr << " EQUALVERIFY CHECKSIG" << endl; // P2PKH
                }
                break;

            case 0x01:
                assert(value.size() == 20);
                addr = get_addr('\005', value);

                if (dump) {
                    cout << "HASH160 " << value.size() << " " << addr << " EQUAL" << endl; // P2SH
                }
                break;

            case 0x02:
            case 0x03:
                assert(value.size() == 32);
                addr = get_addr('\000', str_to_ripesha(old_value));

                if (dump) {
                    cout << "PUSHDATA(33) " << addr << " CHECKSIG" << endl; // P2PK
                }
                break;

            case 0x04:
            case 0x05:
                memset(pub, 0, PUBLIC_KEY_SIZE);
                pubkey_decompress(script_type, value.c_str(), (unsigned char*) &pub, &publen);
                addr = get_addr('\000', str_to_ripesha(string((const char*)pub, PUBLIC_KEY_SIZE)));

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
                    exit(0);
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
            cout << string_to_hex(tx) << ";" << addr << ";" << amount << endl;
        } else {
            cout << string_to_hex(tx) << ";Invalid address or lost;" << amount << endl;
        }
    }

    delete it;
    delete db;
}
