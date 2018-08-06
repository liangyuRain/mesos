---
title: Apache Mesos - Developer Guide
layout: documentation
---

# Developer Guide

This document is distinct from the [C++ Style Guide](c++-style-guide.md) as it
covers best practices, design patterns, and other tribal knowledge, not just how
to format code correctly.

# General

## How to Navigate the Source

For a complete IDE-like experience, see the documentation on using
[cquery](cquery.md).

## When to Introduce Abstractions

Don't introduce an abstraction just for code de-duplication. Always think about
if the abstraction makes sense.

## Include What You Use

IWYU: the principle that if you use a type or symbol from a header file, that
header file should be included.

While IWYU should always be followed in C++, we have a problem specifically with
the `os` namespace. Originally, all functions like `os::realpath` were
implemented in `stout/os.hpp`. At some point, however, each of these were moved
to their own file (i.e. `stout/os/realpath.hpp`). Unfortunately, it is very easy
to use an `os` function without including its respective header because
`stout/posix/os.hpp` includes almost all of these headers. This tends to break
other platforms, as `stout/windows/os.hpp` _does not_ include all of these
headers. The guideline is to Include What You Use, especially for
`stout/os/*.hpp`.

## Error message reporting

The general pattern is to just include the reason for an error, and to not
include any information the caller already has, because otherwise the callers
will double log:

```c++
namespace os {

Try<Nothing> copyfile(string source, string destination)
{
  if (... copying failed ...) {
    return Error("Failed to copy '" + source + "' to '" + destination + "'");
  }

  return Nothing();
}

} // namespace os

Try<Nothing> copy = os::copyfile(source, destination);

if (copy.isError()) {
  return ("Failed to copy '" + source + "'"
          " to '" + destination + "': " + copy.error();
}
```

This would emit:

> Failed to copy 's' to 'd': Failed to copy 's' to 'd': No disk space left

A good way to think of this is: "what is the 'actual' error message?"

An error message consists of several parts, much like an exception: the "reason"
for the error, and multiple "stacks" of context. If you're referring to the
"reason" when you said "actual", both approaches (the one Mesos uses, or the
above example) include the reason in their returned error message. The
distinction lies in _where_ the "stacks" of context get included.

The decision Mesos took some time ago was to have the "owner" of the context be
responsible for including it. So if we call `os::copyfile` we know which
function we're calling and which `source` and `destination` we're passing into
it. This matches POSIX-style programming, which is likely why this approach was
chosen.

The POSIX-style code:

```c++
int main()
{
  int fd = open("/file");

  if (fd == -1) {
    // Caller logs the thing it was doing, and gets the reason for the error:
    LOG(ERROR) << "Failed to initialize: Failed to open '/file': " << strerror(errno);
  }
}
```

is similar to the following Mesos-style code:

```c++
int main()
{
  Try<int> fd = open("/file");

  if (fd.isError()) {
    // Caller logs the thing it was doing, and gets the reason for the error:
    LOG(ERROR) << "Failed to initialize: Failed to open '/file': " << fd.error();
  }
}
```

If we use the alternative approach to have the leaf include all the information
it has, then we have to compose differently:

```c++
int main()
{
  Try<int> fd = os::open("/file");

  if (fd.isError()) {
    // Caller knows that no additional context needs to be added because callee has all of it.
    LOG(ERROR) << "Failed to initialize: " << fd.error();
  }
}
```

The approach we chose was to treat the error as just the "reason" (much like
`strerror`), so if the caller wants to add context to it, they can. Both
approaches work, but we have to pick one and apply it consistently as best we
can. So don't add information to an error message that the caller already has.

# Handling Strings

Especially for paths in Windows, many functions need to support both
narrow string and wide string (`std::string` and `std::wstring`). We have
helper functions using template programming to handle these, so developer
do not need to write repeated code for both type of strings. Here are some
guides to use the helper functions.

## `decide_string`

`decide_string` is used to decide the type of string to use with a given type.
For any given type `T`, `decide_string<T>::type` will be either `std::string`
or `std::wstring` based on following rules.

1. If `T` is convertible to `std::string` or is `char`, 
`decide_string<T>::type` will be `std::string`.

2. If `T` cannot fall into the first category but is convertible to
`std::wstring` or is `wchar_t`, `decide_string<T>::type` will be
`std::wstring`.

3. If `T` cannot fall into the first two categories `decide_string<T>::type`
will be `std::string`.

## `stringify`

The latest version of `stringify` will stringify the object to the type of
string decided by `decide_string` unless explicitly overloaded. Here are some
examples of it:

* `stringify(std::string/std::wstring) -> std::string/std::wstring`
* `stringify(char*/wchar_t*) -> std::string/std::wstring`
* `stringify(char/wchar_t) -> std::string/std::wstring`
* `stringify(Path/WPath) -> std::string/std::wstring` for `Path` and `WPath` in
`stout` library. They implement implicit conversion to `std::string` and
`std::wstring` respectively.

For performance consideration, `stringify` will not create new string if the
type passed in is already `std::string` or `std::wstring`. In such case,
`stringify` returns a const reference to the paremeter instead. Here is some
examples:

```c++
char cstr[] = "abcdef";
std::string str = "abcdef";

const std::string& a = stringify(str);   // a is a reference to str
std::string b = stringify(str);          // b is a copy of str
const std::string& c = stringify(cstr);  // c is a copy of cstr
std::string d = stringify(cstr);         // d is a copy of cstr

std::string& e = stringify(str);         // Illegal, e must be const reference
```

`wchar_t` and `std::wstring` work in the same way as `char` and `std::string` do.

## `string_convert`

`string_convert` is similar to `stringify`. It accepts any argument `stringify`
accepts but you must explicitly specify which type of string you want as a
template argument. For example, to get a wide string out of `str`, we can do
`string_convert<wchar_t>(str)`. Note that the template argument is `wchar_t` but
not `std::wstring`, and `str` is not necessary to be narrow string.

`string_convert` is also implemented to avoid copy as possible. If the passed in
type is already in the wanted string type, it also returns const reference as
what `stringify` does.

`narrow_stringify` is equivalent to `string_convert<char>`, and `wide_stringify` is
equivalent to `string_convert<wchar_t>`.

## Guide for Writing String Functions

If you want your function to support different type of strings, here is a guide
for this. Example codes can be found in 
[`strings.hpp`](https://github.com/apache/mesos/blob/master/3rdparty/stout/include/stout/strings.hpp).

### Function Behavior

The type of the return value and the types of the parameters must be the same
type of string if applied with `decide_string`. For example, let the function
you want to write be `func(s1, s2)`, then
`decide_string<decltype(func(s1, s2))>::type`,
`decide_string<decltype(s1)>::type`, and `decide_string<decltype(s2)>::type`
should all be the same type. Here are more examples:

```c++
char n_cstr[] = "abcdef";
wchar_t w_cstr[] = L"abcdef";

std::string n_str = "abcdef";
std::wstring w_str = L"abcdef";

std::string a = func(n_cstr, n_str);  // Legal
std::wstring b = func(w_str, w_cstr); // Legal

func(n_str, w_str);                   // Illegal, parameters have different
                                      // `stringify` types
std::wstring c = func(n_cstr, n_str); // Illegal, return value has different
                                      // `stringify` type from parameters
```

### Declaration

For function `func(s1, s2)` the recommended declaration is:

```c++
template <typename T1, typename T2>
typename decide_string<T1>::type func(T1&& s1, T2&& s2);
```

Note that each parameter has a unique template argument to specify the type,
because parameters may be different types like `s1` being a `std::string` while
`s2` being a `char*`. In addition, the return type is decided by the type of
parameter. It does not matter which parameter decides the type of return value,
since it should be guaranteed that all parameters should decide to same type
of string.

### Function body

A function definition for `func(s1, s2)` should be:

```c++
template <typename T1, typename T2>
typename decide_string<T1>::type func(T1&& s1, T2&& s2) {
  typedef typename decide_string<T1>::type STRING;
  const STRING& s1_str(stringify(std::forward<T1>(s1)));
  const STRING& s2_str(stringify(std::forward<T2>(s2)));

  // From here, use s1_str and s2_str instead. Using s1 and s2
  // again causes undefined behavior.
  ......
}
```

`STRING` is guaranteed to be either `std::string` or `std::wstring`. By using
this beginning, we guarantee `s1_str` and `s2_str` to be the type of string we
want, no matter what types are `s1` and `s2`.

**It is crucial not to use parameter again in the function body, because `s1`
and `s2` may be `rvalue` that `stringify` will use move constructor to
construct string. Using them again causes undefined behavior.**

In some versions of C++ compiler, another kind of beginning is strongly
recommended:

```c++
template <typename T1, typename T2>
typename decide_string<T1>::type func(T1&& s1, T2&& s2) {
  typedef typename decide_string<T1>::type STRING;
  {
    const STRING& s1(stringify(std::forward<T1>(s1)));
    const STRING& s2(stringify(std::forward<T2>(s2)));

    ......
  }
}
```

In such beginning, the name `s1` and `s2` got overwritten in the inner
namespace. Such coding guarantees absolutely safe use. However, currently, this
only works on Windows but not linux because of different compiler versions.

**It is strongly advised not to overload such functions with same number of
parameters, because templated parameter type `T&&` can be basically deducted to
any reference type. For more information on this, see item 26 from**
*Effective Modern C++* (Scott Meyers, 2015).

# Windows

## Unicode

Mesos is explicitly compiled with `UNICODE` and `_UNICODE` preprocess
defintions, forcing the use of the wide `wchar_t` versions of ambiguous APIs.
Nonetheless, developers should be explicit when using an API: use
`::SetCurrentDirectoryW` over the ambiguous macro `::SetCurrentyDirectory`.

When converting from `std::string` to `std::wstring`, do not reinvent the wheel!
Use the `wide_stringify()` and `narrow_stringify()` or `string_convert<T>()` functions
from
[`stringify.hpp`](https://github.com/apache/mesos/blob/master/3rdparty/stout/include/stout/stringify.hpp).

## Long Path Support

Mesos has built-in NTFS long path support. On Windows, the usual maximum path is
about 255 characters (it varies per API). This is unusable because Mesos uses
directories with GUIDs, and easily exceeds this limitation. To support this, we
use the Unicode versions of the Windows APIs, and explicitly preprend the long
path marker `\\?\` to any path sent to these APIs.

The pattern, when using a Windows API which takes a path, is to:

1. Use the wide version of the API (suffixed with `W`).
2. Ensure the API supports long paths (check MSDN for the API).
3. Use `::internal::windows::longpath(std::string path)` to safely convert the path.
4. Only use the `longpath` for Windows APIs, or internal Windows API wrappers.

For an example, see
[`chdir.hpp`](https://github.com/apache/mesos/blob/master/3rdparty/stout/include/stout/os/windows/chdir.hpp).

The long path helper is found in
[`longpath.hpp`](https://github.com/apache/mesos/blob/master/3rdparty/stout/include/stout/internal/windows/longpath.hpp).

### Windows CRT

While it is tempting to use the Windows CRT to ease porting, we explicitly avoid
using it as much as possible for several reasons:

* It does not interact well with Windows APIs. For instance, an environment
  variable set by the Win32 API `SetEnvironmentVariable` will not be visible in
  the CRT API `environ`.

* The CRT APIs tend to be difficult to encapsulate properly with RAII.

* Parts of the CRT have been deprecated, and even more are marked unsafe.

It is almost always preferable to use Win32 APIs, which is akin to "Windows
system programming" rather than porting Mesos onto a POSIX compatibility layer.
It may not always be possible to avoid the CRT, but consider the implementation
carefully before using it.

## Handles

The Windows API is flawed and has multiple invalid semantic values for the
`HANDLE` type, i.e. some APIs return `-1` or `INVALID_HANDLE_VALUE`, and other
APIs return `nullptr`. It is simply
[inconsistent](https://blogs.msdn.microsoft.com/oldnewthing/20040302-00/?p=40443),
so developers must take extra caution when checking handles returned from the
Windows APIs. Please double check the documentation to determine which value
will indicate it is invalid.

Using raw handles (or indeed raw pointers anywhere) in C++ is treachorous. Mesos
has a `SafeHandle` class which should be used immediately when obtaining a
`HANDLE` from a Windows API, with the deleter likely set to `::CloseHandle`.

## Nano Server Compatibility

We would like to target Microsoft Nano Server. This means we are restricted to
the set of Windows APIs available on Nano,
[Nano Server APIs](https://msdn.microsoft.com/en-us/library/mt588480(v=vs.85).aspx).
An example of an *excluded and unavailable* set of APIs is `Shell32.dll` AKA
`<shlobj.h>`.
