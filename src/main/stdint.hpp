#pragma once

#include <cstdint>

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

static_assert(sizeof(uint8)  == 1, "uint8 is not 1 byte");
static_assert(sizeof(int8)   == 1, "int8 is not 1 byte");
static_assert(sizeof(uint16) == 2, "uint16 is not 2 bytes");
static_assert(sizeof(int16)  == 2, "int16 is not 2 bytes");
static_assert(sizeof(uint32) == 4, "uint32 is not 4 bytes");
static_assert(sizeof(int32)  == 4, "int32 is not 4 bytes");
