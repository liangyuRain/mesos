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

#ifndef __STOUT_STRINGIFY_HPP__
#define __STOUT_STRINGIFY_HPP__

#include <iostream> // For 'std::cerr' and 'std::endl'.
#include <list>
#include <map>
#include <set>
#include <sstream> // For 'std::ostringstream'.
#include <string>
#include <vector>

#ifdef __WINDOWS__
// `codecvt` is not available on older versions of Linux. Until it is needed on
// other platforms, it's easiest to just build the UTF converter for Windows.
#include <codecvt>
#include <locale>
#endif // __WINDOWS__

#include "abort.hpp"
#include "error.hpp"
#include "hashmap.hpp"
#include "set.hpp"


template <typename T>
struct decide {
  typedef typename std::conditional<
      std::is_convertible<T, std::string>::value,
      std::string,
      typename std::conditional<
          std::is_convertible<T, std::wstring>::value,
          std::wstring,
          std::string>::type>::type type;
};


template <bool DoCast, typename T1, typename T2>
struct convert_type {
  std::basic_string<T2> operator()(const T1& obj);
};


template <typename T1, typename T2>
struct convert_type<true, T1, T2> {
  std::basic_string<T2> operator()(const T1& obj) {
    return (std::basic_string<T2>) obj;
  }
};


template <typename T1, typename T2>
struct convert_type<false, T1, T2> {
  std::basic_string<T2> operator()(const T1& obj) {
    std::basic_ostringstream<T2> stream;
    stream << obj;
    if (!stream.good()) {
      ABORT("Failed to stringify!");
    }
    return stream.str();
  }
};


template <typename T>
static inline auto stringify(const T& cstr) -> typename decide<T>::type {
  typedef typename decide<T>::type STRING;
  typedef typename STRING::value_type CHAR;
  return convert_type<std::is_convertible<T, STRING>::value, T, CHAR>()(cstr);
}


template <typename T>
static inline const std::basic_string<T>& stringify(
    const std::basic_string<T>& str) {
  return str;
}


template <typename T>
static inline std::basic_string<T>&& stringify(
    std::basic_string<T>&& str) {
  return std::forward<std::basic_string<T>>(str);
}


#ifdef __WINDOWS__
inline std::string short_stringify(const std::wstring& str)
{
  // Convert UTF-16 `wstring` to UTF-8 `string`.
  static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>
    converter(
        "UTF-16 to UTF-8 conversion failed",
        L"UTF-16 to UTF-8 conversion failed");

  return converter.to_bytes(str);
}


inline std::wstring wide_stringify(const std::string& str)
{
  // Convert UTF-8 `string` to UTF-16 `wstring`.
  static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>
    converter(
        "UTF-8 to UTF-16 conversion failed",
        L"UTF-8 to UTF-16 conversion failed");

  return converter.from_bytes(str);
}
#endif // __WINDOWS__


template<>
inline std::string stringify<bool>(const bool& b)
{
  return b ? "true" : "false";
}


template <typename T>
std::string stringify(const std::set<T>& set)
{
  std::ostringstream out;
  out << "{ ";
  typename std::set<T>::const_iterator iterator = set.begin();
  while (iterator != set.end()) {
    out << stringify(*iterator);
    if (++iterator != set.end()) {
      out << ", ";
    }
  }
  out << " }";
  return out.str();
}


template <typename T>
std::string stringify(const std::list<T>& list)
{
  std::ostringstream out;
  out << "[ ";
  typename std::list<T>::const_iterator iterator = list.begin();
  while (iterator != list.end()) {
    out << stringify(*iterator);
    if (++iterator != list.end()) {
      out << ", ";
    }
  }
  out << " ]";
  return out.str();
}


template <typename T>
std::string stringify(const std::vector<T>& vector)
{
  std::ostringstream out;
  out << "[ ";
  typename std::vector<T>::const_iterator iterator = vector.begin();
  while (iterator != vector.end()) {
    out << stringify(*iterator);
    if (++iterator != vector.end()) {
      out << ", ";
    }
  }
  out << " ]";
  return out.str();
}


template <typename K, typename V>
std::string stringify(const std::map<K, V>& map)
{
  std::ostringstream out;
  out << "{ ";
  typename std::map<K, V>::const_iterator iterator = map.begin();
  while (iterator != map.end()) {
    out << stringify(iterator->first);
    out << ": ";
    out << stringify(iterator->second);
    if (++iterator != map.end()) {
      out << ", ";
    }
  }
  out << " }";
  return out.str();
}


template <typename T>
std::string stringify(const hashset<T>& set)
{
  std::ostringstream out;
  out << "{ ";
  typename hashset<T>::const_iterator iterator = set.begin();
  while (iterator != set.end()) {
    out << stringify(*iterator);
    if (++iterator != set.end()) {
      out << ", ";
    }
  }
  out << " }";
  return out.str();
}


template <typename K, typename V>
std::string stringify(const hashmap<K, V>& map)
{
  std::ostringstream out;
  out << "{ ";
  typename hashmap<K, V>::const_iterator iterator = map.begin();
  while (iterator != map.end()) {
    out << stringify(iterator->first);
    out << ": ";
    out << stringify(iterator->second);
    if (++iterator != map.end()) {
      out << ", ";
    }
  }
  out << " }";
  return out.str();
}


// TODO(chhsiao): This overload returns a non-const rvalue for consistency.
// Consider the following overloads instead for better performance:
//   const std::string& stringify(const Error&);
//   std::string stringify(Error&&);
template<>
inline std::string stringify<Error>(const Error& error)
{
  return error.message;
}

#endif // __STOUT_STRINGIFY_HPP__
