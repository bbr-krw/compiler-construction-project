#pragma once

#include "bytecode.hpp"

#include <istream>
#include <ostream>
#include <stdexcept>

// ── .dbc binary format ────────────────────────────────────────────────────────
// All integers are little-endian.  Layout:
//
//   Header  4 bytes: magic 'D' 'B' 'C' '\x01'
//
//   Proto (recursive):
//     name      u16 len + bytes
//     params    i32
//     regs      i32
//     cells     i32
//     nconsts   u32
//     consts[]  tag u8 then:
//                 0=none  (no payload)
//                 1=int   i64
//                 2=real  f64  (IEEE 754 little-endian)
//                 3=str   u32 len + bytes
//     nupvals   u32
//     upvals[]  name(u16+bytes)  is_local u8  idx i32
//     ncode     u32
//     code[]    op u8  a i32  b i32  c i32  d i32   (17 bytes each)
//     nprotos   u32
//     protos[]  <recursive Proto>

// Serialize a module to the .dbc binary format.
void write_module(const Module& m, std::ostream& os);

// Parse a module from the .dbc binary format.
// Throws std::runtime_error on malformed input.
Module read_module(std::istream& is);
