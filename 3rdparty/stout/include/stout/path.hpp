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

#ifndef __STOUT_PATH_HPP__
#define __STOUT_PATH_HPP__

#include <string>
#include <utility>
#include <vector>

#include <stout/stringify.hpp>
#include <stout/strings.hpp>

#include <stout/os/constants.hpp>


namespace path {

// Converts a fully formed URI to a filename for the platform.
//
// On all platforms, the optional "file://" prefix is removed if it
// exists.
//
// On Windows, this also converts "/" characters to "\" characters.
// The Windows file system APIs don't work with "/" in the filename
// when using long paths (although they do work fine if the file
// path happens to be short).
//
// NOTE: Currently, Mesos uses URIs and files somewhat interchangably.
// For compatibility, lack of "file://" prefix is not considered an
// error.
template <typename T>
inline auto from_uri(T&& uri) -> GET_TYPE(uri)
{
  typedef GET_TYPE(uri) STRING;

  // Remove the optional "file://" if it exists.
  // TODO(coffler): Remove the `hostname` component.
  const STRING path = strings::remove(std::forward<T>(uri),
      utf_convert<typename STRING::value_type>("file://"),
      strings::PREFIX);

#ifndef __WINDOWS__
  return path;
#else
  return strings::replace(path,
      utf_convert<typename STRING::value_type>("/"),
      utf_convert<typename STRING::value_type>("\\"));
#endif // __WINDOWS__
}


// Base case.
template <typename T1, typename T2>
inline auto join(
    T1&& path1,
    T2&& path2,
    const char _separator) -> GET_TYPE(path1)
{
  typedef GET_TYPE(path1) STRING;
  const STRING separator =
      utf_convert<typename STRING::value_type>(_separator);
  return strings::remove(std::forward<T1>(path1), separator, strings::SUFFIX) +
         separator +
         strings::remove(std::forward<T2>(path2), separator, strings::PREFIX);
}


template <typename T1, typename T2>
inline auto join(
    T1&& path1,
    T2&& path2) -> GET_TYPE(path1)
{
  return join(std::forward<T1>(path1),
      std::forward<T2>(path2),
      os::PATH_SEPARATOR);
}


template <typename T1, typename T2, typename... Paths>
inline auto join(
    T1&& path1,
    T2&& path2,
    Paths&&... paths) -> GET_TYPE(path1)
{
  return join(std::forward<T1>(path1),
      join(std::forward<T2>(path2), std::forward<Paths>(paths)...));
}


template <typename T>
inline std::basic_string<T> join(
    const std::vector<std::basic_string<T>>& paths)
{
  if (paths.empty()) {
    return std::basic_string<T>();
  }

  std::basic_string<T> result = paths[0];
  for (size_t i = 1; i < paths.size(); ++i) {
    result = join(result, paths[i]);
  }
  return result;
}


/**
 * Returns whether the given path is an absolute path.
 * If an invalid path is given, the return result is also invalid.
 */
template <typename T>
inline bool absolute(T&& path)
{
  typedef GET_TYPE(path) STRING;
  const STRING& path_str(stringify(std::forward<T>(path)));
#ifndef __WINDOWS__
  return strings::startsWith(path_str,
      utf_convert<typename STRING::value_type>(os::PATH_SEPARATOR));
#else
  // NOTE: We do not use `PathIsRelative` Windows utility function
  // here because it does not support long paths.
  //
  // See https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
  // for details on paths. In short, an absolute path for files on Windows
  // looks like one of the following:
  //   * "[A-Za-z]:\"
  //   * "[A-Za-z]:/"
  //   * "\\?\..."
  //   * "\\server\..." where "server" is a network host.
  //
  // NOLINT(whitespace/line_length)

  // A uniform naming convention (UNC) name of any format,
  // always starts with two backslash characters.
  if (strings::startsWith(path_str,
      utf_convert<typename STRING::value_type>("\\\\"))) {
    return true;
  }

  // A disk designator with a slash, for example "C:\" or "d:/".
  if (path_str.length() < 3) {
    return false;
  }

  const STRING::value_type letter = path_str[0];
  if (!((letter >= 'A' && letter <= 'Z') ||
        (letter >= 'a' && letter <= 'z'))) {
    return false;
  }

  STRING colon = path_str.substr(1, 2);
  return colon == utf_convert<typename STRING::value_type>(":\\") ||
      colon == utf_convert<typename STRING::value_type>(":/");
#endif // __WINDOWS__
}

} // namespace path {


/**
 * Represents a POSIX or Windows file system path and offers common path
 * manipulations. When reading the comments below, keep in mind that '/' refers
 * to the path separator character, so read it as "'/' or '\', depending on
 * platform".
 */
template <typename T>
class basic_path
{
public:
  basic_path() : value(), separator((T) os::PATH_SEPARATOR) {}

  explicit basic_path(
      const std::basic_string<T>& path,
      const T path_separator = (T) os::PATH_SEPARATOR)
    : value(strings::remove(path,
                            utf_convert<T>("file://"),
                            strings::PREFIX)),
      separator(path_separator)
  {}

  // TODO(cmaloney): Add more useful operations such as 'directoryname()',
  // 'filename()', etc.

  /**
   * Extracts the component following the final '/'. Trailing '/'
   * characters are not counted as part of the pathname.
   *
   * Like the standard '::basename()' except it is thread safe.
   *
   * The following list of examples (taken from SUSv2) shows the
   * strings returned by basename() for different paths:
   *
   * path        | basename
   * ----------- | -----------
   * "/usr/lib"  | "lib"
   * "/usr/"     | "usr"
   * "usr"       | "usr"
   * "/"         | "/"
   * "."         | "."
   * ".."        | ".."
   *
   * @return The component following the final '/'. If Path does not
   *   contain a '/', this returns a copy of Path. If Path is the
   *   string "/", then this returns the string "/". If Path is an
   *   empty string, then it returns the string ".".
   */
  inline std::basic_string<T> basename() const
  {
    if (value.empty()) {
      return utf_convert<T>(".");
    }

    size_t end = value.size() - 1;

    // Remove trailing slashes.
    if (value[end] == separator) {
      end = value.find_last_not_of(separator, end);

      // Paths containing only slashes result into "/".
      if (end == std::basic_string<T>::npos) {
        return stringify(separator);
      }
    }

    // 'start' should point towards the character after the last slash
    // that is non trailing.
    size_t start = value.find_last_of(separator, end);

    if (start == std::basic_string<T>::npos) {
      start = 0;
    } else {
      start++;
    }

    return value.substr(start, end + 1 - start);
  }

  // TODO(hausdorff) Make sure this works on Windows for very short path names,
  // such as "C:\Temp". There is a distinction between "C:" and "C:\", the
  // former means "current directory of the C drive", while the latter means
  // "The root of the C drive". Also make sure that UNC paths are handled.
  // Will probably need to use the Windows path functions for that.
  /**
   * Extracts the component up to, but not including, the final '/'.
   * Trailing '/' characters are not counted as part of the pathname.
   *
   * Like the standard '::dirname()' except it is thread safe.
   *
   * The following list of examples (taken from SUSv2) shows the
   * strings returned by dirname() for different paths:
   *
   * path        | dirname
   * ----------- | -----------
   * "/usr/lib"  | "/usr"
   * "/usr/"     | "/"
   * "usr"       | "."
   * "/"         | "/"
   * "."         | "."
   * ".."        | "."
   *
   * @return The component up to, but not including, the final '/'. If
   *   Path does not contain a '/', then this returns the string ".".
   *   If Path is the string "/", then this returns the string "/".
   *   If Path is an empty string, then this returns the string ".".
   */
  inline std::basic_string<T> dirname() const
  {
    if (value.empty()) {
      return utf_convert<T>(".");
    }

    size_t end = value.size() - 1;

    // Remove trailing slashes.
    if (value[end] == separator) {
      end = value.find_last_not_of(separator, end);
    }

    // Remove anything trailing the last slash.
    end = value.find_last_of(separator, end);

    // Paths containing no slashes result in ".".
    if (end == std::basic_string<T>::npos) {
      return utf_convert<T>(".");
    }

    // Paths containing only slashes result in "/".
    if (end == 0) {
      return stringify(separator);
    }

    // 'end' should point towards the last non slash character
    // preceding the last slash.
    end = value.find_last_not_of(separator, end);

    // Paths containing no non slash characters result in "/".
    if (end == std::basic_string<T>::npos) {
      return stringify(separator);
    }

    return value.substr(0, end + 1);
  }

  /**
   * Returns the file extension of the path, including the dot.
   *
   * Returns None if the basename contains no dots, or consists
   * entirely of dots (i.e. '.', '..').
   *
   * Examples:
   *
   *   path         | extension
   *   ----------   | -----------
   *   "a.txt"      |  ".txt"
   *   "a.tar.gz"   |  ".gz"
   *   ".bashrc"    |  ".bashrc"
   *   "a"          |  None
   *   "."          |  None
   *   ".."         |  None
   */
  inline Option<std::basic_string<T>> extension() const
  {
    std::basic_string<T> _basename = basename();
    size_t index = _basename.rfind((T) '.');

    if (_basename == utf_convert<T>(".") ||
        _basename == utf_convert<T>("..") ||
        index == std::basic_string<T>::npos) {
      return None();
    }

    return _basename.substr(index);
  }

  // Checks whether the path is absolute.
  inline bool absolute() const
  {
    return path::absolute(value);
  }

  // Implicit conversion from Path to string.
  operator std::basic_string<T>() const
  {
    return value;
  }

  const std::basic_string<T>& string() const
  {
    return value;
  }

private:
  std::basic_string<T> value;
  T separator;
};


typedef basic_path<char> Path;
typedef basic_path<wchar_t> WPath;


template <typename T>
inline bool operator==(const basic_path<T>& left, const basic_path<T>& right)
{
  return left.string() == right.string();
}


template <typename T>
inline bool operator!=(const basic_path<T>& left, const basic_path<T>& right)
{
  return !(left == right);
}


template <typename T>
inline bool operator<(const basic_path<T>& left, const basic_path<T>& right)
{
  return left.string() < right.string();
}


template <typename T>
inline bool operator>(const basic_path<T>& left, const basic_path<T>& right)
{
  return right < left;
}


template <typename T>
inline bool operator<=(const basic_path<T>& left, const basic_path<T>& right)
{
  return !(left > right);
}


template <typename T>
inline bool operator>=(const basic_path<T>& left, const basic_path<T>& right)
{
  return !(left < right);
}


template <typename T>
inline std::ostream& operator<<(
    std::ostream& stream,
    const basic_path<T>& path)
{
  return stream << path.string();
}

#endif // __STOUT_PATH_HPP__
