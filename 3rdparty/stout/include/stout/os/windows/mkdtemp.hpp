// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __STOUT_OS_WINDOWS_MKDTEMP_HPP__
#define __STOUT_OS_WINDOWS_MKDTEMP_HPP__

#include <random>
#include <string>

#include <stout/error.hpp>
#include <stout/nothing.hpp>
#include <stout/path.hpp>
#include <stout/strings.hpp>
#include <stout/try.hpp>

#include <stout/os/mkdir.hpp>
#include <stout/os/temp.hpp>


namespace os {

// Creates a temporary directory using the specified path
// template. The template may be any path with _6_ `Xs' appended to
// it, for example /tmp/temp.XXXXXX. The trailing `Xs' are replaced
// with a unique alphanumeric combination.
template <typename T>
inline Try<std::string> mkdtemp(T&& path)
{
  {
    const std::wstring& path(::internal::windows::longpath(std::forward<T>(path)));
    // NOTE: We'd like to avoid reallocating `postfixTemplate` and `alphabet`,
    // and to avoid  recomputing their sizes on each call to `mkdtemp`, so we
    // make them `static const` and use the slightly awkward `sizeof` trick to
    // compute their sizes once instead of calling `strlen` for each call.
    static const wchar_t postfixTemplate[] = L"XXXXXX";
    static const size_t postfixSize =
        sizeof(postfixTemplate) / sizeof(wchar_t) - 1;

    if (!strings::endsWith(path, postfixTemplate)) {
      return Error(
          "Invalid template passed to `os::mkdtemp`: template '" +
          short_stringify(path) +
          "' should end with 6 'X' characters");
    }

    static const wchar_t alphabet[] =
      L"0123456789"
      L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      L"abcdefghijklmnopqrstuvwxyz";

    // NOTE: The maximum addressable index in a string is the total length of the
    // string minus 1; but C strings have an extra null character at the end, so
    // the size of the array is actually one more than the length of the string,
    // which is why we're subtracting 2 here.
    static const size_t maxAlphabetIndex =
        sizeof(alphabet) / sizeof(wchar_t) - 2;

    wchar_t postfix[postfixSize + 1];
    static thread_local std::mt19937 generator((std::random_device())());

    for (int i = 0; i < postfixSize; ++i) {
      int index = generator() % maxAlphabetIndex;
      postfix[i] = alphabet[index];
    }
    postfix[postfixSize] = L'\0';

    // Replace template, make directory.
    std::wstring tempPath = path
      .substr(0, path.length() - postfixSize)
      .append(postfix);

    Try<Nothing> mkdir = os::mkdir(tempPath, false);

    if (mkdir.isError()) {
      return Error(mkdir.error());
    }

    return short_stringify(tempPath);
  }
}


inline Try<std::string> mkdtemp()
{
  return mkdtemp(path::join(os::temp(), "XXXXXX"));
}

} // namespace os {


#endif // __STOUT_OS_WINDOWS_MKDTEMP_HPP__
