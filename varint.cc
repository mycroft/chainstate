#include <string>

using namespace std;

typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

uint32_t read_varint(uint32_t n)            
{                         
    int len = sizeof(n);  
    int out = 0;          
    while (len >= 0) {    
        unsigned char ch = ((unsigned char*)&n)[(len--)-1];                                              
        if (len < 0) {    
            break;        
        }                 

        out = (out << 7) | (ch & 0x7f);             
        if (ch & 0x80) {  
            out ++;       
        }                 
    }                     

    return out;           
}                         

uint32_t get_next_varint(string& str)
{
    unsigned char c;
    uint32_t idx = 0;
    uint32_t n = 0;

    do {
        c = str[idx];
        n |= c;

        if (c < 0x80) {
            str = str.substr(++idx);
            return read_varint(n);
        } else {
            idx ++;
            n <<= 8;
        }
    } while(true);
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

