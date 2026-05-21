#include "bytecode.hpp"

namespace sm {

uint32_t packLock(Location loc) {
  return *reinterpret_cast<uint32_t*>(&loc);
}

Location unpackLock(uint32_t data) {
  return *reinterpret_cast<Location*>(&data);
}

std::ostream& operator<< (std::ostream& os, const Location& loc) {
  switch (loc.type) {
    case LOCAL:
      os << "Loc(" << loc.index << ")";
      break;
    case ARGUMENT:
      os << "Arg(" << loc.index << ")";
      break;
    case CAPTURED:
      os << "Capture(" << loc.index << ")";
      break;
  }
  return os;
}

} // namespace sm
