#pragma once

namespace wax::ring {


// @class basic
//
// Base class from which all practical ring buffers inherit.  At this level
// there is only the concept of a write cursor. There is no thread-safety. You
// can do atomic writes with write() or deferred writes using at(), next(), and
// last().
//
// @todo Decide how much of this can be reimplemented in terms of the anonymous
// ring buffer.
//
template<class data_point, std::size_t n_elements>
class basic
{
  // Number of elements must be a non-zero power of 2.  This is so we can do a
  // clever hack for wrapping the indexes without a decision.  We fail at
  // compile time if the hack is not possible.
  //
  static_assert(n_elements > 0);
  static_assert((n_elements & (~n_elements + 1)) == n_elements);

  static constexpr std::size_t storage_size = n_elements * sizeof(data_point);

 public:
  using value_type = data_point;

  // Comparator function for find().  Should return true if the value_types are
  // equivalent.
  //
  using comp_func = std::function<bool(const value_type &, const value_type &)>;

 public:
  basic() noexcept
  {
    ring.reset(new value_type[n_elements]);

    // Class types are default-constructed when allocated as an array.  Arrays
    // of builtin types are not default-constructed.  In that case fill the
    // storage with zeroes.
    //
    if (!std::is_class<value_type>::value)
      std::memset(ring.get(), 0, storage_size);
  }

  ~basic() = default;

  // @return the number of bytes occupied by this ring buffer.
  constexpr std::size_t storage() const noexcept { return storage_size; }

  //! @return number of data points that can be stored in this ring buffer.
  constexpr std::size_t capacity() const noexcept { return n_elements; }

  //! @brief Write the supplied user data into the next available buffer slot.
  //
  // Writes the user data into the next available buffer slot and increments the
  // write cursor.
  //
  // @return Index of written data
  //
  std::size_t write(value_type * const data)
  {
    if (data == nullptr)
      throw std::invalid_argument("null pointer");
    const auto index = write_at;
    auto * const dest = at();
    (void) std::memcpy(dest, data, sizeof(*dest)); // @todo Pay attention to return value.
    next();
    return index;
  }

  //! @brief Write into the next available buffer slot with move semantics.
  // @return Index of written data.
  //
  std::size_t write(value_type && data)
  {
    const auto index = write_at;
    auto * const dest = at();
    *dest = std::move<value_type>(std::forward<value_type &&>(data));
    next();
    return index;
  }


  //! @todo emplace(), using constructor arguments.

  //!
  // @brief Point to current writable slot
  //
  // Gets a pointer to the current write position without advancing the write
  // cursor. This is useful when the buffer is expected to be filled by a
  // subsequent I/O operation, so as to avoid copying user data.
  //
  // N.B. Subsequent calls to at() without a followup next() call will return
  // the same pointer. The expected pattern is to retrieve a pointer with at(),
  // populate the slot, then call next() to advance the write cursor.
  //
  // @return a pointer to the current writable slot
  //
  value_type *at() noexcept { return &ring[write_at]; }


  //! @brief Return a slot for writing
  //
  // Does the same as at() but also advances the write cursor. The customary
  // pattern is to get a pointer using at() and then call next() after
  // populating the data.
  //
  // @return pointer to newly allocated user data memory
  //
  value_type *next() noexcept
  {
    value_type *at = &ring[write_at];
    write_at = wrap(++write_at);
    has_data = true;
    return at;
  }

  //! @brief Get the last-written element
  //
  // @return Pointer to most recently-written element, or nullptr if buffer is
  // empty.
  //
  value_type *last() noexcept
  {
    if ( ! has_data)
      return nullptr;

    // Backspace-ish one element.
    const auto idx = wrap(write_at - 1);
    return &ring[idx];
  }


  //! @brief Random-access dereference, with bounds checking.
  //
  // Returns a reference to the value at the given index, provided the index is
  // in the allowable range.
  //
  // @return the value at index i
  //
  // @note This operator doesn't assume anything about the validity of data
  // contained at the location.
  //
  value_type & operator [] (std::size_t i)
  {
    if (i >= n_elements)
      throw std::out_of_range("index out of range");
    return ring[i];
  }
  const value_type & operator [] (std::size_t i) const {
    return (*this)[i];
  }

  //! @brief Find an element
  //
  //! @param val The value to look for
  //! @param p Comparison predicate
  //
  //! @return index of found value, or wax::npos if not found
  //
  //! @note Because the basic ring buffer has no concept of laps, find() called
  //! from here will be unable to distinguish default-constructed (i.e.,
  //! unfilled) buffer elements from valid data during the initial lap.
  //! Therefore the caller is responsible for determining whether the value at
  //! the returned index is actually desirable.  In other words, if you search
  //! for the default-constructed value, you deserve what you get.
  //
  virtual std::size_t find(
    const value_type & val,
    comp_func && p =
      [] (const value_type & lhs, const value_type & rhs) -> bool {
        return lhs == rhs;
      }
    ) const noexcept
  {
    return find(
      std::forward<const value_type &>(val),
      std::forward<comp_func &&>(p),
      0,
      capacity());
  }

 protected:

  // @brief Wrap an index, if necessary.
  //
  // Wrap the index using bitwise magic instead of modulo arithmetic.
  //
  // @param i the candidate new index
  //
  // @return The new index modulo buffer size
  //
  inline std::size_t wrap(std::size_t i) const noexcept {
    static constexpr std::size_t mask = n_elements - 1;
    return i & mask;
  }

  //! @brief Find an element inside a subset of the buffer
  //
  // Search a portion of the buffer for the desired data, i.e., a
  // specifically-scoped search.
  //
  // @param val The value to look for
  // @param p Comparison predicate
  // @param lower_bound The index at which to start the search
  // @param upper_bound The upper limit of the search.  The item at index
  //                    upper_bound will not be tested.
  //
  // @return index of found value, or npos::npos if not found.
  //
  // @todo Implement search hints such that the lower_boud becomes a useful
  // parameter and the search is efficient for large buffers.
  //
  std::size_t find(
    const value_type  &val,
    comp_func        &&p,
    std::size_t        lower_bound,
    std::size_t        upper_bound) const noexcept
  {
    if (! has_data)
      return wax::npos;
    if (lower_bound > upper_bound)
      return wax::npos;
    for (std::size_t i = lower_bound; i < upper_bound; ++i)
      if (p(ring[i], val))
        return i;
    return wax::npos;
  }

  std::unique_ptr<value_type[]>  ring        { nullptr };
  std::size_t                    write_at    { 0 };
  bool                           has_data    { false };
};




// @class lappable
//
// This is a practical ring buffer. It offers a write cursor and multiple read
// cursors as well as thread-safety and lapping detection for the readers.
//
template <class data_point, std::size_t n_elements>
class lappable : public basic<data_point, n_elements>
{
 public:

  using self_type     = lappable<data_point, n_elements>;
  using parent_type   = basic<data_point, n_elements>;

  using lap_counter_t = uint64_t;

  // Namespace for all the cursors.
  class cursor
  {
   public:
    enum class err { none, was_lapped, is_empty };

   private:
    // Cursor base class, mostly just a place to provide access to the buffer to
    // which it is connected.
    //
    class __basic
    {
     public:
      __basic(self_type &buf) noexcept: buf{ buf } {}

     protected:
      self_type  &buf;
      err         errno_   {err::none};

     public:
      //! @return current error condition for this cursor.
      err error() const noexcept { return errno_; }
    };

   public:

    //! @class ring::lappable::cursor::read
    //
    // Read cursor.
    //
    class read : public __basic
    {
     public:

      using __basic::__basic;

      //! @brief swap the current read index with the given one
      // @param idx the new read index; must be in range
      // @return the previous index
      //
      std::size_t swap(std::size_t idx)
      {
        if (idx >= n_elements)
          throw std::invalid_argument("swap");
        const auto old = this->read_at;
        this->read_at = idx;
        return old;
      }

      //! @brief Get a pointer to the next data element
      //
      // Return a pointer to the data element at the cursor without advancing
      // the cursor. If the reader has been lapped, it is reset to the oldest
      // available data, nullptr is returned, and the error condition is set to
      // was_lapped.  In this case, a subsequent call to peek() will return the
      // oldest available data point.  If there is no more data left to read,
      // nullptr is returned and the error condition is set to is_empty.
      // Subsequent calls to get() may continue returning nullptr/is_empty until
      // the writer has forged ahead.
      //
      // @return Pointer to the next data element or an error.
      //
      // @todo Determine whether the was_lapped case should be detected and
      // contained here, i.e., the inevitable retry should maybe occur here.
      //
      typename parent_type::value_type * peek()
      {
        wax::lock l(this->buf.mux);
        return this->peek_unsafe();
      }

      //! @brief Get a pointer to the next data element
      //
      // Return a pointer to the data element at the cursor and advance the
      // cursor.  If the reader has been lapped, it is reset to the oldest
      // available data, nullptr is returned, and the error condition is set to
      // was_lapped.  In this case, a subsequent call to get() will return the
      // oldest available data point.  If there is no more data left to read,
      // nullptr is returned and the error condition is set to is_empty.
      // Subsequent calls to get() may continue returning nullptr/is_empty until
      // the writer has forged ahead.
      //
      // @return Pointer to the next data element or an error.
      //
      typename parent_type::value_type * get()
      {
        wax::lock l(this->buf.mux);
        auto * const ref = this->peek_unsafe();
        if (ref) {
          read_at = this->buf.wrap(++read_at);
          if (read_at == 0)
            ++this->lap;
        }
        return ref;
      }

     protected:

      //! @brief The operational guts of peek(), not thread-safe
      //
      // @return Pointer to the next data element or an error.
      //
      typename parent_type::value_type * peek_unsafe()
      {
        this->errno_ = err::none;

        // Check buffer isn't empty.
        if (!this->buf.has_data) {
          this->errno_ = err::is_empty;
          return nullptr;
        }

        // See if we're at least a lap behind.  If so, reset to current lap - 1,
        // and oldest data point in that lap.
        //
        if (this->read_at < this->buf.write_at) {
          if (this->lap < this->buf.lap()) {
            // Yes, you're reading this right. The proper place for the read
            // cursor to be after this condition is 1 lap behind the writer, but
            // then set to the write-cursor position, the oldest available data
            // in that lap.  The non-error position, if read is less than write,
            // is for the reader lap to be equal to the writer lap.
            //
            this->lap = this->buf.lap() - 1;
            this->read_at = this->buf.oldest_unsafe();
            this->errno_ = err::was_lapped;
            return nullptr;
          }
        }

        // We've caught up to writer.  If the laps match, we're empty.  If they
        // don't, check whether we're way far behind.  One behind is okay; it
        // means we're just reading the oldest data.  More than one behind means
        // we're lapped.
        //
        else if (this->read_at == this->buf.write_at) {
          if (this->lap == this->buf.lap()) {
            this->errno_ = err::is_empty;
            return nullptr;
          }
          else if ((this->lap + 1) < this->buf.lap()) {
            this->lap = this->buf.lap() - 1;
            this->errno_ = err::was_lapped;
            return nullptr;
          }
        }

        // We're ahead of the writer, but must check lap in order to see if we
        // are just not wrapped identically to the writer.
        //
        else /* this->read_at > this->buf.write_at */ {
          if ((this->lap + 1) != this->buf.lap()) {
            this->lap = this->buf.lap();
            this->read_at = this->buf.oldest_unsafe();
            this->errno_ = err::was_lapped;
            return nullptr;
          }
        }

        // All error conditions checked and eliminated.  Read the data.

        auto * const ref = &this->buf[read_at];
        return ref;
      }

      // Read cursor.
      std::size_t   read_at { 0 };
      lap_counter_t lap     { 0 };
    };

    // Write cursor.  All writers share the same write cursor, which is
    // encapsulated in the buffer itself.  So the best thing is for there only
    // ever to be one writer.  We don't enforce that in the code, but caveat
    // lector.
    //
    class write : public __basic
    {
     public:
      using __basic::__basic;

      //! @brief Get an address for new data.
      //
      // Get the address where new data should be written, such as for the
      // similar case of write() above.  This is for when we want to populate an
      // element of the buffer later, to avoid copy.
      //
      // N.B. Subsequent calls to ptr() without intervening calls to ready()
      // will return the same pointer.
      //
      // @return Address for new data.
      //
      typename parent_type::value_type *ptr()
      {
        this->errno_ = err::none;
        wax::lock l(this->buf.mux);
        return this->buf.at();
      }

      //! @brief Advance read pointer
      //
      // Indicates that a value_type entry previously obtained by ptr() is now
      // ready to be read.
      //
      void ready()
      {
        this->errno_ = err::none;
        wax::lock l(this->buf.mux);
        (void) this->buf.next();
        if (this->buf.write_at == 0)
          ++this->buf.lap_;
      }

      //! @brief Copy the given user data into the buffer
      //
      // @param val The value to write
      //
      // @return the index at which the data was written.
      //
      std::size_t put(typename parent_type::value_type && val)
      {
        const auto was = this->buf.write_at;
        auto * const p = this->ptr();
        (void) std::memcpy(p, &val, sizeof(*p));
        this->ready();
        return was;
      }
    };
  };

  friend class cursor::read;
  friend class cursor::write;

 public:
  lappable()  = default;
  ~lappable() = default;

  //! @return The index of the oldest data point in the buffer, or npos if the
  //! buffer is empty.
  std::size_t oldest() const noexcept
  {
    wax::lock l(this->mux);
    return oldest_unsafe();
  }

  //! @return the current lap number
  lap_counter_t lap() const noexcept { return this->lap_; }

  //! @brief Find an element
  //
  //! @param val The value to look for
  //! @param p comparison predicate
  //
  //! @return index of found value, or wax::npos if not found
  std::size_t find(
    const typename parent_type::value_type & val,
    typename parent_type::comp_func && p =
    [] (const typename parent_type::value_type & lhs,
        const typename parent_type::value_type & rhs) -> bool
    {
      return lhs == rhs;
    } ) const noexcept override
  {
    return this->parent_type::find(
      std::forward<const typename parent_type::value_type &>(val),
      std::forward<typename parent_type::comp_func &&>(p),
      0,
      (lap_ == 0) ? this->write_at : this->capacity());
  }

 protected:
  lap_counter_t lap_ { 0 };
  mutable wax::mutex mux;

  //! @return The index of the oldest data point in the buffer, or npos if the
  //! buffer is empty.
  // @note meant to be called with mux already locked.
  std::size_t oldest_unsafe() const noexcept
  {
    if (! this->has_data)
      return wax::npos;
    return (lap_ == 0) ? 0 : this->write_at;
  }
};

} // namespace wax::ring
