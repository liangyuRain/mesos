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

#define GET_TYPE(X) typename decide<decltype(X)>::type

template <typename T>
struct decide {
  typedef typename 
      std::remove_cv<typename std::remove_reference<T>::type>::type original;
  typedef typename std::conditional<
      std::is_convertible<T, std::string>::value || 
          std::is_same<original, char>::value,
      std::string,
      typename std::conditional<
          std::is_convertible<T, std::wstring>::value ||
              std::is_same<original, wchar_t>::value,
          std::wstring,
          std::string>::type>::type type;
};


template <bool DoCast, typename T1>
struct convert_type {
  template <typename T2>
  std::basic_string<T1> operator()(T2&& obj);
};


template <typename T1>
struct convert_type<true, T1> {
  template <typename T2>
  std::basic_string<T1> operator()(T2&& obj) {
    return std::forward<T2>(obj);
  }
};


template <typename T1>
struct convert_type<false, T1> {
  template <typename T2>
  std::basic_string<T1> operator()(T2&& obj) {
    std::basic_ostringstream<T1> stream;
    stream << std::forward<T2>(obj);
    if (!stream.good()) {
      ABORT("Failed to stringify!");
    }
    return stream.str();
  }
};


template <typename T>
static inline auto stringify(const T& obj) -> typename decide<T>::type {
  typedef typename decide<T>::type STRING;
  typedef typename STRING::value_type CHAR;
  return convert_type<std::is_convertible<T, STRING>::value, CHAR>()(obj);
}


template <typename T>
static inline const std::basic_string<T>& stringify(
    const std::basic_string<T>& str) {
  return str;
}


template <typename T>
static inline std::basic_string<T> stringify(
    std::basic_string<T>&& str) {
  return std::move(str);
}


template <bool same, typename T1, typename T2>
struct utf_convert_internal {
  std::basic_string<T1> operator()(const std::basic_string<T2>& str);
  std::basic_string<T1> operator()(std::basic_string<T2>&& str);
};


template <typename T>
struct utf_convert_internal<true, T, T> {
  std::basic_string<T> operator()(const std::basic_string<T>& str) {
    return str;
  }

  std::basic_string<T> operator()(std::basic_string<T>&& str) {
    return std::move(str);
  }
};


template <typename T1, typename T2>
struct utf_convert_internal<false, T1, T2> {
  std::basic_string<T1> func(const std::basic_string<T2>& str) {
    return std::basic_string<T1>(str.cbegin(), str.cend());
  }

  std::basic_string<T1> operator()(const std::basic_string<T2>& str) {
    return func(str);
  }

  std::basic_string<T1> operator()(std::basic_string<T2>&& str) {
    return func(str);
  }
};


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


inline const std::string& short_stringify(const std::string& str)
{
  return str;
}


inline std::string short_stringify(std::string&& str)
{
  return std::move(str);
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


inline const std::wstring& wide_stringify(const std::wstring& str)
{
  return str;
}


inline std::wstring wide_stringify(std::wstring&& str)
{
  return std::move(str);
}


template <>
struct utf_convert_internal<false, wchar_t, char> {
  std::basic_string<wchar_t> operator()(const std::basic_string<char>& str) {
    return wide_stringify(str);
  }

  std::basic_string<wchar_t> operator()(std::basic_string<char>&& str) {
    return wide_stringify(std::move(str));
  }
};


template <>
struct utf_convert_internal<false, char, wchar_t> {
  std::basic_string<char> operator()(const std::basic_string<wchar_t>& str) {
    return short_stringify(str);
  }

  std::basic_string<char> operator()(std::basic_string<wchar_t>&& str) {
    return short_stringify(std::move(str));
  }
};
#endif // __WINDOWS__


template <typename T1, typename T2>
inline std::basic_string<T1> utf_convert(T2&& str) {
  typedef GET_TYPE(str) STRING;
  typedef typename STRING::value_type CHAR;
  return utf_convert_internal<std::is_same<T1, CHAR>::value,
                              T1,
                              CHAR>()(stringify(std::forward<T2>(str)));
}


inline std::string stringify(const bool& b)
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
inline std::string stringify(const Error& error)
{
  return error.message;
}

#endif // __STOUT_STRINGIFY_HPP__
