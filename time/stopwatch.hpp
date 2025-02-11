#pragma once

#include <stdio.h>
#include <utility>
#include <vector>
#include "impl/sw_base.ipp"

namespace wax {

//! @namespace stopwatch
//  @brief Real and process-time stopwatches with scoping semantics.
//
namespace stopwatch {

namespace clock {
using type = ::wax::__impl::clock::_hw_type;
static constexpr type real = ::wax::__impl::clock::real;

namespace cpu {
static constexpr type thread  = ::wax::__impl::clock::cpu::thread;
static constexpr type proc    = ::wax::__impl::clock::cpu::proc;
} // namespace cpu
} // namespace clock

//! @class stopwatch::real
//! @class stopwatch::cpu::thread
//! @class stopwatch::cpu::proc

using real = ::wax::__impl::stopwatch::base< clock::real >;
namespace cpu {
using thread = ::wax::__impl::stopwatch::base< clock::cpu::thread >;
using proc   = ::wax::__impl::stopwatch::base< clock::cpu::proc   >;
} // namespace cpu

} // namespace stopwatch
} // namespace wax
