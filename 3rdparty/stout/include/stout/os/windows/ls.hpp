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

#ifndef __STOUT_OS_WINDOWS_LS_HPP__
#define __STOUT_OS_WINDOWS_LS_HPP__

#include <list>
#include <string>

#include <stout/error.hpp>
#include <stout/try.hpp>

#include <stout/internal/windows/longpath.hpp>


namespace os {

inline Try<std::list<std::string>> ls(const std::wstring& directory)
{
  // Ensure the path ends with a backslash.
  std::wstring path = directory;
  if (!strings::endsWith(path, L"\\")) {
    path += L"\\";
  }

  // Get first file matching pattern `X:\path\to\wherever\*`.
  WIN32_FIND_DATAW found;
  const std::wstring search_pattern = path + L"*";

  const SharedHandle search_handle(
      ::FindFirstFileW(search_pattern.data(), &found),
      ::FindClose);

  if (search_handle.get() == INVALID_HANDLE_VALUE) {
    return WindowsError("Failed to search '" +
                        short_stringify(directory) + "'");
  }

  std::list<std::string> result;

  do {
    // NOTE: do-while is appropriate here because folder is guaranteed to have
    // at least a file called `.` (and probably also one called `..`).
    const std::wstring current_file(found.cFileName);

    const bool is_current_directory = current_file.compare(L".") == 0;
    const bool is_parent_directory = current_file.compare(L"..") == 0;

    // Ignore the `.` and `..` files in the directory.
    if (is_current_directory || is_parent_directory) {
      continue;
    }

    result.push_back(short_stringify(current_file));
  } while (::FindNextFileW(search_handle.get(), &found));

  return result;
}

inline Try<std::list<std::string>> ls(const std::string& directory)
{
  std::string path = directory;
  if (!strings::endsWith(path, "\\")) {
    path += "\\";
  }
  return ls(::internal::windows::longpath(path));
}

} // namespace os {

#endif // __STOUT_OS_WINDOWS_LS_HPP__
