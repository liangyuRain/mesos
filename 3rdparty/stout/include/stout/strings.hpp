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

namespace strings {

const std::string WHITESPACE = " \t\n\r";

// Flags indicating how 'remove' or 'trim' should operate.
enum Mode
{
  PREFIX,
  SUFFIX,
  ANY
};

// `strings` functions support both `std::string` and `std::wstring`, as long
// as all the parameters and return value are in the same UTF encoding.

template <typename T1, typename T2>
inline GET_TYPE(T1) remove(
    T1&& from,
    T2&& substring,
    Mode mode = ANY)
{
  typedef GET_TYPE(T1) STRING;
  const STRING& from_str(stringify(std::forward<T1>(from))),
      substring_str(stringify(std::forward<T2>(substring)));

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


template <typename T1, typename T2>
inline GET_TYPE(T1) trim(
    T1&& from,
    Mode mode,
    T2&& chars)
{
  typedef GET_TYPE(T1) STRING;
  const STRING& from_str(stringify(std::forward<T1>(from))),
      chars_str(stringify(std::forward<T2>(chars)));

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


template <typename T>
inline GET_TYPE(T) trim(T&& from, Mode mode = ANY)
{
  typedef GET_TYPE(T) STRING;
  return trim(std::forward<T>(from), mode,
      utf_convert<typename STRING::value_type>(WHITESPACE));
}


// Helper providing some syntactic sugar for when 'mode' is ANY but
// the 'chars' are specified.
template <typename T1, typename T2>
inline GET_TYPE(T1) trim(T1&& from, T2&& chars)
{
  return trim(std::forward<T1>(from), ANY, std::forward<T2>(chars));
}


// Replaces all the occurrences of the 'from' string with the 'to' string.
template <typename T1, typename T2, typename T3>
inline GET_TYPE(T1) replace(T1&& s, T2&& from, T3&& to)
{
  typedef GET_TYPE(T1) STRING;
  const STRING& s_str(stringify(std::forward<T1>(s))),
      from_str(stringify(std::forward<T2>(from))),
      to_str(stringify(std::forward<T3>(to)));

  if (from_str.empty()) {
    return s_str;
  }

  STRING result;
  size_t begin = 0, end = 0;

  do {
    end = s_str.find(from_str, begin);
    if (end == STRING::npos) {
      end = s_str.size();
    }
    while(begin < end) {
      result += s_str[begin++];
    }
    if (end != s_str.size()) {
      result += to_str;
    }
    begin = end + from_str.size();
  } while(begin < s_str.size());

  return result;
}


// Tokenizes the string using the delimiters. Empty tokens will not be
// included in the result.
//
// Optionally, the maximum number of tokens to be returned can be
// specified. If the maximum number of tokens is reached, the last
// token returned contains the remainder of the input string.
template <typename T1, typename T2>
inline std::vector<GET_TYPE(T1)> tokenize(
    T1&& s,
    T2&& delims,
    const Option<size_t>& maxTokens = None())
{
  typedef GET_TYPE(T1) STRING;
  const STRING& s_str(stringify(std::forward<T1>(s))),
      delims_str(stringify(std::forward<T2>(delims)));

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
template <typename T1, typename T2>
inline std::vector<GET_TYPE(T1)> split(
    T1&& s,
    T2&& delims,
    const Option<size_t>& maxTokens = None())
{
  typedef GET_TYPE(T1) STRING;
  const STRING& s_str(stringify(std::forward<T1>(s))),
      delims_str(stringify(std::forward<T2>(delims)));

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
template <typename T1, typename T2, typename T3>
inline std::map<GET_TYPE(T1), std::vector<GET_TYPE(T1)>> pairs(
    T1&& s,
    T2&& delims1,
    T3&& delims2)
{
  typedef GET_TYPE(T1) STRING;
  const STRING& s_str(stringify(std::forward<T1>(s))),
      delims1_str(stringify(std::forward<T2>(delims1))),
      delims2_str(stringify(std::forward<T3>(delims2)));

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

template <typename T1, typename T2>
inline std::basic_stringstream<T1>& append(
    std::basic_stringstream<T1>& stream,
    T2&& value)
{
  stream << ::stringify(std::forward<T2>(value));
  return stream;
}


template <typename T1, typename T2, typename T3>
std::basic_stringstream<T1>& join(
    std::basic_stringstream<T1>& stream,
    T3&& separator,
    T2&& tail)
{
  return append(stream, std::forward<T2>(tail));
}


template <typename T1, typename T2, typename THead, typename... TTail>
std::basic_stringstream<T1>& join(
    std::basic_stringstream<T1>& stream,
    T2&& separator,
    THead&& head,
    TTail&&... tail)
{
  append<T1, THead>(stream, std::forward<THead>(head))
      << std::forward<T2>(separator);
  internal::join(stream, separator, std::forward<TTail>(tail)...);
  return stream;
}

} // namespace internal {


template <typename T1, typename T2, typename... T>
std::basic_stringstream<T1>& join(
    std::basic_stringstream<T1>& stream,
    T2&& separator,
    T&&... args)
{
  internal::join(stream, std::forward<T2>(separator),
      std::forward<T>(args)...);
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
GET_TYPE(T) join(
    const T& separator,
    THead1&& head1,
    THead2&& head2,
    TTail&&... tail)
{
  typedef GET_TYPE(T) STRING;
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
template <typename T1, typename T2>
inline std::basic_string<T2> join(
    T1&& separator,
    const std::basic_string<T2>& s) {
  return s;
}


template <typename T1, typename T2>
inline std::basic_string<T2> join(
    T1&& separator,
    const T2*& s) {
  return stringify(s);
}


// Use duck-typing to join any iterable.
template <typename Iterable, typename T>
inline GET_TYPE(T) join(
    T&& separator,
    const Iterable& i)
{
  typedef GET_TYPE(T) STRING;
  const STRING& sep_str(stringify(std::forward<T>(separator)));
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


template <typename T>
inline bool checkBracketsMatching(
    T&& s,
    const GET_TYPE(T)::value_type openBracket,
    const GET_TYPE(T)::value_type closeBracket)
{
  typedef GET_TYPE(T) STRING;
  const STRING& s_str(stringify(std::forward<T>(s)));
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


template <typename T1, typename T2>
inline bool startsWith(T1&& s, T2&& prefix)
{
  typedef GET_TYPE(T1) STRING;
  const STRING& s_str(stringify(std::forward<T1>(s))),
      prefix_str(stringify(std::forward<T2>(prefix)));
  return s_str.size() >= prefix_str.size() &&
         std::equal(prefix_str.begin(), prefix_str.end(), s_str.begin());
}


template <typename T1, typename T2>
inline bool endsWith(T1&& s, T2&& suffix)
{
  typedef GET_TYPE(T1) STRING;
  const STRING& s_str(stringify(std::forward<T1>(s))),
      suffix_str(stringify(std::forward<T2>(suffix)));
  return s_str.size() >= suffix_str.size() &&
         std::equal(suffix_str.rbegin(), suffix_str.rend(), s_str.rbegin());
}


template <typename T1, typename T2>
inline bool contains(T1&& s, T2&& substr)
{
  typedef GET_TYPE(T1) STRING;
  const STRING& s_str(stringify(std::forward<T1>(s))),
      substr_str(stringify(std::forward<T2>(substr)));
  return s_str.find(substr_str) != STRING::npos;
}


template <typename T>
inline GET_TYPE(T) lower(T&& s)
{
  typedef GET_TYPE(T) STRING;
  STRING result = stringify(std::forward<T>(s));
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}


template <typename T>
inline GET_TYPE(T) upper(T&& s)
{
  typedef GET_TYPE(T) STRING;
  STRING result = stringify(std::forward<T>(s));
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

} // namespace strings {

#endif // __STOUT_STRINGS_HPP__
