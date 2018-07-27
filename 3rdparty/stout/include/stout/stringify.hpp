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

// Any type that is convertible to `std::string` including `char` will be
// `decide` to be `std::string` after stringify. Any type that cannot fall into
// the first category but is convertible to `std::wstring` including `wchar_t`
// will be `decide` to be `std::wstring`. Any type cannot fall into these two
// catefories will be `decide` to be `std::string`.
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


// `stringify` will always preserve the UTF encoding. For example,
// `std::string`, `const char*`, and `char` are all convert to `std::string`,
// and `std::wstring`, `const wchar_t*`, and `wchar_t` are all convert to
// `std::wstring`.
//
// For performance consideration, if the type passed in is already in
// `std::basic_string` type, `stringify` will not create any new copy of it.
// If the passed in is a lvalue, the return type will be a const reference to
// that lvalue `const std::basic_string &`. If the passed in is a rvalue, the
// return type will be "move" constructed `std::basic_string`.
template <typename T>
inline typename decide<T>::type stringify(const T& obj) {
  typedef typename decide<T>::type STRING;
  typedef typename STRING::value_type CHAR;
  return convert_type<std::is_convertible<T, STRING>::value, CHAR>()(obj);
}


template <typename T>
inline const std::basic_string<T>& stringify(
    std::basic_string<T>& str) {
  return str;
}


template <typename T>
inline const std::basic_string<T>& stringify(
    const std::basic_string<T>& str) {
  return str;
}


template <typename T>
inline std::basic_string<T> stringify(
    std::basic_string<T>&& str) {
  return std::move(str);
}


template <typename T1, typename T2>
struct utf_convert__ {
  std::basic_string<T1> operator()(const std::basic_string<T2>& str) {
    return std::basic_string<T1>(str.cbegin(), str.cend());
  }
};


#ifdef __WINDOWS__
template <>
struct utf_convert__<wchar_t, char> {
  std::basic_string<wchar_t> operator()(const std::basic_string<char>& str) {
    // Convert UTF-8 `string` to UTF-16 `wstring`.
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>
      converter(
          "UTF-8 to UTF-16 conversion failed",
          L"UTF-8 to UTF-16 conversion failed");

    return converter.from_bytes(str);
  }
};


template <>
struct utf_convert__<char, wchar_t> {
  std::basic_string<char> operator()(const std::basic_string<wchar_t>& str) {
    // Convert UTF-16 `wstring` to UTF-8 `string`.
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t>
      converter(
          "UTF-16 to UTF-8 conversion failed",
          L"UTF-16 to UTF-8 conversion failed");

    return converter.to_bytes(str);
  }
};
#endif // __WINDOWS__


template <typename T1, typename T2>
struct convert_decide {
  typedef typename std::conditional<
      std::is_same<T2, std::basic_string<T1>&>::value ||
          std::is_same<T2, const std::basic_string<T1>&>::value,
      const std::basic_string<T1>&,
      std::basic_string<T1>>::type type;
};


template <typename T1, typename T2>
struct utf_convert_ {
  inline typename convert_decide<T1, T2&&>::type operator()(T2&& str) {
    typedef GET_TYPE(str) STRING;
    typedef typename STRING::value_type CHAR;
    return utf_convert__<T1, CHAR>()(stringify(std::forward<T2>(str)));
  }
};


template <typename T>
struct utf_convert_<T, const std::basic_string<T>&> {
  inline const std::basic_string<T>& operator()(
      const std::basic_string<T>& str) {
    return str;
  }
};


template <typename T>
struct utf_convert_<T, std::basic_string<T>&> {
  inline const std::basic_string<T>& operator()(
      std::basic_string<T>& str) {
    return str;
  }
};


template <typename T>
struct utf_convert_<T, std::basic_string<T>&&> {
  inline std::basic_string<T> operator()(
      std::basic_string<T>&& str) {
    return std::move(str);
  }
};


// `utf_convert` can accept any argument `stringify` accept, and it can also
// change the UTF encoding by providing a single template parameter. For
// example, to convert a C `char*` string to wide string:
//
//   std::wstring wstr = utf_convert<wchar_t>(cstr);
//
// Like `stringify`, `utf_convert` is also designed to avoid copy as possible.
template <typename T1, typename T2>
inline typename convert_decide<T1, T2&&>::type utf_convert(T2&& str) {
  return utf_convert_<T1, T2&&>()(std::forward<T2>(str));
};


template <typename T>
inline typename convert_decide<char, T&&>::type short_stringify(T&& str)
{
  return utf_convert<char>(std::forward<T>(str));
}


template <typename T>
inline typename convert_decide<wchar_t, T&&>::type wide_stringify(T&& str)
{
  return utf_convert<wchar_t>(std::forward<T>(str));
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
