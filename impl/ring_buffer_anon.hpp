#pragma once

// Ring (or circular) buffer
//
// This is the version that can store anonymous contents, such as when you have
// to select the contents at runtime, or if the contents are heterogeneous. See
// ring_buffer.h for the statically-typed version, which should be preferred
// where possible.

namespace wax::ring {
namespace anon {

//! @class basic
//
// This is the base class from which all practical ring buffers inhereit. At
// this level there is only the concept of a write cursor. There is no
// thread-safety. You can do atomic-ish writes with write() or deferred writes
// using at(), next(), and last().
//
class basic
{
 public:

  //!
  // @param stride Size in bytes of the object being stored; the row width.
  // @param n_rows Number of rows in the buffer
  //
  basic(std::size_t stride, std::size_t n_rows) :
    stride { stride },
    n_rows { n_rows }
  {
    if (stride == 0)
      throw std::invalid_argument("stride must be nonzero");
    if (n_rows == 0)
      throw std::invalid_argument("n_rows must be nonzero");

    // Allocate memory into the smart pointer and zero it.
    ring.reset(new std::byte[storage_size]);
    std::memset(ring.get(), 0, storage_size);
  }

  ~basic() = default;
  basic()  = delete;
  //! @todo copy and move constructors.

  //! @return The number of bytes of storage in the buffer.
  std::size_t storage() const noexcept { return this->storage_size; }

  //! @return The number of objects this buffer can store.
  std::size_t capacity() const noexcept { return this->n_rows; }

  //! @brief Write bytes into the buffer
  //
  // Writes len bytes into the buffer from data, at the current write position
  // in the buffer, then increments the write position to the next location,
  // wrapping if necessary. Throws an exception if len is larger than the
  // stride.
  //
  // @param data [in] The address of the source data
  // @param len [in] The number of bytes to write
  //
  // @return The byte index of the position where writing began. This is
  // suitable for the [] operator.
  //
  std::size_t write(const std::byte * const data, size_t len) {
    if (data == nullptr)
      throw std::invalid_argument("null pointer");
    if (len > stride)
      throw std::invalid_argument("length too long");
    const auto index = write_at;
    auto * const dest = at();
    (void) std::memcpy(dest, data, len);
    (void) next();
    return index;
  }

  //! @brief Write an object into the buffer.
  //
  // Writes a typed object byte-wise into the buffer. This is a shallow copy.
  //
  // @param obj [in] Pointer to the object to copy.
  //
  // @return The byte index of the position where writing began. This is
  // suitable for the [] operator.
  //
  // @todo See whether std::copy() will deep-copy objects with nontrivial
  // construction.
  //
  template <class value_type>
  std::size_t write(const value_type * const obj) {
    return this->write((std::byte *) obj, sizeof(value_type));
  }

  //! @return The byte address of the NEXT position to be written.
  //
  // This is meant to obtain space in the buffer to be filled later, such as by
  // a write from a communications device. When the write completes, call last()
  // to advance the write cursor so that subsequent at() calls will return
  // addresses of subsequent positions.
  //
  std::byte *at() noexcept { return &ring[write_at]; }

  //! @return the typed-object address of the NEXT position to be written.
  //
  template <class value_type>
  value_type *at() noexcept { return (value_type *) this->at(); }

  //! @brief Advance the write cursor
  //
  // Advance the write cursor to the next write position in the buffer without
  // modifying the buffer cntents. When used in conjunction with at() to obtain
  // a space in the buffer to fill later, this signals the succesful end of the
  // write operation.
  //
  // @return the byte address of the previous write position.
  //
  // @todo Think about whether returning the next address (a la at()) would be
  // more useful.
  //
  std::byte *next() noexcept {
    std::byte *at = &ring[write_at];
    write_at = wrap(write_at + stride);
    has_data = true;
    return at;
  }

  //! @brief Advance the write cursor
  //
  // @return the typed-object address of the previous write position.
  //
  template <class value_type>
  value_type *next() noexcept { return (value_type *) this->next(); }

  //! @return the byte address of the most recently written position.
  //
  std::byte *last() noexcept {
    if ( ! has_data )
      return nullptr;

    const auto idx = wrap(write_at - stride);
    return &ring[idx];
  }

  //! @return the typed-object address of the most recently written position.
  //
  template <class value_type>
  value_type *last() noexcept { return (value_type *) this->last(); }

  //! @return the byte address for the index
  //
  // Finds the byte address for the index, which must be in the allowable
  // storage size or else an exception is thrown.
  //
  // @param i [in] The byte index whose address is wanted.
  //
  std::byte * operator [] (std::size_t i)
  {
    if (i >= storage_size)
      throw std::out_of_range("index out of range");
    return &ring[i];
  }
  const std::byte * operator [] (std::size_t i) const {
    return (*this)[i];
  }

 protected:
  inline std::size_t wrap(std::size_t i) const noexcept {
    return i % storage_size;
  }

  std::unique_ptr<std::byte[]>  ring;
  const std::size_t             stride;
  const std::size_t             n_rows;
  const std::size_t             storage_size   { stride * n_rows };
  std::size_t                   write_at       { 0 };
  bool                          has_data       { false };
};



//! @class lappable
//
// This is a practical ring buffer. It offers a write cursor and multiple read
// cursors as well as thread-safety and lapping detection for the readers.
//
class lappable : public basic
{
 public:

  using lap_counter_t = uint64_t;

  //! Namespace for cursors, e.g., cursor::write and cursor::read
  //
  class cursor
  {
   public:

    //! If a cursor returns nullptr, the error() method will return one of
    // these telling why.
    //
    enum class err { none, was_lapped, is_empty };

   private:

    // The base class for all cursors. The cursor holds a reference to the
    // buffer to which it belongs and the last error encountered. Cursors are
    // friends of the buffer and manipulate it more or less directly.
    //
    class __basic
    {
     public:
      __basic(lappable &buf) noexcept : buf { buf } {}
     protected:
      lappable &buf;
      err errno_  {err::none};
     public:
      err error() const noexcept { return errno_; }
    }; // __basic


   public:

    //! @brief Read cursor
    //
    // There can be any number of read cursors on a buffer, but there really
    // should ever only be one write cursor.
    //
    class read : public __basic {
     public:
      using __basic::__basic;

      //! @return Byte address of the current read position; may be nullptr if
      //! error.
      //
      // @todo Think about whether we should let exceptions leak out of here
      // from peek_unsafe().
      //
      std::byte * peek()
      {
        wax::lock l(this->buf.mux);
        return this->peek_unsafe();
      }

      //! @return Typed-object address of the current read position.
      //
      template <class value_type>
      value_type * peek() { return (value_type *) this->peek(); }

      //! @return Byte address of the current read position, or nullptr on
      //! error.
      //
      // Get the current object from the buffer and advance the read pointer to
      // the next object. Returns nullptr on error, including the error in which
      // the read cursor has been lapped. Use the error() method to verify
      // why. If the reader has been lapped, calling get() again will retrieve
      // the oldest available item still in the buffer.
      //
      std::byte * get()
      {
        wax::lock l(this->buf.mux);
        auto * const ref = this->peek_unsafe();
        if (ref) {
          read_at = this->buf.wrap(read_at + buf.stride);
          if (read_at == 0)
            ++this->lap;
        }
        return ref;
      }

      //! @return Typed-object pointer of the current read position, or nullptr
      //! on error.
      //
      template <class value_type>
      value_type * get() { return (value_type *) this->get(); }

     protected:
      std::byte * peek_unsafe()
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
        auto * const ref = this->buf[read_at];
        return ref;
      }

      std::size_t    read_at  { 0 };
      lap_counter_t  lap      { 0 };
    }; // read


    //! Write cursor.
    //
    // There can be any number of read cursors on a buffer, but there should
    // ever only be one write cursor since the write position is maintained by
    // the buffer itself. Nothing prevents creating more than one write cursor
    // on a buffer. But they will affect each other such that writing via one
    // will advance the write position for all write cursors on that buffer.
    //
    // @todo Think about applications where multiple writers would make sense,
    // and how their writes could be interleaved in a useful and safe way.
    //
    class write : public __basic {
     public:
      using __basic::__basic;

      //! @return the byte address of the current write position.
      //
      // @note This is meant to be used with ready() to implement deferred
      // writes, such as from a network. Call ptr() to obtain a place to write
      // data when it becomes available. Call ready() to advance the write
      // pointer to the next write position when the write has succeeded.
      //
      std::byte * ptr()
      {
        this->errno_ = err::none;
        wax::lock l(this->buf.mux);
        return this->buf.at();
      }

      //! @return the typed-object pointer to the current write position.
      //
      template <class value_type>
      value_type * ptr() { return (value_type *) this->ptr(); }

      //! @brief Advance the write pointer.
      //
      // Advance the write pointer to the next write position.
      //
      // @note This is meant to be used with ptr() to implement deferred writes,
      // such as from a network.Call ptr() to obtain a place to write data when
      // it becomes available. Call ready() to advance the write pointer to the
      // next write position when the write has succeeded.
      //
      void ready()
      {
        this->errno_ = err::none;
        wax::lock l(this->buf.mux);
        (void) this->buf.next();
        if (this->buf.write_at == 0)
          ++this->buf.lap_;
      }

      //! @brief Write data to the buffer.
      //
      // Write the data to the current write position in the buffer and advance
      // the write cursor to the next position. Len must be less than or equal
      // to the buffer stride.
      //
      // @param data [in] Address of the data to write
      // @param len [in] Number of bytes to write
      //
      // @return the byte index of the place where the data started to be
      // written. Suitable for the [] operator.
      //
      std::size_t put(const std::byte * const data, size_t len)
      {
        assert(data); // Or throw?
        assert(len <= this->buf.stride); //Or throw?
        const auto was = this->buf.write_at;
        auto * const p = this->ptr();
        (void) std::memcpy(p, data, len);
        this->ready();
        return was;
      }

      //! @brief Write the typed object to the buffer.
      //
      // @param obj [in] Pointer to the object to write.
      //
      // @return the byte index of the place where the data started to be
      // written. Suitable for the [] operator.
      //
      template <class value_type>
      std::size_t put(const value_type * const obj) {
        return this->put((std::byte *) obj, sizeof(value_type));
      }
    }; // write
  }; // cursor




  friend class cursor::read;
  friend class cursor::write;

 public:

  //!
  // @param stride Size in bytes of the object being stored; the row width.
  // @param n_rows The number of rows in the buffer.
  //
  lappable(std::size_t stride, std::size_t n_rows) : basic { stride, n_rows } {}
  ~lappable() = default;
  lappable()  = delete;

  //! @return the byte index of the oldest data in the buffer
  //
  std::size_t oldest() const noexcept
  {
    wax::lock l(this->mux);
    return oldest_unsafe();
  }

  //! @return the lap counter, how many times this buffer has been overwritten
  //
  lap_counter_t lap() const noexcept { return this->lap_; }

 protected:
  lap_counter_t lap_ { 0 };
  mutable wax::mutex mux;

  std::size_t oldest_unsafe() const noexcept
  {
    if ( ! this->has_data )
      return wax::npos;
    return (lap_ == 0) ? 0 : this->write_at;
  }
};

} // namespace anon
} // namespace wax::ring
