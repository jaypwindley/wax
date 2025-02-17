#pragma once

#include <errno.h>

#include <cstdio>
#include <stdexcept>

#ifdef _WIN32
# include <fileapi.h>
# include <handleapi.h>
#endif /*_WIN32*/

//! @section Guards
//
// In the tradition of std::lock_guard in the Standard Library, these guards
// take ownership of a resource that can leak, deadlock, or otherwise cause
// mayhem if left around. Create objects of these types on the stack using the
// resource (or the arguments commonly used to construct the resource). Then no
// matter how the stack is unwound--exception, natural return--the resource is
// cleaned up when the guard stack object passes out of scope.
//
// There is no guard for memory allocation. Manage memory allocations with
// std::unique_ptr<T> instead.
//
// @todo Separate this into different implementations for Windows and UN*X in
// the same way the stopwatch change simplementation.

namespace wax {

//! @brief Guard a FILE * from stdio.
//
struct FILE_guard
{
  //! @brief Take ownership of an existing FILE.
  //
  // @note The raw FILE * may still be used in the program, but it should not be
  // explicitly closed by the program.
  //
  FILE_guard(FILE *f) : file(f) {}


  //! brief Open a file and guard it.
  //
  // @param [in] pathname The path name to open, as for fopen(3)
  // @param [in] mode     The mode in which to open the file, as for fopen(3)
  //
  FILE_guard(const char *pathname, const char *mode) {
    this->file = ::fopen(pathname, mode);
    if (file == nullptr) {
      const auto err = errno;
      char msg[1024];
      (void) ::snprintf(msg, sizeof(msg), "%s: %s", pathname, ::strerror(err));
      throw std::runtime_error(msg);
    }
  }

  ~FILE_guard() {
    if (this->file != nullptr) {
      ::fclose(file);
      file = nullptr;
    }
  }

  inline FILE * f() const noexcept { return this->file; }

  FILE_guard()                                 = default;
  FILE_guard(const FILE_guard &)               = delete;
  FILE_guard & operator = (const FILE_guard &) = delete;

  // Can move, but not copy.
  FILE_guard(FILE_guard && other) {
    this->file = other.file;
    other.file = nullptr;
  }

  FILE_guard & operator = (FILE_guard && other ) {
    this->file = other.file;
    other.file = nullptr;
    return *this;
  }

 private:
  FILE *file { nullptr };
};


#ifdef _WIN32

//! @brief Guard a Windows HANDLE
//
struct HANDLE_guard
{
  //! @brief Take ownership of an already-created handle.
  //
  HANDLE_guard(HANDLE h) : handle(h) {}

  //! @brief Open a file and assign it to a handle.
  //
  // @note See Microsoft CreateFile2() for arguments.
  //
  HANDLE_guard(
    const wchar_t                     *wpath,
    DWORD                              desired_access,
    DWORD                              share_mode,
    DWORD                              creation_disposition,
    LPCREATEFILE2_EXTENDED_PARAMETERS  params)
  {
    this->handle = CreateFile2(
      wpath,
      desired_access,
      share_mode,
      creation_disposition,
      params);
    if (this->handle == INVALID_HANDLE_VALUE) {
      const auto err = GetLastError();
      char msg[1024];

      //! @todo Figure out how to render wide path name in exception.
      ::snprintf(msg, sizeof(msg), "CreatFile2() failed: %d", err);
      throw std::runtime_error(msg);
    }
  }

  ~HANDLE_guard() { CloseHandle(this->handle); }

  inline HANDLE h() const noexcept { return this->handle; }

  HANDLE_guard()                                 = default;
  HANDLE_guard(const HANDLE_guard &)               = delete;
  HANDLE_guard & operator = (const HANDLE_guard &) = delete;

  // Can move but not copy.
  HANDLE_guard(HANDLE_guard && other) {
    this->handle = other.handle;
    other.handle = INVALID_HANDLE_VALUE;
  }

  HANDLE_guard & operator = (HANDLE_guard && other ) {
    this->handle = other.handle;
    other.handle = INVALID_HANDLE_VALUE;
    return *this;
  }

 private:
  HANDLE handle { INVALID_HANDLE_VALUE };
};

#endif /*_WIN32*/

} // namespace wax
