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
            using type = ::wax::_impl::clock::_hw_type;
            static constexpr     type real    = ::wax::_impl::clock::real;
            namespace cpu {
                static constexpr type thread  = ::wax::_impl::clock::cpu::thread;
                static constexpr type proc    = ::wax::_impl::clock::cpu::proc;
            }
        }

        //! @class stopwatch::real
        //! @class stopwatch::cpu::thread
        //! @class stopwatch::cpu::proc

        using real = ::wax::_impl::stopwatch::base< clock::real >;
        namespace cpu {
            using thread = ::wax::_impl::stopwatch::base< clock::cpu::thread >;
            using proc   = ::wax::_impl::stopwatch::base< clock::cpu::proc   >;
        }
    }
}
