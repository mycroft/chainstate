#ifndef __VARINT_HH__
#define __VARINT_HH__

#include <stdint.h>

uint64_t get_next_varint(std::string&);
uint64_t decompress_amount(uint64_t);

#endif