#pragma once

#include "sm_runtime.hpp"

namespace sm {

void interprete(Runtime* runtime, const BcFile& bcFile, std::ostream& out);

} // namespace sm
