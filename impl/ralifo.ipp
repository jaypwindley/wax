#pragma once
#include <vector>

namespace wax {

//! @class ralifo
// @brief random-access LIFO
//
// LIFO that lets you randomly erase from the middle if necessary.  The pure
// FIFOs and LIFOs from the standard library are great when the problem calls
// for abstract, elegantly correct solutions.  In the real world, sometimes you
// need to step out of the concessions line because your movie is starting.
//
// @note No internal mutex on purpose. Threaded clients may need integrity of
// popped values for some time after the pop.
//
// @todo This was originally meant for atomic types only. Expand it to include
// non trivially constructed types.
//
template <class value_type>
class ralifo : public std::vector<value_type>
{
 public:

  //! @brief Push value onto top of queue.
  // @param v The value to push.
  //
  void push(value_type v) {
    this->emplace(this->begin(), v);
  }

  //! @return The value at the top of the queue, the most recently-added value.
  //
  value_type top() {
    if (this->size())
      return this->front();
    else
      return static_cast<value_type>(nullptr); //! @todo Something better.
  }

  //! @brief Remove the value, wherever it is.
  // @param v [in] value to remove
  void erase(value_type v) {
    for (auto i = this->begin(); i != this->end(); ++i)
      if (*i == v) {
        (void) std::vector<value_type>::erase(i);
        break;
      }
  }
};
} // namespace wax
