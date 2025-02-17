#pragma once

#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>

namespace wax {

using namespace std::chrono_literals;

//! @class thread_loop
//
// @brief Spawns and manages a thread that calls a function at regular
// intervals.
//
class thread_loop
{
 public:

  //! @typedef grain
  //
  // The granularity at which the thread wait clock operates.
  //
  using grain = std::chrono::microseconds;

  //! @typedef result
  //
  // The return value type of the service function. The service function may
  // alter the thread timing or signal other actions via this type.
  //
  // @var r_stop Return this to stop the thread
  // @var r_ok   Return this to keep the same wait interval
  //
  // Return a valued grain (see hz_to_grain()) to change the thread's wait time.
  //
  using            result =  grain;
  static constexpr result    r_stop       = grain::max();
  static constexpr result    r_ok         = grain::zero();

  //! @typdef func
  //
  // Singature of service function. The function is called at regular
  // intervals. It should return ok under normal conditions. This causes the
  // thread loop to wait an interval between the next call. To stop the thread,
  // it should return stop. If the thread is stopped via this method, it cannot
  // be restarted. To change the wait interval, it can return a new grain value
  // set to the desired interval;
  //
  using func = std::function<grain()>;

  //! @var dfl_wait The default wait interval in grains.
  //
  static constexpr unsigned dflt_wait_hz = 1000;

  //! @brief Convert a frequency in Hz to a grain
  //
  // @param hz [in] The frequency in Hz
  // @return the corresponding grain value
  //
  static inline grain
  hz_to_grain(unsigned hz) {
    //! @todo Rewrite the formula to use other numerators.
    static_assert(grain::period::num == 1, "thread granularity");
    return grain(grain::period::den / hz);
  }

  thread_loop() = default;

  //!
  // @param [in] svc_func The service function
  // @param [in] wait_hz  The desired interval in Hz
  // @param [in] subdiv   Number of times to run the loop before calling the
  //                      service function. For fast loops, the default = 1 is
  //                      the right answer. For long-period loops, set this to
  //                      a value such that wait_hz / subdiv is on the order of
  //                      hundreds of milliseconds, or the desired degree of
  //                      responsiveness in thread control.
  //
  thread_loop(
    func     svc_func,
    unsigned wait_hz = dflt_wait_hz,
    unsigned subdiv  = 1)
    :
    svc { svc_func },
    subdiv { subdiv },
    delay { hz_to_grain(wait_hz) / subdiv }
  {
    if (this->delay.count() == 0)
      throw std::range_error("thread_loop subdivision");
  }

  ~thread_loop() {
    try {
      this->stop();
    } catch (...) {}
  }

  //! @brief Start the thread loop.
  //
  // If the thread loop has already been started, this function has no effect.
  //
  void start() {
    if (th.joinable())
      return;
    this->should_stop = false;
    th = std::thread(
      [this] () -> void {
        while ( ! this->should_stop ) {
          if (--this->duty_cycle == 0) {
            this->duty_cycle = this->subdiv;
            const auto result = this->svc();
            if (result == r_stop)
              break;
            if (result > result::zero())
              this->delay = result;
          }
          std::this_thread::sleep_for(delay);
        }
      } );
  }

  //! @brief Stop the thread loop
  //
  // If the thread loop has already been stopped, this function has no effect.
  //
  void stop() {
    this->should_stop = true;
    std::this_thread::sleep_for(delay + 10us);
    if (th.joinable())
      th.join();
  }

 private:
  func         svc;
  bool         should_stop  { false };
  std::thread  th;
  unsigned     subdiv       {      1 };
  unsigned     duty_cycle   { subdiv };
  grain        delay        {  100us };
};

} // namespace wax
