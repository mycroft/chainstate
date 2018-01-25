#include <algorithm>
#include <iostream>
#include <string>

#include <leveldb/db.h>

using namespace std;

string string_to_hex(const string&);
string hex_to_string(const string&);
uint32_t get_next_varint(string&);
uint64_t decompress_amount(uint64_t);

int main(void)
{
    leveldb::DB* db;
    leveldb::Options options;

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

        cout << "TX: " << string_to_hex(tx) << endl;
        cout << "VA: " << string_to_hex(value) << endl;

        uint32_t code = get_next_varint(value);    
        uint32_t fCoinBase;
        uint32_t nHeight;
        uint64_t amount;

        nHeight = code >> 1;
        fCoinBase = code & 1;

        amount = decompress_amount(get_next_varint(value));

        cout << 
            "IsCoinBase: " << fCoinBase <<
            " | " << 
            "Height: " << nHeight << 
            " | " <<
            "Amount: " << amount <<
            endl;

        // Uncompress Script
        // see src/compressor.cpp:88
        unsigned char script_type = value[0];

        // Do not assert as there is a few invalid scripts out there..
        // assert(script_type >= 0x00 && script_type <= 0x05);

        value = value.substr(1);

        switch(script_type) {
            case 0x00:
                assert(value.size() == 20);
                cout << "DUP HASH160 " << value.size() << " " << string_to_hex(value) << " EQUALVERIFY CHECKSIG" << endl;
                break;
            case 0x01:
                assert(value.size() == 20);
                cout << "HASH160 " << value.size() << " " << string_to_hex(value) << " EQUAL" << endl;
                break;
            case 0x02:
            case 0x03:
                assert(value.size() == 32);
                cout << value.size() << " " << string_to_hex(value) << " CHECKSIG" << endl;
                break;
            case 0x04:
            case 0x05:
                cout << 65 << "Uncompress pubkey(" << string_to_hex(value) << ") CHECKSIG" << endl;
                break;
            default:
                cout << "Invalid output..." << endl;
        }

        cout << endl;
    }

    delete db;
}
