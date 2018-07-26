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

#ifndef __STOUT_OS_WINDOWS_MKDIR_HPP__
#define __STOUT_OS_WINDOWS_MKDIR_HPP__

#include <string>
#include <vector>

#include <stout/error.hpp>
#include <stout/nothing.hpp>
#include <stout/strings.hpp>
#include <stout/try.hpp>
#include <stout/windows.hpp>

#include <stout/os/exists.hpp>
#include <stout/os/constants.hpp>

#include <stout/internal/windows/longpath.hpp>


namespace os {

template <typename T>
inline Try<Nothing> mkdir(T&& path, bool recursive = true)
{
  const std::wstring& longpath(
      ::internal::windows::longpath(std::forward<T>(path)));
  if (!recursive) {
    // NOTE: We check for existence because parts of certain directories
    // like `C:\` will return an error if passed to `CreateDirectory`,
    // even though the drive may already exist.
    if (os::exists(longpath)) {
      return Nothing();
    }

    if (::CreateDirectoryW(longpath.data(), nullptr) == 0) {
      return WindowsError("Failed to create directory: " +
                          short_stringify(longpath));
    }
  } else {
    // Remove the long path prefix, if it already exists, otherwise the
    // tokenizer includes the long path prefix (`\\?\`) as the first part
    // of the path.
    const std::vector<std::wstring> tokens = strings::tokenize(
        strings::remove(longpath, os::W_LONGPATH_PREFIX,
                        strings::Mode::PREFIX),
        os::W_PATH_SEPARATOR);

    std::wstring path;

    foreach (const std::wstring& token, tokens) {
      path += token + os::W_PATH_SEPARATOR;
      const Try<Nothing> result = mkdir(path, false);
      if (result.isError()) {
        return result;
      }
    }
  }

  return Nothing();
}

} // namespace os {

#endif // __STOUT_OS_WINDOWS_MKDIR_HPP__
