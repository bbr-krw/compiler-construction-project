#include "bytecode.hpp"

namespace sm {

uint32_t packLock(Location loc) {
  return *reinterpret_cast<uint32_t*>(&loc);
}

Location unpackLock(uint32_t data) {
  return *reinterpret_cast<Location*>(&data);
}

} // namespace sm
