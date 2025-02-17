#pragma once

#include <mutex>

// Shorthands for standard mutexes and some helper types.

namespace wax {

using mutex = std::mutex;
using lock  = std::lock_guard<mutex>;
using ulock = std::unique_lock<mutex>;

//! @todo See whether we need to implement a barrier while we wait for C++20.

} // namespace wax
