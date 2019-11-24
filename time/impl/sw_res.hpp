#pragma once

namespace wax {
    namespace stopwatch {
        namespace res {

            // Clock resolution for stopwatch::lap() as convenience labels.
            //
            static constexpr unsigned sec  = 1000000000U;    //< seconds
            static constexpr unsigned msec = 1000000U;       //< milliseconds
            static constexpr unsigned usec = 1000U;          //< microseconds
            static constexpr unsigned nsec = 1U;             //< nanoseconds

            namespace _impl {
                using suffix_entry = std::pair< decltype( sec ), const char * >;
                const static std::vector<suffix_entry> suffix_table {
                    { sec, "s"   },
                    { msec, "ms" },
                    { usec, "μs" },
                    { nsec, "ns" }
                };
            }

            //! @return A suitable suffix for the resolution, or the empty string if the resolution
            //! is not known.
            //
            //  @param res
            //
            static inline const char * const units( decltype( sec ) res ) {
                // Yes, linear search is hell-and-gone faster for small N.
                for ( const auto s : _impl::suffix_table )
                    if ( s.first == res ) return s.second;
                return "";
            }
        }
    }
}
