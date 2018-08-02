// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __STOUT_OS_WINDOWS_REALPATH_HPP__
#define __STOUT_OS_WINDOWS_REALPATH_HPP__


#include <stout/error.hpp>
#include <stout/result.hpp>
#include <stout/stringify.hpp>
#include <stout/strings.hpp>
#include <stout/windows.hpp>

#include <stout/internal/windows/longpath.hpp>
#include <stout/internal/windows/reparsepoint.hpp>


namespace os {

// This should behave like the POSIX `realpath` API: specifically it should
// resolve symlinks in the path, and succeed only if the target file exists.
// This requires that the user has permissions to resolve each component of the
// path.
template <typename T>
inline Result<std::string> realpath(T&& path)
{
  const Try<SharedHandle> handle =
      ::internal::windows::get_handle_follow(std::forward<T>(path));
  if (handle.isError()) {
    return Error(handle.error());
  }

  // First query for the buffer size required.
  const DWORD length = ::GetFinalPathNameByHandleW(
      handle.get().get_handle(), nullptr, 0, FILE_NAME_NORMALIZED);
  if (length == 0) {
    return WindowsError("Failed to retrieve realpath buffer size");
  }

  std::vector<wchar_t> buffer(length);

  DWORD result = ::GetFinalPathNameByHandleW(
      handle.get().get_handle(), buffer.data(), length, FILE_NAME_NORMALIZED);

  if (result == 0) {
    return WindowsError("Failed to determine realpath");
  }

  return narrow_stringify(strings::remove(
      buffer.data(),
      os::W_LONGPATH_PREFIX,
      strings::Mode::PREFIX));
}

} // namespace os {

#endif // __STOUT_OS_WINDOWS_REALPATH_HPP__
