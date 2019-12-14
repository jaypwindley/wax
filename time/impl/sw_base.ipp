#pragma once

#include <time.h>
#include "sw_res.hpp"

namespace wax {
namespace _impl {

namespace clock {
    using _hw_type = clockid_t;
    static constexpr     _hw_type real    = CLOCK_REALTIME;
    namespace cpu {
        static constexpr _hw_type thread  = CLOCK_THREAD_CPUTIME_ID;
        static constexpr _hw_type proc    = CLOCK_PROCESS_CPUTIME_ID;
    }
}

namespace stopwatch {

template <clock::_hw_type clock_type>
class base
{
  public:

    base()
        : resolution( []() -> unsigned long {
                struct timespec grain { 0, 0 };
                (void) ::clock_getres( clock_type, &grain );
                return grain.tv_sec * 1000000000UL + grain.tv_nsec; }() )
    {
        this->reset();
    }

    //! @param fd File descriptor to write final timing.
    template< typename... other_args >
    base( const int fd, other_args... args )
        :
        base( args... )
    {
        this->fd = fd;
    }

    //! @param label Name for this stopwatch.
    template< typename... other_args >
    base( const char *label, other_args... args )
        :
        base( args... )
    {
        this->label = label;
    }

    ~base() {
        if ( fd >= 0 ) {
            using namespace ::wax::stopwatch;
            auto f = ::fdopen( fd, "a" );
            if ( f ) {
                static constexpr auto dflt_res = res::msec;
                ::fprintf(
                    f, "%s: %.3f %s\n",
                    label ? label : "<anon>", lap<dflt_res>(), res::units( dflt_res ) );
                ::fclose( f );
            }
        }
    }

    //! @brief Reset the clock.
    //  @return 0 on success, -1 on failure.  Errno set to cause of failure.
    int reset() noexcept( true ) {
        return ::clock_gettime( clock_type, &start );
    }

    //! @return Current lap time in microseconds.
    //  @param Template parameter is the desired resolution of the value returned.
    //
    // @todo Do something clever with the resolution so that it doesn't report more resolution than
    // is actually available.
    //
    template< unsigned divisor = 1UL >
    float lap() const noexcept( true ) {
        struct timespec stop { start.tv_sec, start.tv_nsec };
        (void) ::clock_gettime( clock_type, &stop );
        return ( stop.tv_sec - start.tv_sec ) * 1000000000
            + ( stop.tv_nsec - start.tv_nsec ) / (float) divisor;
    }

    //! @return The label associated with this stopwatch, or nullptr if there is no name;
    const char * const name() const noexcept( true ) { return this->label; }

    //! @return The resolution of the stopwatch in nanoseconds.
    unsigned long res() const noexcept( true ) { return resolution; }

  private:

    int                  fd           {       -1 };
    struct timespec      start        {     0, 0 };
    const char          *label        { nullptr  };
    const unsigned long  resolution   {        1 };
};

}
}
}
