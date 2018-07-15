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

#ifndef __STOUT_STRINGS_HPP__
#define __STOUT_STRINGS_HPP__

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "foreach.hpp"
#include "format.hpp"
#include "option.hpp"
#include "stringify.hpp"

#define GET_ORIGINAL(X) \
    typename std::remove_const< \
    typename std::remove_reference< \
    typename std::remove_pointer< \
    typename std::remove_all_extents<X>::type>::type>::type>::type
#define GET_TYPE(X) typename decide<decltype(X)>::type

namespace strings {

const std::string WHITESPACE = " \t\n\r";

// Flags indicating how 'remove' or 'trim' should operate.
enum Mode
{
  PREFIX,
  SUFFIX,
  ANY
};


template <typename T>
struct decide {
  typedef typename std::conditional<
      std::is_convertible<T, std::string>::value,
      std::string,
      typename std::conditional<
          std::is_convertible<T, std::wstring>::value,
          std::wstring,
          std::basic_string<GET_ORIGINAL(T)>>::type>::type type;
};


template <bool DoCast, typename T1, typename T2>
struct convert {
  std::basic_string<T2> operator()(const T1 &obj);
};

template <typename T1, typename T2>
struct convert<true, T1, T2> {
  std::basic_string<T2> operator()(const T1 &obj) {
    return (std::basic_string<T2>) obj;
  }
};

template <typename T1, typename T2>
struct convert<false, T1, T2> {
  std::basic_string<T2> operator()(const T1 &obj) {
    return std::basic_string<T2>() + obj;
  }
};


template <typename T>
static inline auto fix_cstr(const T &cstr) -> typename decide<T>::type {
  typedef typename decide<T>::type STRING;
  typedef typename STRING::value_type CHAR;
  return convert<std::is_convertible<T, STRING>::value, T, CHAR>()(cstr);
}

template <typename T>
static inline const std::basic_string<T> &fix_cstr(
    const std::basic_string<T> &str) {
  return str;
}

template <typename T>
static inline std::basic_string<T> &&fix_cstr(
    const std::basic_string<T> &&str) {
  return std::forward<std::basic_string<T>>(str);
}


template <typename T1, typename T2>
static inline std::basic_string<T1> fix_literal(const T2& literal) {
  const GET_TYPE(literal)& str(literal);
  return std::basic_string<T1>(str.cbegin(), str.cend());
}


template <typename T1 = std::string, typename T2 = std::string>
inline auto remove(
    const T1& from,
    const T2& substring,
    Mode mode = ANY) -> GET_TYPE(from)
{
  typedef GET_TYPE(from) STRING;
  const STRING& from_str(fix_cstr(from)), substring_str(fix_cstr(substring));

  STRING result = from_str;

  if (mode == PREFIX) {
    if (from_str.find(substring_str) == 0) {
      result = from_str.substr(substring_str.size());
    }
  } else if (mode == SUFFIX) {
    if (from_str.rfind(substring_str) ==
        from_str.size() - substring_str.size()) {
      result = from_str.substr(0, from_str.size() - substring_str.size());
    }
  } else {
    size_t index;
    while ((index = result.find(substring_str)) != std::string::npos) {
      result = result.erase(index, substring_str.size());
    }
  }

  return result;
}


template <typename T1 = std::string, typename T2 = std::string>
inline auto trim(
    const T1& from,
    Mode mode,
    const T2& chars) -> GET_TYPE(from)
{
  typedef GET_TYPE(from) STRING;
  const STRING& from_str(fix_cstr(from)), chars_str(fix_cstr(chars));

  size_t start = 0;
  Option<size_t> end = None();

  if (mode == ANY) {
    start = from_str.find_first_not_of(chars_str);
    end = from_str.find_last_not_of(chars_str);
  } else if (mode == PREFIX) {
    start = from_str.find_first_not_of(chars_str);
  } else if (mode == SUFFIX) {
    end = from_str.find_last_not_of(chars_str);
  }

  // Bail early if 'from' contains only characters in 'chars'.
  if (start == STRING::npos) {
    return STRING();
  }

  // Calculate the length of the substring, defaulting to the "end" of
  // string if there were no characters to remove from the suffix.
  size_t length = STRING::npos;

  // Found characters to trim at the end.
  if (end.isSome() && end.get() != STRING::npos) {
    length = end.get() + 1 - start;
  }

  return from_str.substr(start, length);
}


template <typename T = std::string>
inline auto trim(const T& from, Mode mode = ANY) -> GET_TYPE(from)
{
  typedef GET_TYPE(from) STRING;
  const STRING& from_str(fix_cstr(from));
  STRING chars(fix_literal<typename STRING::value_type>(WHITESPACE));
  return trim<STRING, STRING>(from_str, mode, chars);
}


// Helper providing some syntactic sugar for when 'mode' is ANY but
// the 'chars' are specified.
template <typename T1 = std::string, typename T2 = std::string>
inline auto trim(const T1& from, const T2& chars) -> GET_TYPE(from)
{
  return trim(fix_cstr(from), ANY, fix_cstr(chars));
}


// Replaces all the occurrences of the 'from' string with the 'to' string.
template <typename T1 = std::string,
          typename T2 = std::string,
          typename T3 = std::string>
inline auto replace(const T1& s, const T2& from, const T3& to) -> GET_TYPE(s)
{
  typedef GET_TYPE(s) STRING;
  const STRING& s_str(fix_cstr(s)),
      from_str(fix_cstr(from)),
      to_str(fix_cstr(to));

  STRING result = s_str;
  size_t index = 0;

  if (from_str.empty()) {
    return result;
  }

  while ((index = result.find(from_str, index)) != std::string::npos) {
    result.replace(index, from_str.length(), to_str);
    index += to_str.length();
  }
  return result;
}


// Tokenizes the string using the delimiters. Empty tokens will not be
// included in the result.
//
// Optionally, the maximum number of tokens to be returned can be
// specified. If the maximum number of tokens is reached, the last
// token returned contains the remainder of the input string.
template <typename T1 = std::string, typename T2 = std::string>
inline auto tokenize(
    const T1& s,
    const T2& delims,
    const Option<size_t>& maxTokens = None())
    -> std::vector<GET_TYPE(s)>
{
  typedef GET_TYPE(s) STRING;
  const STRING& s_str(fix_cstr(s)), delims_str(fix_cstr(delims));

  if (maxTokens.isSome() && maxTokens.get() == 0) {
    return {};
  }

  std::vector<STRING> tokens;
  size_t offset = 0;

  while (true) {
    size_t nonDelim = s_str.find_first_not_of(delims_str, offset);

    if (nonDelim == STRING::npos) {
      break; // Nothing left.
    }

    size_t delim = s_str.find_first_of(delims_str, nonDelim);

    // Finish tokenizing if this is the last token,
    // or we've found enough tokens.
    if (delim == STRING::npos ||
        (maxTokens.isSome() && tokens.size() == maxTokens.get() - 1)) {
      tokens.push_back(s_str.substr(nonDelim));
      break;
    }

    tokens.push_back(s_str.substr(nonDelim, delim - nonDelim));
    offset = delim;
  }

  return tokens;
}


// Splits the string using the provided delimiters. The string is
// split each time at the first character that matches any of the
// characters specified in delims.  Empty tokens are allowed in the
// result.
//
// Optionally, the maximum number of tokens to be returned can be
// specified. If the maximum number of tokens is reached, the last
// token returned contains the remainder of the input string.
template <typename T1 = std::string, typename T2 = std::string>
inline auto split(
    const T1& s,
    const T2& delims,
    const Option<size_t>& maxTokens = None())
    -> std::vector<GET_TYPE(s)>
{
  typedef GET_TYPE(s) STRING;
  const STRING& s_str(fix_cstr(s)), delims_str(fix_cstr(delims));

  if (maxTokens.isSome() && maxTokens.get() == 0) {
    return {};
  }

  std::vector<STRING> tokens;
  size_t offset = 0;

  while (true) {
    size_t next = s_str.find_first_of(delims_str, offset);

    // Finish splitting if this is the last token,
    // or we've found enough tokens.
    if (next == STRING::npos ||
        (maxTokens.isSome() && tokens.size() == maxTokens.get() - 1)) {
      tokens.push_back(s_str.substr(offset));
      break;
    }

    tokens.push_back(s_str.substr(offset, next - offset));
    offset = next + 1;
  }

  return tokens;
}


// Returns a map of strings to strings based on calling tokenize
// twice. All non-pairs are discarded. For example:
//
//   pairs("foo=1;bar=2;baz;foo=3;bam=1=2", ";&", "=")
//
// Would return a map with the following:
//   bar: ["2"]
//   foo: ["1", "3"]
template <typename T1 = std::string,
          typename T2 = std::string,
          typename T3 = std::string>
inline auto pairs(
    const T1& s,
    const T2& delims1,
    const T3& delims2)
    -> std::map<GET_TYPE(s), std::vector<GET_TYPE(s)>>
{
  typedef GET_TYPE(s) STRING;
  const STRING& s_str(fix_cstr(s)),
      delims1_str(fix_cstr(delims1)),
      delims2_str(fix_cstr(delims2));

  std::map<STRING, std::vector<STRING>> result;

  const std::vector<STRING> tokens = tokenize(s_str, delims1_str);
  foreach (const STRING& token, tokens) {
    const std::vector<STRING> pairs = tokenize(token, delims2_str);
    if (pairs.size() == 2) {
      result[pairs[0]].push_back(pairs[1]);
    }
  }

  return result;
}


namespace internal {

template <typename T1 = char, typename T2 = std::string>
inline std::basic_stringstream<T1>& append(
    std::basic_stringstream<T1>& stream,
    const T2& value)
{
  stream << ::stringify(value);
  return stream;
}


template <typename T = char>
inline std::basic_stringstream<T>& append(
    std::basic_stringstream<T>& stream,
    const std::basic_string<T>& value)
{
  stream << value;
  return stream;
}


template <typename T = char>
inline std::basic_stringstream<T>& append(
    std::basic_stringstream<T>& stream,
    const std::basic_string<T>&& value)
{
  stream << std::forward<std::basic_string<T>>(value);
  return stream;
}


template <typename T = char>
inline std::basic_stringstream<T>& append(
    std::basic_stringstream<T>& stream,
    const T*& value)
{
  stream << value;
  return stream;
}


template <typename T1 = char,
          typename T2 = std::string,
          typename T3 = std::string>
std::basic_stringstream<T1>& join(
    std::basic_stringstream<T1>& stream,
    const T3& separator,
    T2&& tail)
{
  return append<T1, T2>(stream, std::forward<T2>(tail));
}


template <typename T1, typename T2, typename THead, typename... TTail>
std::basic_stringstream<T1>& join(
    std::basic_stringstream<T1>& stream,
    const T2& separator,
    THead&& head,
    TTail&&... tail)
{
  append<T1, THead>(stream, std::forward<THead>(head)) << separator;
  internal::join(stream, separator, std::forward<TTail>(tail)...);
  return stream;
}

} // namespace internal {


template <typename T1, typename T2, typename... T>
std::basic_stringstream<T1>& join(
    std::basic_stringstream<T1>& stream,
    const T2& separator,
    T&&... args)
{
  internal::join(stream, separator, std::forward<T>(args)...);
  return stream;
}


// Use 2 heads here to disambiguate variadic argument join from the
// templatized Iterable join below. This means this implementation of
// strings::join() is only activated if there are 2 or more things to
// join.
template <typename T,
          typename THead1,
          typename THead2,
          typename... TTail>
auto join(
    const T& separator,
    THead1&& head1,
    THead2&& head2,
    TTail&&... tail) -> GET_TYPE(separator)
{
  typedef GET_TYPE(separator) STRING;
  std::basic_stringstream<typename STRING::value_type> stream;
  internal::join(
      stream,
      separator,
      std::forward<THead1>(head1),
      std::forward<THead2>(head2),
      std::forward<TTail>(tail)...);
  return stream.str();
}


// Ensure std::string doesn't fall into the iterable case
template <typename T1 = std::string, typename T2 = char>
inline std::basic_string<T2> join(
    const T1& separator,
    const std::basic_string<T2>& s) {
  return s;
}


template <typename T1 = std::string, typename T2 = char>
inline std::basic_string<T2> join(
    const T1& separator,
    const T2*& s) {
  return fix_cstr(s);
}


// Use duck-typing to join any iterable.
template <typename Iterable, typename T = std::string>
inline auto join(
    const T& separator,
    const Iterable& i) -> GET_TYPE(separator)
{
  typedef GET_TYPE(separator) STRING;
  const STRING& sep_str(fix_cstr(separator));
  STRING result;
  typename Iterable::const_iterator iterator = i.begin();
  while (iterator != i.end()) {
    result += stringify(*iterator);
    if (++iterator != i.end()) {
      result += sep_str;
    }
  }
  return result;
}


template <typename T = std::string>
inline bool checkBracketsMatching(
    const T& s,
    GET_TYPE(s)::value_type openBracket,
    GET_TYPE(s)::value_type closeBracket)
{
  typedef GET_TYPE(s) STRING;
  const STRING& s_str(fix_cstr(s));
  int count = 0;
  for (size_t i = 0; i < s_str.length(); i++) {
    if (s_str[i] == openBracket) {
      count++;
    } else if (s_str[i] == closeBracket) {
      count--;
    }
    if (count < 0) {
      return false;
    }
  }
  return count == 0;
}


template <typename T1 = std::string, typename T2 = std::string>
inline bool startsWith(const T1& s, const T2& prefix)
{
  typedef GET_TYPE(s) STRING;
  const STRING& s_str(fix_cstr(s)), prefix_str(fix_cstr(prefix));
  return s_str.size() >= prefix_str.size() &&
         std::equal(prefix_str.begin(), prefix_str.end(), s_str.begin());
}


template <typename T1 = std::string, typename T2 = std::string>
inline bool endsWith(const T1& s, const T2& suffix)
{
  typedef GET_TYPE(s) STRING;
  const STRING& s_str(fix_cstr(s)), suffix_str(fix_cstr(suffix));
  return s_str.size() >= suffix_str.size() &&
         std::equal(suffix_str.rbegin(), suffix_str.rend(), s_str.rbegin());
}


template <typename T1 = std::string, typename T2 = std::string>
inline bool contains(const T1& s, const T2& substr)
{
  typedef GET_TYPE(s) STRING;
  const STRING& s_str(fix_cstr(s)), substr_str(fix_cstr(substr));
  return s_str.find(substr_str) != STRING::npos;
}


template <typename T = std::string>
inline auto lower(const T& s) -> GET_TYPE(s)
{
  typedef GET_TYPE(s) STRING;
  STRING result = fix_cstr(s);
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}


template <typename T = std::string>
inline auto upper(const T& s) -> GET_TYPE(s)
{
  typedef GET_TYPE(s) STRING;
  STRING result = fix_cstr(s);
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

} // namespace strings {

#endif // __STOUT_STRINGS_HPP__
