#include <string>
#include "varint.hh"

using namespace std;

uint64_t get_next_varint(string& str)
{
    int len = 0;
    uint64_t out = 0;

    while(true) {
        unsigned char ch = str[len];

        out = (out << 7) | (ch & 0x7f);
        if (ch & 0x80)
            out ++;
        else {
            str = str.substr(++len);
            return out;
        }

        len ++;
    }
}


uint64_t compress_amount(uint64_t n)
{
    if (n == 0)
        return 0;
    int e = 0;
    while (((n % 10) == 0) && e < 9) {
        n /= 10;
        e++;
    }
    if (e < 9) {
        int d = (n % 10);
        // assert(d >= 1 && d <= 9);
        n /= 10;
        return 1 + (n*9 + d - 1)*10 + e;
    } else {
        return 1 + (n - 1)*10 + 9;
    }
}

uint64_t decompress_amount(uint64_t x)
{
    // x = 0  OR  x = 1+10*(9*n + d - 1) + e  OR  x = 1+10*(n - 1) + 9
    if (x == 0)
        return 0;
    x--;
    // x = 10*(9*n + d - 1) + e
    int e = x % 10;
    x /= 10;
    uint64_t n = 0;
    if (e < 9) {
        // x = 9*n + d - 1
        int d = (x % 9) + 1;
        x /= 9;
        // x = n
        n = x*10 + d;
    } else {
        n = x+1;
    }
    while (e) {
        n *= 10;
        e--;
    }
    return n;
}
