#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

#include "./mutex.h"

namespace wax {

//! @brief simple message queue
//
// A simple queue for messages to be sent from other threads to a receiving
// thread. Blocking and non-blocking reads are supported. Timeouts are
// supported.
//
// @note This object is not movable or copyable. Use carefully in design.
//
template<typename msg_type>
class msg_queue
{
 public:

  using value_type = msg_type;
  static_assert(std::is_move_constructible_v<msg_type>);

  //! @brief Push a message onto the queue
  //
  // @param [in] msg The message to add
  //
  void push(msg_type const & msg) {
    {
      lock _(mux);
      this->q.push(msg);
    }
    cond.notify_one();
  }

  //! @brief Construct a message onto the queue
  //
  // @param [in] args... The arguments from which to construct the message
  //
  template <typename... arg_list>
  void push(arg_list... args) {
    {
      lock _(mux);
      this->q.emplace(std::forward<arg_list>(args)...);
    }
    cond.notify_one();
  }

  //! @return true if message queue is empty
  //
  bool empty() const {
    lock _(mux);
    return this->q.empty();
  }

  //! @brief Pop a message, waiting forever
  //
  // Wait (forever) for a message to become available on the queue and then
  // store it in msg. Msg is guaranteed to contain the message upon return.
  //
  // @param [out] Where to put the message
  //
  void pop(msg_type & msg) {
    ulock lk(mux);
    while (this->q.empty())
      cond.wait(lk);
    if (std::is_move_constructible_v<msg_type>)
      msg = std::move(this->q.front());
    else
      msg = this->q.front();
    q.pop();
  }

  //! @brief Non-blocking pop()
  //
  // Non-blockingly check whether there is a message on the queue and return
  // immediately either way. If a message was available and has now been stored
  // in msg, this function returns true. Returns false if the queue was empty.
  //
  // param [out] msg Where to put the message
  //
  // @return Whether value was popped.
  //
  bool pop_if(msg_type & msg) {
    lock _(mux);
    if (this->q.empty())
      return false;
    if (std::is_move_constructible_v<msg_type>)
      msg = std::move(this->q.front());
    else
      msg = this->q.front();
    q.pop();
    return true;
  }

  //! @brief Pop() with timeout
  //
  // Block until a message becomes available or until a timeout occurs. Store
  // the message in msg if there was one.
  //
  // @param [out] msg Where to put the message
  // @param [in]  t   The timeout value. Don't let the template formalism scare
  //                  you. You can (and should) use standard shortcuts for
  //                  std::chrono durations, such as
  //                       std::chromo::microseconds(25)
  //                  They work fine for this parameter.
  //
  // @return True if message was popped, false if timeout occurred.
  //
  template <class rep_type, class period_type>
  bool pop_until(
    msg_type & msg,
    const std::chrono::duration<rep_type, period_type> & t)
  {
    ulock lk(mux);
    while (this->q.empty()) {
      if (cond.wait_for(lk, t) == std::cv_status::timeout)
        return false;
    }
    if (std::is_move_constructible_v<msg_type>)
      msg = std::move(this->q.front());
    else
      msg = this->q.front();
    q.pop();
    return true;
  }

  //! @brief Clear the queue
  //
  void clear() {
    lock _(mux);
    while ( ! this->q.empty())
      this->q.pop();
  }

  //! @return Size of queue.
  //
  std::size_t size() {
    lock _(mux);
    return this->q.size();
  }

  msg_queue()                                = default;
  virtual ~msg_queue()                       = default;
  msg_queue(const msg_queue &)               = delete;  // Can't move or
  msg_queue & operator = (const msg_queue &) = delete;  // ...copy mutexes.
  msg_queue(msg_queue &&)                    = delete;  //
  msg_queue & operator = (msg_queue &&)      = delete;  // Sorry.

private:
  std::queue<msg_type>    q;
  mutable mutex           mux;
  std::condition_variable cond;
};

} // namespace wax
