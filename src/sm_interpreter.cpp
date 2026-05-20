#pragma once

#include "sm_interpreter.hpp"
#include "sm_runtime.hpp"

namespace sm {

void interprete(Runtime* runtime, void* entrypoint) {
    DValue return_value;

    Frame initial_frame;

    runtime->stack.push_back(initial_frame);

    // TODO: switch/case
}

} // namespace sm
