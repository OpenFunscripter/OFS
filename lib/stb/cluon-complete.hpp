// This is an auto-generated header-only single-file distribution of libcluon.
// Date: Thu, 04 Feb 2021 12:20:57 +0000
// Version: 0.0.140
//
//
// Implementation of N4562 std::experimental::any (merged into C++17) for C++11 compilers.
//
// See also:
//   + http://en.cppreference.com/w/cpp/any
//   + http://en.cppreference.com/w/cpp/experimental/any
//   + http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4562.html#any
//   + https://cplusplus.github.io/LWG/lwg-active.html#2509
//
//
// Copyright (c) 2016 Denilson das Mercês Amorim
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef LINB_ANY_HPP
#define LINB_ANY_HPP
//#pragma once
#include <typeinfo>
#include <type_traits>
#include <stdexcept>


#if defined(PARTICLE)
#if !defined(__cpp_exceptions) && !defined(ANY_IMPL_NO_EXCEPTIONS) && !defined(ANY_IMPL_EXCEPTIONS)
#   define ANY_IMPL_NO_EXCEPTIONS
# endif
#else
// you can opt-out of exceptions by definining ANY_IMPL_NO_EXCEPTIONS,
// but you must ensure not to cast badly when passing an `any' object to any_cast<T>(any)
#endif

#if defined(PARTICLE)
#if !defined(__cpp_rtti) && !defined(ANY_IMPL_NO_RTTI) && !defined(ANY_IMPL_RTTI)
#   define ANY_IMPL_NO_RTTI
# endif
#else
// you can opt-out of RTTI by defining ANY_IMPL_NO_RTTI,
// in order to disable functions working with the typeid of a type
#endif


namespace linb
{

class bad_any_cast : public std::bad_cast
{
public:
    const char* what() const noexcept override
    {
        return "bad any cast";
    }
};

class any final
{
public:
    /// Constructs an object of type any with an empty state.
    any() :
        vtable(nullptr)
    {
    }

    /// Constructs an object of type any with an equivalent state as other.
    any(const any& rhs) :
        vtable(rhs.vtable)
    {
        if(!rhs.empty())
        {
            rhs.vtable->copy(rhs.storage, this->storage);
        }
    }

    /// Constructs an object of type any with a state equivalent to the original state of other.
    /// rhs is left in a valid but otherwise unspecified state.
    any(any&& rhs) noexcept :
        vtable(rhs.vtable)
    {
        if(!rhs.empty())
        {
            rhs.vtable->move(rhs.storage, this->storage);
            rhs.vtable = nullptr;
        }
    }

    /// Same effect as this->clear().
    ~any()
    {
        this->clear();
    }

    /// Constructs an object of type any that contains an object of type T direct-initialized with std::forward<ValueType>(value).
    ///
    /// T shall satisfy the CopyConstructible requirements, otherwise the program is ill-formed.
    /// This is because an `any` may be copy constructed into another `any` at any time, so a copy should always be allowed.
    template<typename ValueType, typename = typename std::enable_if<!std::is_same<typename std::decay<ValueType>::type, any>::value>::type>
    any(ValueType&& value)
    {
        static_assert(std::is_copy_constructible<typename std::decay<ValueType>::type>::value,
            "T shall satisfy the CopyConstructible requirements.");
        this->construct(std::forward<ValueType>(value));
    }

    /// Has the same effect as any(rhs).swap(*this). No effects if an exception is thrown.
    any& operator=(const any& rhs)
    {
        any(rhs).swap(*this);
        return *this;
    }

    /// Has the same effect as any(std::move(rhs)).swap(*this).
    ///
    /// The state of *this is equivalent to the original state of rhs and rhs is left in a valid
    /// but otherwise unspecified state.
    any& operator=(any&& rhs) noexcept
    {
        any(std::move(rhs)).swap(*this);
        return *this;
    }

    /// Has the same effect as any(std::forward<ValueType>(value)).swap(*this). No effect if a exception is thrown.
    ///
    /// T shall satisfy the CopyConstructible requirements, otherwise the program is ill-formed.
    /// This is because an `any` may be copy constructed into another `any` at any time, so a copy should always be allowed.
    template<typename ValueType, typename = typename std::enable_if<!std::is_same<typename std::decay<ValueType>::type, any>::value>::type>
    any& operator=(ValueType&& value)
    {
        static_assert(std::is_copy_constructible<typename std::decay<ValueType>::type>::value,
            "T shall satisfy the CopyConstructible requirements.");
        any(std::forward<ValueType>(value)).swap(*this);
        return *this;
    }

    /// If not empty, destroys the contained object.
    void clear() noexcept
    {
        if(!empty())
        {
            this->vtable->destroy(storage);
            this->vtable = nullptr;
        }
    }

    /// Returns true if *this has no contained object, otherwise false.
    bool empty() const noexcept
    {
        return this->vtable == nullptr;
    }

#ifndef ANY_IMPL_NO_RTTI
    /// If *this has a contained object of type T, typeid(T); otherwise typeid(void).
    const std::type_info& type() const noexcept
    {
        return empty()? typeid(void) : this->vtable->type();
    }
#endif

    /// Exchange the states of *this and rhs.
    void swap(any& rhs) noexcept
    {
        if(this->vtable != rhs.vtable)
        {
            any tmp(std::move(rhs));

            // move from *this to rhs.
            rhs.vtable = this->vtable;
            if(this->vtable != nullptr)
            {
                this->vtable->move(this->storage, rhs.storage);
                //this->vtable = nullptr; -- unneeded, see below
            }

            // move from tmp (previously rhs) to *this.
            this->vtable = tmp.vtable;
            if(tmp.vtable != nullptr)
            {
                tmp.vtable->move(tmp.storage, this->storage);
                tmp.vtable = nullptr;
            }
        }
        else // same types
        {
            if(this->vtable != nullptr)
                this->vtable->swap(this->storage, rhs.storage);
        }
    }

private: // Storage and Virtual Method Table

    union storage_union
    {
        using stack_storage_t = typename std::aligned_storage<2 * sizeof(void*), std::alignment_of<void*>::value>::type;

        void*               dynamic;
        stack_storage_t     stack;      // 2 words for e.g. shared_ptr
    };

    /// Base VTable specification.
    struct vtable_type
    {
        // Note: The caller is responssible for doing .vtable = nullptr after destructful operations
        // such as destroy() and/or move().

#ifndef ANY_IMPL_NO_RTTI
        /// The type of the object this vtable is for.
        const std::type_info& (*type)() noexcept;
#endif

        /// Destroys the object in the union.
        /// The state of the union after this call is unspecified, caller must ensure not to use src anymore.
        void(*destroy)(storage_union&) noexcept;

        /// Copies the **inner** content of the src union into the yet unitialized dest union.
        /// As such, both inner objects will have the same state, but on separate memory locations.
        void(*copy)(const storage_union& src, storage_union& dest);

        /// Moves the storage from src to the yet unitialized dest union.
        /// The state of src after this call is unspecified, caller must ensure not to use src anymore.
        void(*move)(storage_union& src, storage_union& dest) noexcept;

        /// Exchanges the storage between lhs and rhs.
        void(*swap)(storage_union& lhs, storage_union& rhs) noexcept;
    };

    /// VTable for dynamically allocated storage.
    template<typename T>
    struct vtable_dynamic
    {
#ifndef ANY_IMPL_NO_RTTI
        static const std::type_info& type() noexcept
        {
            return typeid(T);
        }
#endif

        static void destroy(storage_union& storage) noexcept
        {
            //assert(reinterpret_cast<T*>(storage.dynamic));
            delete reinterpret_cast<T*>(storage.dynamic);
        }

        static void copy(const storage_union& src, storage_union& dest)
        {
            dest.dynamic = new T(*reinterpret_cast<const T*>(src.dynamic));
        }

        static void move(storage_union& src, storage_union& dest) noexcept
        {
            dest.dynamic = src.dynamic;
            src.dynamic = nullptr;
        }

        static void swap(storage_union& lhs, storage_union& rhs) noexcept
        {
            // just exchage the storage pointers.
            std::swap(lhs.dynamic, rhs.dynamic);
        }
    };

    /// VTable for stack allocated storage.
    template<typename T>
    struct vtable_stack
    {
#ifndef ANY_IMPL_NO_RTTI
        static const std::type_info& type() noexcept
        {
            return typeid(T);
        }
#endif

        static void destroy(storage_union& storage) noexcept
        {
            reinterpret_cast<T*>(&storage.stack)->~T();
        }

        static void copy(const storage_union& src, storage_union& dest)
        {
            new (&dest.stack) T(reinterpret_cast<const T&>(src.stack));
        }

        static void move(storage_union& src, storage_union& dest) noexcept
        {
            // one of the conditions for using vtable_stack is a nothrow move constructor,
            // so this move constructor will never throw a exception.
            new (&dest.stack) T(std::move(reinterpret_cast<T&>(src.stack)));
            destroy(src);
        }

        static void swap(storage_union& lhs, storage_union& rhs) noexcept
        {
            storage_union tmp_storage;
            move(rhs, tmp_storage);
            move(lhs, rhs);
            move(tmp_storage, lhs);
        }
    };

    /// Whether the type T must be dynamically allocated or can be stored on the stack.
    template<typename T>
    struct requires_allocation :
        std::integral_constant<bool,
                !(std::is_nothrow_move_constructible<T>::value      // N4562 §6.3/3 [any.class]
                  && sizeof(T) <= sizeof(storage_union::stack)
                  && std::alignment_of<T>::value <= std::alignment_of<storage_union::stack_storage_t>::value)>
    {};

    /// Returns the pointer to the vtable of the type T.
    template<typename T>
    static vtable_type* vtable_for_type()
    {
        using VTableType = typename std::conditional<requires_allocation<T>::value, vtable_dynamic<T>, vtable_stack<T>>::type;
        static vtable_type table = {
#ifndef ANY_IMPL_NO_RTTI
            VTableType::type,
#endif
            VTableType::destroy,
            VTableType::copy, VTableType::move,
            VTableType::swap,
        };
        return &table;
    }

protected:
    template<typename T>
    friend const T* any_cast(const any* operand) noexcept;
    template<typename T>
    friend T* any_cast(any* operand) noexcept;

    /// Casts (with no type_info checks) the storage pointer as const T*.
    template<typename T>
    const T* cast() const noexcept
    {
        return requires_allocation<typename std::decay<T>::type>::value?
            reinterpret_cast<const T*>(storage.dynamic) :
            reinterpret_cast<const T*>(&storage.stack);
    }

    /// Casts (with no type_info checks) the storage pointer as T*.
    template<typename T>
    T* cast() noexcept
    {
        return requires_allocation<typename std::decay<T>::type>::value?
            reinterpret_cast<T*>(storage.dynamic) :
            reinterpret_cast<T*>(&storage.stack);
    }

private:
    storage_union storage; // on offset(0) so no padding for align
    vtable_type*  vtable;

    template<typename ValueType, typename T>
    typename std::enable_if<requires_allocation<T>::value>::type
    do_construct(ValueType&& value)
    {
        storage.dynamic = new T(std::forward<ValueType>(value));
    }

    template<typename ValueType, typename T>
    typename std::enable_if<!requires_allocation<T>::value>::type
    do_construct(ValueType&& value)
    {
        new (&storage.stack) T(std::forward<ValueType>(value));
    }

    /// Chooses between stack and dynamic allocation for the type decay_t<ValueType>,
    /// assigns the correct vtable, and constructs the object on our storage.
    template<typename ValueType>
    void construct(ValueType&& value)
    {
        using T = typename std::decay<ValueType>::type;

        this->vtable = vtable_for_type<T>();

        do_construct<ValueType,T>(std::forward<ValueType>(value));
    }
};



namespace detail
{
    template<typename ValueType>
    inline ValueType any_cast_move_if_true(typename std::remove_reference<ValueType>::type* p, std::true_type)
    {
        return std::move(*p);
    }

    template<typename ValueType>
    inline ValueType any_cast_move_if_true(typename std::remove_reference<ValueType>::type* p, std::false_type)
    {
        return *p;
    }
}

/// Performs *any_cast<add_const_t<remove_reference_t<ValueType>>>(&operand), or throws bad_any_cast on failure.
template<typename ValueType>
inline ValueType any_cast(const any& operand)
{
    auto p = any_cast<typename std::add_const<typename std::remove_reference<ValueType>::type>::type>(&operand);
#ifndef ANY_IMPL_NO_EXCEPTIONS
    if(p == nullptr) throw bad_any_cast();
#endif
    return *p;
}

/// Performs *any_cast<remove_reference_t<ValueType>>(&operand), or throws bad_any_cast on failure.
template<typename ValueType>
inline ValueType any_cast(any& operand)
{
    auto p = any_cast<typename std::remove_reference<ValueType>::type>(&operand);
#ifndef ANY_IMPL_NO_EXCEPTIONS
    if(p == nullptr) throw bad_any_cast();
#endif
    return *p;
}

///
/// If ValueType is MoveConstructible and isn't a lvalue reference, performs
/// std::move(*any_cast<remove_reference_t<ValueType>>(&operand)), otherwise
/// *any_cast<remove_reference_t<ValueType>>(&operand). Throws bad_any_cast on failure.
///
template<typename ValueType>
inline ValueType any_cast(any&& operand)
{
    using can_move = std::integral_constant<bool,
        std::is_move_constructible<ValueType>::value
        && !std::is_lvalue_reference<ValueType>::value>;

    auto p = any_cast<typename std::remove_reference<ValueType>::type>(&operand);
#ifndef ANY_IMPL_NO_EXCEPTIONS
    if(p == nullptr) throw bad_any_cast();
#endif
    return detail::any_cast_move_if_true<ValueType>(p, can_move());
}

/// If operand != nullptr && operand->type() == typeid(ValueType), a pointer to the object
/// contained by operand, otherwise nullptr.
template<typename ValueType>
inline const ValueType* any_cast(const any* operand) noexcept
{
    using T = typename std::decay<ValueType>::type;

    if (operand && operand->vtable == any::vtable_for_type<T>())
        return operand->cast<ValueType>();
    else
        return nullptr;
}

/// If operand != nullptr && operand->type() == typeid(ValueType), a pointer to the object
/// contained by operand, otherwise nullptr.
template<typename ValueType>
inline ValueType* any_cast(any* operand) noexcept
{
    using T = typename std::decay<ValueType>::type;

    if (operand && operand->vtable == any::vtable_for_type<T>())
        return operand->cast<ValueType>();
    else
        return nullptr;
}

}

namespace std
{
    inline void swap(linb::any& lhs, linb::any& rhs) noexcept
    {
        lhs.swap(rhs);
    }
}

#endif
//
//  peglib.h
//
//  Copyright (c) 2020 Yuji Hirose. All rights reserved.
//  MIT License
//

#ifndef CPPPEGLIB_PEGLIB_H
#define CPPPEGLIB_PEGLIB_H

#ifndef PEGLIB_USE_STD_ANY
#ifdef _MSVC_LANG
#define PEGLIB_USE_STD_ANY _MSVC_LANG >= 201703L
#elif defined(__cplusplus)
#define PEGLIB_USE_STD_ANY __cplusplus >= 201703L
#endif
#endif // PEGLIB_USE_STD_ANY

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#if PEGLIB_USE_STD_ANY
#include <any>
#endif

// guard for older versions of VC++
#ifdef _MSC_VER
#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
#error "Requires complete C+11 support"
#endif
#endif

namespace peg {

/*-----------------------------------------------------------------------------
 *  any
 *---------------------------------------------------------------------------*/

#if PEGLIB_USE_STD_ANY
using any = std::any;

// Define a function alias to std::any_cast using perfect forwarding
template <typename T, typename... Args>
auto any_cast(Args &&... args)
    -> decltype(std::any_cast<T>(std::forward<Args>(args)...)) {
  return std::any_cast<T>(std::forward<Args>(args)...);
}
#else
class any {
public:
  any() = default;

  any(const any &rhs) : content_(rhs.clone()) {}

  any(any &&rhs) : content_(rhs.content_) { rhs.content_ = nullptr; }

  template <typename T> any(const T &value) : content_(new holder<T>(value)) {}

  any &operator=(const any &rhs) {
    if (this != &rhs) {
      if (content_) { delete content_; }
      content_ = rhs.clone();
    }
    return *this;
  }

  any &operator=(any &&rhs) {
    if (this != &rhs) {
      if (content_) { delete content_; }
      content_ = rhs.content_;
      rhs.content_ = nullptr;
    }
    return *this;
  }

  ~any() { delete content_; }

  bool has_value() const { return content_ != nullptr; }

  template <typename T> friend T &any_cast(any &val);

  template <typename T> friend const T &any_cast(const any &val);

private:
  struct placeholder {
    virtual ~placeholder() {}
    virtual placeholder *clone() const = 0;
  };

  template <typename T> struct holder : placeholder {
    holder(const T &value) : value_(value) {}
    placeholder *clone() const override { return new holder(value_); }
    T value_;
  };

  placeholder *clone() const { return content_ ? content_->clone() : nullptr; }

  placeholder *content_ = nullptr;
};

template <typename T> T &any_cast(any &val) {
  if (!val.content_) { throw std::bad_cast(); }
  auto p = dynamic_cast<any::holder<T> *>(val.content_);
  assert(p);
  if (!p) { throw std::bad_cast(); }
  return p->value_;
}

template <> inline any &any_cast<any>(any &val) { return val; }

template <typename T> const T &any_cast(const any &val) {
  assert(val.content_);
  auto p = dynamic_cast<any::holder<T> *>(val.content_);
  assert(p);
  if (!p) { throw std::bad_cast(); }
  return p->value_;
}

template <> inline const any &any_cast<any>(const any &val) { return val; }
#endif

/*-----------------------------------------------------------------------------
 *  scope_exit
 *---------------------------------------------------------------------------*/

// This is based on
// "http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4189".

template <typename EF> struct scope_exit {
  explicit scope_exit(EF &&f)
      : exit_function(std::move(f)), execute_on_destruction{true} {}

  scope_exit(scope_exit &&rhs)
      : exit_function(std::move(rhs.exit_function)),
        execute_on_destruction{rhs.execute_on_destruction} {
    rhs.release();
  }

  ~scope_exit() {
    if (execute_on_destruction) { this->exit_function(); }
  }

  void release() { this->execute_on_destruction = false; }

private:
  scope_exit(const scope_exit &) = delete;
  void operator=(const scope_exit &) = delete;
  scope_exit &operator=(scope_exit &&) = delete;

  EF exit_function;
  bool execute_on_destruction;
};

template <typename EF>
auto make_scope_exit(EF &&exit_function) -> scope_exit<EF> {
  return scope_exit<typename std::remove_reference<EF>::type>(
      std::forward<EF>(exit_function));
}

/*-----------------------------------------------------------------------------
 *  UTF8 functions
 *---------------------------------------------------------------------------*/

inline size_t codepoint_length(const char *s8, size_t l) {
  if (l) {
    auto b = static_cast<uint8_t>(s8[0]);
    if ((b & 0x80) == 0) {
      return 1;
    } else if ((b & 0xE0) == 0xC0) {
      return 2;
    } else if ((b & 0xF0) == 0xE0) {
      return 3;
    } else if ((b & 0xF8) == 0xF0) {
      return 4;
    }
  }
  return 0;
}

inline size_t encode_codepoint(char32_t cp, char *buff) {
  if (cp < 0x0080) {
    buff[0] = static_cast<char>(cp & 0x7F);
    return 1;
  } else if (cp < 0x0800) {
    buff[0] = static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
    buff[1] = static_cast<char>(0x80 | (cp & 0x3F));
    return 2;
  } else if (cp < 0xD800) {
    buff[0] = static_cast<char>(0xE0 | ((cp >> 12) & 0xF));
    buff[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buff[2] = static_cast<char>(0x80 | (cp & 0x3F));
    return 3;
  } else if (cp < 0xE000) {
    // D800 - DFFF is invalid...
    return 0;
  } else if (cp < 0x10000) {
    buff[0] = static_cast<char>(0xE0 | ((cp >> 12) & 0xF));
    buff[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buff[2] = static_cast<char>(0x80 | (cp & 0x3F));
    return 3;
  } else if (cp < 0x110000) {
    buff[0] = static_cast<char>(0xF0 | ((cp >> 18) & 0x7));
    buff[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    buff[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buff[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
  }
  return 0;
}

inline std::string encode_codepoint(char32_t cp) {
  char buff[4];
  auto l = encode_codepoint(cp, buff);
  return std::string(buff, l);
}

inline bool decode_codepoint(const char *s8, size_t l, size_t &bytes,
                             char32_t &cp) {
  if (l) {
    auto b = static_cast<uint8_t>(s8[0]);
    if ((b & 0x80) == 0) {
      bytes = 1;
      cp = b;
      return true;
    } else if ((b & 0xE0) == 0xC0) {
      if (l >= 2) {
        bytes = 2;
        cp = ((static_cast<char32_t>(s8[0] & 0x1F)) << 6) |
             (static_cast<char32_t>(s8[1] & 0x3F));
        return true;
      }
    } else if ((b & 0xF0) == 0xE0) {
      if (l >= 3) {
        bytes = 3;
        cp = ((static_cast<char32_t>(s8[0] & 0x0F)) << 12) |
             ((static_cast<char32_t>(s8[1] & 0x3F)) << 6) |
             (static_cast<char32_t>(s8[2] & 0x3F));
        return true;
      }
    } else if ((b & 0xF8) == 0xF0) {
      if (l >= 4) {
        bytes = 4;
        cp = ((static_cast<char32_t>(s8[0] & 0x07)) << 18) |
             ((static_cast<char32_t>(s8[1] & 0x3F)) << 12) |
             ((static_cast<char32_t>(s8[2] & 0x3F)) << 6) |
             (static_cast<char32_t>(s8[3] & 0x3F));
        return true;
      }
    }
  }
  return false;
}

inline size_t decode_codepoint(const char *s8, size_t l, char32_t &out) {
  size_t bytes;
  if (decode_codepoint(s8, l, bytes, out)) { return bytes; }
  return 0;
}

inline char32_t decode_codepoint(const char *s8, size_t l) {
  char32_t out = 0;
  decode_codepoint(s8, l, out);
  return out;
}

inline std::u32string decode(const char *s8, size_t l) {
  std::u32string out;
  size_t i = 0;
  while (i < l) {
    auto beg = i++;
    while (i < l && (s8[i] & 0xc0) == 0x80) {
      i++;
    }
    out += decode_codepoint(&s8[beg], (i - beg));
  }
  return out;
}

/*-----------------------------------------------------------------------------
 *  resolve_escape_sequence
 *---------------------------------------------------------------------------*/

inline bool is_hex(char c, int &v) {
  if ('0' <= c && c <= '9') {
    v = c - '0';
    return true;
  } else if ('a' <= c && c <= 'f') {
    v = c - 'a' + 10;
    return true;
  } else if ('A' <= c && c <= 'F') {
    v = c - 'A' + 10;
    return true;
  }
  return false;
}

inline bool is_digit(char c, int &v) {
  if ('0' <= c && c <= '9') {
    v = c - '0';
    return true;
  }
  return false;
}

inline std::pair<int, size_t> parse_hex_number(const char *s, size_t n,
                                               size_t i) {
  int ret = 0;
  int val;
  while (i < n && is_hex(s[i], val)) {
    ret = static_cast<int>(ret * 16 + val);
    i++;
  }
  return std::make_pair(ret, i);
}

inline std::pair<int, size_t> parse_octal_number(const char *s, size_t n,
                                                 size_t i) {
  int ret = 0;
  int val;
  while (i < n && is_digit(s[i], val)) {
    ret = static_cast<int>(ret * 8 + val);
    i++;
  }
  return std::make_pair(ret, i);
}

inline std::string resolve_escape_sequence(const char *s, size_t n) {
  std::string r;
  r.reserve(n);

  size_t i = 0;
  while (i < n) {
    auto ch = s[i];
    if (ch == '\\') {
      i++;
      if (i == n) { throw std::runtime_error("Invalid escape sequence..."); }
      switch (s[i]) {
      case 'n':
        r += '\n';
        i++;
        break;
      case 'r':
        r += '\r';
        i++;
        break;
      case 't':
        r += '\t';
        i++;
        break;
      case '\'':
        r += '\'';
        i++;
        break;
      case '"':
        r += '"';
        i++;
        break;
      case '[':
        r += '[';
        i++;
        break;
      case ']':
        r += ']';
        i++;
        break;
      case '\\':
        r += '\\';
        i++;
        break;
      case 'x':
      case 'u': {
        char32_t cp;
        std::tie(cp, i) = parse_hex_number(s, n, i + 1);
        r += encode_codepoint(cp);
        break;
      }
      default: {
        char32_t cp;
        std::tie(cp, i) = parse_octal_number(s, n, i);
        r += encode_codepoint(cp);
        break;
      }
      }
    } else {
      r += ch;
      i++;
    }
  }
  return r;
}

/*-----------------------------------------------------------------------------
 *  Trie
 *---------------------------------------------------------------------------*/

class Trie {
public:
  Trie() = default;
  Trie(const Trie &) = default;

  Trie(const std::vector<std::string> &items) {
    for (const auto &item : items) {
      for (size_t len = 1; len <= item.size(); len++) {
        auto last = len == item.size();
        std::string s(item.c_str(), len);
        auto it = dic_.find(s);
        if (it == dic_.end()) {
          dic_.emplace(s, Info{last, last});
        } else if (last) {
          it->second.match = true;
        } else {
          it->second.done = false;
        }
      }
    }
  }

  size_t match(const char *text, size_t text_len) const {
    size_t match_len = 0;
    {
      auto done = false;
      size_t len = 1;
      while (!done && len <= text_len) {
        std::string s(text, len);
        auto it = dic_.find(s);
        if (it == dic_.end()) {
          done = true;
        } else {
          if (it->second.match) { match_len = len; }
          if (it->second.done) { done = true; }
        }
        len += 1;
      }
    }
    return match_len;
  }

private:
  struct Info {
    bool done;
    bool match;
  };
  std::unordered_map<std::string, Info> dic_;
};

/*-----------------------------------------------------------------------------
 *  PEG
 *---------------------------------------------------------------------------*/

/*
 * Line information utility function
 */
inline std::pair<size_t, size_t> line_info(const char *start, const char *cur) {
  auto p = start;
  auto col_ptr = p;
  auto no = 1;

  while (p < cur) {
    if (*p == '\n') {
      no++;
      col_ptr = p + 1;
    }
    p++;
  }

  auto col = p - col_ptr + 1;

  return std::make_pair(no, col);
}

/*
 * String tag
 */
inline constexpr unsigned int str2tag(const char *str, unsigned int h = 0) {
  return (*str == '\0')
             ? h
             : str2tag(str + 1, (h * 33) ^ static_cast<unsigned char>(*str));
}

namespace udl {

inline constexpr unsigned int operator"" _(const char *s, size_t) {
  return str2tag(s);
}

} // namespace udl

/*
 * Semantic values
 */
struct SemanticValues : protected std::vector<any> {
  // Input text
  const char *path = nullptr;
  const char *ss = nullptr;
  const std::vector<size_t> *source_line_index = nullptr;

  // Matched string
  const char *c_str() const { return s_; }
  size_t length() const { return n_; }

  std::string str() const { return std::string(s_, n_); }

  // Definition name
  const std::string &name() const { return name_; }

  std::vector<unsigned int> tags;

  // Line number and column at which the matched string is
  std::pair<size_t, size_t> line_info() const {
    const auto &idx = *source_line_index;

    auto cur = static_cast<size_t>(std::distance(ss, s_));
    auto it = std::lower_bound(
        idx.begin(), idx.end(), cur,
        [](size_t element, size_t value) { return element < value; });

    auto id = static_cast<size_t>(std::distance(idx.begin(), it));
    auto off = cur - (id == 0 ? 0 : idx[id - 1] + 1);
    return std::make_pair(id + 1, off + 1);
  }

  // Choice count
  size_t choice_count() const { return choice_count_; }

  // Choice number (0 based index)
  size_t choice() const { return choice_; }

  // Tokens
  std::vector<std::pair<const char *, size_t>> tokens;

  std::string token(size_t id = 0) const {
    if (!tokens.empty()) {
      assert(id < tokens.size());
      const auto &tok = tokens[id];
      return std::string(tok.first, tok.second);
    }
    return std::string(s_, n_);
  }

  // Transform the semantic value vector to another vector
  template <typename T>
  auto transform(size_t beg = 0, size_t end = static_cast<size_t>(-1)) const
      -> vector<T> {
    return this->transform(beg, end,
                           [](const any &v) { return any_cast<T>(v); });
  }

  using std::vector<any>::iterator;
  using std::vector<any>::const_iterator;
  using std::vector<any>::size;
  using std::vector<any>::empty;
  using std::vector<any>::assign;
  using std::vector<any>::begin;
  using std::vector<any>::end;
  using std::vector<any>::rbegin;
  using std::vector<any>::rend;
  using std::vector<any>::operator[];
  using std::vector<any>::at;
  using std::vector<any>::resize;
  using std::vector<any>::front;
  using std::vector<any>::back;
  using std::vector<any>::push_back;
  using std::vector<any>::pop_back;
  using std::vector<any>::insert;
  using std::vector<any>::erase;
  using std::vector<any>::clear;
  using std::vector<any>::swap;
  using std::vector<any>::emplace;
  using std::vector<any>::emplace_back;

private:
  friend class Context;
  friend class Sequence;
  friend class PrioritizedChoice;
  friend class Holder;
  friend class PrecedenceClimbing;

  const char *s_ = nullptr;
  size_t n_ = 0;
  size_t choice_count_ = 0;
  size_t choice_ = 0;
  std::string name_;

  template <typename F>
  auto transform(F f) const
      -> vector<typename std::remove_const<decltype(f(any()))>::type> {
    vector<typename std::remove_const<decltype(f(any()))>::type> r;
    for (const auto &v : *this) {
      r.emplace_back(f(v));
    }
    return r;
  }

  template <typename F>
  auto transform(size_t beg, size_t end, F f) const
      -> vector<typename std::remove_const<decltype(f(any()))>::type> {
    vector<typename std::remove_const<decltype(f(any()))>::type> r;
    end = (std::min)(end, size());
    for (size_t i = beg; i < end; i++) {
      r.emplace_back(f((*this)[i]));
    }
    return r;
  }
};

/*
 * Semantic action
 */
template <typename R, typename F,
          typename std::enable_if<std::is_void<R>::value,
                                  std::nullptr_t>::type = nullptr,
          typename... Args>
any call(F fn, Args &&... args) {
  fn(std::forward<Args>(args)...);
  return any();
}

template <typename R, typename F,
          typename std::enable_if<
              std::is_same<typename std::remove_cv<R>::type, any>::value,
              std::nullptr_t>::type = nullptr,
          typename... Args>
any call(F fn, Args &&... args) {
  return fn(std::forward<Args>(args)...);
}

template <typename R, typename F,
          typename std::enable_if<
              !std::is_void<R>::value &&
                  !std::is_same<typename std::remove_cv<R>::type, any>::value,
              std::nullptr_t>::type = nullptr,
          typename... Args>
any call(F fn, Args &&... args) {
  return any(fn(std::forward<Args>(args)...));
}

class Action {
public:
  Action() = default;
  Action(const Action &rhs) = default;

  template <typename F,
            typename std::enable_if<!std::is_pointer<F>::value &&
                                        !std::is_same<F, std::nullptr_t>::value,
                                    std::nullptr_t>::type = nullptr>
  Action(F fn) : fn_(make_adaptor(fn, &F::operator())) {}

  template <typename F, typename std::enable_if<std::is_pointer<F>::value,
                                                std::nullptr_t>::type = nullptr>
  Action(F fn) : fn_(make_adaptor(fn, fn)) {}

  template <typename F,
            typename std::enable_if<std::is_same<F, std::nullptr_t>::value,
                                    std::nullptr_t>::type = nullptr>
  Action(F /*fn*/) {}

  template <typename F,
            typename std::enable_if<!std::is_pointer<F>::value &&
                                        !std::is_same<F, std::nullptr_t>::value,
                                    std::nullptr_t>::type = nullptr>
  void operator=(F fn) {
    fn_ = make_adaptor(fn, &F::operator());
  }

  template <typename F, typename std::enable_if<std::is_pointer<F>::value,
                                                std::nullptr_t>::type = nullptr>
  void operator=(F fn) {
    fn_ = make_adaptor(fn, fn);
  }

  template <typename F,
            typename std::enable_if<std::is_same<F, std::nullptr_t>::value,
                                    std::nullptr_t>::type = nullptr>
  void operator=(F /*fn*/) {}

  Action &operator=(const Action &rhs) = default;

  operator bool() const { return bool(fn_); }

  any operator()(SemanticValues &sv, any &dt) const { return fn_(sv, dt); }

private:
  template <typename R> struct TypeAdaptor_sv {
    TypeAdaptor_sv(std::function<R(SemanticValues &sv)> fn) : fn_(fn) {}
    any operator()(SemanticValues &sv, any & /*dt*/) {
      return call<R>(fn_, sv);
    }
    std::function<R(SemanticValues &sv)> fn_;
  };

  template <typename R> struct TypeAdaptor_csv {
    TypeAdaptor_csv(std::function<R(const SemanticValues &sv)> fn) : fn_(fn) {}
    any operator()(SemanticValues &sv, any & /*dt*/) {
      return call<R>(fn_, sv);
    }
    std::function<R(const SemanticValues &sv)> fn_;
  };

  template <typename R> struct TypeAdaptor_sv_dt {
    TypeAdaptor_sv_dt(std::function<R(SemanticValues &sv, any &dt)> fn)
        : fn_(fn) {}
    any operator()(SemanticValues &sv, any &dt) { return call<R>(fn_, sv, dt); }
    std::function<R(SemanticValues &sv, any &dt)> fn_;
  };

  template <typename R> struct TypeAdaptor_csv_dt {
    TypeAdaptor_csv_dt(std::function<R(const SemanticValues &sv, any &dt)> fn)
        : fn_(fn) {}
    any operator()(SemanticValues &sv, any &dt) { return call<R>(fn_, sv, dt); }
    std::function<R(const SemanticValues &sv, any &dt)> fn_;
  };

  typedef std::function<any(SemanticValues &sv, any &dt)> Fty;

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (F::*)(SemanticValues &sv) const) {
    return TypeAdaptor_sv<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (F::*)(const SemanticValues &sv) const) {
    return TypeAdaptor_csv<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (F::*)(SemanticValues &sv)) {
    return TypeAdaptor_sv<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (F::*)(const SemanticValues &sv)) {
    return TypeAdaptor_csv<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (*)(SemanticValues &sv)) {
    return TypeAdaptor_sv<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (*)(const SemanticValues &sv)) {
    return TypeAdaptor_csv<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (F::*)(SemanticValues &sv, any &dt) const) {
    return TypeAdaptor_sv_dt<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (F::*)(const SemanticValues &sv, any &dt) const) {
    return TypeAdaptor_csv_dt<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (F::*)(SemanticValues &sv, any &dt)) {
    return TypeAdaptor_sv_dt<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (F::*)(const SemanticValues &sv, any &dt)) {
    return TypeAdaptor_csv_dt<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (*)(SemanticValues &sv, any &dt)) {
    return TypeAdaptor_sv_dt<R>(fn);
  }

  template <typename F, typename R>
  Fty make_adaptor(F fn, R (*)(const SemanticValues &sv, any &dt)) {
    return TypeAdaptor_csv_dt<R>(fn);
  }

  Fty fn_;
};

/*
 * Semantic predicate
 */
// Note: 'parse_error' exception class should be be used in sematic action
// handlers to reject the rule.
struct parse_error {
  parse_error() = default;
  parse_error(const char *s) : s_(s) {}
  const char *what() const { return s_.empty() ? nullptr : s_.c_str(); }

private:
  std::string s_;
};

/*
 * Result
 */
inline bool success(size_t len) { return len != static_cast<size_t>(-1); }

inline bool fail(size_t len) { return len == static_cast<size_t>(-1); }

/*
 * Context
 */
class Context;
class Ope;
class Definition;

typedef std::function<void(const char *name, const char *s, size_t n,
                           const SemanticValues &sv, const Context &c,
                           const any &dt)>
    TracerEnter;

typedef std::function<void(const char *name, const char *s, size_t n,
                           const SemanticValues &sv, const Context &c,
                           const any &dt, size_t)>
    TracerLeave;

class Context {
public:
  const char *path;
  const char *s;
  const size_t l;
  std::vector<size_t> source_line_index;

  const char *error_pos = nullptr;
  const char *message_pos = nullptr;
  std::string message; // TODO: should be `int`.

  std::vector<std::shared_ptr<SemanticValues>> value_stack;
  size_t value_stack_size = 0;
  std::vector<std::vector<std::shared_ptr<Ope>>> args_stack;

  bool in_token = false;

  std::shared_ptr<Ope> whitespaceOpe;
  bool in_whitespace = false;

  std::shared_ptr<Ope> wordOpe;

  std::vector<std::map<std::string, std::string>> capture_scope_stack;
  size_t capture_scope_stack_size = 0;

  const size_t def_count;
  const bool enablePackratParsing;
  std::vector<bool> cache_registered;
  std::vector<bool> cache_success;

  std::map<std::pair<size_t, size_t>, std::tuple<size_t, any>> cache_values;

  TracerEnter tracer_enter;
  TracerLeave tracer_leave;

  Context(const char *a_path, const char *a_s, size_t a_l, size_t a_def_count,
          std::shared_ptr<Ope> a_whitespaceOpe, std::shared_ptr<Ope> a_wordOpe,
          bool a_enablePackratParsing, TracerEnter a_tracer_enter,
          TracerLeave a_tracer_leave)
      : path(a_path), s(a_s), l(a_l), whitespaceOpe(a_whitespaceOpe),
        wordOpe(a_wordOpe), def_count(a_def_count),
        enablePackratParsing(a_enablePackratParsing),
        cache_registered(enablePackratParsing ? def_count * (l + 1) : 0),
        cache_success(enablePackratParsing ? def_count * (l + 1) : 0),
        tracer_enter(a_tracer_enter), tracer_leave(a_tracer_leave) {

    for (size_t pos = 0; pos < l; pos++) {
      if (s[pos] == '\n') { source_line_index.push_back(pos); }
    }
    source_line_index.push_back(l);

    args_stack.resize(1);

    push_capture_scope();
  }

  ~Context() { assert(!value_stack_size); }

  Context(const Context &) = delete;
  Context(Context &&) = delete;
  Context operator=(const Context &) = delete;

  template <typename T>
  void packrat(const char *a_s, size_t def_id, size_t &len, any &val, T fn) {
    if (!enablePackratParsing) {
      fn(val);
      return;
    }

    auto col = a_s - s;
    auto idx = def_count * static_cast<size_t>(col) + def_id;

    if (cache_registered[idx]) {
      if (cache_success[idx]) {
        auto key = std::make_pair(col, def_id);
        std::tie(len, val) = cache_values[key];
        return;
      } else {
        len = static_cast<size_t>(-1);
        return;
      }
    } else {
      fn(val);
      cache_registered[idx] = true;
      cache_success[idx] = success(len);
      if (success(len)) {
        auto key = std::make_pair(col, def_id);
        cache_values[key] = std::make_pair(len, val);
      }
      return;
    }
  }

  SemanticValues &push() {
    assert(value_stack_size <= value_stack.size());
    if (value_stack_size == value_stack.size()) {
      value_stack.emplace_back(std::make_shared<SemanticValues>());
    } else {
      auto &sv = *value_stack[value_stack_size];
      if (!sv.empty()) {
        sv.clear();
        sv.tags.clear();
      }
      sv.s_ = nullptr;
      sv.n_ = 0;
      sv.choice_count_ = 0;
      sv.choice_ = 0;
      sv.tokens.clear();
    }

    auto &sv = *value_stack[value_stack_size++];
    sv.path = path;
    sv.ss = s;
    sv.source_line_index = &source_line_index;
    return sv;
  }

  void pop() { value_stack_size--; }

  void push_args(std::vector<std::shared_ptr<Ope>> &&args) {
    args_stack.emplace_back(args);
  }

  void pop_args() { args_stack.pop_back(); }

  const std::vector<std::shared_ptr<Ope>> &top_args() const {
    return args_stack[args_stack.size() - 1];
  }

  void push_capture_scope() {
    assert(capture_scope_stack_size <= capture_scope_stack.size());
    if (capture_scope_stack_size == capture_scope_stack.size()) {
      capture_scope_stack.emplace_back(std::map<std::string, std::string>());
    } else {
      auto &cs = capture_scope_stack[capture_scope_stack_size];
      cs.clear();
    }
    capture_scope_stack_size++;
  }

  void pop_capture_scope() { capture_scope_stack_size--; }

  void shift_capture_values() {
    assert(capture_scope_stack.size() >= 2);
    auto curr = &capture_scope_stack[capture_scope_stack_size - 1];
    auto prev = curr - 1;
    for (const auto &kv : *curr) {
      (*prev)[kv.first] = kv.second;
    }
  }

  void set_error_pos(const char *a_s) {
    if (error_pos < a_s) error_pos = a_s;
  }

  void trace_enter(const char *name, const char *a_s, size_t n,
                   SemanticValues &sv, any &dt) const;
  void trace_leave(const char *name, const char *a_s, size_t n,
                   SemanticValues &sv, any &dt, size_t len) const;
  bool is_traceable(const Ope &ope) const;

  mutable size_t next_trace_id = 0;
  mutable std::list<size_t> trace_ids;
};

/*
 * Parser operators
 */
class Ope {
public:
  struct Visitor;

  virtual ~Ope() {}
  size_t parse(const char *s, size_t n, SemanticValues &sv, Context &c,
               any &dt) const;
  virtual size_t parse_core(const char *s, size_t n, SemanticValues &sv,
                            Context &c, any &dt) const = 0;
  virtual void accept(Visitor &v) = 0;
};

class Sequence : public Ope {
public:
  template <typename... Args>
  Sequence(const Args &... args)
      : opes_{static_cast<std::shared_ptr<Ope>>(args)...} {}
  Sequence(const std::vector<std::shared_ptr<Ope>> &opes) : opes_(opes) {}
  Sequence(std::vector<std::shared_ptr<Ope>> &&opes) : opes_(opes) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override {
    auto &chldsv = c.push();
    auto pop_se = make_scope_exit([&]() { c.pop(); });
    size_t i = 0;
    for (const auto &ope : opes_) {
      const auto &rule = *ope;
      auto len = rule.parse(s + i, n - i, chldsv, c, dt);
      if (fail(len)) { return static_cast<size_t>(-1); }
      i += len;
    }
    if (!chldsv.empty()) {
      for (size_t j = 0; j < chldsv.size(); j++) {
        sv.emplace_back(std::move(chldsv[j]));
      }
    }
    if (!chldsv.tags.empty()) {
      for (size_t j = 0; j < chldsv.tags.size(); j++) {
        sv.tags.emplace_back(std::move(chldsv.tags[j]));
      }
    }
    sv.s_ = chldsv.c_str();
    sv.n_ = chldsv.length();
    if (!chldsv.tokens.empty()) {
      for (size_t j = 0; j < chldsv.tokens.size(); j++) {
        sv.tokens.emplace_back(std::move(chldsv.tokens[j]));
      }
    }
    return i;
  }

  void accept(Visitor &v) override;

  std::vector<std::shared_ptr<Ope>> opes_;
};

class PrioritizedChoice : public Ope {
public:
  template <typename... Args>
  PrioritizedChoice(const Args &... args)
      : opes_{static_cast<std::shared_ptr<Ope>>(args)...} {}
  PrioritizedChoice(const std::vector<std::shared_ptr<Ope>> &opes)
      : opes_(opes) {}
  PrioritizedChoice(std::vector<std::shared_ptr<Ope>> &&opes) : opes_(opes) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override {
    size_t id = 0;
    for (const auto &ope : opes_) {
      auto &chldsv = c.push();
      c.push_capture_scope();
      auto se = make_scope_exit([&]() {
        c.pop();
        c.pop_capture_scope();
      });
      const auto &rule = *ope;
      auto len = rule.parse(s, n, chldsv, c, dt);
      if (success(len)) {
        if (!chldsv.empty()) {
          for (size_t i = 0; i < chldsv.size(); i++) {
            sv.emplace_back(std::move(chldsv[i]));
          }
        }
        if (!chldsv.tags.empty()) {
          for (size_t i = 0; i < chldsv.tags.size(); i++) {
            sv.tags.emplace_back(std::move(chldsv.tags[i]));
          }
        }
        sv.s_ = chldsv.c_str();
        sv.n_ = chldsv.length();
        sv.choice_count_ = opes_.size();
        sv.choice_ = id;
        if (!chldsv.tokens.empty()) {
          for (size_t i = 0; i < chldsv.tokens.size(); i++) {
            sv.tokens.emplace_back(std::move(chldsv.tokens[i]));
          }
        }

        c.shift_capture_values();
        return len;
      }
      id++;
    }
    return static_cast<size_t>(-1);
  }

  void accept(Visitor &v) override;

  size_t size() const { return opes_.size(); }

  std::vector<std::shared_ptr<Ope>> opes_;
};

class Repetition : public Ope {
public:
  Repetition(const std::shared_ptr<Ope> &ope, size_t min, size_t max)
      : ope_(ope), min_(min), max_(max) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override {
    size_t count = 0;
    size_t i = 0;
    while (count < min_) {
      c.push_capture_scope();
      auto se = make_scope_exit([&]() { c.pop_capture_scope(); });
      const auto &rule = *ope_;
      auto len = rule.parse(s + i, n - i, sv, c, dt);
      if (success(len)) {
        c.shift_capture_values();
      } else {
        return static_cast<size_t>(-1);
      }
      i += len;
      count++;
    }

    auto save_error_pos = c.error_pos;
    while (n - i > 0 && count < max_) {
      c.push_capture_scope();
      auto se = make_scope_exit([&]() { c.pop_capture_scope(); });
      auto save_sv_size = sv.size();
      auto save_tok_size = sv.tokens.size();
      const auto &rule = *ope_;
      auto len = rule.parse(s + i, n - i, sv, c, dt);
      if (success(len)) {
        c.shift_capture_values();
      } else {
        if (sv.size() != save_sv_size) {
          sv.erase(sv.begin() + static_cast<std::ptrdiff_t>(save_sv_size));
          sv.tags.erase(sv.tags.begin() +
                        static_cast<std::ptrdiff_t>(save_sv_size));
        }
        if (sv.tokens.size() != save_tok_size) {
          sv.tokens.erase(sv.tokens.begin() +
                          static_cast<std::ptrdiff_t>(save_tok_size));
        }
        c.error_pos = save_error_pos;
        break;
      }
      i += len;
      count++;
    }
    return i;
  }

  void accept(Visitor &v) override;

  bool is_zom() const {
    return min_ == 0 && max_ == std::numeric_limits<size_t>::max();
  }

  static std::shared_ptr<Repetition> zom(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 0,
                                        std::numeric_limits<size_t>::max());
  }

  static std::shared_ptr<Repetition> oom(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 1,
                                        std::numeric_limits<size_t>::max());
  }

  static std::shared_ptr<Repetition> opt(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 0, 1);
  }

  std::shared_ptr<Ope> ope_;
  size_t min_;
  size_t max_;
};

class AndPredicate : public Ope {
public:
  AndPredicate(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues & /*sv*/,
                    Context &c, any &dt) const override {
    auto &chldsv = c.push();
    c.push_capture_scope();
    auto se = make_scope_exit([&]() {
      c.pop();
      c.pop_capture_scope();
    });
    const auto &rule = *ope_;
    auto len = rule.parse(s, n, chldsv, c, dt);
    if (success(len)) {
      return 0;
    } else {
      return static_cast<size_t>(-1);
    }
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class NotPredicate : public Ope {
public:
  NotPredicate(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues & /*sv*/,
                    Context &c, any &dt) const override {
    auto save_error_pos = c.error_pos;
    auto &chldsv = c.push();
    c.push_capture_scope();
    auto se = make_scope_exit([&]() {
      c.pop();
      c.pop_capture_scope();
    });
    const auto &rule = *ope_;
    auto len = rule.parse(s, n, chldsv, c, dt);
    if (success(len)) {
      c.set_error_pos(s);
      return static_cast<size_t>(-1);
    } else {
      c.error_pos = save_error_pos;
      return 0;
    }
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class Dictionary : public Ope, public std::enable_shared_from_this<Dictionary> {
public:
  Dictionary(const std::vector<std::string> &v) : trie_(v) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override;

  void accept(Visitor &v) override;

  Trie trie_;
};

class LiteralString : public Ope,
                      public std::enable_shared_from_this<LiteralString> {
public:
  LiteralString(const std::string &s, bool ignore_case)
      : lit_(s), ignore_case_(ignore_case), init_is_word_(false),
        is_word_(false) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override;

  void accept(Visitor &v) override;

  std::string lit_;
  bool ignore_case_;
  mutable bool init_is_word_;
  mutable bool is_word_;
};

class CharacterClass : public Ope,
                       public std::enable_shared_from_this<CharacterClass> {
public:
  CharacterClass(const std::string &s, bool negated) : negated_(negated) {
    auto chars = decode(s.c_str(), s.length());
    auto i = 0u;
    while (i < chars.size()) {
      if (i + 2 < chars.size() && chars[i + 1] == '-') {
        auto cp1 = chars[i];
        auto cp2 = chars[i + 2];
        ranges_.emplace_back(std::make_pair(cp1, cp2));
        i += 3;
      } else {
        auto cp = chars[i];
        ranges_.emplace_back(std::make_pair(cp, cp));
        i += 1;
      }
    }
    assert(!ranges_.empty());
  }

  CharacterClass(const std::vector<std::pair<char32_t, char32_t>> &ranges,
                 bool negated)
      : ranges_(ranges), negated_(negated) {
    assert(!ranges_.empty());
  }

  size_t parse_core(const char *s, size_t n, SemanticValues & /*sv*/,
                    Context &c, any & /*dt*/) const override {
    if (n < 1) {
      c.set_error_pos(s);
      return static_cast<size_t>(-1);
    }

    char32_t cp = 0;
    auto len = decode_codepoint(s, n, cp);

    for (const auto &range : ranges_) {
      if (range.first <= cp && cp <= range.second) {
        if (negated_) {
          c.set_error_pos(s);
          return static_cast<size_t>(-1);
        } else {
          return len;
        }
      }
    }

    if (negated_) {
      return len;
    } else {
      c.set_error_pos(s);
      return static_cast<size_t>(-1);
    }
  }

  void accept(Visitor &v) override;

  std::vector<std::pair<char32_t, char32_t>> ranges_;
  bool negated_;
};

class Character : public Ope, public std::enable_shared_from_this<Character> {
public:
  Character(char ch) : ch_(ch) {}

  size_t parse_core(const char *s, size_t n, SemanticValues & /*sv*/,
                    Context &c, any & /*dt*/) const override {
    if (n < 1 || s[0] != ch_) {
      c.set_error_pos(s);
      return static_cast<size_t>(-1);
    }
    return 1;
  }

  void accept(Visitor &v) override;

  char ch_;
};

class AnyCharacter : public Ope,
                     public std::enable_shared_from_this<AnyCharacter> {
public:
  size_t parse_core(const char *s, size_t n, SemanticValues & /*sv*/,
                    Context &c, any & /*dt*/) const override {
    auto len = codepoint_length(s, n);
    if (len < 1) {
      c.set_error_pos(s);
      return static_cast<size_t>(-1);
    }
    return len;
  }

  void accept(Visitor &v) override;
};

class CaptureScope : public Ope {
public:
  CaptureScope(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override {
    c.push_capture_scope();
    auto se = make_scope_exit([&]() { c.pop_capture_scope(); });
    const auto &rule = *ope_;
    auto len = rule.parse(s, n, sv, c, dt);
    return len;
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class Capture : public Ope {
public:
  typedef std::function<void(const char *s, size_t n, Context &c)> MatchAction;

  Capture(const std::shared_ptr<Ope> &ope, MatchAction ma)
      : ope_(ope), match_action_(ma) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override {
    const auto &rule = *ope_;
    auto len = rule.parse(s, n, sv, c, dt);
    if (success(len) && match_action_) { match_action_(s, len, c); }
    return len;
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
  MatchAction match_action_;
};

class TokenBoundary : public Ope {
public:
  TokenBoundary(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override;

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class Ignore : public Ope {
public:
  Ignore(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues & /*sv*/,
                    Context &c, any &dt) const override {
    const auto &rule = *ope_;
    auto &chldsv = c.push();
    auto se = make_scope_exit([&]() { c.pop(); });
    return rule.parse(s, n, chldsv, c, dt);
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

typedef std::function<size_t(const char *s, size_t n, SemanticValues &sv,
                             any &dt)>
    Parser;

class User : public Ope {
public:
  User(Parser fn) : fn_(fn) {}
  size_t parse_core(const char *s, size_t n, SemanticValues &sv,
                    Context & /*c*/, any &dt) const override {
    assert(fn_);
    return fn_(s, n, sv, dt);
  }
  void accept(Visitor &v) override;
  std::function<size_t(const char *s, size_t n, SemanticValues &sv, any &dt)>
      fn_;
};

class WeakHolder : public Ope {
public:
  WeakHolder(const std::shared_ptr<Ope> &ope) : weak_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override {
    auto ope = weak_.lock();
    assert(ope);
    const auto &rule = *ope;
    return rule.parse(s, n, sv, c, dt);
  }

  void accept(Visitor &v) override;

  std::weak_ptr<Ope> weak_;
};

class Holder : public Ope {
public:
  Holder(Definition *outer) : outer_(outer) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override;

  void accept(Visitor &v) override;

  any reduce(SemanticValues &sv, any &dt) const;

  const char *trace_name() const;

  std::shared_ptr<Ope> ope_;
  Definition *outer_;
  mutable std::string trace_name_;

  friend class Definition;
};

typedef std::unordered_map<std::string, Definition> Grammar;

class Reference : public Ope, public std::enable_shared_from_this<Reference> {
public:
  Reference(const Grammar &grammar, const std::string &name, const char *s,
            bool is_macro, const std::vector<std::shared_ptr<Ope>> &args)
      : grammar_(grammar), name_(name), s_(s), is_macro_(is_macro), args_(args),
        rule_(nullptr), iarg_(0) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override;

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> get_core_operator() const;

  const Grammar &grammar_;
  const std::string name_;
  const char *s_;

  const bool is_macro_;
  const std::vector<std::shared_ptr<Ope>> args_;

  Definition *rule_;
  size_t iarg_;
};

class Whitespace : public Ope {
public:
  Whitespace(const std::shared_ptr<Ope> &ope) : ope_(ope) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override {
    if (c.in_whitespace) { return 0; }
    c.in_whitespace = true;
    auto se = make_scope_exit([&]() { c.in_whitespace = false; });
    const auto &rule = *ope_;
    return rule.parse(s, n, sv, c, dt);
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> ope_;
};

class BackReference : public Ope {
public:
  BackReference(const std::string &name) : name_(name) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override;

  void accept(Visitor &v) override;

  std::string name_;
};

class PrecedenceClimbing : public Ope {
public:
  using BinOpeInfo = std::map<std::string, std::pair<size_t, char>>;

  PrecedenceClimbing(const std::shared_ptr<Ope> &atom,
                     const std::shared_ptr<Ope> &binop, const BinOpeInfo &info,
                     const Definition &rule)
      : atom_(atom), binop_(binop), info_(info), rule_(rule) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &sv, Context &c,
                    any &dt) const override {
    return parse_expression(s, n, sv, c, dt, 0);
  }

  void accept(Visitor &v) override;

  std::shared_ptr<Ope> atom_;
  std::shared_ptr<Ope> binop_;
  BinOpeInfo info_;
  const Definition &rule_;

private:
  size_t parse_expression(const char *s, size_t n, SemanticValues &sv,
                          Context &c, any &dt, size_t min_prec) const;

  Definition &get_reference_for_binop(Context &c) const;
};

/*
 * Factories
 */
template <typename... Args> std::shared_ptr<Ope> seq(Args &&... args) {
  return std::make_shared<Sequence>(static_cast<std::shared_ptr<Ope>>(args)...);
}

template <typename... Args> std::shared_ptr<Ope> cho(Args &&... args) {
  return std::make_shared<PrioritizedChoice>(
      static_cast<std::shared_ptr<Ope>>(args)...);
}

inline std::shared_ptr<Ope> zom(const std::shared_ptr<Ope> &ope) {
  return Repetition::zom(ope);
}

inline std::shared_ptr<Ope> oom(const std::shared_ptr<Ope> &ope) {
  return Repetition::oom(ope);
}

inline std::shared_ptr<Ope> opt(const std::shared_ptr<Ope> &ope) {
  return Repetition::opt(ope);
}

inline std::shared_ptr<Ope> rep(const std::shared_ptr<Ope> &ope, size_t min,
                                size_t max) {
  return std::make_shared<Repetition>(ope, min, max);
}

inline std::shared_ptr<Ope> apd(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<AndPredicate>(ope);
}

inline std::shared_ptr<Ope> npd(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<NotPredicate>(ope);
}

inline std::shared_ptr<Ope> dic(const std::vector<std::string> &v) {
  return std::make_shared<Dictionary>(v);
}

inline std::shared_ptr<Ope> lit(const std::string &s) {
  return std::make_shared<LiteralString>(s, false);
}

inline std::shared_ptr<Ope> liti(const std::string &s) {
  return std::make_shared<LiteralString>(s, true);
}

inline std::shared_ptr<Ope> cls(const std::string &s) {
  return std::make_shared<CharacterClass>(s, false);
}

inline std::shared_ptr<Ope>
cls(const std::vector<std::pair<char32_t, char32_t>> &ranges) {
  return std::make_shared<CharacterClass>(ranges, false);
}

inline std::shared_ptr<Ope> ncls(const std::string &s) {
  return std::make_shared<CharacterClass>(s, true);
}

inline std::shared_ptr<Ope>
ncls(const std::vector<std::pair<char32_t, char32_t>> &ranges) {
  return std::make_shared<CharacterClass>(ranges, true);
}

inline std::shared_ptr<Ope> chr(char dt) {
  return std::make_shared<Character>(dt);
}

inline std::shared_ptr<Ope> dot() { return std::make_shared<AnyCharacter>(); }

inline std::shared_ptr<Ope> csc(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<CaptureScope>(ope);
}

inline std::shared_ptr<Ope> cap(const std::shared_ptr<Ope> &ope,
                                Capture::MatchAction ma) {
  return std::make_shared<Capture>(ope, ma);
}

inline std::shared_ptr<Ope> tok(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<TokenBoundary>(ope);
}

inline std::shared_ptr<Ope> ign(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<Ignore>(ope);
}

inline std::shared_ptr<Ope>
usr(std::function<size_t(const char *s, size_t n, SemanticValues &sv, any &dt)>
        fn) {
  return std::make_shared<User>(fn);
}

inline std::shared_ptr<Ope> ref(const Grammar &grammar, const std::string &name,
                                const char *s, bool is_macro,
                                const std::vector<std::shared_ptr<Ope>> &args) {
  return std::make_shared<Reference>(grammar, name, s, is_macro, args);
}

inline std::shared_ptr<Ope> wsp(const std::shared_ptr<Ope> &ope) {
  return std::make_shared<Whitespace>(std::make_shared<Ignore>(ope));
}

inline std::shared_ptr<Ope> bkr(const std::string &name) {
  return std::make_shared<BackReference>(name);
}

inline std::shared_ptr<Ope> pre(const std::shared_ptr<Ope> &atom,
                                const std::shared_ptr<Ope> &binop,
                                const PrecedenceClimbing::BinOpeInfo &info,
                                const Definition &rule) {
  return std::make_shared<PrecedenceClimbing>(atom, binop, info, rule);
}

/*
 * Visitor
 */
struct Ope::Visitor {
  virtual ~Visitor() {}
  virtual void visit(Sequence & /*ope*/) {}
  virtual void visit(PrioritizedChoice & /*ope*/) {}
  virtual void visit(Repetition & /*ope*/) {}
  virtual void visit(AndPredicate & /*ope*/) {}
  virtual void visit(NotPredicate & /*ope*/) {}
  virtual void visit(Dictionary & /*ope*/) {}
  virtual void visit(LiteralString & /*ope*/) {}
  virtual void visit(CharacterClass & /*ope*/) {}
  virtual void visit(Character & /*ope*/) {}
  virtual void visit(AnyCharacter & /*ope*/) {}
  virtual void visit(CaptureScope & /*ope*/) {}
  virtual void visit(Capture & /*ope*/) {}
  virtual void visit(TokenBoundary & /*ope*/) {}
  virtual void visit(Ignore & /*ope*/) {}
  virtual void visit(User & /*ope*/) {}
  virtual void visit(WeakHolder & /*ope*/) {}
  virtual void visit(Holder & /*ope*/) {}
  virtual void visit(Reference & /*ope*/) {}
  virtual void visit(Whitespace & /*ope*/) {}
  virtual void visit(BackReference & /*ope*/) {}
  virtual void visit(PrecedenceClimbing & /*ope*/) {}
};

struct IsReference : public Ope::Visitor {
  void visit(Reference & /*ope*/) override { is_reference = true; }
  bool is_reference = false;
};

struct TraceOpeName : public Ope::Visitor {
  void visit(Sequence & /*ope*/) override { name = "Sequence"; }
  void visit(PrioritizedChoice & /*ope*/) override {
    name = "PrioritizedChoice";
  }
  void visit(Repetition & /*ope*/) override { name = "Repetition"; }
  void visit(AndPredicate & /*ope*/) override { name = "AndPredicate"; }
  void visit(NotPredicate & /*ope*/) override { name = "NotPredicate"; }
  void visit(Dictionary & /*ope*/) override { name = "Dictionary"; }
  void visit(LiteralString & /*ope*/) override { name = "LiteralString"; }
  void visit(CharacterClass & /*ope*/) override { name = "CharacterClass"; }
  void visit(Character & /*ope*/) override { name = "Character"; }
  void visit(AnyCharacter & /*ope*/) override { name = "AnyCharacter"; }
  void visit(CaptureScope & /*ope*/) override { name = "CaptureScope"; }
  void visit(Capture & /*ope*/) override { name = "Capture"; }
  void visit(TokenBoundary & /*ope*/) override { name = "TokenBoundary"; }
  void visit(Ignore & /*ope*/) override { name = "Ignore"; }
  void visit(User & /*ope*/) override { name = "User"; }
  void visit(WeakHolder & /*ope*/) override { name = "WeakHolder"; }
  void visit(Holder &ope) override { name = ope.trace_name(); }
  void visit(Reference & /*ope*/) override { name = "Reference"; }
  void visit(Whitespace & /*ope*/) override { name = "Whitespace"; }
  void visit(BackReference & /*ope*/) override { name = "BackReference"; }
  void visit(PrecedenceClimbing & /*ope*/) override {
    name = "PrecedenceClimbing";
  }

  const char *name = nullptr;
};

struct AssignIDToDefinition : public Ope::Visitor {
  void visit(Sequence &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(Repetition &ope) override { ope.ope_->accept(*this); }
  void visit(AndPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(NotPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Holder &ope) override;
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }

  std::unordered_map<void *, size_t> ids;
};

struct IsLiteralToken : public Ope::Visitor {
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      if (!IsLiteralToken::check(*op)) { return; }
    }
    result_ = true;
  }

  void visit(Dictionary & /*ope*/) override { result_ = true; }
  void visit(LiteralString & /*ope*/) override { result_ = true; }

  static bool check(Ope &ope) {
    IsLiteralToken vis;
    ope.accept(vis);
    return vis.result_;
  }

private:
  bool result_ = false;
};

struct TokenChecker : public Ope::Visitor {
  void visit(Sequence &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(Repetition &ope) override { ope.ope_->accept(*this); }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary & /*ope*/) override { has_token_boundary_ = true; }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
  void visit(PrecedenceClimbing &ope) override { ope.atom_->accept(*this); }

  static bool is_token(Ope &ope) {
    if (IsLiteralToken::check(ope)) { return true; }

    TokenChecker vis;
    ope.accept(vis);
    return vis.has_token_boundary_ || !vis.has_rule_;
  }

private:
  bool has_token_boundary_ = false;
  bool has_rule_ = false;
};

struct DetectLeftRecursion : public Ope::Visitor {
  DetectLeftRecursion(const std::string &name) : name_(name) {}

  void visit(Sequence &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
      if (done_) {
        break;
      } else if (error_s) {
        done_ = true;
        break;
      }
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
      if (error_s) {
        done_ = true;
        break;
      }
    }
  }
  void visit(Repetition &ope) override {
    ope.ope_->accept(*this);
    done_ = ope.min_ > 0;
  }
  void visit(AndPredicate &ope) override {
    ope.ope_->accept(*this);
    done_ = false;
  }
  void visit(NotPredicate &ope) override {
    ope.ope_->accept(*this);
    done_ = false;
  }
  void visit(Dictionary & /*ope*/) override { done_ = true; }
  void visit(LiteralString &ope) override { done_ = !ope.lit_.empty(); }
  void visit(CharacterClass & /*ope*/) override { done_ = true; }
  void visit(Character & /*ope*/) override { done_ = true; }
  void visit(AnyCharacter & /*ope*/) override { done_ = true; }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  void visit(User & /*ope*/) override { done_ = true; }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Holder &ope) override { ope.ope_->accept(*this); }
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
  void visit(BackReference & /*ope*/) override { done_ = true; }
  void visit(PrecedenceClimbing &ope) override { ope.atom_->accept(*this); }

  const char *error_s = nullptr;

private:
  std::string name_;
  std::set<std::string> refs_;
  bool done_ = false;
};

struct HasEmptyElement : public Ope::Visitor {
  HasEmptyElement(std::list<std::pair<const char *, std::string>> &refs)
      : refs_(refs) {}

  void visit(Sequence &ope) override {
    bool save_is_empty = false;
    const char *save_error_s = nullptr;
    std::string save_error_name;
    for (auto op : ope.opes_) {
      op->accept(*this);
      if (!is_empty) { return; }
      save_is_empty = is_empty;
      save_error_s = error_s;
      save_error_name = error_name;
      is_empty = false;
      error_name.clear();
    }
    is_empty = save_is_empty;
    error_s = save_error_s;
    error_name = save_error_name;
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
      if (is_empty) { return; }
    }
  }
  void visit(Repetition &ope) override {
    if (ope.min_ == 0) {
      set_error();
    } else {
      ope.ope_->accept(*this);
    }
  }
  void visit(AndPredicate & /*ope*/) override { set_error(); }
  void visit(NotPredicate & /*ope*/) override { set_error(); }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Holder &ope) override { ope.ope_->accept(*this); }
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
  void visit(PrecedenceClimbing &ope) override { ope.atom_->accept(*this); }

  bool is_empty = false;
  const char *error_s = nullptr;
  std::string error_name;

private:
  void set_error() {
    is_empty = true;
    error_s = refs_.back().first;
    error_name = refs_.back().second;
  }
  std::list<std::pair<const char *, std::string>> &refs_;
};

struct DetectInfiniteLoop : public Ope::Visitor {
  DetectInfiniteLoop(const char *s, const std::string &name) {
    refs_.emplace_back(s, name);
  }

  void visit(Sequence &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
      if (has_error) { return; }
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
      if (has_error) { return; }
    }
  }
  void visit(Repetition &ope) override {
    if (ope.max_ == std::numeric_limits<size_t>::max()) {
      HasEmptyElement vis(refs_);
      ope.ope_->accept(vis);
      if (vis.is_empty) {
        has_error = true;
        error_s = vis.error_s;
        error_name = vis.error_name;
      }
    } else {
      ope.ope_->accept(*this);
    }
  }
  void visit(AndPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(NotPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Holder &ope) override { ope.ope_->accept(*this); }
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
  void visit(PrecedenceClimbing &ope) override { ope.atom_->accept(*this); }

  bool has_error = false;
  const char *error_s = nullptr;
  std::string error_name;

private:
  std::list<std::pair<const char *, std::string>> refs_;
};

struct ReferenceChecker : public Ope::Visitor {
  ReferenceChecker(const Grammar &grammar,
                   const std::vector<std::string> &params)
      : grammar_(grammar), params_(params) {}

  void visit(Sequence &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(Repetition &ope) override { ope.ope_->accept(*this); }
  void visit(AndPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(NotPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Holder &ope) override { ope.ope_->accept(*this); }
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
  void visit(PrecedenceClimbing &ope) override { ope.atom_->accept(*this); }

  std::unordered_map<std::string, const char *> error_s;
  std::unordered_map<std::string, std::string> error_message;

private:
  const Grammar &grammar_;
  const std::vector<std::string> &params_;
};

struct LinkReferences : public Ope::Visitor {
  LinkReferences(Grammar &grammar, const std::vector<std::string> &params)
      : grammar_(grammar), params_(params) {}

  void visit(Sequence &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(Repetition &ope) override { ope.ope_->accept(*this); }
  void visit(AndPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(NotPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Holder &ope) override { ope.ope_->accept(*this); }
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
  void visit(PrecedenceClimbing &ope) override { ope.atom_->accept(*this); }

private:
  Grammar &grammar_;
  const std::vector<std::string> &params_;
};

struct FindReference : public Ope::Visitor {
  FindReference(const std::vector<std::shared_ptr<Ope>> &args,
                const std::vector<std::string> &params)
      : args_(args), params_(params) {}

  void visit(Sequence &ope) override {
    std::vector<std::shared_ptr<Ope>> opes;
    for (auto o : ope.opes_) {
      o->accept(*this);
      opes.push_back(found_ope);
    }
    found_ope = std::make_shared<Sequence>(opes);
  }
  void visit(PrioritizedChoice &ope) override {
    std::vector<std::shared_ptr<Ope>> opes;
    for (auto o : ope.opes_) {
      o->accept(*this);
      opes.push_back(found_ope);
    }
    found_ope = std::make_shared<PrioritizedChoice>(opes);
  }
  void visit(Repetition &ope) override {
    ope.ope_->accept(*this);
    found_ope = rep(found_ope, ope.min_, ope.max_);
  }
  void visit(AndPredicate &ope) override {
    ope.ope_->accept(*this);
    found_ope = apd(found_ope);
  }
  void visit(NotPredicate &ope) override {
    ope.ope_->accept(*this);
    found_ope = npd(found_ope);
  }
  void visit(Dictionary &ope) override { found_ope = ope.shared_from_this(); }
  void visit(LiteralString &ope) override {
    found_ope = ope.shared_from_this();
  }
  void visit(CharacterClass &ope) override {
    found_ope = ope.shared_from_this();
  }
  void visit(Character &ope) override { found_ope = ope.shared_from_this(); }
  void visit(AnyCharacter &ope) override { found_ope = ope.shared_from_this(); }
  void visit(CaptureScope &ope) override {
    ope.ope_->accept(*this);
    found_ope = csc(found_ope);
  }
  void visit(Capture &ope) override {
    ope.ope_->accept(*this);
    found_ope = cap(found_ope, ope.match_action_);
  }
  void visit(TokenBoundary &ope) override {
    ope.ope_->accept(*this);
    found_ope = tok(found_ope);
  }
  void visit(Ignore &ope) override {
    ope.ope_->accept(*this);
    found_ope = ign(found_ope);
  }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Holder &ope) override { ope.ope_->accept(*this); }
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override {
    ope.ope_->accept(*this);
    found_ope = wsp(found_ope);
  }
  void visit(PrecedenceClimbing &ope) override {
    ope.atom_->accept(*this);
    found_ope = csc(found_ope);
  }

  std::shared_ptr<Ope> found_ope;

private:
  const std::vector<std::shared_ptr<Ope>> &args_;
  const std::vector<std::string> &params_;
};

struct IsPrioritizedChoice : public Ope::Visitor {
  void visit(PrioritizedChoice & /*ope*/) override { result_ = true; }

  static bool check(Ope &ope) {
    IsPrioritizedChoice vis;
    ope.accept(vis);
    return vis.result_;
  }

private:
  bool result_ = false;
};

/*
 * Keywords
 */
static const char *WHITESPACE_DEFINITION_NAME = "%whitespace";
static const char *WORD_DEFINITION_NAME = "%word";

/*
 * Definition
 */
class Definition {
public:
  struct Result {
    bool ret;
    size_t len;
    const char *error_pos;
    const char *message_pos;
    const std::string message;
  };

  Definition() : holder_(std::make_shared<Holder>(this)) {}

  Definition(const Definition &rhs) : name(rhs.name), holder_(rhs.holder_) {
    holder_->outer_ = this;
  }

  Definition(const std::shared_ptr<Ope> &ope)
      : holder_(std::make_shared<Holder>(this)) {
    *this <= ope;
  }

  operator std::shared_ptr<Ope>() {
    return std::make_shared<WeakHolder>(holder_);
  }

  Definition &operator<=(const std::shared_ptr<Ope> &ope) {
    holder_->ope_ = ope;
    return *this;
  }

  Result parse(const char *s, size_t n, const char *path = nullptr) const {
    SemanticValues sv;
    any dt;
    return parse_core(s, n, sv, dt, path);
  }

  Result parse(const char *s, const char *path = nullptr) const {
    auto n = strlen(s);
    return parse(s, n, path);
  }

  Result parse(const char *s, size_t n, any &dt,
               const char *path = nullptr) const {
    SemanticValues sv;
    return parse_core(s, n, sv, dt, path);
  }

  Result parse(const char *s, any &dt, const char *path = nullptr) const {
    auto n = strlen(s);
    return parse(s, n, dt, path);
  }

  template <typename T>
  Result parse_and_get_value(const char *s, size_t n, T &val,
                             const char *path = nullptr) const {
    SemanticValues sv;
    any dt;
    auto r = parse_core(s, n, sv, dt, path);
    if (r.ret && !sv.empty() && sv.front().has_value()) {
      val = any_cast<T>(sv[0]);
    }
    return r;
  }

  template <typename T>
  Result parse_and_get_value(const char *s, T &val,
                             const char *path = nullptr) const {
    auto n = strlen(s);
    return parse_and_get_value(s, n, val, path);
  }

  template <typename T>
  Result parse_and_get_value(const char *s, size_t n, any &dt, T &val,
                             const char *path = nullptr) const {
    SemanticValues sv;
    auto r = parse_core(s, n, sv, dt, path);
    if (r.ret && !sv.empty() && sv.front().has_value()) {
      val = any_cast<T>(sv[0]);
    }
    return r;
  }

  template <typename T>
  Result parse_and_get_value(const char *s, any &dt, T &val,
                             const char *path = nullptr) const {
    auto n = strlen(s);
    return parse_and_get_value(s, n, dt, val, path);
  }

  Action operator=(Action a) {
    action = a;
    return a;
  }

  template <typename T> Definition &operator,(T fn) {
    operator=(fn);
    return *this;
  }

  Definition &operator~() {
    ignoreSemanticValue = true;
    return *this;
  }

  void accept(Ope::Visitor &v) { holder_->accept(v); }

  std::shared_ptr<Ope> get_core_operator() const { return holder_->ope_; }

  bool is_token() const {
    std::call_once(is_token_init_, [this]() {
      is_token_ = TokenChecker::is_token(*get_core_operator());
    });
    return is_token_;
  }

  std::string name;
  const char *s_ = nullptr;

  size_t id = 0;
  Action action;
  std::function<void(const char *s, size_t n, any &dt)> enter;
  std::function<void(const char *s, size_t n, size_t matchlen, any &value,
                     any &dt)>
      leave;
  std::function<std::string()> error_message;
  bool ignoreSemanticValue = false;
  std::shared_ptr<Ope> whitespaceOpe;
  std::shared_ptr<Ope> wordOpe;
  bool enablePackratParsing = false;
  bool is_macro = false;
  std::vector<std::string> params;
  TracerEnter tracer_enter;
  TracerLeave tracer_leave;
  bool disable_action = false;

private:
  friend class Reference;
  friend class ParserGenerator;

  Definition &operator=(const Definition &rhs);
  Definition &operator=(Definition &&rhs);

  void initialize_definition_ids() const {
    std::call_once(definition_ids_init_, [&]() {
      AssignIDToDefinition vis;
      holder_->accept(vis);
      if (whitespaceOpe) { whitespaceOpe->accept(vis); }
      if (wordOpe) { wordOpe->accept(vis); }
      definition_ids_.swap(vis.ids);
    });
  }

  Result parse_core(const char *s, size_t n, SemanticValues &sv, any &dt,
                    const char *path) const {
    initialize_definition_ids();

    std::shared_ptr<Ope> ope = holder_;
    if (whitespaceOpe) { ope = std::make_shared<Sequence>(whitespaceOpe, ope); }

    Context cxt(path, s, n, definition_ids_.size(), whitespaceOpe, wordOpe,
                enablePackratParsing, tracer_enter, tracer_leave);

    auto len = ope->parse(s, n, sv, cxt, dt);
    return Result{success(len), len, cxt.error_pos, cxt.message_pos,
                  cxt.message};
  }

  std::shared_ptr<Holder> holder_;
  mutable std::once_flag is_token_init_;
  mutable bool is_token_ = false;
  mutable std::once_flag assign_id_to_definition_init_;
  mutable std::once_flag definition_ids_init_;
  mutable std::unordered_map<void *, size_t> definition_ids_;
};

/*
 * Implementations
 */

inline size_t parse_literal(const char *s, size_t n, SemanticValues &sv,
                            Context &c, any &dt, const std::string &lit,
                            bool &init_is_word, bool &is_word,
                            bool ignore_case) {
  size_t i = 0;
  for (; i < lit.size(); i++) {
    if (i >= n || (ignore_case ? (std::tolower(s[i]) != std::tolower(lit[i]))
                               : (s[i] != lit[i]))) {
      c.set_error_pos(s);
      return static_cast<size_t>(-1);
    }
  }

  // Word check
  static Context dummy_c(nullptr, c.s, c.l, 0, nullptr, nullptr, false, nullptr,
                         nullptr);
  static SemanticValues dummy_sv;
  static any dummy_dt;

  if (!init_is_word) { // TODO: Protect with mutex
    if (c.wordOpe) {
      auto len =
          c.wordOpe->parse(lit.data(), lit.size(), dummy_sv, dummy_c, dummy_dt);
      is_word = success(len);
    }
    init_is_word = true;
  }

  if (is_word) {
    auto ope = std::make_shared<NotPredicate>(c.wordOpe);
    auto len = ope->parse(s + i, n - i, dummy_sv, dummy_c, dummy_dt);
    if (fail(len)) { return static_cast<size_t>(-1); }
    i += len;
  }

  // Skip whiltespace
  if (!c.in_token) {
    if (c.whitespaceOpe) {
      auto len = c.whitespaceOpe->parse(s + i, n - i, sv, c, dt);
      if (fail(len)) { return static_cast<size_t>(-1); }
      i += len;
    }
  }

  return i;
}

inline void Context::trace_enter(const char *name, const char *a_s, size_t n,
                                 SemanticValues &sv, any &dt) const {
  trace_ids.push_back(next_trace_id++);
  tracer_enter(name, a_s, n, sv, *this, dt);
}

inline void Context::trace_leave(const char *name, const char *a_s, size_t n,
                                 SemanticValues &sv, any &dt,
                                 size_t len) const {
  tracer_leave(name, a_s, n, sv, *this, dt, len);
  trace_ids.pop_back();
}

inline bool Context::is_traceable(const Ope &ope) const {
  if (tracer_enter && tracer_leave) {
    IsReference vis;
    const_cast<Ope &>(ope).accept(vis);
    return !vis.is_reference;
  }
  return false;
}

inline size_t Ope::parse(const char *s, size_t n, SemanticValues &sv,
                         Context &c, any &dt) const {
  if (c.is_traceable(*this)) {
    TraceOpeName vis;
    const_cast<Ope &>(*this).accept(vis);
    c.trace_enter(vis.name, s, n, sv, dt);
    auto len = parse_core(s, n, sv, c, dt);
    c.trace_leave(vis.name, s, n, sv, dt, len);
    return len;
  }
  return parse_core(s, n, sv, c, dt);
}

inline size_t Dictionary::parse_core(const char *s, size_t n,
                                     SemanticValues & /*sv*/, Context &c,
                                     any & /*dt*/) const {
  auto len = trie_.match(s, n);
  if (len > 0) { return len; }
  c.set_error_pos(s);
  return static_cast<size_t>(-1);
}

inline size_t LiteralString::parse_core(const char *s, size_t n,
                                        SemanticValues &sv, Context &c,
                                        any &dt) const {
  return parse_literal(s, n, sv, c, dt, lit_, init_is_word_, is_word_,
                       ignore_case_);
}

inline size_t TokenBoundary::parse_core(const char *s, size_t n,
                                        SemanticValues &sv, Context &c,
                                        any &dt) const {
  c.in_token = true;
  auto se = make_scope_exit([&]() { c.in_token = false; });
  const auto &rule = *ope_;
  auto len = rule.parse(s, n, sv, c, dt);
  if (success(len)) {
    sv.tokens.push_back(std::make_pair(s, len));

    if (c.whitespaceOpe) {
      auto l = c.whitespaceOpe->parse(s + len, n - len, sv, c, dt);
      if (fail(l)) { return static_cast<size_t>(-1); }
      len += l;
    }
  }
  return len;
}

inline size_t Holder::parse_core(const char *s, size_t n, SemanticValues &sv,
                                 Context &c, any &dt) const {
  if (!ope_) {
    throw std::logic_error("Uninitialized definition ope was used...");
  }

  // Macro reference
  // TODO: need packrat support
  if (outer_->is_macro) { return ope_->parse(s, n, sv, c, dt); }

  size_t len;
  any val;

  c.packrat(s, outer_->id, len, val, [&](any &a_val) {
    if (outer_->enter) { outer_->enter(s, n, dt); }

    auto se2 = make_scope_exit([&]() {
      c.pop();

      if (outer_->leave) { outer_->leave(s, n, len, a_val, dt); }
    });

    auto &chldsv = c.push();

    len = ope_->parse(s, n, chldsv, c, dt);

    // Invoke action
    if (success(len)) {
      chldsv.s_ = s;
      chldsv.n_ = len;
      chldsv.name_ = outer_->name;

      if (!IsPrioritizedChoice::check(*ope_)) {
        chldsv.choice_count_ = 0;
        chldsv.choice_ = 0;
      }

      try {
        a_val = reduce(chldsv, dt);
      } catch (const parse_error &e) {
        if (e.what()) {
          if (c.message_pos < s) {
            c.message_pos = s;
            c.message = e.what();
          }
        }
        len = static_cast<size_t>(-1);
      }
    }
  });

  if (success(len)) {
    if (!outer_->ignoreSemanticValue) {
      sv.emplace_back(val);
      sv.tags.emplace_back(str2tag(outer_->name.c_str()));
    }
  } else {
    if (outer_->error_message) {
      if (c.message_pos < s) {
        c.message_pos = s;
        c.message = outer_->error_message();
      }
    }
  }

  return len;
}

inline any Holder::reduce(SemanticValues &sv, any &dt) const {
  if (outer_->action && !outer_->disable_action) {
    return outer_->action(sv, dt);
  } else if (sv.empty()) {
    return any();
  } else {
    return std::move(sv.front());
  }
}

inline const char *Holder::trace_name() const {
  if (trace_name_.empty()) { trace_name_ = "[" + outer_->name + "]"; }
  return trace_name_.c_str();
}

inline size_t Reference::parse_core(const char *s, size_t n, SemanticValues &sv,
                                    Context &c, any &dt) const {
  if (rule_) {
    // Reference rule
    if (rule_->is_macro) {
      // Macro
      FindReference vis(c.top_args(), rule_->params);

      // Collect arguments
      std::vector<std::shared_ptr<Ope>> args;
      for (auto arg : args_) {
        arg->accept(vis);
        args.push_back(vis.found_ope);
      }

      c.push_args(std::move(args));
      auto se = make_scope_exit([&]() { c.pop_args(); });
      auto ope = get_core_operator();
      return ope->parse(s, n, sv, c, dt);
    } else {
      // Definition
      auto ope = get_core_operator();
      return ope->parse(s, n, sv, c, dt);
    }
  } else {
    // Reference parameter in macro
    const auto &args = c.top_args();
    return args[iarg_]->parse(s, n, sv, c, dt);
  }
}

inline std::shared_ptr<Ope> Reference::get_core_operator() const {
  return rule_->holder_;
}

inline size_t BackReference::parse_core(const char *s, size_t n,
                                        SemanticValues &sv, Context &c,
                                        any &dt) const {
  auto size = static_cast<int>(c.capture_scope_stack_size);
  for (auto i = size - 1; i >= 0; i--) {
    auto index = static_cast<size_t>(i);
    const auto &cs = c.capture_scope_stack[index];
    if (cs.find(name_) != cs.end()) {
      const auto &lit = cs.at(name_);
      auto init_is_word = false;
      auto is_word = false;
      return parse_literal(s, n, sv, c, dt, lit, init_is_word, is_word, false);
    }
  }
  throw std::runtime_error("Invalid back reference...");
}

inline Definition &
PrecedenceClimbing::get_reference_for_binop(Context &c) const {
  if (rule_.is_macro) {
    // Reference parameter in macro
    const auto &args = c.top_args();
    auto iarg = dynamic_cast<Reference &>(*binop_).iarg_;
    auto arg = args[iarg];
    return *dynamic_cast<Reference &>(*arg).rule_;
  }

  return *dynamic_cast<Reference &>(*binop_).rule_;
}

inline size_t PrecedenceClimbing::parse_expression(const char *s, size_t n,
                                                   SemanticValues &sv,
                                                   Context &c, any &dt,
                                                   size_t min_prec) const {
  auto len = atom_->parse(s, n, sv, c, dt);
  if (fail(len)) { return len; }

  std::string tok;
  auto &rule = get_reference_for_binop(c);
  auto action = rule.action;

  rule.action = [&](SemanticValues &sv2, any &dt2) -> any {
    tok = sv2.token();
    if (action) {
      return action(sv2, dt2);
    } else if (!sv2.empty()) {
      return sv2[0];
    }
    return any();
  };
  auto action_se = make_scope_exit([&]() { rule.action = action; });

  auto save_error_pos = c.error_pos;

  auto i = len;
  while (i < n) {
    std::vector<any> save_values(sv.begin(), sv.end());
    auto save_tokens = sv.tokens;

    auto chv = c.push();
    auto chl = binop_->parse(s + i, n - i, chv, c, dt);
    c.pop();

    if (fail(chl)) {
      c.error_pos = save_error_pos;
      break;
    }

    auto it = info_.find(tok);
    if (it == info_.end()) { break; }

    auto level = std::get<0>(it->second);
    auto assoc = std::get<1>(it->second);

    if (level < min_prec) { break; }

    sv.emplace_back(std::move(chv[0]));
    i += chl;

    auto next_min_prec = level;
    if (assoc == 'L') { next_min_prec = level + 1; }

    chv = c.push();
    chl = parse_expression(s + i, n - i, chv, c, dt, next_min_prec);
    c.pop();

    if (fail(chl)) {
      sv.assign(save_values.begin(), save_values.end());
      sv.tokens = save_tokens;
      c.error_pos = save_error_pos;
      break;
    }

    sv.emplace_back(std::move(chv[0]));
    i += chl;

    any val;
    if (rule_.action) {
      sv.s_ = s;
      sv.n_ = i;
      val = rule_.action(sv, dt);
    } else if (!sv.empty()) {
      val = sv[0];
    }
    sv.clear();
    sv.emplace_back(std::move(val));
  }

  return i;
}

inline void Sequence::accept(Visitor &v) { v.visit(*this); }
inline void PrioritizedChoice::accept(Visitor &v) { v.visit(*this); }
inline void Repetition::accept(Visitor &v) { v.visit(*this); }
inline void AndPredicate::accept(Visitor &v) { v.visit(*this); }
inline void NotPredicate::accept(Visitor &v) { v.visit(*this); }
inline void Dictionary::accept(Visitor &v) { v.visit(*this); }
inline void LiteralString::accept(Visitor &v) { v.visit(*this); }
inline void CharacterClass::accept(Visitor &v) { v.visit(*this); }
inline void Character::accept(Visitor &v) { v.visit(*this); }
inline void AnyCharacter::accept(Visitor &v) { v.visit(*this); }
inline void CaptureScope::accept(Visitor &v) { v.visit(*this); }
inline void Capture::accept(Visitor &v) { v.visit(*this); }
inline void TokenBoundary::accept(Visitor &v) { v.visit(*this); }
inline void Ignore::accept(Visitor &v) { v.visit(*this); }
inline void User::accept(Visitor &v) { v.visit(*this); }
inline void WeakHolder::accept(Visitor &v) { v.visit(*this); }
inline void Holder::accept(Visitor &v) { v.visit(*this); }
inline void Reference::accept(Visitor &v) { v.visit(*this); }
inline void Whitespace::accept(Visitor &v) { v.visit(*this); }
inline void BackReference::accept(Visitor &v) { v.visit(*this); }
inline void PrecedenceClimbing::accept(Visitor &v) { v.visit(*this); }

inline void AssignIDToDefinition::visit(Holder &ope) {
  auto p = static_cast<void *>(ope.outer_);
  if (ids.count(p)) { return; }
  auto id = ids.size();
  ids[p] = id;
  ope.outer_->id = id;
  ope.ope_->accept(*this);
}

inline void AssignIDToDefinition::visit(Reference &ope) {
  if (ope.rule_) {
    for (auto arg : ope.args_) {
      arg->accept(*this);
    }
    ope.rule_->accept(*this);
  }
}

inline void TokenChecker::visit(Reference &ope) {
  if (ope.is_macro_) {
    ope.rule_->accept(*this);
    for (auto arg : ope.args_) {
      arg->accept(*this);
    }
  } else {
    has_rule_ = true;
  }
}

inline void DetectLeftRecursion::visit(Reference &ope) {
  if (ope.name_ == name_) {
    error_s = ope.s_;
  } else if (!refs_.count(ope.name_)) {
    refs_.insert(ope.name_);
    if (ope.rule_) {
      ope.rule_->accept(*this);
      if (done_ == false) { return; }
    }
  }
  done_ = true;
}

inline void HasEmptyElement::visit(Reference &ope) {
  auto it = std::find_if(refs_.begin(), refs_.end(),
                         [&](const std::pair<const char *, std::string> &ref) {
                           return ope.name_ == ref.second;
                         });
  if (it != refs_.end()) { return; }

  if (ope.rule_) {
    refs_.emplace_back(ope.s_, ope.name_);
    ope.rule_->accept(*this);
    refs_.pop_back();
  }
}

inline void DetectInfiniteLoop::visit(Reference &ope) {
  auto it = std::find_if(refs_.begin(), refs_.end(),
                         [&](const std::pair<const char *, std::string> &ref) {
                           return ope.name_ == ref.second;
                         });
  if (it != refs_.end()) { return; }

  if (ope.rule_) {
    refs_.emplace_back(ope.s_, ope.name_);
    ope.rule_->accept(*this);
    refs_.pop_back();
  }
}

inline void ReferenceChecker::visit(Reference &ope) {
  auto it = std::find(params_.begin(), params_.end(), ope.name_);
  if (it != params_.end()) { return; }

  if (!grammar_.count(ope.name_)) {
    error_s[ope.name_] = ope.s_;
    error_message[ope.name_] = "'" + ope.name_ + "' is not defined.";
  } else {
    const auto &rule = grammar_.at(ope.name_);
    if (rule.is_macro) {
      if (!ope.is_macro_ || ope.args_.size() != rule.params.size()) {
        error_s[ope.name_] = ope.s_;
        error_message[ope.name_] = "incorrect number of arguments.";
      }
    } else if (ope.is_macro_) {
      error_s[ope.name_] = ope.s_;
      error_message[ope.name_] = "'" + ope.name_ + "' is not macro.";
    }
  }
}

inline void LinkReferences::visit(Reference &ope) {
  // Check if the reference is a macro parameter
  auto found_param = false;
  for (size_t i = 0; i < params_.size(); i++) {
    const auto &param = params_[i];
    if (param == ope.name_) {
      ope.iarg_ = i;
      found_param = true;
      break;
    }
  }

  // Check if the reference is a definition rule
  if (!found_param && grammar_.count(ope.name_)) {
    auto &rule = grammar_.at(ope.name_);
    ope.rule_ = &rule;
  }

  for (auto arg : ope.args_) {
    arg->accept(*this);
  }
}

inline void FindReference::visit(Reference &ope) {
  for (size_t i = 0; i < args_.size(); i++) {
    const auto &name = params_[i];
    if (name == ope.name_) {
      found_ope = args_[i];
      return;
    }
  }
  found_ope = ope.shared_from_this();
}

/*-----------------------------------------------------------------------------
 *  PEG parser generator
 *---------------------------------------------------------------------------*/

typedef std::unordered_map<std::string, std::shared_ptr<Ope>> Rules;
typedef std::function<void(size_t, size_t, const std::string &)> Log;

class ParserGenerator {
public:
  static std::shared_ptr<Grammar> parse(const char *s, size_t n,
                                        const Rules &rules, std::string &start,
                                        Log log) {
    return get_instance().perform_core(s, n, rules, start, log);
  }

  static std::shared_ptr<Grammar> parse(const char *s, size_t n,
                                        std::string &start, Log log) {
    Rules dummy;
    return parse(s, n, dummy, start, log);
  }

  // For debuging purpose
  static Grammar &grammar() { return get_instance().g; }

private:
  static ParserGenerator &get_instance() {
    static ParserGenerator instance;
    return instance;
  }

  ParserGenerator() {
    make_grammar();
    setup_actions();
  }

  struct Instruction {
    std::string type;
    any data;
  };

  struct Data {
    std::shared_ptr<Grammar> grammar;
    std::string start;
    const char *start_pos = nullptr;
    std::vector<std::pair<std::string, const char *>> duplicates;
    std::map<std::string, Instruction> instructions;

    Data() : grammar(std::make_shared<Grammar>()) {}
  };

  void make_grammar() {
    // Setup PEG syntax parser
    g["Grammar"] <= seq(g["Spacing"], oom(g["Definition"]), g["EndOfFile"]);
    g["Definition"] <=
        cho(seq(g["Ignore"], g["IdentCont"], g["Parameters"], g["LEFTARROW"],
                g["Expression"], opt(g["Instruction"])),
            seq(g["Ignore"], g["Identifier"], g["LEFTARROW"], g["Expression"],
                opt(g["Instruction"])));
    g["Expression"] <= seq(g["Sequence"], zom(seq(g["SLASH"], g["Sequence"])));
    g["Sequence"] <= zom(g["Prefix"]);
    g["Prefix"] <= seq(opt(cho(g["AND"], g["NOT"])), g["Suffix"]);
    g["Suffix"] <= seq(g["Primary"], opt(g["Loop"]));
    g["Loop"] <= cho(g["QUESTION"], g["STAR"], g["PLUS"], g["Repetition"]);
    g["Primary"] <=
        cho(seq(g["Ignore"], g["IdentCont"], g["Arguments"],
                npd(g["LEFTARROW"])),
            seq(g["Ignore"], g["Identifier"],
                npd(seq(opt(g["Parameters"]), g["LEFTARROW"]))),
            seq(g["OPEN"], g["Expression"], g["CLOSE"]),
            seq(g["BeginTok"], g["Expression"], g["EndTok"]),
            seq(g["BeginCapScope"], g["Expression"], g["EndCapScope"]),
            seq(g["BeginCap"], g["Expression"], g["EndCap"]), g["BackRef"],
            g["LiteralI"], g["Dictionary"], g["Literal"], g["NegatedClass"],
            g["Class"], g["DOT"]);

    g["Identifier"] <= seq(g["IdentCont"], g["Spacing"]);
    g["IdentCont"] <= seq(g["IdentStart"], zom(g["IdentRest"]));

    const static std::vector<std::pair<char32_t, char32_t>> range = {
        {0x0080, 0xFFFF}};
    g["IdentStart"] <= cho(cls("a-zA-Z_%"), cls(range));

    g["IdentRest"] <= cho(g["IdentStart"], cls("0-9"));

    g["Dictionary"] <= seq(g["LiteralD"], oom(seq(g["PIPE"], g["LiteralD"])));

    auto lit_ope = cho(seq(cls("'"), tok(zom(seq(npd(cls("'")), g["Char"]))),
                           cls("'"), g["Spacing"]),
                       seq(cls("\""), tok(zom(seq(npd(cls("\"")), g["Char"]))),
                           cls("\""), g["Spacing"]));
    g["Literal"] <= lit_ope;
    g["LiteralD"] <= lit_ope;

    g["LiteralI"] <=
        cho(seq(cls("'"), tok(zom(seq(npd(cls("'")), g["Char"]))), lit("'i"),
                g["Spacing"]),
            seq(cls("\""), tok(zom(seq(npd(cls("\"")), g["Char"]))), lit("\"i"),
                g["Spacing"]));

    // NOTE: The original Brian Ford's paper uses 'zom' instead of 'oom'.
    g["Class"] <= seq(chr('['), npd(chr('^')),
                      tok(oom(seq(npd(chr(']')), g["Range"]))), chr(']'),
                      g["Spacing"]);
    g["NegatedClass"] <= seq(lit("[^"),
                             tok(oom(seq(npd(chr(']')), g["Range"]))), chr(']'),
                             g["Spacing"]);

    g["Range"] <= cho(seq(g["Char"], chr('-'), g["Char"]), g["Char"]);
    g["Char"] <= cho(seq(chr('\\'), cls("nrt'\"[]\\^")),
                     seq(chr('\\'), cls("0-3"), cls("0-7"), cls("0-7")),
                     seq(chr('\\'), cls("0-7"), opt(cls("0-7"))),
                     seq(lit("\\x"), cls("0-9a-fA-F"), opt(cls("0-9a-fA-F"))),
                     seq(lit("\\u"), cls("0-9a-fA-F"), cls("0-9a-fA-F"),
                         cls("0-9a-fA-F"), cls("0-9a-fA-F")),
                     seq(npd(chr('\\')), dot()));

    g["Repetition"] <=
        seq(g["BeginBlacket"], g["RepetitionRange"], g["EndBlacket"]);
    g["RepetitionRange"] <= cho(seq(g["Number"], g["COMMA"], g["Number"]),
                                seq(g["Number"], g["COMMA"]), g["Number"],
                                seq(g["COMMA"], g["Number"]));
    g["Number"] <= seq(oom(cls("0-9")), g["Spacing"]);

    g["LEFTARROW"] <=
        seq(cho(lit("<-"), lit(reinterpret_cast<const char *>(u8"←"))),
            g["Spacing"]);
    ~g["SLASH"] <= seq(chr('/'), g["Spacing"]);
    ~g["PIPE"] <= seq(chr('|'), g["Spacing"]);
    g["AND"] <= seq(chr('&'), g["Spacing"]);
    g["NOT"] <= seq(chr('!'), g["Spacing"]);
    g["QUESTION"] <= seq(chr('?'), g["Spacing"]);
    g["STAR"] <= seq(chr('*'), g["Spacing"]);
    g["PLUS"] <= seq(chr('+'), g["Spacing"]);
    ~g["OPEN"] <= seq(chr('('), g["Spacing"]);
    ~g["CLOSE"] <= seq(chr(')'), g["Spacing"]);
    g["DOT"] <= seq(chr('.'), g["Spacing"]);

    ~g["Spacing"] <= zom(cho(g["Space"], g["Comment"]));
    g["Comment"] <=
        seq(chr('#'), zom(seq(npd(g["EndOfLine"]), dot())), g["EndOfLine"]);
    g["Space"] <= cho(chr(' '), chr('\t'), g["EndOfLine"]);
    g["EndOfLine"] <= cho(lit("\r\n"), chr('\n'), chr('\r'));
    g["EndOfFile"] <= npd(dot());

    ~g["BeginTok"] <= seq(chr('<'), g["Spacing"]);
    ~g["EndTok"] <= seq(chr('>'), g["Spacing"]);

    ~g["BeginCapScope"] <= seq(chr('$'), chr('('), g["Spacing"]);
    ~g["EndCapScope"] <= seq(chr(')'), g["Spacing"]);

    g["BeginCap"] <= seq(chr('$'), tok(g["IdentCont"]), chr('<'), g["Spacing"]);
    ~g["EndCap"] <= seq(chr('>'), g["Spacing"]);

    g["BackRef"] <= seq(chr('$'), tok(g["IdentCont"]), g["Spacing"]);

    g["IGNORE"] <= chr('~');

    g["Ignore"] <= opt(g["IGNORE"]);
    g["Parameters"] <= seq(g["OPEN"], g["Identifier"],
                           zom(seq(g["COMMA"], g["Identifier"])), g["CLOSE"]);
    g["Arguments"] <= seq(g["OPEN"], g["Expression"],
                          zom(seq(g["COMMA"], g["Expression"])), g["CLOSE"]);
    ~g["COMMA"] <= seq(chr(','), g["Spacing"]);

    // Instruction grammars
    g["Instruction"] <=
        seq(g["BeginBlacket"], cho(g["PrecedenceClimbing"]), g["EndBlacket"]);

    ~g["SpacesZom"] <= zom(g["Space"]);
    ~g["SpacesOom"] <= oom(g["Space"]);
    ~g["BeginBlacket"] <= seq(chr('{'), g["Spacing"]);
    ~g["EndBlacket"] <= seq(chr('}'), g["Spacing"]);

    // PrecedenceClimbing instruction
    g["PrecedenceClimbing"] <=
        seq(lit("precedence"), g["SpacesZom"], g["PrecedenceInfo"],
            zom(seq(g["SpacesOom"], g["PrecedenceInfo"])), g["SpacesZom"]);
    g["PrecedenceInfo"] <=
        seq(g["PrecedenceAssoc"],
            oom(seq(ign(g["SpacesOom"]), g["PrecedenceOpe"])));
    g["PrecedenceOpe"] <=
        tok(oom(
            seq(npd(cho(g["PrecedenceAssoc"], g["Space"], chr('}'))), dot())));
    g["PrecedenceAssoc"] <= cls("LR");

    // Set definition names
    for (auto &x : g) {
      x.second.name = x.first;
    }
  }

  void setup_actions() {
    g["Definition"] = [&](const SemanticValues &sv, any &dt) {
      Data &data = *any_cast<Data *>(dt);

      auto is_macro = sv.choice() == 0;
      auto ignore = any_cast<bool>(sv[0]);
      auto name = any_cast<std::string>(sv[1]);

      std::vector<std::string> params;
      std::shared_ptr<Ope> ope;
      if (is_macro) {
        params = any_cast<std::vector<std::string>>(sv[2]);
        ope = any_cast<std::shared_ptr<Ope>>(sv[4]);
        if (sv.size() == 6) {
          data.instructions[name] = any_cast<Instruction>(sv[5]);
        }
      } else {
        ope = any_cast<std::shared_ptr<Ope>>(sv[3]);
        if (sv.size() == 5) {
          data.instructions[name] = any_cast<Instruction>(sv[4]);
        }
      }

      auto &grammar = *data.grammar;
      if (!grammar.count(name)) {
        auto &rule = grammar[name];
        rule <= ope;
        rule.name = name;
        rule.s_ = sv.c_str();
        rule.ignoreSemanticValue = ignore;
        rule.is_macro = is_macro;
        rule.params = params;

        if (data.start.empty()) {
          data.start = name;
          data.start_pos = sv.c_str();
        }
      } else {
        data.duplicates.emplace_back(name, sv.c_str());
      }
    };

    g["Expression"] = [&](const SemanticValues &sv) {
      if (sv.size() == 1) {
        return any_cast<std::shared_ptr<Ope>>(sv[0]);
      } else {
        std::vector<std::shared_ptr<Ope>> opes;
        for (auto i = 0u; i < sv.size(); i++) {
          opes.emplace_back(any_cast<std::shared_ptr<Ope>>(sv[i]));
        }
        const std::shared_ptr<Ope> ope =
            std::make_shared<PrioritizedChoice>(opes);
        return ope;
      }
    };

    g["Sequence"] = [&](const SemanticValues &sv) {
      if (sv.size() == 1) {
        return any_cast<std::shared_ptr<Ope>>(sv[0]);
      } else {
        std::vector<std::shared_ptr<Ope>> opes;
        for (const auto &x : sv) {
          opes.emplace_back(any_cast<std::shared_ptr<Ope>>(x));
        }
        const std::shared_ptr<Ope> ope = std::make_shared<Sequence>(opes);
        return ope;
      }
    };

    g["Prefix"] = [&](const SemanticValues &sv) {
      std::shared_ptr<Ope> ope;
      if (sv.size() == 1) {
        ope = any_cast<std::shared_ptr<Ope>>(sv[0]);
      } else {
        assert(sv.size() == 2);
        auto tok = any_cast<char>(sv[0]);
        ope = any_cast<std::shared_ptr<Ope>>(sv[1]);
        if (tok == '&') {
          ope = apd(ope);
        } else { // '!'
          ope = npd(ope);
        }
      }
      return ope;
    };

    struct Loop {
      enum class Type { opt = 0, zom, oom, rep };
      Type type;
      std::pair<size_t, size_t> range;
    };

    g["Suffix"] = [&](const SemanticValues &sv) {
      auto ope = any_cast<std::shared_ptr<Ope>>(sv[0]);
      if (sv.size() == 1) {
        return ope;
      } else {
        assert(sv.size() == 2);
        auto loop = any_cast<Loop>(sv[1]);
        switch (loop.type) {
        case Loop::Type::opt: return opt(ope);
        case Loop::Type::zom: return zom(ope);
        case Loop::Type::oom: return oom(ope);
        case Loop::Type::rep: // Regex-like repetition
          return rep(ope, loop.range.first, loop.range.second);
        }
      }
    };

    g["Loop"] = [&](const SemanticValues &sv) {
      switch (sv.choice()) {
      case 0: // Option
        return Loop{Loop::Type::opt, std::pair<size_t, size_t>()};
      case 1: // Zero or More
        return Loop{Loop::Type::zom, std::pair<size_t, size_t>()};
      case 2: // One or More
        return Loop{Loop::Type::oom, std::pair<size_t, size_t>()};
      default: // Regex-like repetition
        return Loop{Loop::Type::rep,
                    any_cast<std::pair<size_t, size_t>>(sv[0])};
      }
    };

    g["RepetitionRange"] = [&](const SemanticValues &sv) {
      switch (sv.choice()) {
      case 0: { // Number COMMA Number
        auto min = any_cast<size_t>(sv[0]);
        auto max = any_cast<size_t>(sv[1]);
        return std::make_pair(min, max);
      }
      case 1: // Number COMMA
        return std::make_pair(any_cast<size_t>(sv[0]),
                              std::numeric_limits<size_t>::max());
      case 2: { // Number
        auto n = any_cast<size_t>(sv[0]);
        return std::make_pair(n, n);
      }
      default: // COMMA Number
        return std::make_pair(std::numeric_limits<size_t>::min(),
                              any_cast<size_t>(sv[0]));
      }
    };
    g["Number"] = [&](const SemanticValues &sv) {
      std::stringstream ss(sv.str());
      size_t n;
      ss >> n;
      return n;
    };

    g["Primary"] = [&](const SemanticValues &sv, any &dt) {
      Data &data = *any_cast<Data *>(dt);

      switch (sv.choice()) {
      case 0:   // Macro Reference
      case 1: { // Reference
        auto is_macro = sv.choice() == 0;
        auto ignore = any_cast<bool>(sv[0]);
        const auto &ident = any_cast<std::string>(sv[1]);

        std::vector<std::shared_ptr<Ope>> args;
        if (is_macro) {
          args = any_cast<std::vector<std::shared_ptr<Ope>>>(sv[2]);
        }

        std::shared_ptr<Ope> ope =
            ref(*data.grammar, ident, sv.c_str(), is_macro, args);

        if (ignore) {
          return ign(ope);
        } else {
          return ope;
        }
      }
      case 2: { // (Expression)
        return any_cast<std::shared_ptr<Ope>>(sv[0]);
      }
      case 3: { // TokenBoundary
        return tok(any_cast<std::shared_ptr<Ope>>(sv[0]));
      }
      case 4: { // CaptureScope
        return csc(any_cast<std::shared_ptr<Ope>>(sv[0]));
      }
      case 5: { // Capture
        const auto &name = any_cast<std::string>(sv[0]);
        auto ope = any_cast<std::shared_ptr<Ope>>(sv[1]);
        return cap(ope, [name](const char *a_s, size_t a_n, Context &c) {
          auto &cs = c.capture_scope_stack[c.capture_scope_stack_size - 1];
          cs[name] = std::string(a_s, a_n);
        });
      }
      default: {
        return any_cast<std::shared_ptr<Ope>>(sv[0]);
      }
      }
    };

    g["IdentCont"] = [](const SemanticValues &sv) {
      return std::string(sv.c_str(), sv.length());
    };

    g["Dictionary"] = [](const SemanticValues &sv) {
      auto items = sv.transform<std::string>();
      return dic(items);
    };

    g["Literal"] = [](const SemanticValues &sv) {
      const auto &tok = sv.tokens.front();
      return lit(resolve_escape_sequence(tok.first, tok.second));
    };
    g["LiteralI"] = [](const SemanticValues &sv) {
      const auto &tok = sv.tokens.front();
      return liti(resolve_escape_sequence(tok.first, tok.second));
    };
    g["LiteralD"] = [](const SemanticValues &sv) {
      auto &tok = sv.tokens.front();
      return resolve_escape_sequence(tok.first, tok.second);
    };

    g["Class"] = [](const SemanticValues &sv) {
      auto ranges = sv.transform<std::pair<char32_t, char32_t>>();
      return cls(ranges);
    };
    g["NegatedClass"] = [](const SemanticValues &sv) {
      auto ranges = sv.transform<std::pair<char32_t, char32_t>>();
      return ncls(ranges);
    };
    g["Range"] = [](const SemanticValues &sv) {
      switch (sv.choice()) {
      case 0: {
        auto s1 = any_cast<std::string>(sv[0]);
        auto s2 = any_cast<std::string>(sv[1]);
        auto cp1 = decode_codepoint(s1.c_str(), s1.length());
        auto cp2 = decode_codepoint(s2.c_str(), s2.length());
        return std::make_pair(cp1, cp2);
      }
      case 1: {
        auto s = any_cast<std::string>(sv[0]);
        auto cp = decode_codepoint(s.c_str(), s.length());
        return std::make_pair(cp, cp);
      }
      }
      return std::make_pair<char32_t, char32_t>(0, 0);
    };
    g["Char"] = [](const SemanticValues &sv) {
      return resolve_escape_sequence(sv.c_str(), sv.length());
    };

    g["AND"] = [](const SemanticValues &sv) { return *sv.c_str(); };
    g["NOT"] = [](const SemanticValues &sv) { return *sv.c_str(); };
    g["QUESTION"] = [](const SemanticValues &sv) { return *sv.c_str(); };
    g["STAR"] = [](const SemanticValues &sv) { return *sv.c_str(); };
    g["PLUS"] = [](const SemanticValues &sv) { return *sv.c_str(); };

    g["DOT"] = [](const SemanticValues & /*sv*/) { return dot(); };

    g["BeginCap"] = [](const SemanticValues &sv) { return sv.token(); };

    g["BackRef"] = [&](const SemanticValues &sv) { return bkr(sv.token()); };

    g["Ignore"] = [](const SemanticValues &sv) { return sv.size() > 0; };

    g["Parameters"] = [](const SemanticValues &sv) {
      return sv.transform<std::string>();
    };

    g["Arguments"] = [](const SemanticValues &sv) {
      return sv.transform<std::shared_ptr<Ope>>();
    };

    g["PrecedenceClimbing"] = [](const SemanticValues &sv) {
      PrecedenceClimbing::BinOpeInfo binOpeInfo;
      size_t level = 1;
      for (auto v : sv) {
        auto tokens = any_cast<std::vector<std::string>>(v);
        auto assoc = tokens[0][0];
        for (size_t i = 1; i < tokens.size(); i++) {
          const auto &tok = tokens[i];
          binOpeInfo[tok] = std::make_pair(level, assoc);
        }
        level++;
      }
      Instruction instruction;
      instruction.type = "precedence";
      instruction.data = binOpeInfo;
      return instruction;
    };
    g["PrecedenceInfo"] = [](const SemanticValues &sv) {
      return sv.transform<std::string>();
    };
    g["PrecedenceOpe"] = [](const SemanticValues &sv) { return sv.token(); };
    g["PrecedenceAssoc"] = [](const SemanticValues &sv) { return sv.token(); };
  }

  bool apply_precedence_instruction(Definition &rule,
                                    const PrecedenceClimbing::BinOpeInfo &info,
                                    const char *s, Log log) {
    try {
      auto &seq = dynamic_cast<Sequence &>(*rule.get_core_operator());
      auto atom = seq.opes_[0];
      auto &rep = dynamic_cast<Repetition &>(*seq.opes_[1]);
      auto &seq1 = dynamic_cast<Sequence &>(*rep.ope_);
      auto binop = seq1.opes_[0];
      auto atom1 = seq1.opes_[1];

      auto atom_name = dynamic_cast<Reference &>(*atom).name_;
      auto binop_name = dynamic_cast<Reference &>(*binop).name_;
      auto atom1_name = dynamic_cast<Reference &>(*atom1).name_;

      if (!rep.is_zom() || atom_name != atom1_name || atom_name == binop_name) {
        if (log) {
          auto line = line_info(s, rule.s_);
          log(line.first, line.second,
              "'precedence' instruction cannt be applied to '" + rule.name +
                  "'.");
        }
        return false;
      }

      rule.holder_->ope_ = pre(atom, binop, info, rule);
      rule.disable_action = true;
    } catch (...) {
      if (log) {
        auto line = line_info(s, rule.s_);
        log(line.first, line.second,
            "'precedence' instruction cannt be applied to '" + rule.name +
                "'.");
      }
      return false;
    }
    return true;
  }

  std::shared_ptr<Grammar> perform_core(const char *s, size_t n,
                                        const Rules &rules, std::string &start,
                                        Log log) {
    Data data;
    any dt = &data;
    auto r = g["Grammar"].parse(s, n, dt);

    if (!r.ret) {
      if (log) {
        if (r.message_pos) {
          auto line = line_info(s, r.message_pos);
          log(line.first, line.second, r.message);
        } else {
          auto line = line_info(s, r.error_pos);
          log(line.first, line.second, "syntax error");
        }
      }
      return nullptr;
    }

    auto &grammar = *data.grammar;

    // User provided rules
    for (const auto &x : rules) {
      auto name = x.first;
      bool ignore = false;
      if (!name.empty() && name[0] == '~') {
        ignore = true;
        name.erase(0, 1);
      }
      if (!name.empty()) {
        auto &rule = grammar[name];
        rule <= x.second;
        rule.name = name;
        rule.ignoreSemanticValue = ignore;
      }
    }

    // Check duplicated definitions
    bool ret = data.duplicates.empty();

    for (const auto &x : data.duplicates) {
      if (log) {
        const auto &name = x.first;
        auto ptr = x.second;
        auto line = line_info(s, ptr);
        log(line.first, line.second, "'" + name + "' is already defined.");
      }
    }

    // Check missing definitions
    for (auto &x : grammar) {
      auto &rule = x.second;

      ReferenceChecker vis(*data.grammar, rule.params);
      rule.accept(vis);
      for (const auto &y : vis.error_s) {
        const auto &name = y.first;
        const auto ptr = y.second;
        if (log) {
          auto line = line_info(s, ptr);
          log(line.first, line.second, vis.error_message[name]);
        }
        ret = false;
      }
    }

    if (!ret) { return nullptr; }

    // Link references
    for (auto &x : grammar) {
      auto &rule = x.second;
      LinkReferences vis(*data.grammar, rule.params);
      rule.accept(vis);
    }

    // Check left recursion
    ret = true;

    for (auto &x : grammar) {
      const auto &name = x.first;
      auto &rule = x.second;

      DetectLeftRecursion vis(name);
      rule.accept(vis);
      if (vis.error_s) {
        if (log) {
          auto line = line_info(s, vis.error_s);
          log(line.first, line.second, "'" + name + "' is left recursive.");
        }
        ret = false;
      }
    }

    if (!ret) { return nullptr; }

    // Set root definition
    auto &start_rule = (*data.grammar)[data.start];

    // Check infinite loop
    {
      DetectInfiniteLoop vis(data.start_pos, data.start);
      start_rule.accept(vis);
      if (vis.has_error) {
        if (log) {
          auto line = line_info(s, vis.error_s);
          log(line.first, line.second,
              "infinite loop is detected in '" + vis.error_name + "'.");
        }
        return nullptr;
      }
    }

    // Automatic whitespace skipping
    if (grammar.count(WHITESPACE_DEFINITION_NAME)) {
      for (auto &x : grammar) {
        auto &rule = x.second;
        auto ope = rule.get_core_operator();
        if (IsLiteralToken::check(*ope)) { rule <= tok(ope); }
      }

      start_rule.whitespaceOpe =
          wsp((*data.grammar)[WHITESPACE_DEFINITION_NAME].get_core_operator());
    }

    // Word expression
    if (grammar.count(WORD_DEFINITION_NAME)) {
      start_rule.wordOpe =
          (*data.grammar)[WORD_DEFINITION_NAME].get_core_operator();
    }

    // Apply instructions
    for (const auto &item : data.instructions) {
      const auto &name = item.first;
      const auto &instruction = item.second;
      auto &rule = grammar[name];

      if (instruction.type == "precedence") {
        const auto &info =
            any_cast<PrecedenceClimbing::BinOpeInfo>(instruction.data);

        if (!apply_precedence_instruction(rule, info, s, log)) {
          return nullptr;
        }
      }
    }

    // Set root definition
    start = data.start;

    return data.grammar;
  }

  Grammar g;
};

/*-----------------------------------------------------------------------------
 *  AST
 *---------------------------------------------------------------------------*/

template <typename Annotation> struct AstBase : public Annotation {
  AstBase(const char *a_path, size_t a_line, size_t a_column,
          const char *a_name, size_t a_position, size_t a_length,
          size_t a_choice_count, size_t a_choice,
          const std::vector<std::shared_ptr<AstBase>> &a_nodes)
      : path(a_path ? a_path : ""), line(a_line), column(a_column),
        name(a_name), position(a_position), length(a_length),
        choice_count(a_choice_count), choice(a_choice), original_name(a_name),
        original_choice_count(a_choice_count), original_choice(a_choice),
        tag(str2tag(a_name)), original_tag(tag), is_token(false),
        nodes(a_nodes) {}

  AstBase(const char *a_path, size_t a_line, size_t a_column,
          const char *a_name, size_t a_position, size_t a_length,
          size_t a_choice_count, size_t a_choice, const std::string &a_token)
      : path(a_path ? a_path : ""), line(a_line), column(a_column),
        name(a_name), position(a_position), length(a_length),
        choice_count(a_choice_count), choice(a_choice), original_name(a_name),
        original_choice_count(a_choice_count), original_choice(a_choice),
        tag(str2tag(a_name)), original_tag(tag), is_token(true),
        token(a_token) {}

  AstBase(const AstBase &ast, const char *a_original_name, size_t a_position,
          size_t a_length, size_t a_original_choice_count,
          size_t a_original_choise)
      : path(ast.path), line(ast.line), column(ast.column), name(ast.name),
        position(a_position), length(a_length), choice_count(ast.choice_count),
        choice(ast.choice), original_name(a_original_name),
        original_choice_count(a_original_choice_count),
        original_choice(a_original_choise), tag(ast.tag),
        original_tag(str2tag(a_original_name)), is_token(ast.is_token),
        token(ast.token), nodes(ast.nodes), parent(ast.parent) {}

  const std::string path;
  const size_t line = 1;
  const size_t column = 1;

  const std::string name;
  size_t position;
  size_t length;
  const size_t choice_count;
  const size_t choice;
  const std::string original_name;
  const size_t original_choice_count;
  const size_t original_choice;
  const unsigned int tag;
  const unsigned int original_tag;

  const bool is_token;
  const std::string token;

  std::vector<std::shared_ptr<AstBase<Annotation>>> nodes;
  std::weak_ptr<AstBase<Annotation>> parent;
};

template <typename T>
void ast_to_s_core(const std::shared_ptr<T> &ptr, std::string &s, int level,
                   std::function<std::string(const T &ast, int level)> fn) {
  const auto &ast = *ptr;
  for (auto i = 0; i < level; i++) {
    s += "  ";
  }
  auto name = ast.original_name;
  if (ast.original_choice_count > 0) {
    name += "/" + std::to_string(ast.original_choice);
  }
  if (ast.name != ast.original_name) { name += "[" + ast.name + "]"; }
  if (ast.is_token) {
    s += "- " + name + " (" + ast.token + ")\n";
  } else {
    s += "+ " + name + "\n";
  }
  if (fn) { s += fn(ast, level + 1); }
  for (auto node : ast.nodes) {
    ast_to_s_core(node, s, level + 1, fn);
  }
}

template <typename T>
std::string
ast_to_s(const std::shared_ptr<T> &ptr,
         std::function<std::string(const T &ast, int level)> fn = nullptr) {
  std::string s;
  ast_to_s_core(ptr, s, 0, fn);
  return s;
}

struct AstOptimizer {
  AstOptimizer(bool optimize_nodes,
               const std::vector<std::string> &filters = {})
      : optimize_nodes_(optimize_nodes), filters_(filters) {}

  template <typename T>
  std::shared_ptr<T> optimize(std::shared_ptr<T> original,
                              std::shared_ptr<T> parent = nullptr) {
    auto found = std::find(filters_.begin(), filters_.end(), original->name) !=
                 filters_.end();
    bool opt = optimize_nodes_ ? !found : found;

    if (opt && original->nodes.size() == 1) {
      auto child = optimize(original->nodes[0], parent);
      return std::make_shared<T>(*child, original->name.c_str(),
                                 original->choice_count, original->position,
                                 original->length, original->choice);
    }

    auto ast = std::make_shared<T>(*original);
    ast->parent = parent;
    ast->nodes.clear();
    for (auto node : original->nodes) {
      auto child = optimize(node, ast);
      ast->nodes.push_back(child);
    }
    return ast;
  }

private:
  const bool optimize_nodes_;
  const std::vector<std::string> filters_;
};

struct EmptyType {};
typedef AstBase<EmptyType> Ast;

/*-----------------------------------------------------------------------------
 *  parser
 *---------------------------------------------------------------------------*/

class parser {
public:
  parser() = default;

  parser(const char *s, size_t n, const Rules &rules) {
    load_grammar(s, n, rules);
  }

  parser(const char *s, const Rules &rules) : parser(s, strlen(s), rules) {}

  parser(const char *s, size_t n) : parser(s, n, Rules()) {}

  parser(const char *s) : parser(s, strlen(s), Rules()) {}

  operator bool() { return grammar_ != nullptr; }

  bool load_grammar(const char *s, size_t n, const Rules &rules) {
    grammar_ = ParserGenerator::parse(s, n, rules, start_, log);
    return grammar_ != nullptr;
  }

  bool load_grammar(const char *s, size_t n) {
    return load_grammar(s, n, Rules());
  }

  bool load_grammar(const char *s, const Rules &rules) {
    auto n = strlen(s);
    return load_grammar(s, n, rules);
  }

  bool load_grammar(const char *s) {
    auto n = strlen(s);
    return load_grammar(s, n);
  }

  bool parse_n(const char *s, size_t n, const char *path = nullptr) const {
    if (grammar_ != nullptr) {
      const auto &rule = (*grammar_)[start_];
      auto r = rule.parse(s, n, path);
      output_log(s, n, r);
      return r.ret && r.len == n;
    }
    return false;
  }

  bool parse(const char *s, const char *path = nullptr) const {
    auto n = strlen(s);
    return parse_n(s, n, path);
  }

  bool parse_n(const char *s, size_t n, any &dt,
               const char *path = nullptr) const {
    if (grammar_ != nullptr) {
      const auto &rule = (*grammar_)[start_];
      auto r = rule.parse(s, n, dt, path);
      output_log(s, n, r);
      return r.ret && r.len == n;
    }
    return false;
  }

  bool parse(const char *s, any &dt, const char *path = nullptr) const {
    auto n = strlen(s);
    return parse_n(s, n, dt, path);
  }

  template <typename T>
  bool parse_n(const char *s, size_t n, T &val,
               const char *path = nullptr) const {
    if (grammar_ != nullptr) {
      const auto &rule = (*grammar_)[start_];
      auto r = rule.parse_and_get_value(s, n, val, path);
      output_log(s, n, r);
      return r.ret && r.len == n;
    }
    return false;
  }

  template <typename T>
  bool parse(const char *s, T &val, const char *path = nullptr) const {
    auto n = strlen(s);
    return parse_n(s, n, val, path);
  }

  template <typename T>
  bool parse_n(const char *s, size_t n, any &dt, T &val,
               const char *path = nullptr) const {
    if (grammar_ != nullptr) {
      const auto &rule = (*grammar_)[start_];
      auto r = rule.parse_and_get_value(s, n, dt, val, path);
      output_log(s, n, r);
      return r.ret && r.len == n;
    }
    return false;
  }

  template <typename T>
  bool parse(const char *s, any &dt, T &val,
             const char * /*path*/ = nullptr) const {
    auto n = strlen(s);
    return parse_n(s, n, dt, val);
  }

  Definition &operator[](const char *s) { return (*grammar_)[s]; }

  const Definition &operator[](const char *s) const { return (*grammar_)[s]; }

  std::vector<std::string> get_rule_names() {
    std::vector<std::string> rules;
    rules.reserve(grammar_->size());
    for (auto const &r : *grammar_) {
      rules.emplace_back(r.first);
    }
    return rules;
  }

  void enable_packrat_parsing() {
    if (grammar_ != nullptr) {
      auto &rule = (*grammar_)[start_];
      rule.enablePackratParsing = true;
    }
  }

  template <typename T = Ast> parser &enable_ast() {
    for (auto &x : *grammar_) {
      const auto &name = x.first;
      auto &rule = x.second;

      if (!rule.action) {
        rule.action = [&](const SemanticValues &sv) {
          auto line = sv.line_info();

          if (rule.is_token()) {
            return std::make_shared<T>(
                sv.path, line.first, line.second, name.c_str(),
                std::distance(sv.ss, sv.c_str()), sv.length(),
                sv.choice_count(), sv.choice(), sv.token());
          }

          auto ast = std::make_shared<T>(
              sv.path, line.first, line.second, name.c_str(),
              std::distance(sv.ss, sv.c_str()), sv.length(), sv.choice_count(),
              sv.choice(), sv.transform<std::shared_ptr<T>>());

          for (auto node : ast->nodes) {
            node->parent = ast;
          }
          return ast;
        };
      }
    }
    return *this;
  }

  void enable_trace(TracerEnter tracer_enter, TracerLeave tracer_leave) {
    if (grammar_ != nullptr) {
      auto &rule = (*grammar_)[start_];
      rule.tracer_enter = tracer_enter;
      rule.tracer_leave = tracer_leave;
    }
  }

  Log log;

private:
  void output_log(const char *s, size_t n, const Definition::Result &r) const {
    if (log) {
      if (!r.ret) {
        if (r.message_pos) {
          auto line = line_info(s, r.message_pos);
          log(line.first, line.second, r.message);
        } else {
          auto line = line_info(s, r.error_pos);
          log(line.first, line.second, "syntax error");
        }
      } else if (r.len != n) {
        auto line = line_info(s, s + r.len);
        log(line.first, line.second, "syntax error");
      }
    }
  }

  std::shared_ptr<Grammar> grammar_;
  std::string start_;
};

} // namespace peg

#endif

// vim: et ts=2 sw=2 cin cino={1s ff=unix
#ifndef ARGH
#define ARGH
//#pragma once

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cassert>

namespace argh
{
   // Terminology:
   // A command line is composed of 2 types of args:
   // 1. Positional args, i.e. free standing values
   // 2. Options: args beginning with '-'. We identify two kinds:
   //    2.1: Flags: boolean options =>  (exist ? true : false)
   //    2.2: Parameters: a name followed by a non-option value

#if !defined(__GNUC__) || (__GNUC__ >= 5)
   using string_stream = std::istringstream;
#else
    // Until GCC 5, istringstream did not have a move constructor.
    // stringstream_proxy is used instead, as a workaround.
   class stringstream_proxy
   {
   public:
      stringstream_proxy() = default;

      // Construct with a value.
      stringstream_proxy(std::string const& value) :
         stream_(value)
      {}

      // Copy constructor.
      stringstream_proxy(const stringstream_proxy& other) :
         stream_(other.stream_.str())
      {
         stream_.setstate(other.stream_.rdstate());
      }

      void setstate(std::ios_base::iostate state) { stream_.setstate(state); }

      // Stream out the value of the parameter.
      // If the conversion was not possible, the stream will enter the fail state,
      // and operator bool will return false.
      template<typename T>
      stringstream_proxy& operator >> (T& thing)
      {
         stream_ >> thing;
         return *this;
      }


      // Get the string value.
      std::string str() const { return stream_.str(); }

      std::stringbuf* rdbuf() const { return stream_.rdbuf(); }

      // Check the state of the stream. 
      // False when the most recent stream operation failed
      operator bool() const { return !!stream_; }

      ~stringstream_proxy() = default;
   private:
      std::istringstream stream_;
   };
   using string_stream = stringstream_proxy;
#endif

   class parser
   {
   public:
      enum Mode { PREFER_FLAG_FOR_UNREG_OPTION = 1 << 0,
                  PREFER_PARAM_FOR_UNREG_OPTION = 1 << 1,
                  NO_SPLIT_ON_EQUALSIGN = 1 << 2,
                  SINGLE_DASH_IS_MULTIFLAG = 1 << 3,
                };

      parser() = default;

      parser(std::initializer_list<char const* const> pre_reg_names)
      {  add_params(pre_reg_names); }

      parser(const char* const argv[], int mode = PREFER_FLAG_FOR_UNREG_OPTION)
      {  parse(argv, mode); }

      parser(int argc, const char* const argv[], int mode = PREFER_FLAG_FOR_UNREG_OPTION)
      {  parse(argc, argv, mode); }

      void add_param(std::string const& name);
      void add_params(std::initializer_list<char const* const> init_list);

      void parse(const char* const argv[], int mode = PREFER_FLAG_FOR_UNREG_OPTION);
      void parse(int argc, const char* const argv[], int mode = PREFER_FLAG_FOR_UNREG_OPTION);

      std::multiset<std::string>          const& flags()    const { return flags_;    }
      std::map<std::string, std::string>  const& params()   const { return params_;   }
      std::vector<std::string>            const& pos_args() const { return pos_args_; }

      // begin() and end() for using range-for over positional args.
      std::vector<std::string>::const_iterator begin() const { return pos_args_.cbegin(); }
      std::vector<std::string>::const_iterator end()   const { return pos_args_.cend();   }
      size_t size()                                    const { return pos_args_.size();   }

      //////////////////////////////////////////////////////////////////////////
      // Accessors

      // flag (boolean) accessors: return true if the flag appeared, otherwise false.
      bool operator[](std::string const& name) const;

      // multiple flag (boolean) accessors: return true if at least one of the flag appeared, otherwise false.
      bool operator[](std::initializer_list<char const* const> init_list) const;

      // returns positional arg string by order. Like argv[] but without the options
      std::string const& operator[](size_t ind) const;

      // returns a std::istream that can be used to convert a positional arg to a typed value.
      string_stream operator()(size_t ind) const;

      // same as above, but with a default value in case the arg is missing (index out of range).
      template<typename T>
      string_stream operator()(size_t ind, T&& def_val) const;

      // parameter accessors, give a name get an std::istream that can be used to convert to a typed value.
      // call .str() on result to get as string
      string_stream operator()(std::string const& name) const;

      // accessor for a parameter with multiple names, give a list of names, get an std::istream that can be used to convert to a typed value.
      // call .str() on result to get as string
      // returns the first value in the list to be found.
      string_stream operator()(std::initializer_list<char const* const> init_list) const;

      // same as above, but with a default value in case the param was missing.
      // Non-string def_val types must have an operator<<() (output stream operator)
      // If T only has an input stream operator, pass the string version of the type as in "3" instead of 3.
      template<typename T>
      string_stream operator()(std::string const& name, T&& def_val) const;

      // same as above but for a list of names. returns the first value to be found.
      template<typename T>
      string_stream operator()(std::initializer_list<char const* const> init_list, T&& def_val) const;

   private:
      string_stream bad_stream() const;
      std::string trim_leading_dashes(std::string const& name) const;
      bool is_number(std::string const& arg) const;
      bool is_option(std::string const& arg) const;
      bool got_flag(std::string const& name) const;
      bool is_param(std::string const& name) const;

   private:
      std::vector<std::string> args_;
      std::map<std::string, std::string> params_;
      std::vector<std::string> pos_args_;
      std::multiset<std::string> flags_;
      std::set<std::string> registeredParams_;
      std::string empty_;
   };


   //////////////////////////////////////////////////////////////////////////

   inline void parser::parse(const char * const argv[], int mode)
   {
      int argc = 0;
      for (auto argvp = argv; *argvp; ++argc, ++argvp);
      parse(argc, argv, mode);
   }

   //////////////////////////////////////////////////////////////////////////

   inline void parser::parse(int argc, const char* const argv[], int mode /*= PREFER_FLAG_FOR_UNREG_OPTION*/)
   {
      // convert to strings
      args_.resize(argc);
      std::transform(argv, argv + argc, args_.begin(), [](const char* const arg) { return arg;  });

      // parse line
      for (auto i = 0u; i < args_.size(); ++i)
      {
         if (!is_option(args_[i]))
         {
            pos_args_.emplace_back(args_[i]);
            continue;
         }

         auto name = trim_leading_dashes(args_[i]);

         if (!(mode & NO_SPLIT_ON_EQUALSIGN))
         {
            auto equalPos = name.find('=');
            if (equalPos != std::string::npos)
            {
               params_.insert({ name.substr(0, equalPos), name.substr(equalPos + 1) });
               continue;
            }
         }

         // if the option is unregistered and should be a multi-flag
         if (1 == (args_[i].size() - name.size()) &&         // single dash
            argh::parser::SINGLE_DASH_IS_MULTIFLAG & mode && // multi-flag mode
            !is_param(name))                                  // unregistered
         {
            std::string keep_param; 
            
            if (!name.empty() && is_param(std::string(1ul, name.back()))) // last char is param
            {
               keep_param += name.back();
               name.resize(name.size() - 1);
            }

            for (auto const& c : name)
            {
               flags_.emplace(std::string{ c });
            }

            if (!keep_param.empty())
            {
               name = keep_param;
            }
            else
            {
               continue; // do not consider other options for this arg
            }
         }

         // any potential option will get as its value the next arg, unless that arg is an option too
         // in that case it will be determined a flag.
         if (i == args_.size() - 1 || is_option(args_[i + 1]))
         {
            flags_.emplace(name);
            continue;
         }

         // if 'name' is a pre-registered option, then the next arg cannot be a free parameter to it is skipped
         // otherwise we have 2 modes:
         // PREFER_FLAG_FOR_UNREG_OPTION: a non-registered 'name' is determined a flag. 
         //                               The following value (the next arg) will be a free parameter.
         //
         // PREFER_PARAM_FOR_UNREG_OPTION: a non-registered 'name' is determined a parameter, the next arg
         //                                will be the value of that option.

         assert(!(mode & argh::parser::PREFER_FLAG_FOR_UNREG_OPTION)
             || !(mode & argh::parser::PREFER_PARAM_FOR_UNREG_OPTION));

         bool preferParam = mode & argh::parser::PREFER_PARAM_FOR_UNREG_OPTION;

         if (is_param(name) || preferParam)
         {
            params_.insert({ name, args_[i + 1] });
            ++i; // skip next value, it is not a free parameter
            continue;
         }
         else
         {
            flags_.emplace(name);
         }
      };
   }

   //////////////////////////////////////////////////////////////////////////

   inline string_stream parser::bad_stream() const
   {
      string_stream bad;
      bad.setstate(std::ios_base::failbit);
      return bad;
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool parser::is_number(std::string const& arg) const
   {
      // inefficient but simple way to determine if a string is a number (which can start with a '-')
      std::istringstream istr(arg);
      double number;
      istr >> number;
      return !(istr.fail() || istr.bad());
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool parser::is_option(std::string const& arg) const
   {
      assert(0 != arg.size());
      if (is_number(arg))
         return false;
      return '-' == arg[0];
   }

   //////////////////////////////////////////////////////////////////////////

   inline std::string parser::trim_leading_dashes(std::string const& name) const
   {
      auto pos = name.find_first_not_of('-');
      return std::string::npos != pos ? name.substr(pos) : name;
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool argh::parser::got_flag(std::string const& name) const
   {
      return flags_.end() != flags_.find(trim_leading_dashes(name));
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool argh::parser::is_param(std::string const& name) const
   {
      return registeredParams_.count(name);
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool parser::operator[](std::string const& name) const
   {
      return got_flag(name);
   }

   //////////////////////////////////////////////////////////////////////////

   inline bool parser::operator[](std::initializer_list<char const* const> init_list) const
   {
      return std::any_of(init_list.begin(), init_list.end(), [&](char const* const name) { return got_flag(name); });
   }

   //////////////////////////////////////////////////////////////////////////

   inline std::string const& parser::operator[](size_t ind) const
   {
      if (ind < pos_args_.size())
         return pos_args_[ind];
      return empty_;
   }

   //////////////////////////////////////////////////////////////////////////

   inline string_stream parser::operator()(std::string const& name) const
   {
      auto optIt = params_.find(trim_leading_dashes(name));
      if (params_.end() != optIt)
         return string_stream(optIt->second);
      return bad_stream();
   }

   //////////////////////////////////////////////////////////////////////////

   inline string_stream parser::operator()(std::initializer_list<char const* const> init_list) const
   {
      for (auto& name : init_list)
      {
         auto optIt = params_.find(trim_leading_dashes(name));
         if (params_.end() != optIt)
            return string_stream(optIt->second);
      }
      return bad_stream();
   }

   //////////////////////////////////////////////////////////////////////////

   template<typename T>
   string_stream parser::operator()(std::string const& name, T&& def_val) const
   {
      auto optIt = params_.find(trim_leading_dashes(name));
      if (params_.end() != optIt)
         return string_stream(optIt->second);

      std::ostringstream ostr;
      ostr << def_val;
      return string_stream(ostr.str()); // use default
   }

   //////////////////////////////////////////////////////////////////////////

   // same as above but for a list of names. returns the first value to be found.
   template<typename T>
   string_stream parser::operator()(std::initializer_list<char const* const> init_list, T&& def_val) const
   {
      for (auto& name : init_list)
      {
         auto optIt = params_.find(trim_leading_dashes(name));
         if (params_.end() != optIt)
            return string_stream(optIt->second);
      }      
      std::ostringstream ostr;
      ostr << def_val;
      return string_stream(ostr.str()); // use default
   }

   //////////////////////////////////////////////////////////////////////////

   inline string_stream parser::operator()(size_t ind) const
   {
      if (pos_args_.size() <= ind)
         return bad_stream();

      return string_stream(pos_args_[ind]);
   }

   //////////////////////////////////////////////////////////////////////////

   template<typename T>
   string_stream parser::operator()(size_t ind, T&& def_val) const
   {
      if (pos_args_.size() <= ind)
      {
         std::ostringstream ostr;
         ostr << def_val;
         return string_stream(ostr.str());
      }

      return string_stream(pos_args_[ind]);
   }

   //////////////////////////////////////////////////////////////////////////

   inline void parser::add_param(std::string const& name)
   {
      registeredParams_.insert(trim_leading_dashes(name));
   }

   //////////////////////////////////////////////////////////////////////////

   inline void parser::add_params(std::initializer_list<char const* const> init_list)
   {
      for (auto& name : init_list)
         registeredParams_.insert(trim_leading_dashes(name));
   }
}


#endif

/*
 * THIS IS AN AUTO-GENERATED FILE. DO NOT MODIFY AS CHANGES MIGHT BE OVERWRITTEN!
 */

#ifndef VISITABLE_TYPE_TRAIT
#define VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct visitorSelector {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct visitorSelector<true> {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<typename T>
struct isVisitable {
    static const bool value = false;
};

template<typename T, class Visitor>
void doVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
    visitorSelector<isVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, visitor);
}
#endif

#ifndef TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#define TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct tripletForwardVisitorSelector {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)preVisit;
        (void)postVisit;
        std::forward<Visitor>(visit)(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct tripletForwardVisitorSelector<true> {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)fieldIdentifier;
        (void)typeName;
        (void)name;
        // Apply preVisit, visit, and postVisit on value.
        value.accept(preVisit, visit, postVisit);
    }
};

template<typename T>
struct isTripletForwardVisitable {
    static const bool value = false;
};

template< typename T, class PreVisitor, class Visitor, class PostVisitor>
void doTripletForwardVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
    tripletForwardVisitorSelector<isTripletForwardVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, std::move(preVisit), std::move(visit), std::move(postVisit)); // NOLINT
}
#endif


#ifndef CLUON_DATA_TIMESTAMP_HPP
#define CLUON_DATA_TIMESTAMP_HPP

#ifdef WIN32
    // Export symbols if compile flags "LIB_SHARED" and "LIB_EXPORTS" are set on Windows.
    #ifdef LIB_SHARED
        #ifdef LIB_EXPORTS
            #define LIB_API __declspec(dllexport)
        #else
            #define LIB_API __declspec(dllimport)
        #endif
    #else
        // Disable definition if linking statically.
        #define LIB_API
    #endif
#else
    // Disable definition for non-Win32 systems.
    #define LIB_API
#endif

#include <string>
#include <utility>
namespace cluon { namespace data {
using namespace std::string_literals; // NOLINT
class LIB_API TimeStamp {
    private:
        static constexpr const char* TheShortName = "TimeStamp";
        static constexpr const char* TheLongName = "cluon.data.TimeStamp";

    public:
        inline static int32_t ID() {
            return 12;
        }
        inline static const std::string ShortName() {
            return TheShortName;
        }
        inline static const std::string LongName() {
            return TheLongName;
        }

    public:
        TimeStamp() = default;
        TimeStamp(const TimeStamp&) = default;
        TimeStamp& operator=(const TimeStamp&) = default;
        TimeStamp(TimeStamp&&) = default;
        TimeStamp& operator=(TimeStamp&&) = default;
        ~TimeStamp() = default;

    public:
        inline TimeStamp& seconds(const int32_t &v) noexcept {
            m_seconds = v;
            return *this;
        }
        inline int32_t seconds() const noexcept {
            return m_seconds;
        }
        inline TimeStamp& microseconds(const int32_t &v) noexcept {
            m_microseconds = v;
            return *this;
        }
        inline int32_t microseconds() const noexcept {
            return m_microseconds;
        }

    public:
        template<class Visitor>
        inline void accept(uint32_t fieldId, Visitor &visitor) {
            (void)fieldId;
            (void)visitor;
//            visitor.preVisit(ID(), ShortName(), LongName());
            if (1 == fieldId) {
                doVisit(1, std::move("int32_t"s), std::move("seconds"s), m_seconds, visitor);
                return;
            }
            if (2 == fieldId) {
                doVisit(2, std::move("int32_t"s), std::move("microseconds"s), m_microseconds, visitor);
                return;
            }
//            visitor.postVisit();
        }

        template<class Visitor>
        inline void accept(Visitor &visitor) {
            visitor.preVisit(ID(), ShortName(), LongName());
            doVisit(1, std::move("int32_t"s), std::move("seconds"s), m_seconds, visitor);
            doVisit(2, std::move("int32_t"s), std::move("microseconds"s), m_microseconds, visitor);
            visitor.postVisit();
        }

        template<class PreVisitor, class Visitor, class PostVisitor>
        inline void accept(PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
            (void)visit; // Prevent warnings from empty messages.
            std::forward<PreVisitor>(preVisit)(ID(), ShortName(), LongName());
            doTripletForwardVisit(1, std::move("int32_t"s), std::move("seconds"s), m_seconds, preVisit, visit, postVisit);
            doTripletForwardVisit(2, std::move("int32_t"s), std::move("microseconds"s), m_microseconds, preVisit, visit, postVisit);
            std::forward<PostVisitor>(postVisit)();
        }

    private:
        int32_t m_seconds{ 0 }; // field identifier = 1.
        int32_t m_microseconds{ 0 }; // field identifier = 2.
};
}}

template<>
struct isVisitable<cluon::data::TimeStamp> {
    static const bool value = true;
};
template<>
struct isTripletForwardVisitable<cluon::data::TimeStamp> {
    static const bool value = true;
};
#endif


/*
 * THIS IS AN AUTO-GENERATED FILE. DO NOT MODIFY AS CHANGES MIGHT BE OVERWRITTEN!
 */

#ifndef VISITABLE_TYPE_TRAIT
#define VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct visitorSelector {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct visitorSelector<true> {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<typename T>
struct isVisitable {
    static const bool value = false;
};

template<typename T, class Visitor>
void doVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
    visitorSelector<isVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, visitor);
}
#endif

#ifndef TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#define TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct tripletForwardVisitorSelector {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)preVisit;
        (void)postVisit;
        std::forward<Visitor>(visit)(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct tripletForwardVisitorSelector<true> {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)fieldIdentifier;
        (void)typeName;
        (void)name;
        // Apply preVisit, visit, and postVisit on value.
        value.accept(preVisit, visit, postVisit);
    }
};

template<typename T>
struct isTripletForwardVisitable {
    static const bool value = false;
};

template< typename T, class PreVisitor, class Visitor, class PostVisitor>
void doTripletForwardVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
    tripletForwardVisitorSelector<isTripletForwardVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, std::move(preVisit), std::move(visit), std::move(postVisit)); // NOLINT
}
#endif


#ifndef CLUON_DATA_ENVELOPE_HPP
#define CLUON_DATA_ENVELOPE_HPP

#ifdef WIN32
    // Export symbols if compile flags "LIB_SHARED" and "LIB_EXPORTS" are set on Windows.
    #ifdef LIB_SHARED
        #ifdef LIB_EXPORTS
            #define LIB_API __declspec(dllexport)
        #else
            #define LIB_API __declspec(dllimport)
        #endif
    #else
        // Disable definition if linking statically.
        #define LIB_API
    #endif
#else
    // Disable definition for non-Win32 systems.
    #define LIB_API
#endif

#include <string>
#include <utility>
namespace cluon { namespace data {
using namespace std::string_literals; // NOLINT
class LIB_API Envelope {
    private:
        static constexpr const char* TheShortName = "Envelope";
        static constexpr const char* TheLongName = "cluon.data.Envelope";

    public:
        inline static int32_t ID() {
            return 1;
        }
        inline static const std::string ShortName() {
            return TheShortName;
        }
        inline static const std::string LongName() {
            return TheLongName;
        }

    public:
        Envelope() = default;
        Envelope(const Envelope&) = default;
        Envelope& operator=(const Envelope&) = default;
        Envelope(Envelope&&) = default;
        Envelope& operator=(Envelope&&) = default;
        ~Envelope() = default;

    public:
        inline Envelope& dataType(const int32_t &v) noexcept {
            m_dataType = v;
            return *this;
        }
        inline int32_t dataType() const noexcept {
            return m_dataType;
        }
        inline Envelope& serializedData(const std::string &v) noexcept {
            m_serializedData = v;
            return *this;
        }
        inline std::string serializedData() const noexcept {
            return m_serializedData;
        }
        inline Envelope& sent(const cluon::data::TimeStamp &v) noexcept {
            m_sent = v;
            return *this;
        }
        inline cluon::data::TimeStamp sent() const noexcept {
            return m_sent;
        }
        inline Envelope& received(const cluon::data::TimeStamp &v) noexcept {
            m_received = v;
            return *this;
        }
        inline cluon::data::TimeStamp received() const noexcept {
            return m_received;
        }
        inline Envelope& sampleTimeStamp(const cluon::data::TimeStamp &v) noexcept {
            m_sampleTimeStamp = v;
            return *this;
        }
        inline cluon::data::TimeStamp sampleTimeStamp() const noexcept {
            return m_sampleTimeStamp;
        }
        inline Envelope& senderStamp(const uint32_t &v) noexcept {
            m_senderStamp = v;
            return *this;
        }
        inline uint32_t senderStamp() const noexcept {
            return m_senderStamp;
        }

    public:
        template<class Visitor>
        inline void accept(uint32_t fieldId, Visitor &visitor) {
            (void)fieldId;
            (void)visitor;
//            visitor.preVisit(ID(), ShortName(), LongName());
            if (1 == fieldId) {
                doVisit(1, std::move("int32_t"s), std::move("dataType"s), m_dataType, visitor);
                return;
            }
            if (2 == fieldId) {
                doVisit(2, std::move("std::string"s), std::move("serializedData"s), m_serializedData, visitor);
                return;
            }
            if (3 == fieldId) {
                doVisit(3, std::move("cluon::data::TimeStamp"s), std::move("sent"s), m_sent, visitor);
                return;
            }
            if (4 == fieldId) {
                doVisit(4, std::move("cluon::data::TimeStamp"s), std::move("received"s), m_received, visitor);
                return;
            }
            if (5 == fieldId) {
                doVisit(5, std::move("cluon::data::TimeStamp"s), std::move("sampleTimeStamp"s), m_sampleTimeStamp, visitor);
                return;
            }
            if (6 == fieldId) {
                doVisit(6, std::move("uint32_t"s), std::move("senderStamp"s), m_senderStamp, visitor);
                return;
            }
//            visitor.postVisit();
        }

        template<class Visitor>
        inline void accept(Visitor &visitor) {
            visitor.preVisit(ID(), ShortName(), LongName());
            doVisit(1, std::move("int32_t"s), std::move("dataType"s), m_dataType, visitor);
            doVisit(2, std::move("std::string"s), std::move("serializedData"s), m_serializedData, visitor);
            doVisit(3, std::move("cluon::data::TimeStamp"s), std::move("sent"s), m_sent, visitor);
            doVisit(4, std::move("cluon::data::TimeStamp"s), std::move("received"s), m_received, visitor);
            doVisit(5, std::move("cluon::data::TimeStamp"s), std::move("sampleTimeStamp"s), m_sampleTimeStamp, visitor);
            doVisit(6, std::move("uint32_t"s), std::move("senderStamp"s), m_senderStamp, visitor);
            visitor.postVisit();
        }

        template<class PreVisitor, class Visitor, class PostVisitor>
        inline void accept(PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
            (void)visit; // Prevent warnings from empty messages.
            std::forward<PreVisitor>(preVisit)(ID(), ShortName(), LongName());
            doTripletForwardVisit(1, std::move("int32_t"s), std::move("dataType"s), m_dataType, preVisit, visit, postVisit);
            doTripletForwardVisit(2, std::move("std::string"s), std::move("serializedData"s), m_serializedData, preVisit, visit, postVisit);
            doTripletForwardVisit(3, std::move("cluon::data::TimeStamp"s), std::move("sent"s), m_sent, preVisit, visit, postVisit);
            doTripletForwardVisit(4, std::move("cluon::data::TimeStamp"s), std::move("received"s), m_received, preVisit, visit, postVisit);
            doTripletForwardVisit(5, std::move("cluon::data::TimeStamp"s), std::move("sampleTimeStamp"s), m_sampleTimeStamp, preVisit, visit, postVisit);
            doTripletForwardVisit(6, std::move("uint32_t"s), std::move("senderStamp"s), m_senderStamp, preVisit, visit, postVisit);
            std::forward<PostVisitor>(postVisit)();
        }

    private:
        int32_t m_dataType{ 0 }; // field identifier = 1.
        std::string m_serializedData{ ""s }; // field identifier = 2.
        cluon::data::TimeStamp m_sent{  }; // field identifier = 3.
        cluon::data::TimeStamp m_received{  }; // field identifier = 4.
        cluon::data::TimeStamp m_sampleTimeStamp{  }; // field identifier = 5.
        uint32_t m_senderStamp{ 0 }; // field identifier = 6.
};
}}

template<>
struct isVisitable<cluon::data::Envelope> {
    static const bool value = true;
};
template<>
struct isTripletForwardVisitable<cluon::data::Envelope> {
    static const bool value = true;
};
#endif


/*
 * THIS IS AN AUTO-GENERATED FILE. DO NOT MODIFY AS CHANGES MIGHT BE OVERWRITTEN!
 */

#ifndef VISITABLE_TYPE_TRAIT
#define VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct visitorSelector {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct visitorSelector<true> {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<typename T>
struct isVisitable {
    static const bool value = false;
};

template<typename T, class Visitor>
void doVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
    visitorSelector<isVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, visitor);
}
#endif

#ifndef TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#define TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct tripletForwardVisitorSelector {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)preVisit;
        (void)postVisit;
        std::forward<Visitor>(visit)(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct tripletForwardVisitorSelector<true> {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)fieldIdentifier;
        (void)typeName;
        (void)name;
        // Apply preVisit, visit, and postVisit on value.
        value.accept(preVisit, visit, postVisit);
    }
};

template<typename T>
struct isTripletForwardVisitable {
    static const bool value = false;
};

template< typename T, class PreVisitor, class Visitor, class PostVisitor>
void doTripletForwardVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
    tripletForwardVisitorSelector<isTripletForwardVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, std::move(preVisit), std::move(visit), std::move(postVisit)); // NOLINT
}
#endif


#ifndef CLUON_DATA_PLAYERCOMMAND_HPP
#define CLUON_DATA_PLAYERCOMMAND_HPP

#ifdef WIN32
    // Export symbols if compile flags "LIB_SHARED" and "LIB_EXPORTS" are set on Windows.
    #ifdef LIB_SHARED
        #ifdef LIB_EXPORTS
            #define LIB_API __declspec(dllexport)
        #else
            #define LIB_API __declspec(dllimport)
        #endif
    #else
        // Disable definition if linking statically.
        #define LIB_API
    #endif
#else
    // Disable definition for non-Win32 systems.
    #define LIB_API
#endif

#include <string>
#include <utility>
namespace cluon { namespace data {
using namespace std::string_literals; // NOLINT
class LIB_API PlayerCommand {
    private:
        static constexpr const char* TheShortName = "PlayerCommand";
        static constexpr const char* TheLongName = "cluon.data.PlayerCommand";

    public:
        inline static int32_t ID() {
            return 9;
        }
        inline static const std::string ShortName() {
            return TheShortName;
        }
        inline static const std::string LongName() {
            return TheLongName;
        }

    public:
        PlayerCommand() = default;
        PlayerCommand(const PlayerCommand&) = default;
        PlayerCommand& operator=(const PlayerCommand&) = default;
        PlayerCommand(PlayerCommand&&) = default;
        PlayerCommand& operator=(PlayerCommand&&) = default;
        ~PlayerCommand() = default;

    public:
        inline PlayerCommand& command(const uint8_t &v) noexcept {
            m_command = v;
            return *this;
        }
        inline uint8_t command() const noexcept {
            return m_command;
        }
        inline PlayerCommand& seekTo(const float &v) noexcept {
            m_seekTo = v;
            return *this;
        }
        inline float seekTo() const noexcept {
            return m_seekTo;
        }

    public:
        template<class Visitor>
        inline void accept(uint32_t fieldId, Visitor &visitor) {
            (void)fieldId;
            (void)visitor;
//            visitor.preVisit(ID(), ShortName(), LongName());
            if (1 == fieldId) {
                doVisit(1, std::move("uint8_t"s), std::move("command"s), m_command, visitor);
                return;
            }
            if (2 == fieldId) {
                doVisit(2, std::move("float"s), std::move("seekTo"s), m_seekTo, visitor);
                return;
            }
//            visitor.postVisit();
        }

        template<class Visitor>
        inline void accept(Visitor &visitor) {
            visitor.preVisit(ID(), ShortName(), LongName());
            doVisit(1, std::move("uint8_t"s), std::move("command"s), m_command, visitor);
            doVisit(2, std::move("float"s), std::move("seekTo"s), m_seekTo, visitor);
            visitor.postVisit();
        }

        template<class PreVisitor, class Visitor, class PostVisitor>
        inline void accept(PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
            (void)visit; // Prevent warnings from empty messages.
            std::forward<PreVisitor>(preVisit)(ID(), ShortName(), LongName());
            doTripletForwardVisit(1, std::move("uint8_t"s), std::move("command"s), m_command, preVisit, visit, postVisit);
            doTripletForwardVisit(2, std::move("float"s), std::move("seekTo"s), m_seekTo, preVisit, visit, postVisit);
            std::forward<PostVisitor>(postVisit)();
        }

    private:
        uint8_t m_command{ 0 }; // field identifier = 1.
        float m_seekTo{ 0.0f }; // field identifier = 2.
};
}}

template<>
struct isVisitable<cluon::data::PlayerCommand> {
    static const bool value = true;
};
template<>
struct isTripletForwardVisitable<cluon::data::PlayerCommand> {
    static const bool value = true;
};
#endif


/*
 * THIS IS AN AUTO-GENERATED FILE. DO NOT MODIFY AS CHANGES MIGHT BE OVERWRITTEN!
 */

#ifndef VISITABLE_TYPE_TRAIT
#define VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct visitorSelector {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct visitorSelector<true> {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<typename T>
struct isVisitable {
    static const bool value = false;
};

template<typename T, class Visitor>
void doVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
    visitorSelector<isVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, visitor);
}
#endif

#ifndef TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#define TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct tripletForwardVisitorSelector {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)preVisit;
        (void)postVisit;
        std::forward<Visitor>(visit)(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct tripletForwardVisitorSelector<true> {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)fieldIdentifier;
        (void)typeName;
        (void)name;
        // Apply preVisit, visit, and postVisit on value.
        value.accept(preVisit, visit, postVisit);
    }
};

template<typename T>
struct isTripletForwardVisitable {
    static const bool value = false;
};

template< typename T, class PreVisitor, class Visitor, class PostVisitor>
void doTripletForwardVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
    tripletForwardVisitorSelector<isTripletForwardVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, std::move(preVisit), std::move(visit), std::move(postVisit)); // NOLINT
}
#endif


#ifndef CLUON_DATA_PLAYERSTATUS_HPP
#define CLUON_DATA_PLAYERSTATUS_HPP

#ifdef WIN32
    // Export symbols if compile flags "LIB_SHARED" and "LIB_EXPORTS" are set on Windows.
    #ifdef LIB_SHARED
        #ifdef LIB_EXPORTS
            #define LIB_API __declspec(dllexport)
        #else
            #define LIB_API __declspec(dllimport)
        #endif
    #else
        // Disable definition if linking statically.
        #define LIB_API
    #endif
#else
    // Disable definition for non-Win32 systems.
    #define LIB_API
#endif

#include <string>
#include <utility>
namespace cluon { namespace data {
using namespace std::string_literals; // NOLINT
class LIB_API PlayerStatus {
    private:
        static constexpr const char* TheShortName = "PlayerStatus";
        static constexpr const char* TheLongName = "cluon.data.PlayerStatus";

    public:
        inline static int32_t ID() {
            return 10;
        }
        inline static const std::string ShortName() {
            return TheShortName;
        }
        inline static const std::string LongName() {
            return TheLongName;
        }

    public:
        PlayerStatus() = default;
        PlayerStatus(const PlayerStatus&) = default;
        PlayerStatus& operator=(const PlayerStatus&) = default;
        PlayerStatus(PlayerStatus&&) = default;
        PlayerStatus& operator=(PlayerStatus&&) = default;
        ~PlayerStatus() = default;

    public:
        inline PlayerStatus& state(const uint8_t &v) noexcept {
            m_state = v;
            return *this;
        }
        inline uint8_t state() const noexcept {
            return m_state;
        }
        inline PlayerStatus& numberOfEntries(const uint32_t &v) noexcept {
            m_numberOfEntries = v;
            return *this;
        }
        inline uint32_t numberOfEntries() const noexcept {
            return m_numberOfEntries;
        }
        inline PlayerStatus& currentEntryForPlayback(const uint32_t &v) noexcept {
            m_currentEntryForPlayback = v;
            return *this;
        }
        inline uint32_t currentEntryForPlayback() const noexcept {
            return m_currentEntryForPlayback;
        }

    public:
        template<class Visitor>
        inline void accept(uint32_t fieldId, Visitor &visitor) {
            (void)fieldId;
            (void)visitor;
//            visitor.preVisit(ID(), ShortName(), LongName());
            if (1 == fieldId) {
                doVisit(1, std::move("uint8_t"s), std::move("state"s), m_state, visitor);
                return;
            }
            if (2 == fieldId) {
                doVisit(2, std::move("uint32_t"s), std::move("numberOfEntries"s), m_numberOfEntries, visitor);
                return;
            }
            if (3 == fieldId) {
                doVisit(3, std::move("uint32_t"s), std::move("currentEntryForPlayback"s), m_currentEntryForPlayback, visitor);
                return;
            }
//            visitor.postVisit();
        }

        template<class Visitor>
        inline void accept(Visitor &visitor) {
            visitor.preVisit(ID(), ShortName(), LongName());
            doVisit(1, std::move("uint8_t"s), std::move("state"s), m_state, visitor);
            doVisit(2, std::move("uint32_t"s), std::move("numberOfEntries"s), m_numberOfEntries, visitor);
            doVisit(3, std::move("uint32_t"s), std::move("currentEntryForPlayback"s), m_currentEntryForPlayback, visitor);
            visitor.postVisit();
        }

        template<class PreVisitor, class Visitor, class PostVisitor>
        inline void accept(PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
            (void)visit; // Prevent warnings from empty messages.
            std::forward<PreVisitor>(preVisit)(ID(), ShortName(), LongName());
            doTripletForwardVisit(1, std::move("uint8_t"s), std::move("state"s), m_state, preVisit, visit, postVisit);
            doTripletForwardVisit(2, std::move("uint32_t"s), std::move("numberOfEntries"s), m_numberOfEntries, preVisit, visit, postVisit);
            doTripletForwardVisit(3, std::move("uint32_t"s), std::move("currentEntryForPlayback"s), m_currentEntryForPlayback, preVisit, visit, postVisit);
            std::forward<PostVisitor>(postVisit)();
        }

    private:
        uint8_t m_state{ 0 }; // field identifier = 1.
        uint32_t m_numberOfEntries{ 0 }; // field identifier = 2.
        uint32_t m_currentEntryForPlayback{ 0 }; // field identifier = 3.
};
}}

template<>
struct isVisitable<cluon::data::PlayerStatus> {
    static const bool value = true;
};
template<>
struct isTripletForwardVisitable<cluon::data::PlayerStatus> {
    static const bool value = true;
};
#endif


/*
 * THIS IS AN AUTO-GENERATED FILE. DO NOT MODIFY AS CHANGES MIGHT BE OVERWRITTEN!
 */

#ifndef VISITABLE_TYPE_TRAIT
#define VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct visitorSelector {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct visitorSelector<true> {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<typename T>
struct isVisitable {
    static const bool value = false;
};

template<typename T, class Visitor>
void doVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
    visitorSelector<isVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, visitor);
}
#endif

#ifndef TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#define TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct tripletForwardVisitorSelector {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)preVisit;
        (void)postVisit;
        std::forward<Visitor>(visit)(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct tripletForwardVisitorSelector<true> {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)fieldIdentifier;
        (void)typeName;
        (void)name;
        // Apply preVisit, visit, and postVisit on value.
        value.accept(preVisit, visit, postVisit);
    }
};

template<typename T>
struct isTripletForwardVisitable {
    static const bool value = false;
};

template< typename T, class PreVisitor, class Visitor, class PostVisitor>
void doTripletForwardVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
    tripletForwardVisitorSelector<isTripletForwardVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, std::move(preVisit), std::move(visit), std::move(postVisit)); // NOLINT
}
#endif


#ifndef CLUON_DATA_RECORDERCOMMAND_HPP
#define CLUON_DATA_RECORDERCOMMAND_HPP

#ifdef WIN32
    // Export symbols if compile flags "LIB_SHARED" and "LIB_EXPORTS" are set on Windows.
    #ifdef LIB_SHARED
        #ifdef LIB_EXPORTS
            #define LIB_API __declspec(dllexport)
        #else
            #define LIB_API __declspec(dllimport)
        #endif
    #else
        // Disable definition if linking statically.
        #define LIB_API
    #endif
#else
    // Disable definition for non-Win32 systems.
    #define LIB_API
#endif

#include <string>
#include <utility>
namespace cluon { namespace data {
using namespace std::string_literals; // NOLINT
class LIB_API RecorderCommand {
    private:
        static constexpr const char* TheShortName = "RecorderCommand";
        static constexpr const char* TheLongName = "cluon.data.RecorderCommand";

    public:
        inline static int32_t ID() {
            return 11;
        }
        inline static const std::string ShortName() {
            return TheShortName;
        }
        inline static const std::string LongName() {
            return TheLongName;
        }

    public:
        RecorderCommand() = default;
        RecorderCommand(const RecorderCommand&) = default;
        RecorderCommand& operator=(const RecorderCommand&) = default;
        RecorderCommand(RecorderCommand&&) = default;
        RecorderCommand& operator=(RecorderCommand&&) = default;
        ~RecorderCommand() = default;

    public:
        inline RecorderCommand& command(const uint8_t &v) noexcept {
            m_command = v;
            return *this;
        }
        inline uint8_t command() const noexcept {
            return m_command;
        }

    public:
        template<class Visitor>
        inline void accept(uint32_t fieldId, Visitor &visitor) {
            (void)fieldId;
            (void)visitor;
//            visitor.preVisit(ID(), ShortName(), LongName());
            if (1 == fieldId) {
                doVisit(1, std::move("uint8_t"s), std::move("command"s), m_command, visitor);
                return;
            }
//            visitor.postVisit();
        }

        template<class Visitor>
        inline void accept(Visitor &visitor) {
            visitor.preVisit(ID(), ShortName(), LongName());
            doVisit(1, std::move("uint8_t"s), std::move("command"s), m_command, visitor);
            visitor.postVisit();
        }

        template<class PreVisitor, class Visitor, class PostVisitor>
        inline void accept(PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
            (void)visit; // Prevent warnings from empty messages.
            std::forward<PreVisitor>(preVisit)(ID(), ShortName(), LongName());
            doTripletForwardVisit(1, std::move("uint8_t"s), std::move("command"s), m_command, preVisit, visit, postVisit);
            std::forward<PostVisitor>(postVisit)();
        }

    private:
        uint8_t m_command{ 0 }; // field identifier = 1.
};
}}

template<>
struct isVisitable<cluon::data::RecorderCommand> {
    static const bool value = true;
};
template<>
struct isTripletForwardVisitable<cluon::data::RecorderCommand> {
    static const bool value = true;
};
#endif

/*
 * MIT License
 *
 * Copyright (c) 2018  Christian Berger
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef STRINGTOOLBOX_HPP
#define STRINGTOOLBOX_HPP

#include <algorithm>
#include <string>
#include <vector>

namespace stringtoolbox {

/**
 * @return std::string without trailing whitespace characters.
 */
inline std::string &rtrim(std::string &str) noexcept {
  str.erase(str.find_last_not_of(" \t") + 1);
  return str;
}

/**
 * @return std::tring without leading whitespace characters.
 */
inline std::string &ltrim(std::string &str) noexcept {
  str.erase(0, str.find_first_not_of(" \t"));
  return str;
}

/**
 * @return std:string without leading and trailing whitespace characters.
 */
inline std::string &trim(std::string &str) noexcept {
  return ltrim(rtrim(str));
}

/**
 * @return std:string where all occurrences of characters FROM are replaced with TO.
 */
inline std::string replaceAll(const std::string &str,
                              const char &FROM,
                              const char &TO) noexcept {
  std::string retVal{str};
  std::replace(retVal.begin(), retVal.end(), FROM, TO);
  return retVal;
}

/**
 * @return std::vector<std:string> where the given string is split along delimiter.
 */
inline std::vector<std::string> split(const std::string &str,
                                      const char &delimiter) noexcept {
  std::vector<std::string> retVal{};
  std::string::size_type prev{0};
  for (std::string::size_type i{str.find_first_of(delimiter, prev)};
       i != std::string::npos;
       prev = i + 1, i = str.find_first_of(delimiter, prev)) {
    if (i != prev) {
      retVal.emplace_back(str.substr(prev, i - prev));
    }
    else {
      retVal.emplace_back("");
    }
  }
  if ((prev > 0) && (prev < str.size())) {
    retVal.emplace_back(str.substr(prev, str.size() - prev));
  }
  else if (prev > 0) {
    retVal.emplace_back("");
  }
  return retVal;
}

} // namespace stringtoolbox

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TIME_HPP
#define CLUON_TIME_HPP

//#include "cluon/cluonDataStructures.hpp"

#include <chrono>

namespace cluon {
namespace time {

/**
 * @param tp to be converted to TimeStamp.
 * @return TimeStamp converted from microseconds.
 */
inline cluon::data::TimeStamp fromMicroseconds(int64_t tp) noexcept {
    cluon::data::TimeStamp ts;
    ts.seconds(static_cast<int32_t>(tp / static_cast<int64_t>(1000 * 1000))).microseconds(static_cast<int32_t>(tp % static_cast<int64_t>(1000 * 1000)));
    return ts;
}

/**
 * @param tp to be converted to microseconds.
 * @return TimeStamp converted to microseconds.
 */
inline int64_t toMicroseconds(const cluon::data::TimeStamp &tp) noexcept {
    return static_cast<int64_t>(tp.seconds()) * static_cast<int64_t>(1000 * 1000) + static_cast<int64_t>(tp.microseconds());
}

/**
 * @param AFTER First time stamp.
 * @param BEFORE Second time stamp.
 * @return Delta (AFTER - BEFORE) between two TimeStamps in microseconds.
 */
inline int64_t deltaInMicroseconds(const cluon::data::TimeStamp &AFTER, const cluon::data::TimeStamp &BEFORE) noexcept {
    return toMicroseconds(AFTER) - toMicroseconds(BEFORE);
}

/**
 * @param tp to be converted to microseconds.
 * @return TimeStamp of converted chrono::time_point.
 */
inline cluon::data::TimeStamp convert(const std::chrono::system_clock::time_point &tp) noexcept {
    cluon::data::TimeStamp timeStamp;

    // Transform chrono time representation to same behavior as gettimeofday.
    typedef std::chrono::duration<int32_t> seconds_type;
    typedef std::chrono::duration<int64_t, std::micro> microseconds_type;

    auto duration                = tp.time_since_epoch();
    seconds_type s               = std::chrono::duration_cast<seconds_type>(duration);
    microseconds_type us         = std::chrono::duration_cast<microseconds_type>(duration);
    microseconds_type partial_us = us - std::chrono::duration_cast<microseconds_type>(s);

    timeStamp.seconds(s.count()).microseconds(static_cast<int32_t>(partial_us.count()));

    return timeStamp;
}

/**
 * @return TimeStamp of now from std::chrono::system_clock.
 */
inline cluon::data::TimeStamp now() noexcept {
    return convert(std::chrono::system_clock::now());
}

} // namespace time
} // namespace cluon

#endif
// "License": Public Domain
// I, Mathias Panzenböck, place this file hereby into the public domain. Use it at your own risk for whatever you like.
// In case there are jurisdictions that don't support putting things in the public domain you can also consider it to
// be "dual licensed" under the BSD, MIT and Apache licenses, if you want to. This code is trivial anyway. Consider it
// an example on how to get the endian conversion functions on different platforms.

// Updated for FreeBSD 10.1+, DragonFly 4.2+, NetBSD 6.1.5+, fixes for Win32,
// and support for emscripten; Christian Berger.

#ifndef CLUON_PORTABLEENDIAN_HPP
#define CLUON_PORTABLEENDIAN_HPP

// clang-format off
#if defined(__linux__) || defined(__CYGWIN__)
    #include <endian.h>
#elif defined(__APPLE__)
    #include <libkern/OSByteOrder.h>
    #define htobe16(x) OSSwapHostToBigInt16(x)
    #define htole16(x) OSSwapHostToLittleInt16(x)
    #define be16toh(x) OSSwapBigToHostInt16(x)
    #define le16toh(x) OSSwapLittleToHostInt16(x)

    #define htobe32(x) OSSwapHostToBigInt32(x)
    #define htole32(x) OSSwapHostToLittleInt32(x)
    #define be32toh(x) OSSwapBigToHostInt32(x)
    #define le32toh(x) OSSwapLittleToHostInt32(x)

    #define htobe64(x) OSSwapHostToBigInt64(x)
    #define htole64(x) OSSwapHostToLittleInt64(x)
    #define be64toh(x) OSSwapBigToHostInt64(x)
    #define le64toh(x) OSSwapLittleToHostInt64(x)
#elif defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #include <sys/endian.h>
#elif (defined(_WIN16) || defined(_WIN32) || defined(_WIN64))
    #if BYTE_ORDER == LITTLE_ENDIAN
        // Add missing definitions for MinGW.
        #ifndef htonll
            #define htonll(x) ((1==htonl(1)) ? (x) : (((uint64_t)htonl((x) & 0xFFFFFFFFUL)) << 32) | htonl((uint32_t)((x) >> 32)))
        #endif
        #ifndef ntohll
            #define ntohll(x) ((1==ntohl(1)) ? (x) : (((uint64_t)ntohl((x) & 0xFFFFFFFFUL)) << 32) | ntohl((uint32_t)((x) >> 32)))
        #endif

        #define htobe16(x) htons(x)
        #define htole16(x) (x)
        #define be16toh(x) ntohs(x)
        #define le16toh(x) (x)

        #define htobe32(x) htonl(x)
        #define htole32(x) (x)
        #define be32toh(x) ntohl(x)
        #define le32toh(x) (x)

        #define htobe64(x) htonll(x)
        #define htole64(x) (x)
        #define be64toh(x) ntohll(x)
        #define le64toh(x) (x)
    #elif BYTE_ORDER == BIG_ENDIAN
        /* that would be xbox 360 */
        #define htobe16(x) (x)
        #define htole16(x) __builtin_bswap16(x)
        #define be16toh(x) (x)
        #define le16toh(x) __builtin_bswap16(x)

        #define htobe32(x) (x)
        #define htole32(x) __builtin_bswap32(x)
        #define be32toh(x) (x)
        #define le32toh(x) __builtin_bswap32(x)

        #define htobe64(x) (x)
        #define htole64(x) __builtin_bswap64(x)
        #define be64toh(x) (x)
        #define le64toh(x) __builtin_bswap64(x)
    #else
        #error byte order not supported
    #endif
#else
    #ifdef __EMSCRIPTEN__
        #include <endian.h>
    #else
        #warning platform not supported
    #endif
#endif
// clang-format on
#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_CLUON_HPP
#define CLUON_CLUON_HPP

// clang-format off
#ifdef WIN32
    #ifdef _WIN64
        #define ssize_t __int64
    #else
        #define ssize_t long
    #endif

    // Disable warning "'<': signed/unsigned mismatch".
    #pragma warning(disable : 4018)
    // Disable warning "Unary minus operator applied to unsigned type, result still unsigned".
    #pragma warning(disable : 4146)
    // Disable warning "Possible loss of precision".
    #pragma warning(disable : 4244)
    // Disable warning "Conversion from 'size_t' to 'type', possible loss of data".
    #pragma warning(disable : 4267)
    // Disable warning "'static_cast': truncation of constant value".
    #pragma warning(disable : 4309)
    // Disable warning "'operator ""s': literal suffix identifiers that do not start with an underscore are reserved".
    #pragma warning(disable : 4455)
    // Disable deprecated API warnings.
    #pragma warning(disable : 4996)

    // Link against ws2_32.lib for networking.
    #pragma comment(lib, "ws2_32.lib")
    // Link against iphlpapi.lib for address resolving.
    #pragma comment(lib, "iphlpapi.lib")

    // Avoid include definitions from Winsock v1.
    #define WIN32_LEAN_AND_MEAN

    // Export symbols.
    #ifdef LIBCLUON_SHARED
        #ifdef LIBCLUON_EXPORTS
            #define LIBCLUON_API __declspec(dllexport)
        #else
            #define LIBCLUON_API __declspec(dllimport)
        #endif
    #else
        // If linking statically:
        #define LIBCLUON_API
    #endif
#else
    // Undefine define for non-Win32 systems:
    #define LIBCLUON_API
#endif
// clang-format on

//#include "cluon/PortableEndian.hpp"

#include <map>
#include <string>

namespace cluon {

/**
This class can be used to define hash keys.
*/
class UseUInt32ValueAsHashKey {
   public:
    inline std::size_t operator()(const uint32_t v) const noexcept {
        return static_cast<std::size_t>(v);
    }
};

/**
 * @return Map for command line parameters passed as --key=value into key->values.
 */
std::map<std::string, std::string> getCommandlineArguments(int32_t argc, char **argv) noexcept;

} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_METAMESSAGE_HPP
#define CLUON_METAMESSAGE_HPP

//#include "cluon/cluon.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cluon {
/**
This class provides a generic description for a message. It is internally
used during the processing of message specifications to generate targets
like C++ or .proto files.
*/
class LIBCLUON_API MetaMessage {
   public:
    /**
     * This inner class provides a generic description for a message's fields.
     * It is internally used during the processing of message specifications.
     */
    class MetaField {
       public:
        enum MetaFieldDataTypes : uint16_t {
            BOOL_T      = 0,
            UINT8_T     = 2,
            INT8_T      = 3,
            UINT16_T    = 4,
            INT16_T     = 5,
            UINT32_T    = 6,
            INT32_T     = 7,
            UINT64_T    = 8,
            INT64_T     = 9,
            CHAR_T      = 11,
            FLOAT_T     = 13,
            DOUBLE_T    = 14,
            BYTES_T     = 49,
            STRING_T    = 51,
            MESSAGE_T   = 53,
            UNDEFINED_T = 0xFFFF,
        };

       private:
        MetaField &operator=(MetaField &&) = delete;

       public:
        MetaField()                  = default;
        MetaField(const MetaField &) = default;
        MetaField(MetaField &&)      = default;
        MetaField &operator=(const MetaField &) = default;

       public:
        /**
         * @return Type of this field.
         */
        MetaFieldDataTypes fieldDataType() const noexcept;
        /**
         * This method sets the type for this field.
         *
         * @param v Type for this field.
         * @return Reference to this instance.
         */
        MetaField &fieldDataType(const MetaFieldDataTypes &v) noexcept;

        /**
         * @return Type name of this field.
         */
        std::string fieldDataTypeName() const noexcept;
        /**
         * This method sets the type name for this field.
         *
         * @param v Type name for this field.
         * @return Reference to this instance.
         */
        MetaField &fieldDataTypeName(const std::string &v) noexcept;

        /**
         * @return Name of this field.
         */
        std::string fieldName() const noexcept;
        /**
         * This method sets the name for this field.
         *
         * @param v Name for this field.
         * @return Reference to this instance.
         */
        MetaField &fieldName(const std::string &v) noexcept;

        /**
         * @return Identifier of this field.
         */
        uint32_t fieldIdentifier() const noexcept;
        /**
         * This method sets the identifier for this field.
         *
         * @param v Identifier for this field.
         * @return Reference to this instance.
         */
        MetaField &fieldIdentifier(uint32_t v) noexcept;

        /**
         * @return Field's default initialization value.
         */
        std::string defaultInitializationValue() const noexcept;
        /**
         * This method sets the field's default initialization value for this field.
         *
         * @param v Field's default initialization value for this field.
         * @return Reference to this instance.
         */
        MetaField &defaultInitializationValue(const std::string &v) noexcept;

       private:
        MetaFieldDataTypes m_fieldDataType{UNDEFINED_T};
        std::string m_fieldDataTypeName{""};
        std::string m_fieldName{""};
        uint32_t m_fieldIdentifier{0};
        std::string m_defaultInitializationValue{""};
    };

   public:
    MetaMessage() noexcept;
    MetaMessage(const MetaMessage &) = default;
    MetaMessage(MetaMessage &&)      = default;
    MetaMessage &operator=(const MetaMessage &) = default;
    MetaMessage &operator=(MetaMessage &&) = default;

    /**
     * This method adds a metafield to this meta message.
     *
     * @param mf Meta field to be added.
     * @return Reference to this instance.
     */
    MetaMessage &add(MetaField &&mf) noexcept;

    /**
     * This method returns a vector of current meta fields.
     *
     * @return Meta fields from this meta message.
     */
    const std::vector<MetaField> &listOfMetaFields() const noexcept;

    /**
     * This method can be used to visit this instance and propagate information
     * details about the contained fields.
     *
     * @param visit std::function object to be called to visit this MetaMessage.
     */
    void accept(const std::function<void(const MetaMessage &)> &visit);

    /**
     * @return Package name.
     */
    std::string packageName() const noexcept;
    /**
     * This method sets the package name.
     *
     * @param v Package name for this message.
     * @return Reference to this instance.
     */
    MetaMessage &packageName(const std::string &v) noexcept;

    /**
     * @return Message name.
     */
    std::string messageName() const noexcept;
    /**
     * This method sets the message name.
     *
     * @param v Message name for this message.
     * @return Reference to this instance.
     */
    MetaMessage &messageName(const std::string &v) noexcept;

    /**
     * @return Message identifier.
     */
    int32_t messageIdentifier() const noexcept;
    /**
     * This method sets the message identifier.
     *
     * @param v Message identifier for this message.
     * @return Reference to this instance.
     */
    MetaMessage &messageIdentifier(int32_t v) noexcept;

   private:
    std::string m_packageName{""};
    std::string m_messageName{""};
    int32_t m_messageIdentifier{0};
    std::vector<MetaField> m_listOfMetaFields{};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_MESSAGEPARSER_HPP
#define CLUON_MESSAGEPARSER_HPP

//#include "cluon/MetaMessage.hpp"
//#include "cluon/cluon.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cluon {
/**
This class provides a parser for message specifications in .odvd format. The
format is inspired by Google Protobuf (https://developers.google.com/protocol-buffers/)
but simplified to enforce backwards and forwards compatibility next to
numerical message identifiers.

This message specification format is also used by OpenDaVINCI (http://code.opendavinci.org).

The parser is based on https://github.com/yhirose/cpp-peglib.

An example for a .odvd compliant message is demonstrated in the following:

\code{.cpp}
const char *spec = R"(
message myMessage.SubName [id = 1] {
    uint8 field1 [id = 1];
    uint32 field2 [id = 2];
    int64 field3 [id = 3];
    string field4 [id = 4];
}
)";

cluon::MessageParser mp;
auto retVal = mp.parse(std::string(spec));
if (retVal.second == cluon::MessageParser::MessageParserErrorCodes::NO_MESSAGEPARSER_ERROR) {
    auto listOfMessages = retVal.first;
    for (auto message : listOfMessages) {
        message.accept([](const cluon::MetaMessage &mm){ std::cout << "Message name = " << mm.messageName() <<
std::endl; });
    }
}
\endcode
*/
class LIBCLUON_API MessageParser {
   public:
    enum MessageParserErrorCodes : uint8_t { NO_MESSAGEPARSER_ERROR = 0, SYNTAX_ERROR = 1, DUPLICATE_IDENTIFIERS = 2 };

   private:
    MessageParser(const MessageParser &) = delete;
    MessageParser(MessageParser &&)      = delete;
    MessageParser &operator=(const MessageParser &) = delete;
    MessageParser &operator=(MessageParser &&) = delete;

   public:
    MessageParser() = default;

    /**
     * This method tries to parse the given message specification.
     *
     * @param input Message specification.
     * @return Pair: List of cluon::MetaMessages describing the specified messages and error code:
     *         NO_MESSAGEPARSER_ERROR: The given specification could be parsed successfully (list moght be non-empty).
     *         SYNTAX_ERROR: The given specification could not be parsed successfully (list is empty).
     *         DUPLICATE_IDENTIFIERS: The given specification contains ambiguous names or identifiers (list is empty).
     */
    std::pair<std::vector<MetaMessage>, MessageParserErrorCodes> parse(const std::string &input);
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TERMINATEHANDLER_HPP
#define CLUON_TERMINATEHANDLER_HPP

//#include "cluon/cluon.hpp"

#include <atomic>
#include <csignal>

namespace cluon {

class LIBCLUON_API TerminateHandler {
   private:
    TerminateHandler(const TerminateHandler &) = delete;
    TerminateHandler(TerminateHandler &&)      = delete;
    TerminateHandler &operator=(const TerminateHandler &) = delete;
    TerminateHandler &operator=(TerminateHandler &&) = delete;

   public:
    /**
     * Define singleton behavior using static initializer (cf. http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2011/n3242.pdf, Sec. 6.7.4).
     * @return singleton for an instance of this class.
     */
    static TerminateHandler &instance() noexcept {
        static TerminateHandler instance;
        return instance;
    }

    ~TerminateHandler() = default;

   public:
    std::atomic<bool> isTerminated{false};

   private:
    TerminateHandler() noexcept;

#ifndef WIN32
    struct sigaction m_signalHandler {};
#endif
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_NOTIFYINGPIPELINE_HPP
#define CLUON_NOTIFYINGPIPELINE_HPP

//#include "cluon/cluon.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace cluon {

template <class T>
class LIBCLUON_API NotifyingPipeline {
   private:
    NotifyingPipeline(const NotifyingPipeline &) = delete;
    NotifyingPipeline(NotifyingPipeline &&)      = delete;
    NotifyingPipeline &operator=(const NotifyingPipeline &) = delete;
    NotifyingPipeline &operator=(NotifyingPipeline &&) = delete;

   public:
    NotifyingPipeline(std::function<void(T &&)> delegate)
        : m_delegate(delegate) {
        m_pipelineThread = std::thread(&NotifyingPipeline::processPipeline, this);

        // Let the operating system spawn the thread.
        using namespace std::literals::chrono_literals; // NOLINT
        do { std::this_thread::sleep_for(1ms); } while (!m_pipelineThreadRunning.load());
    }

    ~NotifyingPipeline() {
        m_pipelineThreadRunning.store(false);

        // Wake any waiting threads.
        m_pipelineCondition.notify_all();

        // Joining the thread could fail.
        try {
            if (m_pipelineThread.joinable()) {
                m_pipelineThread.join();
            }
        } catch (...) {} // LCOV_EXCL_LINE
    }

   public:
    inline void add(T &&entry) noexcept {
        std::unique_lock<std::mutex> lck(m_pipelineMutex);
        m_pipeline.emplace_back(entry);
    }

    inline void notifyAll() noexcept { m_pipelineCondition.notify_all(); }

    inline bool isRunning() noexcept { return m_pipelineThreadRunning.load(); }

   private:
    inline void processPipeline() noexcept {
        // Indicate to caller that we are ready.
        m_pipelineThreadRunning.store(true);

        while (m_pipelineThreadRunning.load()) {
            std::unique_lock<std::mutex> lck(m_pipelineMutex);
            // Wait until the thread should stop or data is available.
            m_pipelineCondition.wait(lck, [this] { return (!this->m_pipelineThreadRunning.load() || !this->m_pipeline.empty()); });

            // The condition will automatically lock the mutex after waking up.
            // As we are locking per entry, we need to unlock the mutex first.
            lck.unlock();

            uint32_t entries{0};
            {
                lck.lock();
                entries = static_cast<uint32_t>(m_pipeline.size());
                lck.unlock();
            }
            for (uint32_t i{0}; i < entries; i++) {
                T entry;
                {
                    lck.lock();
                    entry = m_pipeline.front();
                    lck.unlock();
                }

                if (nullptr != m_delegate) {
                    m_delegate(std::move(entry));
                }

                {
                    lck.lock();
                    m_pipeline.pop_front();
                    lck.unlock();
                }
            }
        }
    }

   private:
    std::function<void(T &&)> m_delegate;

    std::atomic<bool> m_pipelineThreadRunning{false};
    std::thread m_pipelineThread{};
    std::mutex m_pipelineMutex{};
    std::condition_variable m_pipelineCondition{};

    std::deque<T> m_pipeline{};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2019  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_IPV4TOOLS_HPP
#define CLUON_IPV4TOOLS_HPP

#include <string>

namespace cluon {

/**
 * @return IPv4-formatted string for the given hostname or the empty string.
 */
std::string getIPv4FromHostname(const std::string &hostname) noexcept;

} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_UDPPACKETSIZECONSTRAINTS_H
#define CLUON_UDPPACKETSIZECONSTRAINTS_H

#include <cstdint>

// clang-format off
namespace cluon {
    enum class UDPPacketSizeConstraints : uint16_t {
        SIZE_IPv4_HEADER    = 20,
        SIZE_UDP_HEADER     = 8,
        MAX_SIZE_UDP_PACKET = 0xFFFF, };
}
// clang-format on

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_UDPSENDER_HPP
#define CLUON_UDPSENDER_HPP

//#include "cluon/cluon.hpp"

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for WSAStartUp
    #include <ws2tcpip.h> // for SOCKET
#else
    #include <netinet/in.h>
#endif
// clang-format on

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

namespace cluon {
/**
To send data using a UDP socket, simply include the header
`#include <cluon/UDPSender.hpp>`.

Next, create an instance of class `cluon::UDPSender` as follows:
`cluon::UDPSender sender("127.0.0.1", 1234);`. The first parameter is of type
`std::string` expecting a numerical IPv4 address and the second parameter
specifies the UDP port to which the data shall be sent to.

To finally send data, simply call the method `send` supplying the data to be
sent: `sender.send(std::move("Hello World!")`. Please note that the data is
supplied using the _move_-semantics. The method `send` returns a
`std::pair<ssize_t, int32_t>` where the first element returns the size of the
successfully sent bytes and the second element contains the error code in case
the transmission of the data failed.

\code{.cpp}
cluon::UDPSender sender("127.0.0.1", 1234);

std::pair<ssize_t, int32_t> retVal = sender.send(std::move("Hello World!"));

std::cout << "Send " << retVal.first << " bytes, error code = " << retVal.second << std::endl;
\endcode

A complete example is available
[here](https://github.com/chrberger/libcluon/blob/master/libcluon/examples/cluon-UDPSender.cpp).
*/
class LIBCLUON_API UDPSender {
   private:
    UDPSender(const UDPSender &) = delete;
    UDPSender(UDPSender &&)      = delete;
    UDPSender &operator=(const UDPSender &) = delete;
    UDPSender &operator=(UDPSender &&) = delete;

   public:
    /**
     * Constructor.
     *
     * @param sendToAddress Numerical IPv4 address to send a UDP packet to.
     * @param sendToPort Port to send a UDP packet to.
     */
    UDPSender(const std::string &sendToAddress, uint16_t sendToPort) noexcept;
    ~UDPSender() noexcept;

    /**
     * Send a given string.
     *
     * @param data Data to send.
     * @return Pair: Number of bytes sent and errno.
     */
    std::pair<ssize_t, int32_t> send(std::string &&data) const noexcept;

   public:
    /**
     * @return Port that this UDP sender will use for sending or 0 if no information available.
     */
    uint16_t getSendFromPort() const noexcept;

   private:
    mutable std::mutex m_socketMutex{};
    int32_t m_socket{-1};
    uint16_t m_portToSentFrom{0};
    struct sockaddr_in m_sendToAddress {};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_UDPRECEIVER_HPP
#define CLUON_UDPRECEIVER_HPP

//#include "cluon/NotifyingPipeline.hpp"
//#include "cluon/cluon.hpp"

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for WSAStartUp
    #include <ws2tcpip.h> // for SOCKET
#else
    #include <netinet/in.h>
#endif
// clang-format on

#include <cstdint>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace cluon {
/**
To receive data from a UDP socket, simply include the header
`#include <cluon/UDPReceiver.hpp>`.

Next, create an instance of class `cluon::UDPReceiver` as follows:
`cluon::UDPReceiver receiver("127.0.0.1", 1234, delegate);`.
The first parameter is of type `std::string` expecting a numerical IPv4 address,
the second parameter specifies the UDP port, from which data shall be received
from, and the last parameter is of type `std::function` that is called whenever
new bytes are available to be processed.

The complete signature for the delegate function is
`std::function<void(std::string &&, std::string &&, std::chrono::system_clock::time_point &&) noexcept>`:
The first parameter contains the bytes that have been received, the second
parameter containes the human-readable representation of the sender
(X.Y.Z.W:ABCD), and the last parameter is the time stamp when the data has been
received. An example using a C++ lambda expression would look as follows:

\code{.cpp}
cluon::UDPReceiver receiver("127.0.0.1", 1234,
    [](std::string &&data, std::string &&sender, std::chrono::system_clock::time_point &&ts) noexcept {
        const auto timestamp(std::chrono::system_clock::to_time_t(ts));
        std::cout << "Received " << data.size() << " bytes"
                  << " from " << sender
                  << " at " << std::put_time(std::localtime(&timestamp), "%Y-%m-%d %X")
                  << ", containing '" << data << "'." << std::endl;
    });
\endcode

After creating an instance of class `cluon::UDPReceiver`, it is immediately
activated and concurrently waiting for data in a separate thread. To check
whether the instance was created successfully and running, the method
`isRunning()` should be called.

A complete example is available
[here](https://github.com/chrberger/libcluon/blob/master/libcluon/examples/cluon-UDPReceiver.cpp).
*/
class LIBCLUON_API UDPReceiver {
   private:
    UDPReceiver(const UDPReceiver &) = delete;
    UDPReceiver(UDPReceiver &&)      = delete;
    UDPReceiver &operator=(const UDPReceiver &) = delete;
    UDPReceiver &operator=(UDPReceiver &&) = delete;

   public:
    /**
     * Constructor.
     *
     * @param receiveFromAddress Numerical IPv4 address to receive UDP packets from.
     * @param receiveFromPort Port to receive UDP packets from.
     * @param delegate Functional (noexcept) to handle received bytes; parameters are received data, sender, timestamp.
     * @param localSendFromPort Port that an application is using to send data. This port (> 0) is ignored when data is received.
     */
    UDPReceiver(const std::string &receiveFromAddress,
                uint16_t receiveFromPort,
                std::function<void(std::string &&, std::string &&, std::chrono::system_clock::time_point &&)> delegate,
                uint16_t localSendFromPort = 0) noexcept;
    ~UDPReceiver() noexcept;

    /**
     * @return true if the UDPReceiver could successfully be created and is able to receive data.
     */
    bool isRunning() const noexcept;

   private:
    /**
     * This method closes the socket.
     *
     * @param errorCode Error code that caused this closing.
     */
    void closeSocket(int errorCode) noexcept;

    void readFromSocket() noexcept;

   private:
    int32_t m_socket{-1};
    bool m_isBlockingSocket{true};
    std::set<unsigned long> m_listOfLocalIPAddresses{};
    uint16_t m_localSendFromPort;
    struct sockaddr_in m_receiveFromAddress {};
    struct ip_mreq m_mreq {};
    bool m_isMulticast{false};

    std::atomic<bool> m_readFromSocketThreadRunning{false};
    std::thread m_readFromSocketThread{};

   private:
    std::function<void(std::string &&, std::string &&, std::chrono::system_clock::time_point)> m_delegate{};

   private:
    class PipelineEntry {
       public:
        std::string m_data;
        std::string m_from;
        std::chrono::system_clock::time_point m_sampleTime;
    };

    std::shared_ptr<cluon::NotifyingPipeline<PipelineEntry>> m_pipeline{};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TCPCONNECTION_HPP
#define CLUON_TCPCONNECTION_HPP

//#include "cluon/NotifyingPipeline.hpp"
//#include "cluon/cluon.hpp"

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for WSAStartUp
    #include <ws2tcpip.h> // for SOCKET
#else
    #include <netinet/in.h>
#endif
// clang-format on

#include <cstdint>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace cluon {
/**
To exchange data via TCP, simply include the header
`#include <cluon/TCPConnection.hpp>`.

Next, create an instance of class `cluon::TCPConnection` as follows:
`cluon::TCPConnection connection("127.0.0.1", 1234, newDataDelegate, connectionLostDelegate);`.
The first parameter is of type `std::string` expecting a numerical IPv4 address,
the second parameter specifies the TCP port, from which data shall be received
from, the third paraemter is of type `std::function` that is called whenever
new bytes are available to be processed, and the last parameter is of type
`std::function` that is called when the connection is lost.

The complete signature for the newDataDelegate function is
`std::function<void(std::string &&, std::string &&, std::chrono::system_clock::time_point &&) noexcept>`:
The first parameter contains the bytes that have been received, the second
parameter containes the human-readable representation of the sender
(X.Y.Z.W:ABCD), and the last parameter is the time stamp when the data has been
received.

The complete signature for the connectionLostDelegate function is
`std::function<void() noexcept>`.

To finally send data, simply call the method `send` supplying the data to be
sent: `connection.send(std::move("Hello World!")`. Please note that the data is
supplied using the _move_-semantics. The method `send` returns a
`std::pair<ssize_t, int32_t>` where the first element returns the size of the
successfully sent bytes and the second element contains the error code in case
the transmission of the data failed.

An example using a C++ lambda expression would look as follows:

\code{.cpp}
cluon::TCPConnection connection("127.0.0.1", 1234,
    [](std::string &&data, std::string &&sender, std::chrono::system_clock::time_point &&ts) noexcept {
        const auto timestamp(std::chrono::system_clock::to_time_t(ts));
        std::cout << "Received " << data.size() << " bytes"
                  << " from " << sender
                  << " at " << std::put_time(std::localtime(&timestamp), "%Y-%m-%d %X")
                  << ", containing '" << data << "'." << std::endl;
    },
    [](){ std::cout << "Connection lost." << std::endl; });

std::pair<ssize_t, int32_t> retVal = connection.send(std::move("Hello World!"));
\endcode

After creating an instance of class `cluon::TCPConnection`, it is immediately
activated and concurrently waiting for data in a separate thread. To check
whether the instance was created successfully and running, the method
`isRunning()` should be called.
*/
class LIBCLUON_API TCPConnection {
   private:
    friend class TCPServer;

    /**
     * Constructor that is only accessible to TCPServer to manage incoming TCP connections.
     *
     * @param socket Socket to handle an existing TCP connection described by this socket.
     */
    TCPConnection(const int32_t &socket) noexcept;

   private:
    TCPConnection(const TCPConnection &) = delete;
    TCPConnection(TCPConnection &&)      = delete;
    TCPConnection &operator=(const TCPConnection &) = delete;
    TCPConnection &operator=(TCPConnection &&) = delete;

   public:
    /**
     * Constructor to connect to a TCP server.
     *
     * @param address Numerical IPv4 address to receive UDP packets from.
     * @param port Port to receive UDP packets from.
     * @param newDataDelegate Functional (noexcept) to handle received bytes; parameters are received data, timestamp.
     * @param connectionLostDelegate Functional (noexcept) to handle a lost connection.
     */
    TCPConnection(const std::string &address,
                  uint16_t port,
                  std::function<void(std::string &&, std::chrono::system_clock::time_point &&)> newDataDelegate = nullptr,
                  std::function<void()> connectionLostDelegate                                                  = nullptr) noexcept;

    ~TCPConnection() noexcept;

   public:
    void setOnNewData(std::function<void(std::string &&, std::chrono::system_clock::time_point &&)> newDataDelegate) noexcept;
    void setOnConnectionLost(std::function<void()> connectionLostDelegate) noexcept;

   public:
    /**
     * @return true if the TCPConnection could successfully be created and is able to receive data.
     */
    bool isRunning() const noexcept;

    /**
     * Send a given string.
     *
     * @param data Data to send.
     * @return Pair: Number of bytes sent and errno.
     */
    std::pair<ssize_t, int32_t> send(std::string &&data) const noexcept;

   private:
    /**
     * This method closes the socket.
     *
     * @param errorCode Error code that caused this closing.
     */
    void closeSocket(int errorCode) noexcept;
    void startReadingFromSocket() noexcept;
    void readFromSocket() noexcept;

   private:
    mutable std::mutex m_socketMutex{};
    int32_t m_socket{-1};
    struct sockaddr_in m_address {};

    std::atomic<bool> m_readFromSocketThreadRunning{false};
    std::thread m_readFromSocketThread{};

    std::mutex m_newDataDelegateMutex{};
    std::function<void(std::string &&, std::chrono::system_clock::time_point)> m_newDataDelegate{};

    mutable std::mutex m_connectionLostDelegateMutex{};
    std::function<void()> m_connectionLostDelegate{};

   private:
    class PipelineEntry {
       public:
        std::string m_data;
        std::chrono::system_clock::time_point m_sampleTime;
    };

    std::shared_ptr<cluon::NotifyingPipeline<PipelineEntry>> m_pipeline{};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TCPSERVER_HPP
#define CLUON_TCPSERVER_HPP

//#include "cluon/TCPConnection.hpp"
//#include "cluon/cluon.hpp"

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for WSAStartUp
    #include <ws2tcpip.h> // for SOCKET
#else
    #include <netinet/in.h>
#endif
// clang-format on

#include <cstdint>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace cluon {

class LIBCLUON_API TCPServer {
   private:
    TCPServer(const TCPServer &) = delete;
    TCPServer(TCPServer &&)      = delete;
    TCPServer &operator=(const TCPServer &) = delete;
    TCPServer &operator=(TCPServer &&) = delete;

   public:
    /**
     * Constructor to create a TCP server.
     *
     * @param port Port to receive UDP packets from.
     * @param newConnectionDelegate Functional to handle incoming TCP connections.
     */
    TCPServer(uint16_t port, std::function<void(std::string &&from, std::shared_ptr<cluon::TCPConnection> connection)> newConnectionDelegate) noexcept;

    ~TCPServer() noexcept;

    /**
     * @return true if the TCPServer could successfully be created and is able to receive data.
     */
    bool isRunning() const noexcept;

   private:
    /**
     * This method closes the socket.
     *
     * @param errorCode Error code that caused this closing.
     */
    void closeSocket(int errorCode) noexcept;
    void readFromSocket() noexcept;

   private:
    mutable std::mutex m_socketMutex{};
    int32_t m_socket{-1};

    std::atomic<bool> m_readFromSocketThreadRunning{false};
    std::thread m_readFromSocketThread{};

    std::mutex m_newConnectionDelegateMutex{};
    std::function<void(std::string &&from, std::shared_ptr<cluon::TCPConnection> connection)> m_newConnectionDelegate{};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_PROTOCONSTANTS_HPP
#define CLUON_PROTOCONSTANTS_HPP

#include <cstdint>

// clang-format off
namespace cluon {
    enum class ProtoConstants : uint8_t {
        VARINT           = 0,
        EIGHT_BYTES      = 1,
        LENGTH_DELIMITED = 2,
        FOUR_BYTES       = 5, };
}
// clang-format on

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TOPROTOVISITOR_HPP
#define CLUON_TOPROTOVISITOR_HPP

//#include "cluon/ProtoConstants.hpp"
//#include "cluon/cluon.hpp"

#include <cstdint>
#include <sstream>
#include <string>

namespace cluon {
/**
This class encodes a given message in Proto format.
*/
class LIBCLUON_API ToProtoVisitor {
   private:
    ToProtoVisitor(const ToProtoVisitor &) = delete;
    ToProtoVisitor(ToProtoVisitor &&)      = delete;
    ToProtoVisitor &operator=(const ToProtoVisitor &) = delete;
    ToProtoVisitor &operator=(ToProtoVisitor &&) = delete;

   public:
    ToProtoVisitor()  = default;
    ~ToProtoVisitor() = default;

    /**
     * @return Encoded data in Proto format.
     */
    std::string encodedData() const noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)typeName;
        (void)name;

        toVarInt(m_buffer, std::move(encodeKey(id, static_cast<uint8_t>(ProtoConstants::LENGTH_DELIMITED))));
        cluon::ToProtoVisitor nestedProtoEncoder;
        value.accept(nestedProtoEncoder);
        encode(m_buffer, std::move(nestedProtoEncoder.encodedData()));
    }

   private:
    std::size_t encode(std::ostream &o, bool &v) noexcept;
    std::size_t encode(std::ostream &o, int8_t &v) noexcept;
    std::size_t encode(std::ostream &o, uint8_t &v) noexcept;
    std::size_t encode(std::ostream &o, int16_t &v) noexcept;
    std::size_t encode(std::ostream &o, uint16_t &v) noexcept;
    std::size_t encode(std::ostream &o, int32_t &v) noexcept;
    std::size_t encode(std::ostream &o, uint32_t &v) noexcept;
    std::size_t encode(std::ostream &o, int64_t &v) noexcept;
    std::size_t encode(std::ostream &o, uint64_t &v) noexcept;
    std::size_t encode(std::ostream &o, float &v) noexcept;
    std::size_t encode(std::ostream &o, double &v) noexcept;
    std::size_t encode(std::ostream &o, const std::string &v) noexcept;

   private:
    uint8_t toZigZag8(int8_t v) noexcept;
    uint16_t toZigZag16(int16_t v) noexcept;
    uint32_t toZigZag32(int32_t v) noexcept;
    uint64_t toZigZag64(int64_t v) noexcept;

    /**
     * This method encodes a given value in VarInt.
     *
     * @param out std::ostream to encode to.
     * @param v Value to encode.
     * @return Bytes written.
     */
    std::size_t toVarInt(std::ostream &out, uint64_t v) noexcept;

    /**
     * This method creates a key/value pair encoded in Proto format.
     *
     * @param fieldIdentifier Field identifier.
     * @param v Value to encode.
     * @return Bytes written.
     */
    template <typename T>
    std::size_t toKeyValue(uint32_t fieldIdentifier, T &v) noexcept {
        std::size_t size{0};
        uint64_t key = encodeKey(fieldIdentifier, static_cast<uint8_t>(ProtoConstants::VARINT));
        size += toVarInt(m_buffer, key);
        size += encode(m_buffer, v);
        return size;
    }

    /**
     * This method creates the key for the key/value pair in Protobuf format.
     *
     * @param fieldIdentifier Field identifier.
     * @param protoType Protobuf type identifier.
     * @return Protobuf fieldIdentifier/key pair.
     */
    uint64_t encodeKey(uint32_t fieldIdentifier, uint8_t protoType) noexcept;

   private:
    std::stringstream m_buffer{""};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_FROMPROTOVISITOR_HPP
#define CLUON_FROMPROTOVISITOR_HPP

//#include "cluon/ProtoConstants.hpp"
//#include "cluon/cluon.hpp"
//#include "cluon/any/any.hpp"

#include <cstdint>
#include <cstddef>
#include <array>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace cluon {
/**
This class decodes a given message from Proto format.
*/
class LIBCLUON_API FromProtoVisitor {
   private:
    FromProtoVisitor(const FromProtoVisitor &) = delete;
    FromProtoVisitor(FromProtoVisitor &&)      = delete;
    FromProtoVisitor &operator=(FromProtoVisitor &&) = delete;

   public:
    FromProtoVisitor()  = default;
    ~FromProtoVisitor() = default;

   public:
    FromProtoVisitor &operator=(const FromProtoVisitor &other) noexcept;

    /**
     * This method decodes a given istream into Proto.
     *
     * @param in istream to decode.
     */
    void decodeFrom(std::istream &in) noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &v) noexcept {
        (void)typeName;
        (void)name;

        if (m_callToDecodeFromWithDirectVisit) {
            std::stringstream sstr{std::move(std::string(m_stringValue.data(), static_cast<std::size_t>(m_value)))};
            cluon::FromProtoVisitor nestedProtoDecoder;
            nestedProtoDecoder.decodeFrom(sstr, v);
        }
        else if (0 < m_mapOfKeyValues.count(id)) {
            try {
                std::stringstream sstr{linb::any_cast<std::string>(m_mapOfKeyValues[id])};
                cluon::FromProtoVisitor nestedProtoDecoder;
                nestedProtoDecoder.decodeFrom(sstr);
                v.accept(nestedProtoDecoder);
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }

   public:
    /**
     * This method decodes a given istream into corresponding fields of v.
     *
     * @param in istream to decode.
     * @param v Data structure to receive the decoded values.
     */
    template<typename T>
    void decodeFrom(std::istream &in, T &v) noexcept {
        m_callToDecodeFromWithDirectVisit = true;
        while (in.good()) {
            // First stage: Read keyFieldType (encoded as VarInt).
            if (0 < fromVarInt(in, m_keyFieldType)) {
                // Succeeded to read keyFieldType entry; extract information.
                m_protoType = static_cast<ProtoConstants>(m_keyFieldType & 0x7);
                m_fieldId = static_cast<uint32_t>(m_keyFieldType >> 3);
                switch (m_protoType) {
                    case ProtoConstants::VARINT:
                    {
                        // Directly decode VarInt value.
                        fromVarInt(in, m_value);
                        v.accept(m_fieldId, *this);
                    }
                    break;
                    case ProtoConstants::EIGHT_BYTES:
                    {
                        readBytesFromStream(in, sizeof(double), m_doubleValue.buffer.data());
                        m_doubleValue.uint64Value = le64toh(m_doubleValue.uint64Value);
                        v.accept(m_fieldId, *this);
                    }
                    break;
                    case ProtoConstants::FOUR_BYTES:
                    {
                        readBytesFromStream(in, sizeof(float), m_floatValue.buffer.data());
                        m_floatValue.uint32Value = le32toh(m_floatValue.uint32Value);
                        v.accept(m_fieldId, *this);
                    }
                    break;
                    case ProtoConstants::LENGTH_DELIMITED:
                    {
                        fromVarInt(in, m_value);
                        const std::size_t BYTES_TO_READ_FROM_STREAM{static_cast<std::size_t>(m_value)};
                        if (m_stringValue.capacity() < BYTES_TO_READ_FROM_STREAM) {
                            m_stringValue.reserve(BYTES_TO_READ_FROM_STREAM);
                        }
                        readBytesFromStream(in, BYTES_TO_READ_FROM_STREAM, m_stringValue.data());
                        v.accept(m_fieldId, *this);
                    }
                    break;
                }
            }
        }
        m_callToDecodeFromWithDirectVisit = false;
    }

   private:
    int8_t fromZigZag8(uint8_t v) noexcept;
    int16_t fromZigZag16(uint16_t v) noexcept;
    int32_t fromZigZag32(uint32_t v) noexcept;
    int64_t fromZigZag64(uint64_t v) noexcept;

    std::size_t fromVarInt(std::istream &in, uint64_t &value) noexcept;

    void readBytesFromStream(std::istream &in, std::size_t bytesToReadFromStream, char *buffer) noexcept;

   private:
    // This Boolean flag indicates whether we consecutively decode from istream
    // and inject the decoded values directly into the receiving data structure.
    bool m_callToDecodeFromWithDirectVisit{false};
    std::unordered_map<uint32_t, linb::any, UseUInt32ValueAsHashKey> m_mapOfKeyValues{};

   private:
    // Fields necessary to decode from an istream.
    uint64_t m_value{0};

    // Union buffer for double values.
    union DoubleValue {
        std::array<char, sizeof(double)> buffer;
        uint64_t uint64Value;
        double doubleValue{0};
    } m_doubleValue;

    // Union buffer for float values.
    union FloatValue {
        std::array<char, sizeof(float)> buffer;
        uint32_t uint32Value;
        float floatValue{0};
    } m_floatValue;

    // Buffer for strings.
    std::vector<char> m_stringValue;

    uint64_t m_keyFieldType{0};
    ProtoConstants m_protoType{ProtoConstants::VARINT};
    uint32_t m_fieldId{0};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_FROMLCMVISITOR_HPP
#define CLUON_FROMLCMVISITOR_HPP

//#include "cluon/cluon.hpp"

#include <cstdint>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace cluon {
/**
This class decodes a given message from LCM format.
*/
class LIBCLUON_API FromLCMVisitor {
   private:
    FromLCMVisitor(std::stringstream &in) noexcept;
    FromLCMVisitor(const FromLCMVisitor &) = delete;
    FromLCMVisitor(FromLCMVisitor &&)      = delete;
    FromLCMVisitor &operator=(const FromLCMVisitor &) = delete;
    FromLCMVisitor &operator=(FromLCMVisitor &&) = delete;

   public:
    FromLCMVisitor() noexcept;
    ~FromLCMVisitor() = default;

   public:
    /**
     * This method decodes a given istream into LCM.
     *
     * @param in istream to decode.
     */
    void decodeFrom(std::istream &in) noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)id;
        (void)typeName;
        // No hash for the type but for name and dimension.
        calculateHash(name);
        calculateHash(0);

        cluon::FromLCMVisitor nestedLCMDecoder(m_buffer);
        value.accept(nestedLCMDecoder);

        m_hashes.push_back(nestedLCMDecoder.hash());
    }

   private:
    int64_t hash() const noexcept;
    void calculateHash(char c) noexcept;
    void calculateHash(const std::string &s) noexcept;

   private:
    int64_t m_calculatedHash{0x12345678};
    int64_t m_expectedHash{0};
    std::stringstream m_internalBuffer{""};
    std::stringstream &m_buffer;
    std::vector<int64_t> m_hashes{};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_MSGPACKCONSTANTS_HPP
#define CLUON_MSGPACKCONSTANTS_HPP

#include <cstdint>

// clang-format off
namespace cluon {
    enum class MsgPackConstants : uint16_t {
        IS_FALSE        = 0xC2,
        IS_TRUE         = 0xC3,
        FLOAT           = 0xCA,
        DOUBLE          = 0xCB,
        UINT8           = 0xCC,
        UINT16          = 0xCD,
        UINT32          = 0xCE,
        UINT64          = 0xCF,
        NEGFIXINT       = 0xE0,
        INT8            = 0xD0,
        INT16           = 0xD1,
        INT32           = 0xD2,
        INT64           = 0xD3,
        FIXSTR          = 0xA0,
        FIXSTR_END      = 0xBF,
        STR8            = 0xD9,
        STR16           = 0xDA,
        STR32           = 0xDB,
        FIXMAP          = 0x80,
        FIXMAP_END      = 0x8F,
        MAP16           = 0xDE,
        MAP32           = 0xDF,
        UNKNOWN_FORMAT  = 0xFF00,
        BOOL_FORMAT     = 0xFF01,
        UINT_FORMAT     = 0xFF02,
        INT_FORMAT      = 0xFF03,
        FLOAT_FORMAT    = 0xFF04,
        STR_FORMAT      = 0xFF05,
        MAP_FORMAT      = 0xFF06, // Indicating also nested types.
   };
}
// clang-format on

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_FROMMSGPACKVISITOR_HPP
#define CLUON_FROMMSGPACKVISITOR_HPP

//#include "cluon/MsgPackConstants.hpp"
//#include "cluon/any/any.hpp"
//#include "cluon/cluon.hpp"

#include <cstdint>
#include <istream>
#include <map>
#include <string>

namespace cluon {
/**
This class decodes a given message from MsgPack format.
*/
class LIBCLUON_API FromMsgPackVisitor {
    /**
     * This class represents a key/value in a MsgPack payload stream of key/values.
     */
    class MsgPackKeyValue {
       private:
        MsgPackKeyValue &operator=(MsgPackKeyValue &&) = delete;

       public:
        MsgPackKeyValue()                        = default;
        MsgPackKeyValue(const MsgPackKeyValue &) = default;
        MsgPackKeyValue(MsgPackKeyValue &&)      = default;
        MsgPackKeyValue &operator=(const MsgPackKeyValue &) = default;
        ~MsgPackKeyValue()                                  = default;

       public:
        std::string m_key{""};
        MsgPackConstants m_formatFamily{MsgPackConstants::BOOL_FORMAT};
        linb::any m_value;
    };

   private:
    FromMsgPackVisitor(const FromMsgPackVisitor &) = delete;
    FromMsgPackVisitor(FromMsgPackVisitor &&)      = delete;
    FromMsgPackVisitor &operator=(FromMsgPackVisitor &&) = delete;
    FromMsgPackVisitor &operator=(const FromMsgPackVisitor &other) = delete;

    /**
     * Internal constructor to pass reference to preset key/values.
     *
     * @param preset Pre-filled key/value map to handle nested fields.
     */
    FromMsgPackVisitor(std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue> &preset) noexcept;

   public:
    FromMsgPackVisitor() noexcept;
    ~FromMsgPackVisitor() = default;

   public:
    /**
     * This method decodes a given istream into an internal key/value representation.
     *
     * @param in istream to decode.
     */
    void decodeFrom(std::istream &in) noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)id;
        (void)typeName;

        if (0 < m_keyValues.count(name)) {
            try {
                std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue> v
                    = linb::any_cast<std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue>>(m_keyValues[name].m_value);
                cluon::FromMsgPackVisitor nestedMsgPackDecoder(v);
                value.accept(nestedMsgPackDecoder);
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }

   private:
    MsgPackConstants getFormatFamily(uint8_t T) noexcept;
    std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue> readKeyValues(std::istream &in) noexcept;
    uint64_t readUint(std::istream &in) noexcept;
    int64_t readInt(std::istream &in) noexcept;
    std::string readString(std::istream &in) noexcept;

   private:
    std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue> m_data{};
    std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue> &m_keyValues;
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_JSONCONSTANTS_HPP
#define CLUON_JSONCONSTANTS_HPP

#include <cstdint>

// clang-format off
namespace cluon {
    enum class JSONConstants : uint16_t {
        IS_FALSE    = 0,
        IS_TRUE     = 1,
        NUMBER      = 2,
        STRING      = 3,
        OBJECT      = 4, // Indicating nested types.
        UNDEFINED   = 99,
   };
}
// clang-format on

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_FROMJSONVISITOR_HPP
#define CLUON_FROMJSONVISITOR_HPP

//#include "cluon/JSONConstants.hpp"
//#include "cluon/any/any.hpp"
//#include "cluon/cluon.hpp"

#include <cstdint>
#include <istream>
#include <map>
#include <string>

namespace cluon {
/**
This class decodes a given message from JSON format.
*/
class LIBCLUON_API FromJSONVisitor {
    /**
     * This class represents a key/value in a JSON list of key/values.
     */
    class JSONKeyValue {
       private:
        JSONKeyValue &operator=(JSONKeyValue &&) = delete;

       public:
        JSONKeyValue()                     = default;
        JSONKeyValue(const JSONKeyValue &) = default;
        JSONKeyValue(JSONKeyValue &&)      = default;
        JSONKeyValue &operator=(const JSONKeyValue &) = default;
        ~JSONKeyValue()                               = default;

       public:
        std::string m_key{""};
        JSONConstants m_type{JSONConstants::UNDEFINED};
        linb::any m_value;
    };

   private:
    FromJSONVisitor(const FromJSONVisitor &) = delete;
    FromJSONVisitor(FromJSONVisitor &&)      = delete;
    FromJSONVisitor &operator=(FromJSONVisitor &&) = delete;
    FromJSONVisitor &operator=(const FromJSONVisitor &other) = delete;

    /**
     * Internal constructor to pass reference to preset key/values.
     *
     * @param preset Pre-filled key/value map to handle nested fields.
     */
    FromJSONVisitor(std::map<std::string, FromJSONVisitor::JSONKeyValue> &preset) noexcept;

   public:
    FromJSONVisitor() noexcept;
    ~FromJSONVisitor() = default;

   public:
    /**
     * This method decodes a given istream into an internal key/value representation.
     *
     * @param in istream to decode.
     */
    void decodeFrom(std::istream &in) noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)id;
        (void)typeName;

        if (0 < m_keyValues.count(name)) {
            try {
                std::map<std::string, FromJSONVisitor::JSONKeyValue> v
                    = linb::any_cast<std::map<std::string, FromJSONVisitor::JSONKeyValue>>(m_keyValues[name].m_value);
                cluon::FromJSONVisitor nestedJSONDecoder(v);
                value.accept(nestedJSONDecoder);
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }

   public:
    /**
     * This method returns the base64-decoded representation for the given input.
     *
     * @param input to decode from base64
     * @return Decoded input.
     */
    static std::string decodeBase64(const std::string &input) noexcept;

   private:
    std::map<std::string, FromJSONVisitor::JSONKeyValue> readKeyValues(std::string &input) noexcept;

   private:
    std::map<std::string, FromJSONVisitor::JSONKeyValue> m_data{};
    std::map<std::string, FromJSONVisitor::JSONKeyValue> &m_keyValues;
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TOJSONVISITOR_HPP
#define CLUON_TOJSONVISITOR_HPP

//#include "cluon/any/any.hpp"
//#include "cluon/cluon.hpp"

#include <cstdint>
#include <map>
#include <sstream>
#include <string>

namespace cluon {
/**
This class provides a visitor to transform a message into JSON:

\code{.cpp}
MyMessage msg;
// Set some values in msg.

cluon::ToJSONVisitor j;
msg.accept(j);

std::cout << j.json() << std::endl;
\endcode
*/
class LIBCLUON_API ToJSONVisitor {
   private:
    ToJSONVisitor(const ToJSONVisitor &) = delete;
    ToJSONVisitor(ToJSONVisitor &&)      = delete;
    ToJSONVisitor &operator=(const ToJSONVisitor &) = delete;
    ToJSONVisitor &operator=(ToJSONVisitor &&) = delete;

   public:
    /**
     * Constructor.
     *
     * @param withOuterCurlyBraces Include the outer curly braces.
     * @param mask Map describing which fields to render. If empty, all
     *             fields will be emitted; individual field identifiers
     *             can be masked setting them to false.
     */
    ToJSONVisitor(bool withOuterCurlyBraces = true, const std::map<uint32_t, bool> &mask = {}) noexcept;

    /**
     * @return JSON-encoded data.
     */
    std::string json() const noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)typeName;
        if ((0 == m_mask.count(id)) || m_mask[id]) {
            try {
                ToJSONVisitor jsonVisitor;
                value.accept(jsonVisitor);
                m_buffer << '\"' << name << '\"' << ':' << jsonVisitor.json() << ',' << '\n';
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }

   public:
    /**
     * This method returns the base64-encoded representation for the given input.
     *
     * @param input to encode as base64
     * @return base64 encoded input.
     */
    static std::string encodeBase64(const std::string &input) noexcept;

   private:
    bool m_withOuterCurlyBraces{true};
    std::map<uint32_t, bool> m_mask;
    std::stringstream m_buffer{};
};

} // namespace cluon
#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TOCSVVISITOR_HPP
#define CLUON_TOCSVVISITOR_HPP

//#include "cluon/cluon.hpp"

#include <cstdint>
#include <map>
#include <sstream>
#include <string>

namespace cluon {
/**
This class provides a visitor to transform a message into CSV with
user-specified delimiters and optional column headers:

\code{.cpp}
MyMessage msg;
// Set some values in msg.

cluon::ToCSVVisitor csv{',', true};
msg.accept(csv);

std::cout << csv.csv() << std::endl;
\endcode

Subsequent use of this visitor will append the data (please keep in mind to not
change the visited messages in between as the generated CSV data will be messed
up otherwise).
*/
class LIBCLUON_API ToCSVVisitor {
   private:
    ToCSVVisitor(const ToCSVVisitor &) = delete;
    ToCSVVisitor(ToCSVVisitor &&)      = delete;
    ToCSVVisitor &operator=(const ToCSVVisitor &) = delete;
    ToCSVVisitor &operator=(ToCSVVisitor &&) = delete;

   public:
    /**
     * Constructor.
     *
     * @param delimiter Delimiter character.
     * @param withHeader If true, the first line in the output contains the
     *        column headers.
     * @param mask Map describing which fields to render. If empty, all
     *             fields will be emitted; individual field identifiers
     *             can be masked setting them to false.
     */
    ToCSVVisitor(char delimiter = ';', bool withHeader = true, const std::map<uint32_t, bool> &mask = {}) noexcept;

   protected:
    /**
     * Constructor for internal use.
     *
     * @param prefix Prefix to prepend per column header.
     * @param delimiter Delimiter character.
     * @param withHeader If true, the first line in the output contains the
     *        column headers.
     * @param isNested If true, the returned CSV values do not have a trailing new line.
     */
    ToCSVVisitor(const std::string &prefix, char delimiter, bool withHeader, bool isNested) noexcept;

   public:
    /**
     * @return CSV-encoded data.
     */
    std::string csv() const noexcept;

    /**
     * This method clears the containing CSV data.
     */
    void clear() noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)id;
        (void)typeName;
        if ((0 == m_mask.count(id)) || m_mask[id]) {
            constexpr bool IS_NESTED{true};
            ToCSVVisitor csvVisitor(name, m_delimiter, m_withHeader, IS_NESTED);
            value.accept(csvVisitor);

            if (m_fillHeader) {
                m_bufferHeader << csvVisitor.m_bufferHeader.str();
            }
            m_bufferValues << csvVisitor.m_bufferValues.str();
        }
    }

   private:
    std::map<uint32_t, bool> m_mask{};
    std::string m_prefix{};
    char m_delimiter{';'};
    bool m_withHeader{true};
    bool m_isNested{false};
    bool m_fillHeader{true};
    std::stringstream m_bufferHeader{};
    std::stringstream m_bufferValues{};
};

} // namespace cluon
#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TOLCMVISITOR_HPP
#define CLUON_TOLCMVISITOR_HPP

//#include "cluon/cluon.hpp"

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace cluon {
/**
This class encodes a given message in LCM format.
*/
class LIBCLUON_API ToLCMVisitor {
   private:
    ToLCMVisitor(const ToLCMVisitor &) = delete;
    ToLCMVisitor(ToLCMVisitor &&)      = delete;
    ToLCMVisitor &operator=(const ToLCMVisitor &) = delete;
    ToLCMVisitor &operator=(ToLCMVisitor &&) = delete;

   public:
    ToLCMVisitor()  = default;
    ~ToLCMVisitor() = default;

    /**
     * @param withHash True if the hash value from the fields shall be included.
     * @return Encoded data in LCM format.
     */
    std::string encodedData(bool withHash = true) const noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)id;
        (void)typeName;
        calculateHash(name);
        calculateHash(0);

        // No hash for the type but for name and dimension.
        cluon::ToLCMVisitor nestedLCMEncoder;
        value.accept(nestedLCMEncoder);

        constexpr bool WITH_HASH{false};
        const std::string s = nestedLCMEncoder.encodedData(WITH_HASH);
        m_buffer.write(s.c_str(), static_cast<std::streamsize>(s.size()));

        // Save this complex field's hash for later to compute final hash.
        m_hashes.push_back(nestedLCMEncoder.hash());
    }

   private:
    int64_t hash() const noexcept;
    void calculateHash(char c) noexcept;
    void calculateHash(const std::string &s) noexcept;

   private:
    int64_t m_hash{0x12345678};
    std::stringstream m_buffer{""};
    std::vector<int64_t> m_hashes{};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TOODVDVISITOR_HPP
#define CLUON_TOODVDVISITOR_HPP

//#include "cluon/cluon.hpp"

#include <cstdint>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace cluon {
/**
This class provides a visitor to transform a message into its corresponding
message specification in ODVD format:

\code{.cpp}
MyMessage msg;
// Set some values in msg.

cluon::ToODVDVisitor odvd;
msg.accept(odvd);

const std::string generatedMessageSpecification{odvd.messageSpecification()};
std::cout << generatedMessageSpecification << std::endl;

cluon::MessageParser mp;
auto retVal = mp.parse(generatedMessageSpecification);
std::cout << (cluon::MessageParser::MessageParserErrorCodes::NO_MESSAGEPARSER_ERROR == retVal.second);
\endcode
*/
class LIBCLUON_API ToODVDVisitor {
   private:
    ToODVDVisitor(const ToODVDVisitor &) = delete;
    ToODVDVisitor(ToODVDVisitor &&)      = delete;
    ToODVDVisitor &operator=(const ToODVDVisitor &) = delete;
    ToODVDVisitor &operator=(ToODVDVisitor &&) = delete;

   public:
    ToODVDVisitor() = default;

    /**
     * @return Message specification data.
     */
    std::string messageSpecification() const noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        try {
            std::string tmp{std::regex_replace(typeName, std::regex("::"), ".")}; // NOLINT

            ToODVDVisitor odvdVisitor;
            value.accept(odvdVisitor);
            m_forwardDeclarations.emplace(m_forwardDeclarations.begin(), odvdVisitor.messageSpecification());

            m_buffer << "    " << tmp << ' ' << name << " [ id = " << id << " ];" << '\n';
        } catch (std::regex_error &) { // LCOV_EXCL_LINE
        }
    }

   private:
    std::vector<std::string> m_forwardDeclarations{};
    std::stringstream m_buffer{};
};

} // namespace cluon
#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_TOMSGPACKVISITOR_HPP
#define CLUON_TOMSGPACKVISITOR_HPP

//#include "cluon/MsgPackConstants.hpp"
//#include "cluon/cluon.hpp"

#include <cstdint>
#include <sstream>
#include <string>

namespace cluon {
/**
This class encodes a given message in MsgPack format.
*/
class LIBCLUON_API ToMsgPackVisitor {
   private:
    ToMsgPackVisitor(const ToMsgPackVisitor &) = delete;
    ToMsgPackVisitor(ToMsgPackVisitor &&)      = delete;
    ToMsgPackVisitor &operator=(const ToMsgPackVisitor &) = delete;
    ToMsgPackVisitor &operator=(ToMsgPackVisitor &&) = delete;

   public:
    ToMsgPackVisitor()  = default;
    ~ToMsgPackVisitor() = default;

    /**
     * @return Encoded data in MsgPack format.
     */
    std::string encodedData() const noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);

    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)id;
        (void)typeName;

        encode(m_buffer, name);
        {
            cluon::ToMsgPackVisitor nestedMsgPackEncoder;
            value.accept(nestedMsgPackEncoder);
            const std::string tmp{nestedMsgPackEncoder.encodedData()};
            const uint32_t LENGTH{static_cast<uint32_t>(tmp.size())};
            m_buffer.write(tmp.c_str(), static_cast<std::streamsize>(LENGTH));
        }
        m_numberOfFields++;
    }

   private:
    void encode(std::ostream &o, const std::string &s);
    void encodeUint(std::ostream &o, uint64_t v);
    void encodeInt(std::ostream &o, int64_t v);

   private:
    uint32_t m_numberOfFields{0};
    std::stringstream m_buffer{""};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_ENVELOPE_HPP
#define CLUON_ENVELOPE_HPP

//#include "cluon/FromProtoVisitor.hpp"
//#include "cluon/ToProtoVisitor.hpp"
//#include "cluon/cluonDataStructures.hpp"

#include <cstring>
#include <istream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace cluon {

/**
 * This method transforms a given Envelope to a string representation to be
 * sent to an OpenDaVINCI session.
 *
 * @param envelope Envelope with payload to be sent.
 * @return String representation of the Envelope to be sent to OpenDaVINCI v4.
 */
inline std::string serializeEnvelope(cluon::data::Envelope &&envelope) noexcept {
    std::string dataToSend;
    {
        std::stringstream sstr;

        cluon::ToProtoVisitor protoEncoder;
        envelope.accept(protoEncoder);

        const std::string tmp{protoEncoder.encodedData()};
        uint32_t length{static_cast<uint32_t>(tmp.size())};
        length <<= 8;
        length = htole32(length);

        // Add OD4 header.
        constexpr unsigned char OD4_HEADER_BYTE0 = 0x0D;
        constexpr unsigned char OD4_HEADER_BYTE1 = 0xA4;
        sstr.put(static_cast<char>(OD4_HEADER_BYTE0));
        auto posByte1 = sstr.tellp();
        sstr.write(reinterpret_cast<char *>(&length), static_cast<std::streamsize>(sizeof(uint32_t)));
        auto posByte5 = sstr.tellp();
        sstr.seekp(posByte1);
        sstr.put(static_cast<char>(OD4_HEADER_BYTE1));
        sstr.seekp(posByte5);

        // Write payload.
        sstr.write(tmp.data(), static_cast<std::streamsize>(tmp.size()));

        dataToSend = sstr.str();
    }
    return dataToSend;
}

/**
 * This method extracts an Envelope from the given istream that holds bytes in
 * format:
 *
 *    0x0D 0xA4 LEN0 LEN1 LEN2 Proto-encoded cluon::data::Envelope
 *
 * 0xA4 LEN0 LEN1 LEN2 are little Endian.
 *
 * @param in Stream to read from.
 * @return cluon::data::Envelope.
 */
inline std::pair<bool, cluon::data::Envelope> extractEnvelope(std::istream &in) noexcept {
    bool retVal{false};
    cluon::data::Envelope env;
    if (in.good()) {
        constexpr uint8_t OD4_HEADER_SIZE{5};
        std::vector<char> buffer;
        buffer.reserve(OD4_HEADER_SIZE);
#ifdef WIN32                                           // LCOV_EXCL_LINE
        buffer.clear();                                // LCOV_EXCL_LINE
        retVal = true;                                 // LCOV_EXCL_LINE
        for (uint8_t i{0}; i < OD4_HEADER_SIZE; i++) { // LCOV_EXCL_LINE
            char c;                                    // LCOV_EXCL_LINE
            in.get(c);                                 // LCOV_EXCL_LINE
            retVal &= in.good();                       // LCOV_EXCL_LINE
            buffer.push_back(c);                       // LCOV_EXCL_LINE
        }
        if (retVal) { // LCOV_EXCL_LINE
#else                 // LCOV_EXCL_LINE
        in.read(&buffer[0], OD4_HEADER_SIZE);
        if (OD4_HEADER_SIZE == in.gcount()) {
#endif
            if ((0x0D == static_cast<uint8_t>(buffer[0])) && (0xA4 == static_cast<uint8_t>(buffer[1]))) {
                const uint32_t LENGTH{le32toh(*reinterpret_cast<uint32_t *>(&buffer[1])) >> 8};
                buffer.reserve(LENGTH);
#ifdef WIN32                                           // LCOV_EXCL_LINE
                buffer.clear();                        // LCOV_EXCL_LINE
                for (uint32_t i{0}; i < LENGTH; i++) { // LCOV_EXCL_LINE
                    char c;                            // LCOV_EXCL_LINE
                    in.get(c);                         // LCOV_EXCL_LINE
                    retVal &= in.good();               // LCOV_EXCL_LINE
                    buffer.push_back(c);               // LCOV_EXCL_LINE
                }
#else // LCOV_EXCL_LINE
                in.read(&buffer[0], static_cast<std::streamsize>(LENGTH));
                retVal = static_cast<int32_t>(LENGTH) == in.gcount();
#endif
                if (retVal) {
                    std::stringstream sstr(std::string(buffer.begin(), buffer.begin() + static_cast<std::streamsize>(LENGTH)));
                    cluon::FromProtoVisitor protoDecoder;
                    protoDecoder.decodeFrom(sstr, env);
                }
            }
        }
    }
    return std::make_pair(retVal, env);
}

/**
 * @return Extract a given Envelope's payload into the desired type.
 */
template <typename T>
inline T extractMessage(cluon::data::Envelope &&envelope) noexcept {
    cluon::FromProtoVisitor decoder;

    std::stringstream sstr(envelope.serializedData());
    decoder.decodeFrom(sstr);

    T msg;
    msg.accept(decoder);

    return msg;
}

} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_ENVELOPECONVERTER_HPP
#define CLUON_ENVELOPECONVERTER_HPP

//#include "cluon/MetaMessage.hpp"
//#include "cluon/cluon.hpp"
//#include "cluon/cluonDataStructures.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cluon {
/**
This class provides various conversion functions to and from Envelope data structures.
*/
class LIBCLUON_API EnvelopeConverter {
   private:
    EnvelopeConverter(const EnvelopeConverter &) = delete;
    EnvelopeConverter(EnvelopeConverter &&)      = delete;
    EnvelopeConverter &operator=(const EnvelopeConverter &) = delete;
    EnvelopeConverter &operator=(EnvelopeConverter &&) = delete;

   public:
    EnvelopeConverter() = default;

    /**
     * This method sets the message specification to be used for
     * interpreting a given Proto-encoded Envelope.
     *
     * @param ms Message specification following the ODVD format.
     * @return -1 in case of invalid message specification; otherwise, number
     *         of successfully parsed messages from given message specification.
     */
    int32_t setMessageSpecification(const std::string &ms) noexcept;

    /**
     * This method transforms the given Proto-encoded Envelope to JSON. The
     * Proto-encoded envelope might be preceded with a 5-bytes OD4-header (optional).
     *
     * @param protoEncodedEnvelope Proto-encoded Envelope.
     * @return JSON representation from given Proto-encoded Envelope using the
     *         given message specification.
     */
    std::string getJSONFromProtoEncodedEnvelope(const std::string &protoEncodedEnvelope) noexcept;

    /**
     * This method transforms the given Envelope to JSON.
     *
     * @param envelope Envelope.
     * @return JSON representation from given Envelope using the given message specification.
     */
    std::string getJSONFromEnvelope(cluon::data::Envelope &envelope) noexcept;

    /**
     * This method transforms a given JSON representation into a Proto-encoded Envelope
     * including the prepended OD4-header.
     *
     * @param json representation according to the given message specification.
     * @param messageIdentifier The given JSON representation shall be interpreted
     *        as the specified message.
     * @param senderStamp to be used in the Envelope.
     * @return Proto-encoded Envelope including OD4-header or empty string.
     */
    std::string getProtoEncodedEnvelopeFromJSONWithoutTimeStamps(const std::string &json, int32_t messageIdentifier, uint32_t senderStamp) noexcept;

    /**
     * This method transforms a given JSON representation into a Proto-encoded Envelope
     * including the prepended OD4-header and setting cluon::time::now() as sampleTimeStamp.
     *
     * @param json representation according to the given message specification.
     * @param messageIdentifier The given JSON representation shall be interpreted
     *        as the specified message.
     * @param senderStamp to be used in the Envelope.
     * @return Proto-encoded Envelope including OD4-header or empty string.
     */
    std::string getProtoEncodedEnvelopeFromJSON(const std::string &json, int32_t messageIdentifier, uint32_t senderStamp) noexcept;

   private:
// clang-format off
    std::string getProtoEncodedEnvelopeFromJSON(const std::string &json, int32_t messageIdentifier, uint32_t senderStamp, cluon::data::TimeStamp sampleTimeStamp) noexcept;
// clang-format on

   private:
    std::vector<cluon::MetaMessage> m_listOfMetaMessages{};
    std::map<int32_t, cluon::MetaMessage> m_scopeOfMetaMessages{};
};
} // namespace cluon
#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_GENERICMESSAGE_HPP
#define CLUON_GENERICMESSAGE_HPP

//#include "cluon/FromProtoVisitor.hpp"
//#include "cluon/MetaMessage.hpp"
//#include "cluon/any/any.hpp"
//#include "cluon/cluon.hpp"
//#include "cluon/cluonDataStructures.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cluon {
/**
GenericMessage is providing an abstraction level to work with concrete
messages. Therefore, it is acting as both, a Visitor to turn concrete
messages into GenericMessages or as Visitable to access the contained
data. GenericMessage would use C++'s std::any type; to allow C++14
compilers, we use the backport from linb::any.

Creating a GenericMessage:
There are several ways to create a GenericMessage. The first option is to
provide a message specification in ODVD format as result from MessageParser,
from which a GenericMessage is created. This instance can then be visited
afterwards by, for instance, an instance of ProtoDecoder to set the
GenericMessage's actual values.

1) This example demonstrates how to process a given message specification
   to decode a Proto-encoded byte sequence (in protoEncodedData). The
   message specification is given in messageSpecification that is parsed
   from MessageParser. On success, it is tried to decode the Proto-encoded
   data into a GenericMesssage representing an instance of "MyMessage".

   Using a message specification that does not match the serialized Proto
   data might result in unexpected behavior.

\code{.cpp}
// protoEncodedData is provided from somewhere, i.e., via network for example
std::string protoEncodedData = <...>
std::stringstream sstr{protoEncodedData};
cluon::FromProtoVisitor protoDecoder;
protoDecoder.decodeFrom(sstr);

const char *messageSpecification = R"(
message MyMessage [id = 123] {
   int32 field1 [id = 1];
   int32 field2 [id = 2];
}

cluon::MessageParser mp;
auto retVal = mp.parse(std::string(messageSpecification));
if (cluon::MessageParser::MessageParserErrorCodes::NO_MESSAGEPARSER_ERROR == retVal.second) {
    cluon::GenericMessage gm;
    auto listOfMetaMessages = retVal.first;
    gm.createFrom(listOfMetaMessages[0], listOfMetaMessages);
    // Set values in GenericMessage from protoDecoder.
    gm.accept(protoDecoder);
}
\endcode


2) This example demonstrates how to turn a given concrete message into a
   GenericMessage. Afterwards, the GenericMessage can be post-processed
   with Visitors.


\code{.cpp}
MyMessage msg;
// set some fields in msg.

cluon::GenericMessage gm;
gm.createFrom<MyMessage>(msg);
\endcode


After an instance of GenericMessage is available, it can be post-processed
into various representations:

1) Printing the contained data ("toString"; GenericMessage is being visited):

\code{.cpp}
GenericMessage gm;
// gm is created using one of the aforementioned options.

std::stringstream buffer;
gm.accept([](uint32_t, const std::string &, const std::string &) {},
          [&buffer](uint32_t, std::string &&, std::string &&n, auto v) { buffer << n << " = " << v << std::endl; },
          []() {});
std::cout << buffer.str() << std::endl;
\endcode


2) Filling the values of another concrete message (GenericMessage is
   acting as Visitor to another message):

\code{.cpp}
GenericMessage gm;
// gm is created using one of the aforementioned options.

Message msg;
msg.accept(gm);
\endcode


3) Serialize the GenericMessage gm into a Proto-format:

\code{.cpp}
GenericMessage gm;
// gm is created using one of the aforementioned options.

cluon::ToProtoVisitor protoEncoder;
gm.accept(protoEncoder);
const std::string{protoEncoder.encodedData()};
\endcode


4) Serialize the GenericMessage gm into JSON:

\code{.cpp}
GenericMessage gm;
// gm is created using one of the aforementioned options.

cluon::ToJSONVisitor j;
gm.accept(j);
std::cout << j.json();
\endcode


4) Dynamically transforming a given Proto-encoded byte sequence into JSON
   at runtime:

\code{.cpp}
// protoEncodedData is provided from somewhere, i.e., via network for example
std::string protoEncodedData = <...>
std::stringstream sstr{protoEncodedData};
cluon::FromProtoVisitor protoDecoder;
protoDecoder.decodeFrom(sstr);

const char *messageSpecification = R"(
message MyMessage [id = 123] {
   int32 field1 [id = 1];
   int32 field2 [id = 2];
}

cluon::MessageParser mp;
auto retVal = mp.parse(std::string(messageSpecification));
if (cluon::MessageParser::MessageParserErrorCodes::NO_MESSAGEPARSER_ERROR == retVal.second) {
    cluon::GenericMessage gm;
    auto listOfMetaMessages = retVal.first;
    gm.createFrom(listOfMetaMessages[0], listOfMetaMessages);
    // Set values in GenericMessage from protoDecoder.
    gm.accept(protoDecoder);
}

cluon::ToJSONVisitor j;
gm.accept(j);
std::cout << j.json();
\endcode
*/
class LIBCLUON_API GenericMessage {
   private:
    class GenericMessageVisitor {
       private:
        GenericMessageVisitor(const GenericMessageVisitor &) = delete;
        GenericMessageVisitor(GenericMessageVisitor &&)      = delete;
        GenericMessageVisitor &operator=(const GenericMessageVisitor &) = delete;
        GenericMessageVisitor &operator=(GenericMessageVisitor &&) = delete;

       public:
        GenericMessageVisitor() = default;

       public:
        // The following methods are provided to allow an instance of this class to
        // be used as visitor for an instance with the method signature void accept<T>(T&);
        void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
        void postVisit() noexcept;

        void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
        void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

        template <typename T>
        void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
            cluon::MetaMessage::MetaField mf;
            mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::MESSAGE_T).fieldDataTypeName(typeName).fieldName(name);

            GenericMessage gm;
            gm.createFrom<T>(value);

            m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{gm};
            m_metaMessage.add(std::move(mf));
        }

       public:
        /**
         * @return MetaMessage for this GenericMessage.
         */
        MetaMessage metaMessage() const noexcept;

        /**
         * @return Intermediate data representation for this GenericMessage.
         */
        std::unordered_map<uint32_t, linb::any, UseUInt32ValueAsHashKey> intermediateDataRepresentation() const noexcept;

       private:
        MetaMessage m_metaMessage{};
        std::unordered_map<uint32_t, linb::any, UseUInt32ValueAsHashKey> m_intermediateDataRepresentation;
    };

   private:
    GenericMessage &operator=(GenericMessage &&) = delete;

   public:
    GenericMessage()                       = default;
    GenericMessage(GenericMessage &&)      = default;
    GenericMessage(const GenericMessage &) = default;
    GenericMessage &operator=(const GenericMessage &) = default;

   public:
    int32_t ID();
    const std::string ShortName();
    const std::string LongName();

   public:
    /**
     * This methods creates this GenericMessage from a given concrete message.
     *
     * @param msg Concrete message used to derive this GenericMessage from.
     */
    template <typename T>
    void createFrom(T &msg) {
        GenericMessageVisitor gmv;
        msg.accept(gmv);

        m_metaMessage = gmv.metaMessage();
        m_intermediateDataRepresentation.clear();
        m_intermediateDataRepresentation = gmv.intermediateDataRepresentation();
    }

    /**
     * This method creates an empty GenericMessage from a given message
     * specification parsed from MessageParser.
     *
     * @param mm MetaMessage describing the fields for the message to be resolved.
     * @param mms List of MetaMessages that are known (used for resolving nested message).
     */
    void createFrom(const MetaMessage &mm, const std::vector<MetaMessage> &mms) noexcept;

   public:
    // The following methods are provided to allow an instance of this class to
    // be used as visitor for an instance with the method signature void accept<T>(T&);
    void preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept;
    void postVisit() noexcept;

    void visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept;
    void visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept;

    template <typename T>
    void visit(uint32_t &id, std::string &&typeName, std::string &&name, T &value) noexcept {
        (void)typeName;
        (void)name;
        if (0 < m_intermediateDataRepresentation.count(id)) {
            try {
                auto &v = linb::any_cast<cluon::GenericMessage &>(m_intermediateDataRepresentation[id]);
                value.accept(v);
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }

   public:
    /**
     * This method allows other instances to visit this GenericMessage for
     * post-processing the contained data on an individual field basis.
     *
     * @param fieldId Identifier for the field to visit.
     * @param visitor Instance of the visitor visiting this GenericMessage.
     */
    template<class Visitor>
    inline void accept(uint32_t fieldId, Visitor &visitor) {
        const bool VISIT_ALL{false};
        accept<Visitor>(fieldId, visitor, VISIT_ALL);
    }

    /**
     * This method allows other instances to visit this GenericMessage for
     * post-processing the contained data.
     *
     * @param visitor Instance of the visitor visiting this GenericMessage.
     */
    template <class Visitor>
    void accept(Visitor &visitor) {
        const uint32_t FIELD_ID{0};
        const bool VISIT_ALL{true};
        accept<Visitor>(FIELD_ID, visitor, VISIT_ALL);
    }

    /**
     * This method allows other instances to visit this GenericMessage for
     * post-processing the contained data.
     *
     * @param _preVisit Instance of the visitor pre-visiting this GenericMessage.
     * @param _visit Instance of the visitor visiting this GenericMessage.
     * @param _postVisit Instance of the visitor post-visiting this GenericMessage.
     */
    template <class PreVisitor, class Visitor, class PostVisitor>
    void accept(PreVisitor &&_preVisit, Visitor &&_visit, PostVisitor &&_postVisit) {
        std::forward<PreVisitor>(_preVisit)(m_metaMessage.messageIdentifier(), m_metaMessage.messageName(), m_longName);

        for (const auto &f : m_metaMessage.listOfMetaFields()) {
            if (f.fieldDataType() == MetaMessage::MetaField::BOOL_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<bool &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::CHAR_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<char &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::UINT8_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<uint8_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::INT8_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<int8_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::UINT16_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<uint16_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::INT16_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<int16_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::UINT32_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<uint32_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::INT32_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<int32_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::UINT64_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<uint64_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::INT64_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<int64_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::FLOAT_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<float &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::DOUBLE_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<double &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (((f.fieldDataType() == MetaMessage::MetaField::STRING_T) || (f.fieldDataType() == MetaMessage::MetaField::BYTES_T))
                       && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<std::string &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            } else if (f.fieldDataType() == MetaMessage::MetaField::MESSAGE_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                try {
                    auto &v = linb::any_cast<cluon::GenericMessage &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                    doTripletForwardVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, _preVisit, _visit, _postVisit);
                } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                }
            }
        }

        std::forward<PostVisitor>(_postVisit)();
    }

   private:
    template<class Visitor>
    inline void accept(uint32_t fieldId, Visitor &visitor, bool visitAll) {
        visitor.preVisit(ID(), ShortName(), LongName());

        for (const auto &f : m_metaMessage.listOfMetaFields()) {
            if (visitAll || (fieldId == f.fieldIdentifier())) {
                if (f.fieldDataType() == MetaMessage::MetaField::BOOL_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<bool &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::CHAR_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<char &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::UINT8_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<uint8_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::INT8_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<int8_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::UINT16_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<uint16_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::INT16_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<int16_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::UINT32_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<uint32_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::INT32_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<int32_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::UINT64_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<uint64_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::INT64_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<int64_t &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::FLOAT_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<float &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::DOUBLE_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<double &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (((f.fieldDataType() == MetaMessage::MetaField::STRING_T) || (f.fieldDataType() == MetaMessage::MetaField::BYTES_T))
                           && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<std::string &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                } else if (f.fieldDataType() == MetaMessage::MetaField::MESSAGE_T && (0 < m_intermediateDataRepresentation.count(f.fieldIdentifier()))) {
                    try {
                        auto &v = linb::any_cast<cluon::GenericMessage &>(m_intermediateDataRepresentation[f.fieldIdentifier()]);
                        doVisit(f.fieldIdentifier(), std::move(f.fieldDataTypeName()), std::move(f.fieldName()), v, visitor);
                    } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
                    }
                }
                // End processing in case of visiting specific fields.
                if (!visitAll && (fieldId == f.fieldIdentifier())) {
                  break;
                }
            }
        }

        visitor.postVisit();
    }

   private:
    MetaMessage m_metaMessage{};
    std::vector<MetaMessage> m_scopeOfMetaMessages{};
    std::unordered_map<std::string, MetaMessage> m_mapForScopeOfMetaMessages{};
    std::string m_longName{""};
    std::unordered_map<uint32_t, linb::any, UseUInt32ValueAsHashKey> m_intermediateDataRepresentation;
};
} // namespace cluon

template <>
struct isVisitable<cluon::GenericMessage> {
    static const bool value = true;
};
template <>
struct isTripletForwardVisitable<cluon::GenericMessage> {
    static const bool value = true;
};
#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_LCMTOGENERICMESSAGE_HPP
#define CLUON_LCMTOGENERICMESSAGE_HPP

//#include "cluon/GenericMessage.hpp"
//#include "cluon/MetaMessage.hpp"
//#include "cluon/cluon.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cluon {
/**
This class transforms a given LCM message into a GenericMessage.
*/
class LIBCLUON_API LCMToGenericMessage {
   private:
    LCMToGenericMessage(const LCMToGenericMessage &) = delete;
    LCMToGenericMessage(LCMToGenericMessage &&)      = delete;
    LCMToGenericMessage &operator=(const LCMToGenericMessage &) = delete;
    LCMToGenericMessage &operator=(LCMToGenericMessage &&) = delete;

   public:
    LCMToGenericMessage() = default;

    /**
     * This method sets the message specification to be used for
     * interpreting a given LCM-encoded message.
     *
     * @param ms Message specification following the ODVD format.
     * @return -1 in case of invalid message specification; otherwise, number
     *         of successfully parsed messages from given message specification.
     */
    int32_t setMessageSpecification(const std::string &ms) noexcept;

    /**
     * This method transforms the given LCM payload into a GenericMessage.
     *
     * @param data LCM Payload.
     * @return GenericMessage representation using the given message specification.
     */
    cluon::GenericMessage getGenericMessage(const std::string &data) noexcept;

   private:
    std::vector<cluon::MetaMessage> m_listOfMetaMessages{};
    std::map<std::string, cluon::MetaMessage> m_scopeOfMetaMessages{};
};
} // namespace cluon
#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_OD4SESSION_HPP
#define CLUON_OD4SESSION_HPP

//#include "cluon/Time.hpp"
//#include "cluon/ToProtoVisitor.hpp"
//#include "cluon/UDPReceiver.hpp"
//#include "cluon/UDPSender.hpp"
//#include "cluon/cluon.hpp"
//#include "cluon/cluonDataStructures.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace cluon {
/**
This class provides an interface to an OpenDaVINCI v4 session. An OpenDaVINCI
v4 session allows the automatic exchange of time-stamped Envelopes carrying
user-defined messages usually using UDP multicast. A running OD4Session will not
receive the bytes that itself has sent to other microservices.

There are two ways to participate in an OpenDaVINCI session. Variant A is simply
calling a user-supplied lambda whenever a new Envelope is received:

\code{.cpp}
cluon::OD4Session od4{111, [](cluon::data::Envelope &&envelope){ std::cout << "Received cluon::Envelope" << std::endl;}
};

// Do something in parallel.

MyMessage msg;
od4.send(msg);
\endcode

Variant B allows a more fine-grained setup where you specify the Envelopes of interest:

\code{.cpp}
cluon::OD4Session od4{111};

od4.dataTrigger(cluon::data::TimeStamp::ID(), [](cluon::data::Envelope &&envelope){ std::cout << "Received cluon::data::TimeStamp" << std::endl;});
od4.dataTrigger(MyMessage::ID(), [](cluon::data::Envelope &&envelope){ std::cout << "Received MyMessage" << std::endl;});

// Do something in parallel.

MyMessage msg;
od4.send(msg);
\endcode

Next to receive Envelopes, OD4Session can call a user-supplied lambda in a time-triggered
way. The lambda is executed as long as it does not return false or throws an exception
that is then caught in the method timeTrigger and the method is exited:

\code{.cpp}
cluon::OD4Session od4{111};

const float FREQ{10}; // 10 Hz.
od4.timeTrigger(FREQ, [](){
  // Do something time-triggered.
  return false;
}); // This call blocks until the lambda returns false.
\endcode
*/
class LIBCLUON_API OD4Session {
   private:
    OD4Session(const OD4Session &) = delete;
    OD4Session(OD4Session &&)      = delete;
    OD4Session &operator=(const OD4Session &) = delete;
    OD4Session &operator=(OD4Session &&) = delete;

   public:
    /**
     * Constructor.
     *
     * @param CID OpenDaVINCI v4 session identifier [1 .. 254]
     * @param delegate Function to call on newly arriving Envelopes ("catch-all");
     *        if a nullptr is passed, the method dataTrigger can be used to set
     *        message specific delegates. Please note that it is NOT possible
     *        to have both: a delegate for "catch-all" and the data-triggered ones.
     */
    OD4Session(uint16_t CID, std::function<void(cluon::data::Envelope &&envelope)> delegate = nullptr) noexcept;

    /**
     * This method will send a given Envelope to this OpenDaVINCI v4 session.
     *
     * @param envelope to be sent.
     */
    void send(cluon::data::Envelope &&envelope) noexcept;

    /**
     * This method sets a delegate to be called data-triggered on arrival
     * of a new Envelope for a given message identifier.
     *
     * @param messageIdentifier Message identifier to assign a delegate.
     * @param delegate Function to call on newly arriving Envelopes; setting it to nullptr will erase it.
     * @return true if the given delegate could be successfully set or unset.
     */
    bool dataTrigger(int32_t messageIdentifier, std::function<void(cluon::data::Envelope &&envelope)> delegate) noexcept;

    /**
     * This method sets a delegate to be called time-triggered using the
     * specified frequency until the delegate returns false. This method
     * blocks until the delegate has returned false or threw an exception.
     * Thus, this method is typically called as last statement in a main
     * function of a program.
     *
     * @param freq Frequency in Hertz to run the given delegate.
     * @param delegate Function to call according to the given frequency.
     */
    void timeTrigger(float freq, std::function<bool()> delegate) noexcept;

    /**
     * This method will send a given message to this OpenDaVINCI v4 session.
     *
     * @param message Message to be sent.
     * @param sampleTimeStamp Time point when this sample to be sent was captured (default = sent time point).
     * @param senderStamp Optional sender stamp (default = 0).
     */
    template <typename T>
    void send(T &message, const cluon::data::TimeStamp &sampleTimeStamp = cluon::data::TimeStamp(), uint32_t senderStamp = 0) noexcept {
        try {
            std::lock_guard<std::mutex> lck(m_senderMutex);
            cluon::ToProtoVisitor protoEncoder;

            cluon::data::Envelope envelope;
            {
                envelope.dataType(static_cast<int32_t>(message.ID()));
                message.accept(protoEncoder);
                envelope.serializedData(protoEncoder.encodedData());
                envelope.sent(cluon::time::now());
                envelope.sampleTimeStamp((0 == (sampleTimeStamp.seconds() + sampleTimeStamp.microseconds())) ? envelope.sent() : sampleTimeStamp);
                envelope.senderStamp(senderStamp);
            }

            send(std::move(envelope));
        } catch (...) {} // LCOV_EXCL_LINE
    }

   public:
    bool isRunning() noexcept;

   private:
    void callback(std::string &&data, std::string &&from, std::chrono::system_clock::time_point &&timepoint) noexcept;
    void sendInternal(std::string &&dataToSend) noexcept;

   private:
    std::unique_ptr<cluon::UDPReceiver> m_receiver;
    cluon::UDPSender m_sender;

    std::mutex m_senderMutex{};

    std::function<void(cluon::data::Envelope &&envelope)> m_delegate{nullptr};

    std::mutex m_mapOfDataTriggeredDelegatesMutex{};
    std::unordered_map<int32_t, std::function<void(cluon::data::Envelope &&envelope)>, UseUInt32ValueAsHashKey> m_mapOfDataTriggeredDelegates{};
};

} // namespace cluon
#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_PLAYER_HPP
#define CLUON_PLAYER_HPP

//#include "cluon/cluon.hpp"
//#include "cluon/cluonDataStructures.hpp"

#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace cluon {

class LIBCLUON_API IndexEntry {
   public:
    IndexEntry() = default;
    IndexEntry(const int64_t &sampleTimeStamp, const uint64_t &filePosition) noexcept;

   public:
    int64_t m_sampleTimeStamp{0};
    uint64_t m_filePosition{0};
    bool m_available{0};
};

class LIBCLUON_API Player {
   private:
    enum {
        ONE_MILLISECOND_IN_MICROSECONDS = 1000,
        ONE_SECOND_IN_MICROSECONDS      = 1000 * ONE_MILLISECOND_IN_MICROSECONDS,
        MAX_DELAY_IN_MICROSECONDS       = 1 * ONE_SECOND_IN_MICROSECONDS,
        LOOK_AHEAD_IN_S                 = 30,
        MIN_ENTRIES_FOR_LOOK_AHEAD      = 5000,
    };

   private:
    Player(const Player &) = delete;
    Player(Player &&)      = delete;
    Player &operator=(Player &&) = delete;
    Player &operator=(const Player &other) = delete;

   public:
    /**
     * Constructor.
     *
     * @param file File to play.
     * @param autoRewind True if the file should be rewind at EOF.
     * @param threading If set to true, player will load new envelopes from the files in background.
     */
    Player(const std::string &file, const bool &autoRewind, const bool &threading) noexcept;
    ~Player();

    /**
     * @return Pair of bool and next cluon::data::Envelope to be replayed;
     *         if bool is false, no next Envelope is available.
     */
    std::pair<bool, cluon::data::Envelope> getNextEnvelopeToBeReplayed() noexcept;

    /**
     * @return real delay in microseconds to be waited before the next cluon::data::Envelope should be delivered.
     */
    uint32_t delay() const noexcept;

    /**
     * @return true if there is more data to replay.
     */
    bool hasMoreData() const noexcept;

    /**
     * This method rewinds the iterators.
     */
    void rewind() noexcept;

    void seekTo(float ratio) noexcept;

    /**
     * @return total amount of cluon::data::Envelopes in the .rec file.
     */
    uint32_t totalNumberOfEnvelopesInRecFile() const noexcept;

   private:
    // Internal methods without Lock.
    bool hasMoreDataFromRecFile() const noexcept;

    /**
     * This method initializes the global index where the sample
     * time stamps are sorted chronocally and mapped to the
     * corresponding cluon::data::Envelope in the rec file.
     */
    void initializeIndex() noexcept;

    /**
     * This method computes the initially required amount of
     * cluon::data::Envelope in the cache and fill the cache accordingly.
     */
    void computeInitialCacheLevelAndFillCache() noexcept;

    /**
     * This method clears all caches.
     */
    void resetCaches() noexcept;

    /**
     * This method resets the iterators.
     */
    inline void resetIterators() noexcept;

    /**
     * This method fills the cache by trying to read up
     * to maxNumberOfEntriesToReadFromFile from the rec file.
     *
     * @param maxNumberOfEntriesToReadFromFile Maximum number of entries to be read from file.
     * @return Number of entries read from file.
     */
    uint32_t fillEnvelopeCache(const uint32_t &maxNumberOfEntriesToReadFromFile) noexcept;

    /**
     * This method checks the availability of the next cluon::data::Envelope
     * to be replayed from the cache.
     */
    inline void checkAvailabilityOfNextEnvelopeToBeReplayed() noexcept;

   private: // Data for the Player.
    bool m_threading;

    std::string m_file;

    // Handle to .rec file.
    std::fstream m_recFile;
    bool m_recFileValid;

   private: // Player states.
    bool m_autoRewind;

   private: // Index and cache management.
    // Global index: Mapping SampleTimeStamp --> cache entry (holding the actual content from .rec file).
    mutable std::mutex m_indexMutex;
    std::multimap<int64_t, IndexEntry> m_index;

    // Pointers to the current envelope to be replayed and the
    // envelope that has be replayed from the global index.
    std::multimap<int64_t, IndexEntry>::iterator m_previousPreviousEnvelopeAlreadyReplayed;
    std::multimap<int64_t, IndexEntry>::iterator m_previousEnvelopeAlreadyReplayed;
    std::multimap<int64_t, IndexEntry>::iterator m_currentEnvelopeToReplay;

    // Information about the index.
    std::multimap<int64_t, IndexEntry>::iterator m_nextEntryToReadFromRecFile;

    uint32_t m_desiredInitialLevel;

    // Fields to compute replay throughput for cache management.
    cluon::data::TimeStamp m_firstTimePointReturningAEnvelope;
    uint64_t m_numberOfReturnedEnvelopesInTotal;

    uint32_t m_delay;

   private:
    /**
     * This method sets the state of the envelopeCacheFilling thread.
     *
     * @param running False if the thread to fill the Envelope cache shall be joined.
     */
    void setEnvelopeCacheFillingRunning(const bool &running) noexcept;
    bool isEnvelopeCacheFillingRunning() const noexcept;

    /**
     * This method manages the cache.
     */
    void manageCache() noexcept;

    /**
     * This method checks whether the cache needs to be refilled.
     *
     * @param numberOfEntries Number of entries in cache.
     * @param refillMultiplicator Multiplicator to modify the amount of envelopes to be refilled.
     * @return Modified refillMultiplicator recommedned to be used next time
     */
    float checkRefillingCache(const uint32_t &numberOfEntries, float refillMultiplicator) noexcept;

   private:
    mutable std::mutex m_envelopeCacheFillingThreadIsRunningMutex;
    bool m_envelopeCacheFillingThreadIsRunning;
    std::thread m_envelopeCacheFillingThread;

    // Mapping of pos_type (within .rec file) --> cluon::data::Envelope (read from .rec file).
    std::map<uint64_t, cluon::data::Envelope> m_envelopeCache;

   public:
    void setPlayerListener(std::function<void(cluon::data::PlayerStatus playerStatus)> playerListener) noexcept;

   private:
    std::mutex m_playerListenerMutex;
    std::function<void(cluon::data::PlayerStatus playerStatus)> m_playerListener{nullptr};
};

} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_SHAREDMEMORY_HPP
#define CLUON_SHAREDMEMORY_HPP

//#include "cluon/cluon.hpp"
//#include "cluon/cluonDataStructures.hpp"

// clang-format off
#ifdef WIN32
    #include <Windows.h>
#else
    #include <pthread.h>
    #include <sys/ipc.h>
#endif
// clang-format on

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <string>
#include <utility>

namespace cluon {

class LIBCLUON_API SharedMemory {
   private:
    SharedMemory(const SharedMemory &) = delete;
    SharedMemory(SharedMemory &&)      = delete;
    SharedMemory &operator=(const SharedMemory &) = delete;
    SharedMemory &operator=(SharedMemory &&) = delete;

   public:
    /**
     * Constructor.
     *
     * @param name Name of the shared memory area; must start with / and must not
     * be longer than NAME_MAX (255) on POSIX or PATH_MAX on WIN32. If the name
     * is missing a leading '/' or is longer than 255, it will be adjusted accordingly.
     * @param size of the shared memory area to create; if size is 0, the class tries to attach to an existing area.
     */
    SharedMemory(const std::string &name, uint32_t size = 0) noexcept;
    ~SharedMemory() noexcept;

    /**
     * @return true when this shared memory area is locked.
     */
    bool isLocked() const noexcept;

    /**
     * This method locks the shared memory area.
     */
    void lock() noexcept;

    /**
     * This method unlocks the shared memory area.
     */
    void unlock() noexcept;

    /**
     * This method waits for being notified from the shared condition.
     */
    void wait() noexcept;

    /**
     * This method notifies all threads waiting on the shared condition.
     */
    void notifyAll() noexcept;

    /**
     * This method sets the time stamp that can be used to
     * express the sample time stamp of the data in residing
     * in the shared memory.
     *
     * This method is only allowed when the shared memory is locked.
     *
     * @param ts TimeStamp.
     * @return true if the timestamp could set; false if the shared memory was not locked.
     */
    bool setTimeStamp(const cluon::data::TimeStamp &ts) noexcept;

    /**
     * This method returns the sample time stamp.
     *
     * This method is only allowed when the shared memory is locked.
     *
     * @return (true, sample time stamp) or (false, 0) in case if the shared memory was not locked.
     */
    std::pair<bool, cluon::data::TimeStamp> getTimeStamp() noexcept;

   public:
    /**
     * @return True if the shared memory area is existing and usable.
     */
    bool valid() noexcept;

    /**
     * @return Pointer to the raw shared memory or nullptr in case of invalid shared memory.
     */
    char *data() noexcept;

    /**
     * @return The size of the shared memory area.
     */
    uint32_t size() const noexcept;

    /**
     * @return Name the shared memory area.
     */
    const std::string name() const noexcept;

#ifdef WIN32
   private:
    void initWIN32() noexcept;
    void deinitWIN32() noexcept;
    void lockWIN32() noexcept;
    void unlockWIN32() noexcept;
    void waitWIN32() noexcept;
    void notifyAllWIN32() noexcept;
#else
   private:
    void initPOSIX() noexcept;
    void deinitPOSIX() noexcept;
    void lockPOSIX() noexcept;
    void unlockPOSIX() noexcept;
    void waitPOSIX() noexcept;
    void notifyAllPOSIX() noexcept;
    bool validPOSIX() noexcept;

    void initSysV() noexcept;
    void deinitSysV() noexcept;
    void lockSysV() noexcept;
    void unlockSysV() noexcept;
    void waitSysV() noexcept;
    void notifyAllSysV() noexcept;
    bool validSysV() noexcept;
#endif

   private:
    std::string m_name{""};
    std::string m_nameForTimeStamping{""};
    uint32_t m_size{0};
    char *m_sharedMemory{nullptr};
    char *m_userAccessibleSharedMemory{nullptr};
    bool m_hasOnlyAttachedToSharedMemory{false};

    std::atomic<bool> m_broken{false};
    std::atomic<bool> m_isLocked{false};

#ifdef WIN32
    HANDLE __conditionEvent{nullptr};
    HANDLE __mutex{nullptr};
    HANDLE __sharedMemory{nullptr};
#else
    int32_t m_fdForTimeStamping{-1};

    bool m_usePOSIX{true};

    // Member fields for POSIX-based shared memory.
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    int32_t m_fd{-1};
    struct SharedMemoryHeader {
        uint32_t __size;
        pthread_mutex_t __mutex;
        pthread_cond_t __condition;
    };
    SharedMemoryHeader *m_sharedMemoryHeader{nullptr};
#endif

    // Member fields for SysV-based shared memory.
    key_t m_shmKeySysV{0};
    key_t m_mutexKeySysV{0};
    key_t m_conditionKeySysV{0};

    int m_sharedMemoryIDSysV{-1};
    int m_mutexIDSysV{-1};
    int m_conditionIDSysV{-1};
#endif
};
} // namespace cluon

#endif
#ifndef BEGIN_HEADER_ONLY_IMPLEMENTATION
#define BEGIN_HEADER_ONLY_IMPLEMENTATION
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "argh/argh.h"

//#include "cluon/cluon.hpp"

namespace cluon {

inline std::map<std::string, std::string> getCommandlineArguments(int32_t argc, char **argv) noexcept {
    argh::parser commandline{argc, argv};
    std::map<std::string, std::string> retVal;

    for (auto &positionalArgument : commandline.pos_args()) { retVal[positionalArgument] = ""; }

    for (auto &flag : commandline.flags()) { retVal[flag] = "1"; }

    for (auto &parameter : commandline.params()) { retVal[parameter.first] = parameter.second; }

    return retVal;
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/MetaMessage.hpp"

namespace cluon {

inline MetaMessage::MetaField::MetaFieldDataTypes MetaMessage::MetaField::fieldDataType() const noexcept {
    return m_fieldDataType;
}

inline MetaMessage::MetaField &MetaMessage::MetaField::fieldDataType(const MetaMessage::MetaField::MetaFieldDataTypes &v) noexcept {
    m_fieldDataType = v;
    return *this;
}

inline std::string MetaMessage::MetaField::fieldDataTypeName() const noexcept {
    return m_fieldDataTypeName;
}

inline MetaMessage::MetaField &MetaMessage::MetaField::fieldDataTypeName(const std::string &v) noexcept {
    m_fieldDataTypeName = v;
    return *this;
}

inline std::string MetaMessage::MetaField::fieldName() const noexcept {
    return m_fieldName;
}

inline MetaMessage::MetaField &MetaMessage::MetaField::fieldName(const std::string &v) noexcept {
    m_fieldName = v;
    return *this;
}

inline uint32_t MetaMessage::MetaField::fieldIdentifier() const noexcept {
    return m_fieldIdentifier;
}

inline MetaMessage::MetaField &MetaMessage::MetaField::fieldIdentifier(uint32_t v) noexcept {
    m_fieldIdentifier = v;
    return *this;
}

inline std::string MetaMessage::MetaField::defaultInitializationValue() const noexcept {
    return m_defaultInitializationValue;
}

inline MetaMessage::MetaField &MetaMessage::MetaField::defaultInitializationValue(const std::string &v) noexcept {
    m_defaultInitializationValue = v;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////

inline MetaMessage::MetaMessage() noexcept {}

inline std::string MetaMessage::packageName() const noexcept {
    return m_packageName;
}

inline std::string MetaMessage::messageName() const noexcept {
    return m_messageName;
}

inline MetaMessage &MetaMessage::packageName(const std::string &v) noexcept {
    m_packageName = v;
    return *this;
}

inline MetaMessage &MetaMessage::messageName(const std::string &v) noexcept {
    m_messageName = v;
    return *this;
}

inline int32_t MetaMessage::messageIdentifier() const noexcept {
    return m_messageIdentifier;
}

inline MetaMessage &MetaMessage::messageIdentifier(int32_t v) noexcept {
    m_messageIdentifier = v;
    return *this;
}

inline MetaMessage &MetaMessage::add(MetaMessage::MetaField &&mf) noexcept {
    m_listOfMetaFields.emplace_back(std::move(mf));
    return *this;
}

inline const std::vector<MetaMessage::MetaField> &MetaMessage::listOfMetaFields() const noexcept {
    return m_listOfMetaFields;
}

inline void MetaMessage::accept(const std::function<void(const MetaMessage &)> &visit) {
    visit(*this);
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/MessageParser.hpp"
//#include "cluon/stringtoolbox.hpp"

//#include "cpp-peglib/peglib.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace cluon {

inline std::pair<std::vector<MetaMessage>, MessageParser::MessageParserErrorCodes> MessageParser::parse(const std::string &input) {
    const char *grammarMessageSpecificationLanguage = R"(
        MESSAGES_SPECIFICATION      <- PACKAGE_DECLARATION? MESSAGE_DECLARATION*
        PACKAGE_DECLARATION         <- 'package' PACKAGE_IDENTIFIER ';'
        PACKAGE_IDENTIFIER          <- < IDENTIFIER ('.' IDENTIFIER)* >

        MESSAGE_DECLARATION         <- 'message' MESSAGE_IDENTIFIER MESSAGE_OPTIONS '{' FIELD* '}'
        MESSAGE_IDENTIFIER          <- < IDENTIFIER ('.' IDENTIFIER)* >
        MESSAGE_OPTIONS             <- '[' 'id' '=' NATURAL_NUMBER ','? ']'

        FIELD                       <- PRIMITIVE_FIELD

        PRIMITIVE_FIELD             <- PRIMITIVE_TYPE IDENTIFIER ('[' PRIMITIVE_FIELD_OPTIONS ']')? ';'
        PRIMITIVE_FIELD_OPTIONS     <- PRIMITIVE_FIELD_DEFAULT? ','? NUMERICAL_FIELD_IDENTIFIER?
        NUMERICAL_FIELD_IDENTIFIER  <- 'id' '=' NATURAL_NUMBER
        PRIMITIVE_FIELD_DEFAULT     <- 'default' '=' (FLOAT_NUMBER / BOOL / CHARACTER / STRING)
        PRIMITIVE_TYPE              <- < 'bool' / 'float' / 'double' /
                                         'char' /
                                         'bytes' / 'string' /
                                         'int8' / 'uint8' / 
                                         'int16' / 'uint16' / 
                                         'int32' / 'uint32' / 
                                         'int64' / 'uint64' /
                                         MESSAGE_TYPE >

        MESSAGE_TYPE                <- < IDENTIFIER ('.' IDENTIFIER)* >

        IDENTIFIER                  <- < [a-zA-Z][a-zA-Z0-9_]* >
        DIGIT                       <- < [0-9] >
        NATURAL_NUMBER              <- < [1-9] DIGIT* >
        FLOAT_NUMBER                <- < ('+' / '-')? DIGIT DIGIT* (('.') DIGIT*)? >
        BOOL                        <- < 'true' > / < 'false' >
        STRING                      <- '"' < (!'"'.)* > '"'
        CHARACTER                   <- '\'' < (!'\'' .) > '\''

        %whitespace                 <- [ \t\r\n]*
    )";

    ////////////////////////////////////////////////////////////////////////////

    // Function to check for unique field names.
    std::function<bool(const peg::Ast &, std::string &, std::vector<std::string> &, std::vector<std::string> &, std::vector<int32_t> &, std::vector<int32_t> &)>
        check4UniqueFieldNames = [&checkForUniqueFieldNames = check4UniqueFieldNames](const peg::Ast &ast,
                                                                                      std::string &prefix,
                                                                                      std::vector<std::string> &messageNames,
                                                                                      std::vector<std::string> &fieldNames,
                                                                                      std::vector<int32_t> &numericalMessageIdentifiers,
                                                                                      std::vector<int32_t> &numericalFieldIdentifiers) {
            bool retVal = true;
            // First, we need to visit the children of AST node MESSAGES_SPECIFICATION.
            if ("MESSAGES_SPECIFICATION" == ast.name) {
                for (const auto &node : ast.nodes) {
                    retVal &= checkForUniqueFieldNames(*node, prefix, messageNames, fieldNames, numericalMessageIdentifiers, numericalFieldIdentifiers);
                }
                // Try finding duplicated message identifiers.
                if (retVal) {
                    std::sort(std::begin(numericalMessageIdentifiers), std::end(numericalMessageIdentifiers));
                    int32_t duplicatedMessageIdentifier{-1};
                    for (auto it{std::begin(numericalMessageIdentifiers)}; it != std::end(numericalMessageIdentifiers); it++) {
                        if (it + 1 != std::end(numericalMessageIdentifiers)) {
                            if (std::find(it + 1, std::end(numericalMessageIdentifiers), *it) != std::end(numericalMessageIdentifiers)) {
                                duplicatedMessageIdentifier = *it;
                            }
                        }
                    }
                    retVal &= (-1 == duplicatedMessageIdentifier);
                    if (!retVal) {
                        std::cerr << "[cluon::MessageParser] Found duplicated numerical message identifier: " << duplicatedMessageIdentifier << '\n';
                    }
                }
                // Try finding duplicated message names.
                if (retVal) {
                    std::sort(std::begin(messageNames), std::end(messageNames));
                    std::string duplicatedMessageName;
                    for (auto it{std::begin(messageNames)}; it != std::end(messageNames); it++) {
                        if (it + 1 != std::end(messageNames)) {
                            if (std::find(it + 1, std::end(messageNames), *it) != std::end(messageNames)) {
                                duplicatedMessageName = *it;
                            }
                        }
                    }
                    retVal &= (duplicatedMessageName.empty());
                    if (!retVal) {
                        std::cerr << "[cluon::MessageParser] Found duplicated message name '" << duplicatedMessageName << "'" << '\n';
                    }
                }
            }
            // Second, we need to visit the children of AST node MESSAGE_DECLARATION.
            if ("MESSAGE_DECLARATION" == ast.name) {
                fieldNames.clear();
                numericalFieldIdentifiers.clear();
                prefix = "";
                retVal = true;

                for (const auto &node : ast.nodes) {
                    if ("MESSAGE_IDENTIFIER" == node->name) {
                        prefix = node->token;
                        messageNames.push_back(::stringtoolbox::trim(prefix));
                    } else if ("NATURAL_NUMBER" == node->name) {
                        numericalMessageIdentifiers.push_back(std::stoi(node->token));
                    } else if ("PRIMITIVE_FIELD" == node->name) {
                        retVal &= checkForUniqueFieldNames(*node, prefix, messageNames, fieldNames, numericalMessageIdentifiers, numericalFieldIdentifiers);
                    }
                }

                // Try finding duplicated numerical field identifiers.
                if (retVal) {
                    std::sort(std::begin(numericalFieldIdentifiers), std::end(numericalFieldIdentifiers));
                    int32_t duplicatedFieldIdentifier{-1};
                    for (auto it{std::begin(numericalFieldIdentifiers)}; it != std::end(numericalFieldIdentifiers); it++) {
                        if (it + 1 != std::end(numericalFieldIdentifiers)) {
                            if (std::find(it + 1, std::end(numericalFieldIdentifiers), *it) != std::end(numericalFieldIdentifiers)) {
                                duplicatedFieldIdentifier = *it;
                            }
                        }
                    }
                    retVal &= (-1 == duplicatedFieldIdentifier);
                    if (!retVal) {
                        std::cerr << "[cluon::MessageParser] Found duplicated numerical field identifier in message "
                                  << "'" << ::stringtoolbox::trim(prefix) << "': " << duplicatedFieldIdentifier << '\n';
                    }
                }
                // Try finding duplicated field names.
                if (retVal) {
                    std::sort(std::begin(fieldNames), std::end(fieldNames));
                    std::string duplicatedFieldName;
                    for (auto it{std::begin(fieldNames)}; it != std::end(fieldNames); it++) {
                        if (it + 1 != std::end(fieldNames)) {
                            if (std::find(it + 1, std::end(fieldNames), *it) != std::end(fieldNames)) {
                                duplicatedFieldName = *it;
                            }
                        }
                    }
                    retVal &= (duplicatedFieldName.empty());
                    if (!retVal) {
                        std::cerr << "[cluon::MessageParser] Found duplicated field name in message '" << ::stringtoolbox::trim(prefix) << "': '"
                                  << duplicatedFieldName << "'" << '\n';
                    }
                }
            }
            // Within AST node MESSAGE_DECLARATION, we have PRIMITIVE_FIELD from
            // which we need to extract the field "token".
            if (ast.name == "PRIMITIVE_FIELD") {
                // Extract the value of entry "IDENTIFIER".
                auto nodeIdentifier = std::find_if(std::begin(ast.nodes), std::end(ast.nodes), [](auto a) { return (a->name == "IDENTIFIER"); });
                if (nodeIdentifier != std::end(ast.nodes)) {
                    fieldNames.push_back((*nodeIdentifier)->token);
                }

                // Visit this node's children to check for duplicated numerical identifiers.
                for (const auto &node : ast.nodes) {
                    retVal &= checkForUniqueFieldNames(*node, prefix, messageNames, fieldNames, numericalMessageIdentifiers, numericalFieldIdentifiers);
                }
            }
            // Within AST node PRIMITIVE_FIELD, we have PRIMITIVE_FIELD_OPTIONS from
            // which we need to extract the field "token".
            if (ast.name == "PRIMITIVE_FIELD_OPTIONS") {
                // Extract the value of entry "IDENTIFIER".
                auto nodeNumericalFieldIdentifier
                    = std::find_if(std::begin(ast.nodes), std::end(ast.nodes), [](auto a) { return (a->name == "NATURAL_NUMBER"); });
                if (nodeNumericalFieldIdentifier != std::end(ast.nodes)) {
                    numericalFieldIdentifiers.push_back(std::stoi((*nodeNumericalFieldIdentifier)->token));
                }
            }

            return retVal;
        };

    ////////////////////////////////////////////////////////////////////////////

    // Function to transform AST into list of MetaMessages.
    std::function<void(const peg::Ast &, std::vector<MetaMessage> &)> transform2MetaMessages
        = [](const peg::Ast &ast, std::vector<MetaMessage> &listOfMetaMessages) {
              // "Inner"-lambda to handle various types of message declarations.
              auto createMetaMessage = [](const peg::Ast &_node, std::string _packageName) -> MetaMessage {
                  MetaMessage mm;
                  mm.packageName(::stringtoolbox::trim(_packageName));
                  uint32_t fieldIdentifierCounter{0};
                  for (const auto &e : _node.nodes) {
                      if ("MESSAGE_IDENTIFIER" == e->name) {
                          std::string _messageName = e->token;
                          mm.messageName(::stringtoolbox::trim(_messageName));
                      } else if ("NATURAL_NUMBER" == e->name) {
                          mm.messageIdentifier(std::stoi(e->token));
                      } else if ("PRIMITIVE_FIELD" == e->name) {
                          std::string _fieldName;
                          auto fieldName = std::find_if(std::begin(e->nodes), std::end(e->nodes), [](auto a) { return (a->name == "IDENTIFIER"); });
                          if (fieldName != std::end(e->nodes)) {
                              _fieldName = (*fieldName)->token;
                          }

                          std::string _fieldDataType;
                          auto fieldDataType = std::find_if(std::begin(e->nodes), std::end(e->nodes), [](auto a) { return (a->name == "PRIMITIVE_TYPE"); });
                          if (fieldDataType != std::end(e->nodes)) {
                              _fieldDataType = (*fieldDataType)->token;
                          }

                          fieldIdentifierCounter++; // Automatically count expected field identifiers in case of missing
                                                    // field options.
                          std::string _fieldIdentifier;
                          auto fieldIdentifier = std::find_if(std::begin(e->nodes), std::end(e->nodes), [](auto a) { return (a->name == "NATURAL_NUMBER"); });
                          if (fieldIdentifier != std::end(e->nodes)) {
                              _fieldIdentifier = (*fieldIdentifier)->token;
                          }

                          std::string _fieldDefaultInitializerValue;
                          auto primitiveFieldOptions
                              = std::find_if(std::begin(e->nodes), std::end(e->nodes), [](auto a) { return (a->name == "PRIMITIVE_FIELD_OPTIONS"); });
                          if (primitiveFieldOptions != std::end(e->nodes)) {
                              for (const auto &f : (*primitiveFieldOptions)->nodes) {
                                  if ("NATURAL_NUMBER" != f->name) {
                                      if ("STRING" == f->name) {
                                          _fieldDefaultInitializerValue = "\"" + f->token + "\""; // NOLINT
                                      } else if ("CHARACTER" == f->name) {
                                          _fieldDefaultInitializerValue = "'" + f->token + "'";
                                      } else {
                                          _fieldDefaultInitializerValue = f->token;
                                      }
                                  }
                              }
                          }

                          std::map<std::string, MetaMessage::MetaField::MetaFieldDataTypes> STRING_TO_DATATYPE_MAP = {
                              {"bool", MetaMessage::MetaField::BOOL_T},
                              {"char", MetaMessage::MetaField::CHAR_T},
                              {"uint8", MetaMessage::MetaField::UINT8_T},
                              {"int8", MetaMessage::MetaField::INT8_T},
                              {"uint16", MetaMessage::MetaField::UINT16_T},
                              {"int16", MetaMessage::MetaField::INT16_T},
                              {"uint32", MetaMessage::MetaField::UINT32_T},
                              {"int32", MetaMessage::MetaField::INT32_T},
                              {"uint64", MetaMessage::MetaField::UINT64_T},
                              {"int64", MetaMessage::MetaField::INT64_T},
                              {"float", MetaMessage::MetaField::FLOAT_T},
                              {"double", MetaMessage::MetaField::DOUBLE_T},
                              {"string", MetaMessage::MetaField::STRING_T},
                              {"bytes", MetaMessage::MetaField::BYTES_T},
                          };

                          MetaMessage::MetaField mf;
                          if (0 < STRING_TO_DATATYPE_MAP.count(_fieldDataType)) {
                              mf.fieldDataType(STRING_TO_DATATYPE_MAP[_fieldDataType]);
                          } else {
                              mf.fieldDataType(MetaMessage::MetaField::MESSAGE_T);
                          }
                          mf.fieldDataTypeName(::stringtoolbox::trim(_fieldDataType));
                          mf.fieldName(::stringtoolbox::trim(_fieldName));
                          mf.fieldIdentifier(
                              (!_fieldIdentifier.empty() ? static_cast<uint32_t>(std::stoi(::stringtoolbox::trim(_fieldIdentifier))) : fieldIdentifierCounter));
                          mf.defaultInitializationValue(_fieldDefaultInitializerValue);
                          mm.add(std::move(mf));
                      }
                  }
                  return mm;
              };

              ////////////////////////////////////////////////////////////////////////

              // Case: "package XYZ" present.
              if ("MESSAGES_SPECIFICATION" == ast.name) {
                  // Extract the value of entry "PACKAGE_IDENTIFIER".
                  auto nodeIdentifier = std::find_if(std::begin(ast.nodes), std::end(ast.nodes), [](auto a) { return (a->name == "PACKAGE_IDENTIFIER"); });
                  std::string packageName;
                  if (nodeIdentifier != std::end(ast.nodes)) {
                      packageName = (*nodeIdentifier)->token;
                  }

                  // Extract the value of entry "MESSAGE_DECLARATION".
                  for (const auto &node : ast.nodes) {
                      if (node->name == "MESSAGE_DECLARATION") {
                          listOfMetaMessages.emplace_back(createMetaMessage(*node, packageName));
                      }
                  }
              } else {
                  // In case we only have one single message and no package.
                  listOfMetaMessages.emplace_back(createMetaMessage(ast, ""));
              }
          };

    ////////////////////////////////////////////////////////////////////////////

    peg::parser p(grammarMessageSpecificationLanguage);
    p.enable_ast();
    p.log = [](size_t row, size_t col, const std::string &msg) {
        std::cerr << "[cluon::MessageParser] Parsing error:" << row << ":" << col << ": " << msg << '\n';
    };

    std::pair<std::vector<MetaMessage>, MessageParserErrorCodes> retVal{};
    std::string inputWithoutComments{input};
    try {
        const std::string MATCH_COMMENTS_REGEX = R"(/\*([\s\S]*?)\*/|//.*)";
        inputWithoutComments                   = std::regex_replace(input, std::regex(MATCH_COMMENTS_REGEX), ""); // NOLINT
    } catch (std::regex_error &) {                                                                                // LCOV_EXCL_LINE
    } catch (std::bad_cast &) {                                                                                   // LCOV_EXCL_LINE
    }
    try {
        std::vector<MetaMessage> listOfMetaMessages{};
        std::shared_ptr<peg::Ast> ast{};
        if (p.parse(inputWithoutComments.c_str(), ast)) {
            ast = peg::AstOptimizer(true).optimize(ast);
            {
                std::string tmpPrefix;
                std::vector<std::string> tmpMessageNames{};
                std::vector<std::string> tmpFieldNames{};
                std::vector<int32_t> tmpNumericalMessageIdentifiers{};
                std::vector<int32_t> tmpNumericalFieldIdentifiers{};
                if (check4UniqueFieldNames(*ast, tmpPrefix, tmpMessageNames, tmpFieldNames, tmpNumericalMessageIdentifiers, tmpNumericalFieldIdentifiers)) {
                    transform2MetaMessages(*ast, listOfMetaMessages);
                    retVal = {listOfMetaMessages, MessageParserErrorCodes::NO_MESSAGEPARSER_ERROR};
                } else {
                    retVal = {listOfMetaMessages, MessageParserErrorCodes::DUPLICATE_IDENTIFIERS};
                }
            }
        } else {
            retVal = {listOfMetaMessages, MessageParserErrorCodes::SYNTAX_ERROR};
        }
    } catch (std::bad_cast &) { // LCOV_EXCL_LINE
    }
    return retVal;
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/TerminateHandler.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace cluon {

inline void cluon_handleExit() {
    TerminateHandler::instance().isTerminated.store(true);
}

inline void cluon_handleSignal(int32_t /*signal*/) {       // LCOV_EXCL_LINE
    TerminateHandler::instance().isTerminated.store(true); // LCOV_EXCL_LINE
}

inline TerminateHandler::TerminateHandler() noexcept {
    if (0 != std::atexit(cluon_handleExit)) {
        std::cerr << "[cluon::TerminateHandler] Failed to register cluon_exitHandler()." << std::endl; // LCOV_EXCL_LINE
    }

#ifdef WIN32
    if (SIG_ERR == ::signal(SIGINT, &cluon_handleSignal)) {
        std::cerr << "[cluon::TerminateHandler] Failed to register signal SIGINT." << std::endl;
    }
    if (SIG_ERR == ::signal(SIGTERM, &cluon_handleSignal)) {
        std::cerr << "[cluon::TerminateHandler] Failed to register signal SIGTERM." << std::endl;
    }
#else
    std::memset(&m_signalHandler, 0, sizeof(m_signalHandler));
    m_signalHandler.sa_handler = &cluon_handleSignal;

    if (::sigaction(SIGINT, &m_signalHandler, NULL) < 0) {
        std::cerr << "[cluon::TerminateHandler] Failed to register signal SIGINT." << std::endl; // LCOV_EXCL_LINE
    }
    if (::sigaction(SIGTERM, &m_signalHandler, NULL) < 0) {
        std::cerr << "[cluon::TerminateHandler] Failed to register signal SIGTERM." << std::endl; // LCOV_EXCL_LINE
    }
#endif
}

} // namespace cluon
/*
 * Copyright (C) 2019  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/IPv4Tools.hpp"

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for WSAStartUp
    #include <ws2tcpip.h>
    #include <iostream>
#else
    #include <arpa/inet.h>
    #include <netdb.h>

    #if defined(BSD)
        #include <netinet/in.h>
        #include <sys/socket.h>
    #endif
#endif
// clang-format on

#include <cstring>

namespace cluon {

inline std::string getIPv4FromHostname(const std::string &hostname) noexcept {
#ifdef WIN32
    // Load Winsock 2.2 DLL.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[cluon::getIPv4FromHostname] Error while calling WSAStartUp: " << WSAGetLastError() << std::endl;
    }
#endif
    std::string result{""};
    if (!hostname.empty()) {
        struct addrinfo hint;
        {
            std::memset(&hint, 1, sizeof(struct addrinfo));
            hint.ai_flags = AI_CANONNAME;
            hint.ai_family = AF_INET;
            hint.ai_socktype = 0;
            hint.ai_protocol = 0;
            hint.ai_addrlen = 0;
            hint.ai_canonname = nullptr;
            hint.ai_addr = nullptr;
            hint.ai_next = nullptr;
        }

        struct addrinfo *listOfHosts{nullptr};
        if (0 == getaddrinfo(hostname.c_str(), nullptr, &hint, &listOfHosts)) {
            for(struct addrinfo *e = listOfHosts; nullptr != listOfHosts; listOfHosts = listOfHosts->ai_next) {
                if (nullptr != e) {
                    if (AF_INET == e->ai_family) {
                        struct sockaddr_in *sinp = reinterpret_cast<struct sockaddr_in*>(e->ai_addr);
                        char buf[INET_ADDRSTRLEN];
                        const char *addr = inet_ntop(AF_INET, &sinp->sin_addr, buf, INET_ADDRSTRLEN);
                        if ( (nullptr != addr) && (result.empty()) ) {
                            result = std::string(addr);
                            break;
                        }
                    }
                }
            }
        }

        if (nullptr != listOfHosts) {
            freeaddrinfo(listOfHosts);
        }
    }
#ifdef WIN32
    WSACleanup();
#endif
    return result;
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/UDPSender.hpp"
//#include "cluon/IPv4Tools.hpp"
//#include "cluon/UDPPacketSizeConstraints.hpp"

// clang-format off
#ifndef WIN32
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <netdb.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif
// clang-format on

#include <cerrno>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

namespace cluon {

inline UDPSender::UDPSender(const std::string &sendToAddress, uint16_t sendToPort) noexcept
    : m_socketMutex()
    , m_sendToAddress() {
    // Decompose given address into tokens to check validity with numerical IPv4 address.
    std::string tmp{cluon::getIPv4FromHostname(sendToAddress)};
    std::replace(tmp.begin(), tmp.end(), '.', ' ');
    std::istringstream sstr{tmp};
    std::vector<int> sendToAddressTokens{std::istream_iterator<int>(sstr), std::istream_iterator<int>()};

    if (!sendToAddress.empty() && (4 == sendToAddressTokens.size())
        && !(std::end(sendToAddressTokens) != std::find_if(sendToAddressTokens.begin(), sendToAddressTokens.end(), [](int a) { return (a < 0) || (a > 255); }))
        && (0 < sendToPort)) {
        ::memset(&m_sendToAddress, 0, sizeof(m_sendToAddress));
        m_sendToAddress.sin_addr.s_addr = ::inet_addr(sendToAddress.c_str());
        m_sendToAddress.sin_family      = AF_INET;
        m_sendToAddress.sin_port        = htons(sendToPort);

#ifdef WIN32
        // Load Winsock 2.2 DLL.
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[cluon::UDPSender] Error while calling WSAStartUp: " << WSAGetLastError() << std::endl;
        }
#endif

        m_socket = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

#ifndef WIN32
        // Check whether given address is a broadcast address.
        bool isBroadcast{false};
        {
            isBroadcast |= (sendToAddress == "255.255.255.255");
            if (!isBroadcast) {
                struct ifaddrs *ifaddr{nullptr};
                if (0 == getifaddrs(&ifaddr)) {
                    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                        if (NULL == ifa->ifa_addr) continue; // LCOV_EXCL_LINE
                        char broadcastAddress[NI_MAXHOST];
#ifdef __APPLE__
                        if (NULL == ifa->ifa_dstaddr) continue; // LCOV_EXCL_LINE
                        if (0 == ::getnameinfo(ifa->ifa_dstaddr,
                               sizeof(struct sockaddr_in),
                               broadcastAddress, NI_MAXHOST,
                               NULL, 0, NI_NUMERICHOST))
#else
                        if (NULL == ifa->ifa_ifu.ifu_broadaddr) continue; // LCOV_EXCL_LINE
                        if (0 == ::getnameinfo(ifa->ifa_ifu.ifu_broadaddr,
                               sizeof(struct sockaddr_in),
                               broadcastAddress, NI_MAXHOST,
                               NULL, 0, NI_NUMERICHOST))
#endif
                        {
                             std::string _tmp{broadcastAddress};
                             isBroadcast |= (_tmp.compare(sendToAddress) == 0);
                        }
                    }
                    freeifaddrs(ifaddr);
                }
            }
        }
#endif

#ifndef WIN32
        if (!(m_socket < 0) && isBroadcast) {
            // Enabling broadcast.
            uint32_t YES = 1;
            // clang-format off
            auto retVal = ::setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char *>(&YES), sizeof(YES)); // NOLINT
            // clang-format on
            if (0 > retVal) {
#ifdef WIN32 // LCOV_EXCL_LINE
                auto errorCode = WSAGetLastError();
#else
                auto errorCode = errno; // LCOV_EXCL_LINE
#endif                                  // LCOV_EXCL_LINE
                std::cerr << "[cluon::UDPSender] Failed to perform socket operation: "; // LCOV_EXCL_LINE
#ifdef WIN32 // LCOV_EXCL_LINE
                std::cerr << errorCode << std::endl; 
#else
                std::cerr << ::strerror(errorCode) << " (" << errorCode << ")" << std::endl; // LCOV_EXCL_LINE
#endif // LCOV_EXCL_LINE
            }
        }
#endif

        // Bind to random address/port but store sender port.
        if (!(m_socket < 0)) {
            struct sockaddr_in sendFromAddress;
            std::memset(&sendFromAddress, 0, sizeof(sendFromAddress));
            sendFromAddress.sin_family = AF_INET;
            sendFromAddress.sin_port   = 0;                                                                              // Randomly choose a port to bind.
            if (0 == ::bind(m_socket, reinterpret_cast<struct sockaddr *>(&sendFromAddress), sizeof(sendFromAddress))) { // NOLINT
                struct sockaddr tmpAddr;
                socklen_t length = sizeof(tmpAddr);
                if (0 == ::getsockname(m_socket, &tmpAddr, &length)) {
                    struct sockaddr_in tmpAddrIn;
                    std::memcpy(&tmpAddrIn, &tmpAddr, sizeof(tmpAddrIn)); /* Flawfinder: ignore */ // NOLINT
                    m_portToSentFrom = ntohs(tmpAddrIn.sin_port);
                }
            }
        }

#ifdef WIN32
        if (m_socket < 0) {
            std::cerr << "[cluon::UDPSender] Error while creating socket: " << WSAGetLastError() << std::endl;
            WSACleanup();
        }
#endif
    }
}

inline UDPSender::~UDPSender() noexcept {
    if (!(m_socket < 0)) {
#ifdef WIN32
        ::shutdown(m_socket, SD_BOTH);
        ::closesocket(m_socket);
        WSACleanup();
#else
        ::shutdown(m_socket, SHUT_RDWR); // Disallow further read/write operations.
        ::close(m_socket);
#endif
    }
    m_socket = -1;
}

inline uint16_t UDPSender::getSendFromPort() const noexcept {
    return m_portToSentFrom;
}

inline std::pair<ssize_t, int32_t> UDPSender::send(std::string &&data) const noexcept {
    if (-1 == m_socket) {
        return {-1, EBADF};
    }

    if (data.empty()) {
        return {0, 0};
    }

    constexpr uint16_t MAX_LENGTH = static_cast<uint16_t>(UDPPacketSizeConstraints::MAX_SIZE_UDP_PACKET)
                                    - static_cast<uint16_t>(UDPPacketSizeConstraints::SIZE_IPv4_HEADER)
                                    - static_cast<uint16_t>(UDPPacketSizeConstraints::SIZE_UDP_HEADER);
    if (MAX_LENGTH < data.size()) {
        return {-1, E2BIG};
    }

    std::lock_guard<std::mutex> lck(m_socketMutex);
    ssize_t bytesSent = ::sendto(m_socket,
                                 data.c_str(),
                                 data.length(),
                                 0,
                                 reinterpret_cast<const struct sockaddr *>(&m_sendToAddress), // NOLINT
                                 sizeof(m_sendToAddress));

    return {bytesSent, (0 > bytesSent ? errno : 0)};
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/UDPReceiver.hpp"
//#include "cluon/IPv4Tools.hpp"
//#include "cluon/TerminateHandler.hpp"
//#include "cluon/UDPPacketSizeConstraints.hpp"

// clang-format off
#ifdef WIN32
    #include <cstdio>
    #include <cerrno>

    #include <winsock2.h>
    #include <iphlpapi.h>
    #include <ws2tcpip.h>

    #include <iostream>
#else
    #ifdef __linux__
        #include <linux/sockios.h>
    #endif

    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

#ifndef WIN32
    #include <ifaddrs.h>
    #include <netdb.h>
#endif
// clang-format on

#include <cstring>
#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

namespace cluon {

inline UDPReceiver::UDPReceiver(const std::string &receiveFromAddress,
                         uint16_t receiveFromPort,
                         std::function<void(std::string &&, std::string &&, std::chrono::system_clock::time_point &&)> delegate,
                         uint16_t localSendFromPort) noexcept
    : m_localSendFromPort(localSendFromPort)
    , m_receiveFromAddress()
    , m_mreq()
    , m_readFromSocketThread()
    , m_delegate(std::move(delegate)) {
    // Decompose given address string to check validity with numerical IPv4 address.
    std::string tmp{cluon::getIPv4FromHostname(receiveFromAddress)};
    std::replace(tmp.begin(), tmp.end(), '.', ' ');
    std::istringstream sstr{tmp};
    std::vector<int> receiveFromAddressTokens{std::istream_iterator<int>(sstr), std::istream_iterator<int>()};

    if ((!receiveFromAddress.empty()) && (4 == receiveFromAddressTokens.size())
        && !(std::end(receiveFromAddressTokens)
             != std::find_if(receiveFromAddressTokens.begin(), receiveFromAddressTokens.end(), [](int a) { return (a < 0) || (a > 255); }))
        && (0 < receiveFromPort)) {
        // Check for valid IP address.
        struct sockaddr_in tmpSocketAddress {};
        const bool isValid = (0 < ::inet_pton(AF_INET, receiveFromAddress.c_str(), &(tmpSocketAddress.sin_addr))) && (224 > receiveFromAddressTokens[0] || 255 == receiveFromAddressTokens[0]); // Accept regular IP addresses (ie., non-multicast addesses and the network-wide broadcast address 255.255.255.255.

        // Check for UDP multicast, i.e., IP address range [225.0.0.1 - 239.255.255.255].
        m_isMulticast = (((224 < receiveFromAddressTokens[0]) && (receiveFromAddressTokens[0] <= 239))
                         && ((0 <= receiveFromAddressTokens[1]) && (receiveFromAddressTokens[1] <= 255))
                         && ((0 <= receiveFromAddressTokens[2]) && (receiveFromAddressTokens[2] <= 255))
                         && ((1 <= receiveFromAddressTokens[3]) && (receiveFromAddressTokens[3] <= 255)));

#ifndef WIN32
        // Check whether given address is a broadcast address.
        bool isBroadcast{false};
        {
            isBroadcast |= (receiveFromAddress == "255.255.255.255");
            if (!isBroadcast) {
                struct ifaddrs *ifaddr{nullptr};
                if (0 == getifaddrs(&ifaddr)) {
                    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                        if (NULL == ifa->ifa_addr) continue; // LCOV_EXCL_LINE

                        if (ifa->ifa_addr->sa_family == AF_INET) {
                            char broadcastAddress[NI_MAXHOST];
#ifdef __APPLE__
                            if (NULL == ifa->ifa_dstaddr) continue; // LCOV_EXCL_LINE
                            if (0 == ::getnameinfo(ifa->ifa_dstaddr,
                                   sizeof(struct sockaddr_in),
                                   broadcastAddress, NI_MAXHOST,
                                   NULL, 0, NI_NUMERICHOST))
#else
                            if (NULL == ifa->ifa_ifu.ifu_broadaddr) continue; // LCOV_EXCL_LINE
                            if (0 == ::getnameinfo(ifa->ifa_ifu.ifu_broadaddr,
                                   sizeof(struct sockaddr_in),
                                   broadcastAddress, NI_MAXHOST,
                                   NULL, 0, NI_NUMERICHOST))
#endif
                            {
                                 std::string _tmp{broadcastAddress};
                                 isBroadcast |= (_tmp.compare(receiveFromAddress) == 0);
                            }
                        }
                    }
                    freeifaddrs(ifaddr);
                }
            }
        }
#endif

        std::memset(&m_receiveFromAddress, 0, sizeof(m_receiveFromAddress));
#ifdef WIN32
        // According to http://www.sockets.com/err_lst1.htm, the binding is
        // different on Windows opposed to POSIX when using the real address
        // here; thus, we need to use INADDR_ANY.
        m_receiveFromAddress.sin_addr.s_addr = (m_isMulticast ? htonl(INADDR_ANY) : ::inet_addr(receiveFromAddress.c_str()));
#else
        m_receiveFromAddress.sin_addr.s_addr = ::inet_addr(receiveFromAddress.c_str());
#endif
        m_receiveFromAddress.sin_family = AF_INET;
        m_receiveFromAddress.sin_port   = htons(receiveFromPort);

#ifdef WIN32
        // Load Winsock 2.2 DLL.
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[cluon::UDPReceiver] Error while calling WSAStartUp: " << WSAGetLastError() << std::endl;
        }
#endif

        m_socket = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

#ifdef WIN32
        if (m_socket < 0) {
            std::cerr << "[cluon::UDPReceiver] Error while creating socket: " << WSAGetLastError() << std::endl;
            WSACleanup();
        }
#endif

        if (!(m_socket < 0)) {
            // Allow reusing of ports by multiple calls with same address/port.
            uint32_t YES = 1;
            // clang-format off
            auto retVal = ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&YES), sizeof(YES)); // NOLINT
            // clang-format on
            if (0 > retVal) {
#ifdef WIN32 // LCOV_EXCL_LINE
                auto errorCode = WSAGetLastError();
#else
                auto errorCode = errno; // LCOV_EXCL_LINE
#endif                                  // LCOV_EXCL_LINE
                closeSocket(errorCode); // LCOV_EXCL_LINE
            }
        }

#ifndef WIN32
        if (!(m_socket < 0) && isBroadcast) {
            // Enabling broadcast.
            uint32_t YES = 1;
            // clang-format off
            auto retVal = ::setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char *>(&YES), sizeof(YES)); // NOLINT
            // clang-format on
            if (0 > retVal) {
#ifdef WIN32 // LCOV_EXCL_LINE
                auto errorCode = WSAGetLastError();
#else
                auto errorCode = errno; // LCOV_EXCL_LINE
#endif                                  // LCOV_EXCL_LINE
                closeSocket(errorCode); // LCOV_EXCL_LINE
            }
        }
#endif

        if (!(m_socket < 0)) {
            // Trying to enable non_blocking mode.
#ifdef WIN32 // LCOV_EXCL_LINE
            u_long nonBlocking = 1;
            m_isBlockingSocket = !(NO_ERROR == ::ioctlsocket(m_socket, FIONBIO, &nonBlocking));
#else
            const int FLAGS    = ::fcntl(m_socket, F_GETFL, 0);
            m_isBlockingSocket = !(0 == ::fcntl(m_socket, F_SETFL, FLAGS | O_NONBLOCK));
#endif
        }

        if (!(m_socket < 0)) {
            // Trying to enable non_blocking mode.
#ifdef WIN32 // LCOV_EXCL_LINE
            u_long nonBlocking = 1;
            m_isBlockingSocket = !(NO_ERROR == ::ioctlsocket(m_socket, FIONBIO, &nonBlocking));
#else
            const int FLAGS    = ::fcntl(m_socket, F_GETFL, 0);
            m_isBlockingSocket = !(0 == ::fcntl(m_socket, F_SETFL, FLAGS | O_NONBLOCK));
#endif
        }

        if (!(m_socket < 0)) {
            // Try setting receiving buffer.
            int recvBuffer{26214400};
            auto retVal = ::setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char *>(&recvBuffer), sizeof(recvBuffer));
            if (retVal < 0) {
#ifdef WIN32 // LCOV_EXCL_LINE
                auto errorCode = WSAGetLastError();
#else
                auto errorCode = errno; // LCOV_EXCL_LINE
#endif                                                                                                                                       // LCOV_EXCL_LINE
                std::cerr << "[cluon::UDPReceiver] Error while trying to set SO_RCVBUF to " << recvBuffer << ": " << errorCode << std::endl; // LCOV_EXCL_LINE
            }
        }

        if (!(m_socket < 0)) {
            // Bind to receive address/port.
            // clang-format off
            auto retVal = ::bind(m_socket, reinterpret_cast<struct sockaddr *>(&m_receiveFromAddress), sizeof(m_receiveFromAddress)); // NOLINT
            // clang-format on
            if (0 > retVal) {
#ifdef WIN32 // LCOV_EXCL_LINE
                auto errorCode = WSAGetLastError();
#else
                auto errorCode = errno; // LCOV_EXCL_LINE
#endif                                  // LCOV_EXCL_LINE
                closeSocket(errorCode); // LCOV_EXCL_LINE
            }
        }

        if (!(m_socket < 0)) {
            if (m_isMulticast) {
                // Join the multicast group.
                m_mreq.imr_multiaddr.s_addr = ::inet_addr(receiveFromAddress.c_str());
                m_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
                // clang-format off
                auto retval                 = ::setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char *>(&m_mreq), sizeof(m_mreq)); // NOLINT
                // clang-format on
                if (0 > retval) { // LCOV_EXCL_LINE
#ifdef WIN32                      // LCOV_EXCL_LINE
                    closeSocket(WSAGetLastError());
#else
                    closeSocket(errno); // LCOV_EXCL_LINE
#endif // LCOV_EXCL_LINE
                }
            } else if (!isValid) {
                closeSocket(EBADF);
            }
        }

        // Fill list of local IP address to avoid sending data to ourselves.
        if (!(m_socket < 0)) {
#ifdef WIN32
            DWORD size{0};
            if (ERROR_BUFFER_OVERFLOW == GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &size)) {
                PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(size));
                if (ERROR_SUCCESS == GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters, &size)) {
                    for (PIP_ADAPTER_ADDRESSES adapter = adapters; nullptr != adapter; adapter = adapter->Next) {
                        for (PIP_ADAPTER_UNICAST_ADDRESS unicastAddress = adapter->FirstUnicastAddress; unicastAddress != NULL;
                             unicastAddress                             = unicastAddress->Next) {
                            if (AF_INET == unicastAddress->Address.lpSockaddr->sa_family) {
                                ::getnameinfo(unicastAddress->Address.lpSockaddr, unicastAddress->Address.iSockaddrLength, nullptr, 0, NULL, 0, NI_NUMERICHOST);
                                std::memcpy(&tmpSocketAddress, unicastAddress->Address.lpSockaddr, sizeof(tmpSocketAddress)); /* Flawfinder: ignore */ // NOLINT
                                const unsigned long LOCAL_IP = tmpSocketAddress.sin_addr.s_addr;
                                m_listOfLocalIPAddresses.insert(LOCAL_IP);
                            }
                        }
                    }
                }
                free(adapters);
            }
#else
            struct ifaddrs *interfaceAddress;
            if (0 == ::getifaddrs(&interfaceAddress)) {
                for (struct ifaddrs *it = interfaceAddress; nullptr != it; it = it->ifa_next) {
                    if ((nullptr != it->ifa_addr) && (it->ifa_addr->sa_family == AF_INET)) {
                        if (0 == ::getnameinfo(it->ifa_addr, sizeof(struct sockaddr_in), nullptr, 0, nullptr, 0, NI_NUMERICHOST)) {
                            std::memcpy(&tmpSocketAddress, it->ifa_addr, sizeof(tmpSocketAddress)); /* Flawfinder: ignore */ // NOLINT
                            const unsigned long LOCAL_IP = tmpSocketAddress.sin_addr.s_addr;
                            m_listOfLocalIPAddresses.insert(LOCAL_IP);
                        }
                    }
                }
                ::freeifaddrs(interfaceAddress);
            }
#endif
        }

        if (!(m_socket < 0)) {
            // Constructing the receiving thread could fail.
            try {
                m_readFromSocketThread = std::thread(&UDPReceiver::readFromSocket, this);

                // Let the operating system spawn the thread.
                using namespace std::literals::chrono_literals; // NOLINT
                do { std::this_thread::sleep_for(1ms); } while (!m_readFromSocketThreadRunning.load());
            } catch (...) { closeSocket(ECHILD); } // LCOV_EXCL_LINE

            try {
                m_pipeline = std::make_shared<cluon::NotifyingPipeline<PipelineEntry>>(
                    [this](PipelineEntry &&entry) { this->m_delegate(std::move(entry.m_data), std::move(entry.m_from), std::move(entry.m_sampleTime)); });
                if (m_pipeline) {
                    // Let the operating system spawn the thread.
                    using namespace std::literals::chrono_literals; // NOLINT
                    do { std::this_thread::sleep_for(1ms); } while (!m_pipeline->isRunning());
                }
            } catch (...) { closeSocket(ECHILD); } // LCOV_EXCL_LINE
        }
    }
}

inline UDPReceiver::~UDPReceiver() noexcept {
    {
        m_readFromSocketThreadRunning.store(false);

        // Joining the thread could fail.
        try {
            if (m_readFromSocketThread.joinable()) {
                m_readFromSocketThread.join();
            }
        } catch (...) {} // LCOV_EXCL_LINE
    }

    m_pipeline.reset();

    closeSocket(0);
}

inline void UDPReceiver::closeSocket(int errorCode) noexcept {
    if (0 != errorCode) {
        std::cerr << "[cluon::UDPReceiver] Failed to perform socket operation: ";
#ifdef WIN32
        std::cerr << errorCode << std::endl;
#else
        std::cerr << ::strerror(errorCode) << " (" << errorCode << ")" << std::endl;
#endif
    }

    if (!(m_socket < 0)) {
        if (m_isMulticast) {
            // clang-format off
            auto retVal = ::setsockopt(m_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<char *>(&m_mreq), sizeof(m_mreq)); // NOLINT
            // clang-format on
            if (0 > retVal) {                                                                         // LCOV_EXCL_LINE
                std::cerr << "[cluon::UDPReceiver] Failed to drop multicast membership" << std::endl; // LCOV_EXCL_LINE
            }
        }

#ifdef WIN32
        ::shutdown(m_socket, SD_BOTH);
        ::closesocket(m_socket);
        WSACleanup();
#else
        ::shutdown(m_socket, SHUT_RDWR); // Disallow further read/write operations.
        ::close(m_socket);
#endif
    }
    m_socket = -1;
}

inline bool UDPReceiver::isRunning() const noexcept {
    return (m_readFromSocketThreadRunning.load() && !TerminateHandler::instance().isTerminated.load());
}

inline void UDPReceiver::readFromSocket() noexcept {
    // Create buffer to store data from socket.
    constexpr uint16_t MAX_LENGTH = static_cast<uint16_t>(UDPPacketSizeConstraints::MAX_SIZE_UDP_PACKET)
                                    - static_cast<uint16_t>(UDPPacketSizeConstraints::SIZE_IPv4_HEADER)
                                    - static_cast<uint16_t>(UDPPacketSizeConstraints::SIZE_UDP_HEADER);
    std::array<char, MAX_LENGTH> buffer{};

    struct timeval timeout {};

    // Define file descriptor set to watch for read operations.
    fd_set setOfFiledescriptorsToReadFrom{};

    // Sender address and port.
    constexpr uint16_t MAX_ADDR_SIZE{1024};
    std::array<char, MAX_ADDR_SIZE> remoteAddress{};

    struct sockaddr_storage remote {};
    socklen_t addrLength{sizeof(remote)};

    // Indicate to main thread that we are ready.
    m_readFromSocketThreadRunning.store(true);

    while (m_readFromSocketThreadRunning.load()) {
        // Define timeout for select system call. The timeval struct must be
        // reinitialized for every select call as it might be modified containing
        // the actual time slept.
        timeout.tv_sec  = 0;
        timeout.tv_usec = 20 * 1000; // Check for new data with 50Hz.

        FD_ZERO(&setOfFiledescriptorsToReadFrom);          // NOLINT
        FD_SET(m_socket, &setOfFiledescriptorsToReadFrom); // NOLINT
        ::select(m_socket + 1, &setOfFiledescriptorsToReadFrom, nullptr, nullptr, &timeout);

        ssize_t totalBytesRead{0};
        if (FD_ISSET(m_socket, &setOfFiledescriptorsToReadFrom)) { // NOLINT
            ssize_t bytesRead{0};
            do {
                bytesRead = ::recvfrom(m_socket,
                                       buffer.data(),
                                       buffer.max_size(),
                                       0,
                                       reinterpret_cast<struct sockaddr *>(&remote), // NOLINT
                                       reinterpret_cast<socklen_t *>(&addrLength));  // NOLINT

                if ((0 < bytesRead) && (nullptr != m_delegate)) {
#ifdef __linux__
                    std::chrono::system_clock::time_point timestamp;
                    struct timeval receivedTimeStamp {};
                    if (0 == ::ioctl(m_socket, SIOCGSTAMP, &receivedTimeStamp)) { // NOLINT
                        // Transform struct timeval to C++ chrono.
                        std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> transformedTimePoint(
                            std::chrono::microseconds(receivedTimeStamp.tv_sec * 1000000L + receivedTimeStamp.tv_usec));
                        timestamp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(transformedTimePoint);
                    } else { // LCOV_EXCL_LINE
                        // In case the ioctl failed, fall back to chrono. // LCOV_EXCL_LINE
                        timestamp = std::chrono::system_clock::now(); // LCOV_EXCL_LINE
                    }
#else
                    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
#endif

                    // Transform sender address to C-string.
                    ::inet_ntop(remote.ss_family,
                                &((reinterpret_cast<struct sockaddr_in *>(&remote))->sin_addr), // NOLINT
                                remoteAddress.data(),
                                remoteAddress.max_size());
                    const unsigned long RECVFROM_IP{reinterpret_cast<struct sockaddr_in *>(&remote)->sin_addr.s_addr}; // NOLINT
                    const uint16_t RECVFROM_PORT{ntohs(reinterpret_cast<struct sockaddr_in *>(&remote)->sin_port)};    // NOLINT

                    // Check if the bytes actually came from us.
                    bool sentFromUs{false};
                    {
                        auto pos                   = m_listOfLocalIPAddresses.find(RECVFROM_IP);
                        const bool sentFromLocalIP = (pos != m_listOfLocalIPAddresses.end() && (*pos == RECVFROM_IP));
                        sentFromUs                 = sentFromLocalIP && (m_localSendFromPort == RECVFROM_PORT);
                    }

                    // Create a pipeline entry to be processed concurrently.
                    if (!sentFromUs) {
                        PipelineEntry pe;
                        pe.m_data       = std::string(buffer.data(), static_cast<size_t>(bytesRead));
                        pe.m_from       = std::string(remoteAddress.data()) + ':' + std::to_string(RECVFROM_PORT);
                        pe.m_sampleTime = timestamp;

                        // Store entry in queue.
                        if (m_pipeline) {
                            m_pipeline->add(std::move(pe));
                        }
                    }
                    totalBytesRead += bytesRead;
                }
            } while (!m_isBlockingSocket && (bytesRead > 0));
        }

        if (static_cast<int32_t>(totalBytesRead) > 0) {
            if (m_pipeline) {
                m_pipeline->notifyAll();
            }
        }
    }
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/TCPConnection.hpp"
//#include "cluon/IPv4Tools.hpp"
//#include "cluon/TerminateHandler.hpp"

// clang-format off
#ifdef WIN32
    #include <errno.h>
    #include <iostream>
#else
    #ifdef __linux__
        #include <linux/sockios.h>
    #endif

    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif
// clang-format on

#include <cstring>
#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

namespace cluon {

inline TCPConnection::TCPConnection(const int32_t &socket) noexcept
    : m_socket(socket)
    , m_newDataDelegate(nullptr)
    , m_connectionLostDelegate(nullptr) {
    if (!(m_socket < 0)) {
        startReadingFromSocket();
    }
}

inline TCPConnection::TCPConnection(const std::string &address,
                             uint16_t port,
                             std::function<void(std::string &&, std::chrono::system_clock::time_point &&)> newDataDelegate,
                             std::function<void()> connectionLostDelegate) noexcept
    : m_newDataDelegate(std::move(newDataDelegate))
    , m_connectionLostDelegate(std::move(connectionLostDelegate)) {
    // Decompose given address string to check validity with numerical IPv4 address.
    std::string resolvedHostname{cluon::getIPv4FromHostname(address)};
    std::string tmp{resolvedHostname};
    std::replace(tmp.begin(), tmp.end(), '.', ' ');
    std::istringstream sstr{tmp};
    std::vector<int> addressTokens{std::istream_iterator<int>(sstr), std::istream_iterator<int>()};

    if ((!addressTokens.empty()) && (4 == addressTokens.size())
        && !(std::end(addressTokens) != std::find_if(addressTokens.begin(), addressTokens.end(), [](int a) { return (a < 0) || (a > 255); })) && (0 < port)) {
        // Check for valid IP address.
        struct sockaddr_in tmpSocketAddress {};
        const bool isValid = (0 < ::inet_pton(AF_INET, resolvedHostname.c_str(), &(tmpSocketAddress.sin_addr)));
        if (isValid) {
            std::memset(&m_address, 0, sizeof(m_address));
            m_address.sin_addr.s_addr = ::inet_addr(resolvedHostname.c_str());
            m_address.sin_family      = AF_INET;
            m_address.sin_port        = htons(port);
#ifdef WIN32
            // Load Winsock 2.2 DLL.
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                std::cerr << "[cluon::TCPConnection] Error while calling WSAStartUp: " << WSAGetLastError() << std::endl;
            }
#endif

            m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

#ifdef WIN32
            if (m_socket < 0) {
                std::cerr << "[cluon::TCPConnection] Error while creating socket: " << WSAGetLastError() << std::endl;
                WSACleanup();
            }
#endif

            if (!(m_socket < 0)) {
                auto retVal = ::connect(m_socket, reinterpret_cast<struct sockaddr *>(&m_address), sizeof(m_address));
                if (0 > retVal) {
#ifdef WIN32 // LCOV_EXCL_LINE
                    auto errorCode = WSAGetLastError();
#else
                    auto errorCode = errno;                                          // LCOV_EXCL_LINE
#endif                                      // LCOV_EXCL_LINE
                    closeSocket(errorCode); // LCOV_EXCL_LINE
                } else {
                    startReadingFromSocket();
                }
            }
        }
    }
}

inline TCPConnection::~TCPConnection() noexcept {
    {
        m_readFromSocketThreadRunning.store(false);

        // Joining the thread could fail.
        try {
            if (m_readFromSocketThread.joinable()) {
                m_readFromSocketThread.join();
            }
        } catch (...) {} // LCOV_EXCL_LINE
    }

    m_pipeline.reset();

    closeSocket(0);
}

inline void TCPConnection::closeSocket(int errorCode) noexcept {
    if (0 != errorCode) {
        std::cerr << "[cluon::TCPConnection] Failed to perform socket operation: "; // LCOV_EXCL_LINE
#ifdef WIN32                                                                        // LCOV_EXCL_LINE
        std::cerr << errorCode << std::endl;
#else
        std::cerr << ::strerror(errorCode) << " (" << errorCode << ")" << std::endl; // LCOV_EXCL_LINE
#endif // LCOV_EXCL_LINE
    }

    if (!(m_socket < 0)) {
#ifdef WIN32
        ::shutdown(m_socket, SD_BOTH);
        ::closesocket(m_socket);
        WSACleanup();
#else
        ::shutdown(m_socket, SHUT_RDWR);                                             // Disallow further read/write operations.
        ::close(m_socket);
#endif
    }
    m_socket = -1;
}

inline void TCPConnection::startReadingFromSocket() noexcept {
    // Constructing a thread could fail.
    try {
        m_readFromSocketThread = std::thread(&TCPConnection::readFromSocket, this);

        // Let the operating system spawn the thread.
        using namespace std::literals::chrono_literals;
        do { std::this_thread::sleep_for(1ms); } while (!m_readFromSocketThreadRunning.load());
    } catch (...) {          // LCOV_EXCL_LINE
        closeSocket(ECHILD); // LCOV_EXCL_LINE
    }

    try {
        m_pipeline = std::make_shared<cluon::NotifyingPipeline<PipelineEntry>>(
            [this](PipelineEntry &&entry) { this->m_newDataDelegate(std::move(entry.m_data), std::move(entry.m_sampleTime)); });
        if (m_pipeline) {
            // Let the operating system spawn the thread.
            using namespace std::literals::chrono_literals; // NOLINT
            do { std::this_thread::sleep_for(1ms); } while (!m_pipeline->isRunning());
        }
    } catch (...) { closeSocket(ECHILD); } // LCOV_EXCL_LINE
}

inline void TCPConnection::setOnNewData(std::function<void(std::string &&, std::chrono::system_clock::time_point &&)> newDataDelegate) noexcept {
    std::lock_guard<std::mutex> lck(m_newDataDelegateMutex);
    m_newDataDelegate = newDataDelegate;
}

inline void TCPConnection::setOnConnectionLost(std::function<void()> connectionLostDelegate) noexcept {
    std::lock_guard<std::mutex> lck(m_connectionLostDelegateMutex);
    m_connectionLostDelegate = connectionLostDelegate;
}

inline bool TCPConnection::isRunning() const noexcept {
    return (m_readFromSocketThreadRunning.load() && !TerminateHandler::instance().isTerminated.load());
}

inline std::pair<ssize_t, int32_t> TCPConnection::send(std::string &&data) const noexcept {
    if (-1 == m_socket) {
        return {-1, EBADF};
    }

    if (data.empty()) {
        return {0, 0};
    }

    if (!m_readFromSocketThreadRunning.load()) {
        std::lock_guard<std::mutex> lck(m_connectionLostDelegateMutex); // LCOV_EXCL_LINE
        if (nullptr != m_connectionLostDelegate) {                      // LCOV_EXCL_LINE
            m_connectionLostDelegate();                                 // LCOV_EXCL_LINE
        }
        return {-1, ENOTCONN}; // LCOV_EXCL_LINE
    }

    constexpr uint16_t MAX_LENGTH{65535};
    if (MAX_LENGTH < data.size()) {
        return {-1, E2BIG};
    }

    std::lock_guard<std::mutex> lck(m_socketMutex);
    ssize_t bytesSent = ::send(m_socket, data.c_str(), data.length(), 0);
    return {bytesSent, (0 > bytesSent ? errno : 0)};
}

inline void TCPConnection::readFromSocket() noexcept {
    // Create buffer to store data from socket.
    constexpr uint16_t MAX_LENGTH{65535};
    std::array<char, MAX_LENGTH> buffer{};

    struct timeval timeout {};

    // Define file descriptor set to watch for read operations.
    fd_set setOfFiledescriptorsToReadFrom{};

    // Indicate to main thread that we are ready.
    m_readFromSocketThreadRunning.store(true);

    // This flag is used to not read data from the socket until this TCPConnection has a proper onNewDataHandler set.
    bool hasNewDataDelegate{false};

    while (m_readFromSocketThreadRunning.load()) {
        // Define timeout for select system call. The timeval struct must be
        // reinitialized for every select call as it might be modified containing
        // the actual time slept.
        timeout.tv_sec  = 0;
        timeout.tv_usec = 20 * 1000; // Check for new data with 50Hz.

        FD_ZERO(&setOfFiledescriptorsToReadFrom);
        FD_SET(m_socket, &setOfFiledescriptorsToReadFrom);
        ::select(m_socket + 1, &setOfFiledescriptorsToReadFrom, nullptr, nullptr, &timeout);

        // Only read data when the newDataDelegate is set.
        if (!hasNewDataDelegate) {
            std::lock_guard<std::mutex> lck(m_newDataDelegateMutex);
            hasNewDataDelegate = (nullptr != m_newDataDelegate);
        }
        if (FD_ISSET(m_socket, &setOfFiledescriptorsToReadFrom) && hasNewDataDelegate) {
            ssize_t bytesRead = ::recv(m_socket, buffer.data(), buffer.max_size(), 0);
            if (0 >= bytesRead) {
                // 0 == bytesRead: peer shut down the connection; 0 > bytesRead: other error.
                m_readFromSocketThreadRunning.store(false);

                {
                    std::lock_guard<std::mutex> lck(m_connectionLostDelegateMutex);
                    if (nullptr != m_connectionLostDelegate) {
                        m_connectionLostDelegate();
                    }
                }
                break;
            }

            {
                std::lock_guard<std::mutex> lck(m_newDataDelegateMutex);
                if ((0 < bytesRead) && (nullptr != m_newDataDelegate)) {
                    // SIOCGSTAMP is not available for a stream-based socket,
                    // thus, falling back to regular chrono timestamping.
                    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
                    {
                        PipelineEntry pe;
                        pe.m_data       = std::string(buffer.data(), static_cast<size_t>(bytesRead));
                        pe.m_sampleTime = timestamp;

                        // Store entry in queue.
                        if (m_pipeline) {
                            m_pipeline->add(std::move(pe));
                        }
                    }

                    if (m_pipeline) {
                        m_pipeline->notifyAll();
                    }
                }
            }
        }
    }
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/TCPServer.hpp"
//#include "cluon/TerminateHandler.hpp"

// clang-format off
#ifdef WIN32
    #include <errno.h>
    #include <iostream>
#else
    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif
// clang-format on

#include <cstring>
#include <array>
#include <iostream>
#include <memory>
#include <sstream>

namespace cluon {

inline TCPServer::TCPServer(uint16_t port, std::function<void(std::string &&from, std::shared_ptr<cluon::TCPConnection> connection)> newConnectionDelegate) noexcept
    : m_newConnectionDelegate(newConnectionDelegate) {
    if (0 < port) {
#ifdef WIN32
        // Load Winsock 2.2 DLL.
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[cluon::TCPServer] Error while calling WSAStartUp: " << WSAGetLastError() << std::endl;
        }
#endif
        m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

#ifdef WIN32
        if (m_socket < 0) {
            std::cerr << "[cluon::TCPServer] Error while creating socket: " << WSAGetLastError() << std::endl;
            WSACleanup();
        }
#endif

        if (!(m_socket < 0)) {
            // Allow reusing of ports by multiple calls with same address/port.
            uint32_t YES = 1;
            // clang-format off
            auto retVal = ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&YES), sizeof(YES)); // NOLINT
            // clang-format on
            if (0 > retVal) {
#ifdef WIN32 // LCOV_EXCL_LINE
                auto errorCode = WSAGetLastError();
#else
                auto errorCode = errno;                                              // LCOV_EXCL_LINE
#endif                                  // LCOV_EXCL_LINE
                closeSocket(errorCode); // LCOV_EXCL_LINE
            }
        }

        if (!(m_socket < 0)) {
            // Setup address and port.
            struct sockaddr_in address;
            ::memset(&address, 0, sizeof(address));
            address.sin_family      = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            address.sin_port        = htons(port);

            auto retVal = ::bind(m_socket, reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
            if (-1 != retVal) {
                constexpr int32_t MAX_PENDING_CONNECTIONS{100};
                retVal = ::listen(m_socket, MAX_PENDING_CONNECTIONS);
                if (-1 != retVal) {
                    // Constructing a thread could fail.
                    try {
                        m_readFromSocketThread = std::thread(&TCPServer::readFromSocket, this);

                        // Let the operating system spawn the thread.
                        using namespace std::literals::chrono_literals;
                        do { std::this_thread::sleep_for(1ms); } while (!m_readFromSocketThreadRunning.load());
                    } catch (...) {          // LCOV_EXCL_LINE
                        closeSocket(ECHILD); // LCOV_EXCL_LINE
                    }
                } else { // LCOV_EXCL_LINE
#ifdef WIN32             // LCOV_EXCL_LINE
                    auto errorCode = WSAGetLastError();
#else
                    auto errorCode = errno;                                          // LCOV_EXCL_LINE
#endif                                      // LCOV_EXCL_LINE
                    closeSocket(errorCode); // LCOV_EXCL_LINE
                }
            } else { // LCOV_EXCL_LINE
#ifdef WIN32         // LCOV_EXCL_LINE
                auto errorCode = WSAGetLastError();
#else
                auto errorCode = errno;                                              // LCOV_EXCL_LINE
#endif                                  // LCOV_EXCL_LINE
                closeSocket(errorCode); // LCOV_EXCL_LINE
            }
        }
    }
}

inline TCPServer::~TCPServer() noexcept {
    m_readFromSocketThreadRunning.store(false);

    // Joining the thread could fail.
    try {
        if (m_readFromSocketThread.joinable()) {
            m_readFromSocketThread.join();
        }
    } catch (...) { // LCOV_EXCL_LINE
    }

    closeSocket(0);
}

inline void TCPServer::closeSocket(int errorCode) noexcept {
    if (0 != errorCode) {
        std::cerr << "[cluon::TCPServer] Failed to perform socket operation: "; // LCOV_EXCL_LINE
#ifdef WIN32                                                                    // LCOV_EXCL_LINE
        std::cerr << errorCode << std::endl;
#else
        std::cerr << ::strerror(errorCode) << " (" << errorCode << ")" << std::endl; // LCOV_EXCL_LINE
#endif // LCOV_EXCL_LINE
    }

    if (!(m_socket < 0)) {
#ifdef WIN32
        ::shutdown(m_socket, SD_BOTH);
        ::closesocket(m_socket);
        WSACleanup();
#else
        ::shutdown(m_socket, SHUT_RDWR);                                             // Disallow further read/write operations.
        ::close(m_socket);
#endif
    }
    m_socket = -1;
}

inline bool TCPServer::isRunning() const noexcept {
    return (m_readFromSocketThreadRunning.load() && !TerminateHandler::instance().isTerminated.load());
}

inline void TCPServer::readFromSocket() noexcept {
    struct timeval timeout {};

    // Define file descriptor set to watch for read operations.
    fd_set setOfFiledescriptorsToReadFrom{};

    // Indicate to main thread that we are ready.
    m_readFromSocketThreadRunning.store(true);

    constexpr uint16_t MAX_ADDR_SIZE{1024};
    std::array<char, MAX_ADDR_SIZE> remoteAddress{};

    while (m_readFromSocketThreadRunning.load()) {
        // Define timeout for select system call. The timeval struct must be
        // reinitialized for every select call as it might be modified containing
        // the actual time slept.
        timeout.tv_sec  = 0;
        timeout.tv_usec = 20 * 1000; // Check for new data with 50Hz.

        FD_ZERO(&setOfFiledescriptorsToReadFrom);
        FD_SET(m_socket, &setOfFiledescriptorsToReadFrom);
        ::select(m_socket + 1, &setOfFiledescriptorsToReadFrom, nullptr, nullptr, &timeout);
        if (FD_ISSET(m_socket, &setOfFiledescriptorsToReadFrom)) {
            struct sockaddr_storage remote;
            socklen_t addrLength     = sizeof(remote);
            int32_t connectingClient = ::accept(m_socket, reinterpret_cast<struct sockaddr *>(&remote), &addrLength);
            if ((0 <= connectingClient) && (nullptr != m_newConnectionDelegate)) {
                ::inet_ntop(remote.ss_family,
                            &((reinterpret_cast<struct sockaddr_in *>(&remote))->sin_addr), // NOLINT
                            remoteAddress.data(),
                            remoteAddress.max_size());
                const uint16_t RECVFROM_PORT{ntohs(reinterpret_cast<struct sockaddr_in *>(&remote)->sin_port)}; // NOLINT
                m_newConnectionDelegate(std::string(remoteAddress.data()) + ':' + std::to_string(RECVFROM_PORT),
                                        std::shared_ptr<cluon::TCPConnection>(new cluon::TCPConnection(connectingClient)));
            }
        }
    }
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/ToProtoVisitor.hpp"

#include <cstring>

namespace cluon {

inline std::string ToProtoVisitor::encodedData() const noexcept {
    std::string s{m_buffer.str()};
    return s;
}

inline void ToProtoVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;
}

inline void ToProtoVisitor::postVisit() noexcept {}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<bool>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)typeName;
    (void)name;
    uint8_t _v = static_cast<uint8_t>(v); // NOLINT
    toKeyValue<uint8_t>(id, _v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<int8_t>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<uint8_t>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<int16_t>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<uint16_t>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<int32_t>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<uint32_t>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<int64_t>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)typeName;
    (void)name;
    toKeyValue<uint64_t>(id, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)typeName;
    (void)name;
    uint64_t key = encodeKey(id, static_cast<uint8_t>(ProtoConstants::FOUR_BYTES));
    toVarInt(m_buffer, key);
    encode(m_buffer, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)typeName;
    (void)name;
    uint64_t key = encodeKey(id, static_cast<uint8_t>(ProtoConstants::EIGHT_BYTES));
    toVarInt(m_buffer, key);
    encode(m_buffer, v);
}

inline void ToProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)typeName;
    (void)name;
    uint64_t key = encodeKey(id, static_cast<uint8_t>(ProtoConstants::LENGTH_DELIMITED));
    toVarInt(m_buffer, key);
    encode(m_buffer, v);
}

////////////////////////////////////////////////////////////////////////////////

inline std::size_t ToProtoVisitor::encode(std::ostream &o, bool &v) noexcept {
    uint64_t _v{(v ? 1u : 0u)};
    return toVarInt(o, _v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, int8_t &v) noexcept {
    uint64_t _v = toZigZag8(v);
    return toVarInt(o, _v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, uint8_t &v) noexcept {
    uint64_t _v = v;
    return toVarInt(o, _v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, int16_t &v) noexcept {
    uint64_t _v = toZigZag16(v);
    return toVarInt(o, _v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, uint16_t &v) noexcept {
    uint64_t _v = v;
    return toVarInt(o, _v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, int32_t &v) noexcept {
    uint64_t _v = toZigZag32(v);
    return toVarInt(o, _v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, uint32_t &v) noexcept {
    uint64_t _v = v;
    return toVarInt(o, _v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, int64_t &v) noexcept {
    uint64_t _v = toZigZag64(v);
    return toVarInt(o, _v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, uint64_t &v) noexcept {
    return toVarInt(o, v);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, float &v) noexcept {
    // Store 4 bytes as little endian encoding.
    uint32_t _v{0};
    std::memmove(&_v, &v, sizeof(float));
    _v = htole32(_v);
    o.write(reinterpret_cast<const char *>(&_v), sizeof(uint32_t)); // NOLINT
    return sizeof(uint32_t);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, double &v) noexcept {
    // Store 8 bytes as little endian encoding.
    uint64_t _v{0};
    std::memmove(&_v, &v, sizeof(double));
    _v = htole64(_v);
    o.write(reinterpret_cast<const char *>(&_v), sizeof(uint64_t)); // NOLINT
    return sizeof(uint64_t);
}

inline std::size_t ToProtoVisitor::encode(std::ostream &o, const std::string &v) noexcept {
    const std::size_t LENGTH = v.length();
    std::size_t size         = toVarInt(o, LENGTH);
    o.write(v.c_str(), static_cast<std::streamsize>(LENGTH)); // LENGTH won't be negative.
    return size + LENGTH;
}

inline uint8_t ToProtoVisitor::toZigZag8(int8_t v) noexcept {
    return static_cast<uint8_t>((v << 1) ^ (v >> ((sizeof(v) * 8) - 1)));
}

inline uint16_t ToProtoVisitor::toZigZag16(int16_t v) noexcept {
    return static_cast<uint16_t>((v << 1) ^ (v >> ((sizeof(v) * 8) - 1)));
}

inline uint32_t ToProtoVisitor::toZigZag32(int32_t v) noexcept {
    return static_cast<uint32_t>((v << 1) ^ (v >> ((sizeof(v) * 8) - 1)));
}

inline uint64_t ToProtoVisitor::toZigZag64(int64_t v) noexcept {
    return static_cast<uint64_t>((v << 1) ^ (v >> ((sizeof(v) * 8) - 1)));
}

inline uint64_t ToProtoVisitor::encodeKey(uint32_t fieldIdentifier, uint8_t protoType) noexcept {
    return (fieldIdentifier << 0x3) | protoType;
}

inline std::size_t ToProtoVisitor::toVarInt(std::ostream &out, uint64_t v) noexcept {
    // Minimum size is of the encoded data.
    std::size_t size{1};
    uint8_t b{0};
    while (0x7f < v) {
        // Use the MSB to indicate value overflow for more bytes to come.
        b = (static_cast<uint8_t>(v & 0x7f)) | 0x80;
        out.put(static_cast<char>(b));
        v >>= 7;
        size++;
    }
    // Write final byte.
    b = (static_cast<uint8_t>(v)) & 0x7f;
    out.put(static_cast<char>(b));

    return size;
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/FromProtoVisitor.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace cluon {

inline void FromProtoVisitor::readBytesFromStream(std::istream &in, std::size_t bytesToReadFromStream, char *buffer) noexcept {
    if (nullptr != buffer) {
        const constexpr std::size_t CHUNK_SIZE{1024};
        std::streamsize bufferPosition{0};
        std::streamsize extractedBytes{0};

        while ((0 < bytesToReadFromStream) && in.good()) {
            // clang-format off
            in.read(&buffer[static_cast<std::size_t>(bufferPosition)], /* Flawfinder: ignore */ /* Cf. buffer.reserve(...) above.  */
                    (bytesToReadFromStream > CHUNK_SIZE) ? CHUNK_SIZE : static_cast<std::streamsize>(bytesToReadFromStream));
            // clang-format on
            extractedBytes = in.gcount();
            bufferPosition += extractedBytes;
            bytesToReadFromStream -= static_cast<std::size_t>(extractedBytes);
        }
    }
}

inline void FromProtoVisitor::decodeFrom(std::istream &in) noexcept {
    // Reset internal states as this deserializer could be reused.
    m_mapOfKeyValues.clear();
    while (in.good()) {
        // First stage: Read keyFieldType (encoded as VarInt).
        if (0 < fromVarInt(in, m_keyFieldType)) {
            // Succeeded to read keyFieldType entry; extract information.
            m_protoType = static_cast<ProtoConstants>(m_keyFieldType & 0x7);
            m_fieldId = static_cast<uint32_t>(m_keyFieldType >> 3);
            switch (m_protoType) {
                case ProtoConstants::VARINT:
                {
                    // Directly decode VarInt value.
                    fromVarInt(in, m_value);
                    m_mapOfKeyValues.emplace(m_fieldId, linb::any(m_value));
                }
                break;
                case ProtoConstants::EIGHT_BYTES:
                {
                    readBytesFromStream(in, sizeof(double), m_doubleValue.buffer.data());
                    m_doubleValue.uint64Value = le64toh(m_doubleValue.uint64Value);
                    m_mapOfKeyValues.emplace(m_fieldId, linb::any(m_doubleValue.doubleValue));
                }
                break;
                case ProtoConstants::FOUR_BYTES:
                {
                    readBytesFromStream(in, sizeof(float), m_floatValue.buffer.data());
                    m_floatValue.uint32Value = le32toh(m_floatValue.uint32Value);
                    m_mapOfKeyValues.emplace(m_fieldId, linb::any(m_floatValue.floatValue));
                }
                break;
                case ProtoConstants::LENGTH_DELIMITED:
                {
                    fromVarInt(in, m_value);
                    const std::size_t BYTES_TO_READ_FROM_STREAM{static_cast<std::size_t>(m_value)};
                    if (m_stringValue.capacity() < BYTES_TO_READ_FROM_STREAM) {
                        m_stringValue.reserve(BYTES_TO_READ_FROM_STREAM);
                    }
                    readBytesFromStream(in, BYTES_TO_READ_FROM_STREAM, m_stringValue.data());
                    m_mapOfKeyValues.emplace(m_fieldId, linb::any(std::string(m_stringValue.data(), static_cast<std::size_t>(m_value))));
                }
                break;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

inline FromProtoVisitor &FromProtoVisitor::operator=(const FromProtoVisitor &other) noexcept {
    m_mapOfKeyValues = other.m_mapOfKeyValues;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////

inline void FromProtoVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;
}

inline void FromProtoVisitor::postVisit() noexcept {}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = (0 != m_value);
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            v = (0 != linb::any_cast<uint64_t>(m_mapOfKeyValues[id]));
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = static_cast<char>(m_value);
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            uint64_t _v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
            v           = static_cast<char>(_v);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = static_cast<int8_t>(fromZigZag8(static_cast<uint8_t>(m_value)));
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            uint64_t _v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
            v           = static_cast<int8_t>(fromZigZag8(static_cast<uint8_t>(_v)));
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = static_cast<uint8_t>(m_value);
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            uint64_t _v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
            v           = static_cast<uint8_t>(_v);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = static_cast<int16_t>(fromZigZag16(static_cast<uint16_t>(m_value)));
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            uint64_t _v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
            v           = static_cast<int16_t>(fromZigZag16(static_cast<uint16_t>(_v)));
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = static_cast<uint16_t>(m_value);
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            uint64_t _v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
            v           = static_cast<uint16_t>(_v);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = static_cast<int32_t>(fromZigZag32(static_cast<uint32_t>(m_value)));
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            uint64_t _v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
            v           = static_cast<int32_t>(fromZigZag32(static_cast<uint32_t>(_v)));
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = static_cast<uint32_t>(m_value);
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            uint64_t _v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
            v           = static_cast<uint32_t>(_v);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = static_cast<int64_t>(fromZigZag64(static_cast<uint64_t>(m_value)));
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            uint64_t _v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
            v           = static_cast<int64_t>(fromZigZag64(_v));
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = m_value;
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            v = linb::any_cast<uint64_t>(m_mapOfKeyValues[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = m_floatValue.floatValue;
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            v = linb::any_cast<float>(m_mapOfKeyValues[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = m_doubleValue.doubleValue;
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            v = linb::any_cast<double>(m_mapOfKeyValues[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromProtoVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)typeName;
    (void)name;
    if (m_callToDecodeFromWithDirectVisit) {
        v = std::string(m_stringValue.data(), static_cast<std::size_t>(m_value));
    }
    else if (m_mapOfKeyValues.count(id) > 0) {
        try {
            v = linb::any_cast<std::string>(m_mapOfKeyValues[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

inline int8_t FromProtoVisitor::fromZigZag8(uint8_t v) noexcept {
    return static_cast<int8_t>((v >> 1) ^ -(v & 1));
}

inline int16_t FromProtoVisitor::fromZigZag16(uint16_t v) noexcept {
    return static_cast<int16_t>((v >> 1) ^ -(v & 1));
}

inline int32_t FromProtoVisitor::fromZigZag32(uint32_t v) noexcept {
    return static_cast<int32_t>((v >> 1) ^ -(v & 1));
}

inline int64_t FromProtoVisitor::fromZigZag64(uint64_t v) noexcept {
    return static_cast<int64_t>((v >> 1) ^ -(v & 1));
}

inline std::size_t FromProtoVisitor::fromVarInt(std::istream &in, uint64_t &value) noexcept {
    value = 0;

    constexpr uint64_t MASK  = 0x7f;
    constexpr uint64_t SHIFT = 0x7;
    constexpr uint64_t MSB   = 0x80;

    std::size_t size = 0;
    uint64_t C{0};
    while (in.good()) {
        auto x = in.get();
        C = static_cast<uint64_t>(x);
        value |= (C & MASK) << (SHIFT * size++);
        if (!(C & MSB)) { // NOLINT
            break;
        }
    }

    return size;
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for ntohll
#endif
// clang-format on

//#include "cluon/FromLCMVisitor.hpp"

#include <cstring>
#include <algorithm>
#include <iostream>
#include <vector>

namespace cluon {

inline FromLCMVisitor::FromLCMVisitor() noexcept
    : m_buffer(m_internalBuffer) {}

inline FromLCMVisitor::FromLCMVisitor(std::stringstream &in) noexcept
    : m_expectedHash{0}
    , m_buffer(in) {}

inline void FromLCMVisitor::decodeFrom(std::istream &in) noexcept {
    // Reset internal states as this deserializer could be reused.
    m_buffer.clear();
    m_buffer.str("");

    in.read(reinterpret_cast<char *>(&m_expectedHash), sizeof(int64_t));
    m_expectedHash = static_cast<int64_t>(be64toh(m_expectedHash));

    m_buffer << in.rdbuf();
}

////////////////////////////////////////////////////////////////////////////////

inline void FromLCMVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;

    // Reset m_buffer read pointer to beginning only if we are not dealing with
    // nested complex types as we are sharing our buffer with our parent message.
    if (0 != m_expectedHash) {
        m_buffer.clear();
        m_buffer.seekg(0);
        m_calculatedHash = 0x12345678;
        m_hashes.clear();
    }
}

inline void FromLCMVisitor::postVisit() noexcept {
    if ((0 != m_expectedHash) && (m_expectedHash != hash())) {
        std::cerr << "[cluon::FromLCMVisitor] Hash mismatch - decoding might have failed" << std::endl; // LCOV_EXCL_LINE
    }
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("boolean");
    calculateHash(0);
    m_buffer.read(reinterpret_cast<char *>(&v), sizeof(bool));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int8_t");
    calculateHash(0);
    m_buffer.read(reinterpret_cast<char *>(&v), sizeof(char));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int8_t");
    calculateHash(0);
    m_buffer.read(reinterpret_cast<char *>(&v), sizeof(int8_t));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int8_t");
    calculateHash(0);
    m_buffer.read(reinterpret_cast<char *>(&v), sizeof(int8_t));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int16_t");
    calculateHash(0);
    int16_t _v{0};
    m_buffer.read(reinterpret_cast<char *>(&_v), sizeof(int16_t));
    v = static_cast<int16_t>(be16toh(_v));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int16_t");
    calculateHash(0);
    int16_t _v{0};
    m_buffer.read(reinterpret_cast<char *>(&_v), sizeof(int16_t));
    v = be16toh(_v);
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int32_t");
    calculateHash(0);
    int32_t _v{0};
    m_buffer.read(reinterpret_cast<char *>(&_v), sizeof(int32_t));
    v = static_cast<int32_t>(be32toh(_v));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int32_t");
    calculateHash(0);
    int32_t _v{0};
    m_buffer.read(reinterpret_cast<char *>(&_v), sizeof(int32_t));
    v = be32toh(_v);
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int64_t");
    calculateHash(0);
    int64_t _v{0};
    m_buffer.read(reinterpret_cast<char *>(&_v), sizeof(int64_t));
    v = static_cast<int64_t>(be64toh(_v));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int64_t");
    calculateHash(0);
    int64_t _v{0};
    m_buffer.read(reinterpret_cast<char *>(&_v), sizeof(int64_t));
    v = be64toh(_v);
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("float");
    calculateHash(0);
    int32_t _v{0};
    m_buffer.read(reinterpret_cast<char *>(&_v), sizeof(int32_t));
    _v = static_cast<int32_t>(be32toh(_v));
    std::memmove(&v, &_v, sizeof(int32_t));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("double");
    calculateHash(0);
    int64_t _v{0};
    m_buffer.read(reinterpret_cast<char *>(&_v), sizeof(int64_t));
    _v = static_cast<int64_t>(be64toh(_v));
    std::memmove(&v, &_v, sizeof(int64_t));
}

inline void FromLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)id;
    (void)typeName;
    (void)name;
    (void)v;
    calculateHash(name);
    calculateHash("string");
    calculateHash(0);

    int32_t length{0};
    m_buffer.read(reinterpret_cast<char *>(&length), sizeof(int32_t));
    length = static_cast<int32_t>(be32toh(length));

    v.clear();
    if (length > 0) {
        std::vector<char> buffer;
        buffer.reserve(static_cast<uint32_t>(length));
#ifdef WIN32
        for (uint32_t i = 0; i < static_cast<uint32_t>(length); i++) {
            char c;
            m_buffer.get(c);
            buffer.push_back(c);
        }
#else
        m_buffer.read(static_cast<char *>(&buffer[0]), static_cast<std::streamsize>(length));
#endif
        const std::string s(buffer.begin(), buffer.begin() + length - 1); // Skip trailing '\0'.
        v = s;
    }
}

////////////////////////////////////////////////////////////////////////////////

inline int64_t FromLCMVisitor::hash() const noexcept {
    // Apply ZigZag encoding for hash from this message's fields and depending
    // hashes for complex nested types.
    int64_t tmp{m_calculatedHash};
    for (int64_t v : m_hashes) { tmp += v; }

    const int64_t hash = (tmp << 1) + ((tmp >> 63) & 1);
    return hash;
}

inline void FromLCMVisitor::calculateHash(char c) noexcept {
    m_calculatedHash = ((m_calculatedHash << 8) ^ (m_calculatedHash >> 55)) + c;
}

inline void FromLCMVisitor::calculateHash(const std::string &s) noexcept {
    const std::string tmp{(s.length() > 255 ? s.substr(0, 255) : s)};
    const uint8_t length{static_cast<uint8_t>(tmp.length())};
    calculateHash(static_cast<char>(length));
    for (auto c : tmp) { calculateHash(c); }
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for ntohl, ntohs
#endif
// clang-format on

//#include "cluon/FromMsgPackVisitor.hpp"

#include <cstring>
#include <vector>

namespace cluon {

inline FromMsgPackVisitor::FromMsgPackVisitor() noexcept
    : m_keyValues{m_data} {}

inline FromMsgPackVisitor::FromMsgPackVisitor(std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue> &preset) noexcept
    : m_keyValues{preset} {}

inline MsgPackConstants FromMsgPackVisitor::getFormatFamily(uint8_t T) noexcept {
    MsgPackConstants formatFamily{MsgPackConstants::UNKNOWN_FORMAT};

    if (static_cast<uint8_t>(MsgPackConstants::IS_FALSE) == T) {
        formatFamily = MsgPackConstants::BOOL_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::IS_TRUE) == T) {
        formatFamily = MsgPackConstants::BOOL_FORMAT;
    } else if (0x7F >= T) {
        formatFamily = MsgPackConstants::UINT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::UINT8) == T) {
        formatFamily = MsgPackConstants::UINT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::UINT16) == T) {
        formatFamily = MsgPackConstants::UINT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::UINT32) == T) {
        formatFamily = MsgPackConstants::UINT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::UINT64) == T) {
        formatFamily = MsgPackConstants::UINT_FORMAT;
    } else if (0xE0 <= T) {
        formatFamily = MsgPackConstants::INT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::INT8) == T) {
        formatFamily = MsgPackConstants::INT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::INT16) == T) {
        formatFamily = MsgPackConstants::INT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::INT32) == T) {
        formatFamily = MsgPackConstants::INT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::INT64) == T) {
        formatFamily = MsgPackConstants::INT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::FLOAT) == T) {
        formatFamily = MsgPackConstants::FLOAT_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::DOUBLE) == T) {
        formatFamily = MsgPackConstants::FLOAT_FORMAT;
    } else if ((static_cast<uint8_t>(MsgPackConstants::FIXSTR) <= T) && (static_cast<uint8_t>(MsgPackConstants::FIXSTR_END) > T)) {
        formatFamily = MsgPackConstants::STR_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::STR8) == T) {
        formatFamily = MsgPackConstants::STR_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::STR16) == T) {
        formatFamily = MsgPackConstants::STR_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::STR32) == T) {
        formatFamily = MsgPackConstants::STR_FORMAT;
    } else if ((static_cast<uint8_t>(MsgPackConstants::FIXMAP) <= T) && (static_cast<uint8_t>(MsgPackConstants::FIXMAP_END) > T)) {
        formatFamily = MsgPackConstants::MAP_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::MAP16) == T) {
        formatFamily = MsgPackConstants::MAP_FORMAT;
    } else if (static_cast<uint8_t>(MsgPackConstants::MAP32) == T) { // LCOV_EXCL_LINE
        formatFamily = MsgPackConstants::MAP_FORMAT;                 // LCOV_EXCL_LINE
    }

    return formatFamily;
}

inline uint64_t FromMsgPackVisitor::readUint(std::istream &in) noexcept {
    uint64_t retVal{0};
    if (in.good()) {
        uint8_t c = static_cast<uint8_t>(in.get());
        if (MsgPackConstants::UINT_FORMAT == getFormatFamily(c)) {
            if (0x7F >= c) {
                retVal = static_cast<uint64_t>(c);
            } else if (static_cast<uint8_t>(MsgPackConstants::UINT8) == c) {
                uint8_t v{0};
                in.read(reinterpret_cast<char *>(&v), sizeof(uint8_t));
                retVal = static_cast<uint64_t>(v);
            } else if (static_cast<uint8_t>(MsgPackConstants::UINT16) == c) {
                uint16_t v{0};
                in.read(reinterpret_cast<char *>(&v), sizeof(uint16_t));
                v      = be16toh(v);
                retVal = static_cast<uint64_t>(v);
            } else if (static_cast<uint8_t>(MsgPackConstants::UINT32) == c) {
                uint32_t v{0};
                in.read(reinterpret_cast<char *>(&v), sizeof(uint32_t));
                v      = be32toh(v);
                retVal = static_cast<uint64_t>(v);
            } else if (static_cast<uint8_t>(MsgPackConstants::UINT64) == c) {
                in.read(reinterpret_cast<char *>(&retVal), sizeof(uint64_t));
                retVal = be64toh(retVal);
            }
        }
    }
    return retVal;
}

inline int64_t FromMsgPackVisitor::readInt(std::istream &in) noexcept {
    int64_t retVal{0};
    if (in.good()) {
        int8_t c = static_cast<int8_t>(in.get());
        if (MsgPackConstants::INT_FORMAT == getFormatFamily(static_cast<uint8_t>(c))) {
            if (0xE0 <= static_cast<uint8_t>(c)) {
                retVal = static_cast<int64_t>(c);
            } else if (static_cast<int8_t>(MsgPackConstants::INT8) == c) {
                int8_t v{0};
                in.read(reinterpret_cast<char *>(&v), sizeof(int8_t));
                retVal = static_cast<int64_t>(v);
            } else if (static_cast<int8_t>(MsgPackConstants::INT16) == c) {
                int16_t v{0};
                in.read(reinterpret_cast<char *>(&v), sizeof(int16_t));
                v      = static_cast<int16_t>(be16toh(v));
                retVal = static_cast<int64_t>(v);
            } else if (static_cast<int8_t>(MsgPackConstants::INT32) == c) {
                int32_t v{0};
                in.read(reinterpret_cast<char *>(&v), sizeof(int32_t));
                v      = static_cast<int32_t>(be32toh(v));
                retVal = static_cast<int64_t>(v);
            } else if (static_cast<int8_t>(MsgPackConstants::INT64) == c) {
                in.read(reinterpret_cast<char *>(&retVal), sizeof(int64_t));
                retVal = static_cast<int64_t>(be64toh(retVal));
            }
        }
    }
    return retVal;
}

inline std::string FromMsgPackVisitor::readString(std::istream &in) noexcept {
    std::string retVal{""};
    if (in.good()) {
        uint8_t c = static_cast<uint8_t>(in.get());
        if (MsgPackConstants::STR_FORMAT == getFormatFamily(c)) {
            uint32_t length{0};
            const uint8_t T = static_cast<uint8_t>(c);
            if ((static_cast<uint8_t>(MsgPackConstants::FIXSTR) <= T) && (static_cast<uint8_t>(MsgPackConstants::FIXSTR_END) > T)) {
                length = T - static_cast<uint8_t>(MsgPackConstants::FIXSTR);
            } else if (static_cast<uint8_t>(MsgPackConstants::STR8) == T) {
                uint8_t _length{0};
                in.read(reinterpret_cast<char *>(&_length), sizeof(uint8_t));
                length = _length;
            } else if (static_cast<uint8_t>(MsgPackConstants::STR16) == T) {
                uint16_t _length{0};
                in.read(reinterpret_cast<char *>(&_length), sizeof(uint16_t));
                length = be16toh(_length);
            } else if (static_cast<uint8_t>(MsgPackConstants::STR32) == T) {
                in.read(reinterpret_cast<char *>(&length), sizeof(uint32_t));
                length = be32toh(length);
            }

            if (0 < length) {
                std::vector<char> buffer;
                buffer.reserve(length);
#ifdef WIN32
                for (uint32_t i = 0; i < static_cast<uint32_t>(length); i++) {
                    char c;
                    in.get(c);
                    buffer.push_back(c);
                }
#else
                in.read(static_cast<char *>(&buffer[0]), static_cast<std::streamsize>(length));
#endif
                retVal = std::string(buffer.data(), length);
            }
        }
    }
    return retVal;
}

inline std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue> FromMsgPackVisitor::readKeyValues(std::istream &in) noexcept {
    std::map<std::string, FromMsgPackVisitor::MsgPackKeyValue> keyValues;
    while (in.good()) {
        uint8_t c = static_cast<uint8_t>(in.get());
        if (MsgPackConstants::MAP_FORMAT == getFormatFamily(c)) {
            // First, search for map opening token.
            const uint8_t T = static_cast<uint8_t>(c);
            uint32_t tokensToRead{0};
            if ((static_cast<uint8_t>(MsgPackConstants::FIXMAP) <= T) && (static_cast<uint8_t>(MsgPackConstants::FIXMAP_END) > T)) {
                tokensToRead = T - static_cast<uint8_t>(MsgPackConstants::FIXMAP);
            } else if (static_cast<uint8_t>(MsgPackConstants::MAP16) == T) {
                uint16_t tokens{0};
                in.read(reinterpret_cast<char *>(&tokens), sizeof(uint16_t));
                tokensToRead = be16toh(tokens);
            } else if (static_cast<uint8_t>(MsgPackConstants::MAP32) == T) {        // LCOV_EXCL_LINE
                in.read(reinterpret_cast<char *>(&tokensToRead), sizeof(uint32_t)); // LCOV_EXCL_LINE
                tokensToRead = be32toh(tokensToRead);                               // LCOV_EXCL_LINE
            }

            // Next, read pairs string/value.
            while (0 < tokensToRead) {
                MsgPackKeyValue entry;
                entry.m_key = readString(in);
                // Read next byte and determine format family.
                c                    = static_cast<uint8_t>(in.get());
                entry.m_formatFamily = getFormatFamily(c);

                if (MsgPackConstants::BOOL_FORMAT == entry.m_formatFamily) {
                    entry.m_value = false;
                    if (static_cast<uint8_t>(c) == static_cast<uint8_t>(MsgPackConstants::IS_TRUE)) {
                        entry.m_value = true;
                    } else if (static_cast<uint8_t>(c) == static_cast<uint8_t>(MsgPackConstants::IS_FALSE)) {
                        entry.m_value = false;
                    }
                } else if (MsgPackConstants::UINT_FORMAT == entry.m_formatFamily) {
                    in.unget(); // Last character needs to be put back to process the uints correctly as it might
                                // contain the value.
                    entry.m_value = readUint(in);
                } else if (MsgPackConstants::INT_FORMAT == entry.m_formatFamily) {
                    in.unget(); // Last character needs to be put back to process the ints correctly as it might contain
                                // the value.
                    entry.m_value = readInt(in);
                } else if (MsgPackConstants::FLOAT_FORMAT == entry.m_formatFamily) {
                    if (static_cast<uint8_t>(c) == static_cast<uint8_t>(MsgPackConstants::FLOAT)) {
                        uint32_t _v{0};
                        in.read(reinterpret_cast<char *>(&_v), sizeof(uint32_t));
                        _v = be32toh(_v);
                        float v{0.0f};
                        std::memmove(&v, &_v, sizeof(float));
                        entry.m_value = v;
                    }
                    if (static_cast<uint8_t>(c) == static_cast<uint8_t>(MsgPackConstants::DOUBLE)) {
                        uint64_t _v{0};
                        in.read(reinterpret_cast<char *>(&_v), sizeof(uint64_t));
                        _v = be64toh(_v);
                        double v{0.0};
                        std::memmove(&v, &_v, sizeof(double));
                        entry.m_value = v;
                    }
                } else if (MsgPackConstants::STR_FORMAT == entry.m_formatFamily) {
                    in.unget(); // Last character needs to be put back to process the string correctly as it might
                                // encode its length.
                    entry.m_value = readString(in);
                } else if (MsgPackConstants::MAP_FORMAT == entry.m_formatFamily) {
                    in.unget(); // Last character needs to be put back to process the contained nested map correctly as
                                // it might encode its length.
                    entry.m_value = readKeyValues(in);
                }

                keyValues[entry.m_key] = entry;
                tokensToRead--;
            }
            // Stop processing further tokens (might be handled from outer decoder).
            break;
        }
    }
    return keyValues;
}

inline void FromMsgPackVisitor::decodeFrom(std::istream &in) noexcept {
    (void)in;

    m_keyValues = readKeyValues(in);
}

inline void FromMsgPackVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;
}

inline void FromMsgPackVisitor::postVisit() noexcept {}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = linb::any_cast<bool>(m_keyValues[name].m_value);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = linb::any_cast<std::string>(m_keyValues[name].m_value).at(0);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = static_cast<int8_t>(linb::any_cast<int64_t>(m_keyValues[name].m_value));
        } catch (const linb::bad_any_cast &) {
            // A positive value was stored.
            try {
                v = static_cast<int8_t>(linb::any_cast<uint64_t>(m_keyValues[name].m_value));
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = static_cast<uint8_t>(linb::any_cast<uint64_t>(m_keyValues[name].m_value));
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = static_cast<int16_t>(linb::any_cast<int64_t>(m_keyValues[name].m_value));
        } catch (const linb::bad_any_cast &) {
            // A positive value was stored.
            try {
                v = static_cast<int16_t>(linb::any_cast<uint64_t>(m_keyValues[name].m_value));
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = static_cast<uint16_t>(linb::any_cast<uint64_t>(m_keyValues[name].m_value));
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = static_cast<int32_t>(linb::any_cast<int64_t>(m_keyValues[name].m_value));
        } catch (const linb::bad_any_cast &) {
            // A positive value was stored.
            try {
                v = static_cast<int32_t>(linb::any_cast<uint64_t>(m_keyValues[name].m_value));
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = static_cast<uint32_t>(linb::any_cast<uint64_t>(m_keyValues[name].m_value));
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = linb::any_cast<int64_t>(m_keyValues[name].m_value);
        } catch (const linb::bad_any_cast &) {
            // A positive value was stored.
            try {
                v = static_cast<int64_t>(linb::any_cast<uint64_t>(m_keyValues[name].m_value));
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = linb::any_cast<uint64_t>(m_keyValues[name].m_value);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = linb::any_cast<float>(m_keyValues[name].m_value);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = linb::any_cast<double>(m_keyValues[name].m_value);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            v = linb::any_cast<std::string>(m_keyValues[name].m_value);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

} // namespace cluon
/*
 * Copyright (C) 2017-2019  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/FromJSONVisitor.hpp"
//#include "cluon/stringtoolbox.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <regex>
#include <sstream>
#include <vector>

#include <iostream>
namespace cluon {

inline FromJSONVisitor::FromJSONVisitor() noexcept
    : m_keyValues{m_data} {}

inline FromJSONVisitor::FromJSONVisitor(std::map<std::string, FromJSONVisitor::JSONKeyValue> &preset) noexcept
    : m_keyValues{preset} {}

inline std::map<std::string, FromJSONVisitor::JSONKeyValue> FromJSONVisitor::readKeyValues(std::string &input) noexcept {
    const std::string MATCH_JSON
        = R"((?:\"|\')(?:[^"]*)(?:\"|\')(?=:)(?:\:\s*)(?:\"|\')?(?:true|false|[\-]{0,1}[0-9]+[\.][0-9]+|[\-]{0,1}[0-9]+|[0-9a-zA-Z\+\-\,\.\$\ \=]*)(?:\"|\')?)";

    std::map<std::string, FromJSONVisitor::JSONKeyValue> result;
    std::string oldInput;
    try {
        std::smatch m;
        do {
            std::regex_search(input, m, std::regex(MATCH_JSON));

            if (m.size() > 0) {
                std::string match{m[0]};
                std::vector<std::string> retVal = stringtoolbox::split(match, ':');
                if ( (retVal.size() == 2) && (stringtoolbox::trim(retVal[1]).size() == 0) ) {
                    std::string keyOfNestedObject{stringtoolbox::trim(retVal[0])};
                    keyOfNestedObject = stringtoolbox::split(keyOfNestedObject, '"')[1];
                    {
                        std::string suffix(m.suffix());
                        suffix   = stringtoolbox::trim(suffix);
                        oldInput = input;
                        input    = suffix;
                    }

                    auto mapOfNestedValues = readKeyValues(input);

                    JSONKeyValue kv;
                    kv.m_key   = keyOfNestedObject;
                    kv.m_type  = JSONConstants::OBJECT;
                    kv.m_value = mapOfNestedValues;

                    result[keyOfNestedObject] = kv;
                }
                if ( (retVal.size() == 2) && (stringtoolbox::trim(retVal[1]).size() > 0) ) {
                    auto e = std::make_pair(stringtoolbox::trim(retVal[0]), stringtoolbox::trim(retVal[1]));

                    JSONKeyValue kv;
                    kv.m_key = stringtoolbox::split(e.first, '"')[1];

                    if ((e.second.size() > 0) && (e.second.at(0) == '"')) {
                        kv.m_type  = JSONConstants::STRING;
                        kv.m_value = std::string(e.second).substr(1);
                    } else if ((e.second.size() > 0) && ((e.second == "false") || (e.second == "true"))) {
                        kv.m_type  = (e.second == "true" ? JSONConstants::IS_TRUE : JSONConstants::IS_FALSE);
                        kv.m_value = e.second == "true";
                    } else {
                        kv.m_type = JSONConstants::NUMBER;
                        std::stringstream tmp(e.second);
                        double v;
                        tmp >> v;
                        kv.m_value = v;
                    }

                    result[kv.m_key] = kv;

                    {
                        std::string suffix(m.suffix());
                        suffix   = stringtoolbox::trim(suffix);
                        oldInput = input;
                        input    = suffix;
                        if (suffix.size() > 0 && suffix.at(0) == '}') {
                            break; // Nested payload complete; return.
                        }
                    }
                }
            }
        } while (!m.empty() && (oldInput != input));
    } catch (std::regex_error &) { // LCOV_EXCL_LINE
    } catch (std::bad_cast &) {}   // LCOV_EXCL_LINE

    return result;
}

inline void FromJSONVisitor::decodeFrom(std::istream &in) noexcept {
    m_keyValues.clear();

    std::string s;
    std::istream_iterator<char> it(in), it_end;
    std::copy(it, it_end, std::insert_iterator<std::string>(s, s.begin()));

    // Remove whitespace characters like newline, carriage return, or tab.
    s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return (c == '\r' || c == '\t' || c == '\n'); }), s.end());

    // Parse JSON from in.
    m_keyValues = readKeyValues(s);
}

inline std::string FromJSONVisitor::decodeBase64(const std::string &input) noexcept {
    const std::string ALPHABET{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
    uint8_t counter{0};
    std::array<char, 4> buffer;
    std::string decoded;
    for (uint32_t i{0}; i < input.size(); i++) {
        char c;
        for (c = 0; c < 64 && (ALPHABET.at(static_cast<uint8_t>(c)) != input.at(i)); c++) {}

        buffer[counter++] = c;
        if (4 == counter) {
            decoded.push_back(static_cast<char>((buffer[0] << 2) + (buffer[1] >> 4)));
            if (64 != buffer[2]) {
                decoded.push_back(static_cast<char>((buffer[1] << 4) + (buffer[2] >> 2)));
            }
            if (64 != buffer[3]) {
                decoded.push_back(static_cast<char>((buffer[2] << 6) + buffer[3]));
            }
            counter = 0;
        }
    }
    return decoded;
}

inline void FromJSONVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;
}

inline void FromJSONVisitor::postVisit() noexcept {}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::IS_FALSE == m_keyValues[name].m_type) {
                v = false;
            } else if (JSONConstants::IS_TRUE == m_keyValues[name].m_type) {
                v = true;
            } else if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = (1 == static_cast<uint32_t>(linb::any_cast<double>(m_keyValues[name].m_value)));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::STRING == m_keyValues[name].m_type) {
                v = linb::any_cast<std::string>(m_keyValues[name].m_value).at(0);
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<int8_t>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<uint8_t>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<int16_t>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<uint16_t>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<int32_t>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<uint32_t>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<int64_t>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<uint64_t>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = static_cast<float>(linb::any_cast<double>(m_keyValues[name].m_value));
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            if (JSONConstants::NUMBER == m_keyValues[name].m_type) {
                v = linb::any_cast<double>(m_keyValues[name].m_value);
            }
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void FromJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)id;
    (void)typeName;
    if (0 < m_keyValues.count(name)) {
        try {
            std::string tmp{linb::any_cast<std::string>(m_keyValues[name].m_value)};
            v = FromJSONVisitor::decodeBase64(tmp);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/GenericMessage.hpp"

#include <istream>
#include <iterator>
#include <regex>

namespace cluon {

inline void GenericMessage::GenericMessageVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)longName;
    m_metaMessage.messageIdentifier(id).messageName(shortName);
    if (!longName.empty()) {
        const auto pos = longName.rfind(shortName);
        if (std::string::npos != pos) {
            m_metaMessage.packageName(longName.substr(0, pos - 1));
        }
    }
}

inline void GenericMessage::GenericMessageVisitor::postVisit() noexcept {}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::BOOL_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::CHAR_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::INT8_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::UINT8_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::INT16_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::UINT16_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::INT32_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::UINT32_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::INT64_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::UINT64_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::FLOAT_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::DOUBLE_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline void GenericMessage::GenericMessageVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    cluon::MetaMessage::MetaField mf;
    mf.fieldIdentifier(id).fieldDataType(cluon::MetaMessage::MetaField::STRING_T).fieldDataTypeName(typeName).fieldName(name);
    m_intermediateDataRepresentation[mf.fieldIdentifier()] = linb::any{v};
    m_metaMessage.add(std::move(mf));
}

inline MetaMessage GenericMessage::GenericMessageVisitor::metaMessage() const noexcept {
    return m_metaMessage;
}

inline std::unordered_map<uint32_t, linb::any, UseUInt32ValueAsHashKey> GenericMessage::GenericMessageVisitor::intermediateDataRepresentation() const noexcept {
    return m_intermediateDataRepresentation;
}

////////////////////////////////////////////////////////////////////////////////

inline int32_t GenericMessage::ID() {
    return m_metaMessage.messageIdentifier();
}

inline const std::string GenericMessage::ShortName() {
    std::string tmp{LongName()};
    std::replace(tmp.begin(), tmp.end(), '.', ' ');
    std::istringstream sstr{tmp};
    std::vector<std::string> tokens{std::istream_iterator<std::string>(sstr), std::istream_iterator<std::string>()};

    return tokens.back();
}

inline const std::string GenericMessage::LongName() {
    return m_metaMessage.packageName() + (!m_metaMessage.packageName().empty() ? "." : "") + m_metaMessage.messageName();
}

inline void GenericMessage::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;
}

inline void GenericMessage::postVisit() noexcept {}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<bool>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<char>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<int8_t>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<uint8_t>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<int16_t>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<uint16_t>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<int32_t>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<uint32_t>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<int64_t>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<uint64_t>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<float>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<double>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

inline void GenericMessage::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)typeName;
    (void)name;
    if (0 < m_intermediateDataRepresentation.count(id)) {
        try {
            v = linb::any_cast<std::string>(m_intermediateDataRepresentation[id]);
        } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

inline void GenericMessage::createFrom(const MetaMessage &mm, const std::vector<MetaMessage> &mms) noexcept {
    m_metaMessage = mm;
    m_longName    = m_metaMessage.messageName();

    m_scopeOfMetaMessages.clear();
    m_scopeOfMetaMessages = mms;

    m_mapForScopeOfMetaMessages.clear();
    for (const auto &e : m_scopeOfMetaMessages) { m_mapForScopeOfMetaMessages[e.messageName()] = e; }

    m_intermediateDataRepresentation.clear();
    for (const auto &f : m_metaMessage.listOfMetaFields()) {
        if (f.fieldDataType() == MetaMessage::MetaField::BOOL_T) {
            try {
                linb::any _v{false};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::CHAR_T) {
            try {
                linb::any _v{static_cast<char>('\0')};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::UINT8_T) {
            try {
                linb::any _v{static_cast<uint8_t>(0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::INT8_T) {
            try {
                linb::any _v{static_cast<int8_t>(0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::UINT16_T) {
            try {
                linb::any _v{static_cast<uint16_t>(0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::INT16_T) {
            try {
                linb::any _v{static_cast<int16_t>(0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::UINT32_T) {
            try {
                linb::any _v{static_cast<uint32_t>(0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::INT32_T) {
            try {
                linb::any _v{static_cast<int32_t>(0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::UINT64_T) {
            try {
                linb::any _v{static_cast<uint64_t>(0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::INT64_T) {
            try {
                linb::any _v{static_cast<int64_t>(0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::FLOAT_T) {
            try {
                linb::any _v{static_cast<float>(0.0f)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::DOUBLE_T) {
            try {
                linb::any _v{static_cast<double>(0.0)};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if ((f.fieldDataType() == MetaMessage::MetaField::STRING_T) || (f.fieldDataType() == MetaMessage::MetaField::BYTES_T)) {
            try {
                linb::any _v                                          = std::string{};
                m_intermediateDataRepresentation[f.fieldIdentifier()] = _v;
            } catch (const linb::bad_any_cast &) { // LCOV_EXCL_LINE
            }
        } else if (f.fieldDataType() == MetaMessage::MetaField::MESSAGE_T) {
            if (0 < m_mapForScopeOfMetaMessages.count(f.fieldDataTypeName())) {
                // Create a GenericMessage from the decoded Proto-data.
                cluon::GenericMessage gm;
                gm.createFrom(m_mapForScopeOfMetaMessages[f.fieldDataTypeName()], m_scopeOfMetaMessages);

                m_intermediateDataRepresentation[f.fieldIdentifier()] = linb::any{gm};
            }
        }
    }
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/ToJSONVisitor.hpp"

#include <iomanip>
#include <sstream>

namespace cluon {

inline ToJSONVisitor::ToJSONVisitor(bool withOuterCurlyBraces, const std::map<uint32_t, bool> &mask) noexcept
    : m_withOuterCurlyBraces(withOuterCurlyBraces)
    , m_mask(mask) {}

inline std::string ToJSONVisitor::json() const noexcept {
    const std::string tmp{m_buffer.str()};
    std::string retVal{"{}"};
    if (2 < tmp.size()) {
        retVal = {(m_withOuterCurlyBraces ? "{" : "") + tmp.substr(0, tmp.size() - 2) + (m_withOuterCurlyBraces ? "}" : "")};
    }
    return retVal;
}

inline void ToJSONVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)longName;
    (void)shortName;
}

inline void ToJSONVisitor::postVisit() noexcept {}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << '\"' << v << '\"' << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << +v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << +v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << +v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << +v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << v << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << std::setprecision(7) << v << std::setprecision(6) << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << std::setprecision(11) << v << std::setprecision(6) << ',' << '\n';
    }
}

inline void ToJSONVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        m_buffer << '\"' << name << '\"' << ':' << '\"' << ToJSONVisitor::encodeBase64(v) << '\"' << ',' << '\n';
    }
}

inline std::string ToJSONVisitor::encodeBase64(const std::string &input) noexcept {
    std::string retVal;

    const std::string ALPHABET{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
    auto length{input.length()};
    uint32_t index{0};
    uint32_t value{0};

    while (length > 2) {
        value = static_cast<uint32_t>(static_cast<unsigned char>(input.at(index++))) << 16;
        value |= static_cast<uint32_t>(static_cast<unsigned char>(input.at(index++))) << 8;
        value |= static_cast<uint32_t>(static_cast<unsigned char>(input.at(index++)));
        retVal += ALPHABET.at((value & 0xFC0000) >> 18);
        retVal += ALPHABET.at((value & 0x3F000) >> 12);
        retVal += ALPHABET.at((value & 0xFC0) >> 6);
        retVal += ALPHABET.at(value & 0x3F);
        length -= 3;
    }
    if (length == 2) {
        value = static_cast<uint32_t>(static_cast<unsigned char>(input.at(index++))) << 16;
        value |= static_cast<uint32_t>(static_cast<unsigned char>(input.at(index++))) << 8;
        retVal += ALPHABET.at((value & 0xFC0000) >> 18);
        retVal += ALPHABET.at((value & 0x3F000) >> 12);
        retVal += ALPHABET.at((value & 0xFC0) >> 6);
        retVal += "=";
    } else if (length == 1) {
        value = static_cast<uint32_t>(static_cast<unsigned char>(input.at(index++))) << 16;
        retVal += ALPHABET.at((value & 0xFC0000) >> 18);
        retVal += ALPHABET.at((value & 0x3F000) >> 12);
        retVal += "==";
    }

    return retVal;
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/ToCSVVisitor.hpp"
//#include "cluon/ToJSONVisitor.hpp"

#include <iomanip>
#include <sstream>

namespace cluon {

inline ToCSVVisitor::ToCSVVisitor(char delimiter, bool withHeader, const std::map<uint32_t, bool> &mask) noexcept
    : m_mask(mask)
    , m_prefix("")
    , m_delimiter(delimiter)
    , m_withHeader(withHeader)
    , m_isNested(false) {}

inline ToCSVVisitor::ToCSVVisitor(const std::string &prefix, char delimiter, bool withHeader, bool isNested) noexcept
    : m_prefix(prefix)
    , m_delimiter(delimiter)
    , m_withHeader(withHeader)
    , m_isNested(isNested) {}

inline void ToCSVVisitor::clear() noexcept {
    m_bufferHeader.str("");
    m_bufferValues.str("");
    m_fillHeader = true;
}

inline std::string ToCSVVisitor::csv() const noexcept {
    std::stringstream tmp;
    if (m_withHeader) {
        tmp << m_bufferHeader.str();
    }
    tmp << m_bufferValues.str();
    const std::string retVal{tmp.str()};
    return retVal;
}

inline void ToCSVVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;
}

inline void ToCSVVisitor::postVisit() noexcept {
    if (m_fillHeader) {
        m_bufferHeader << (m_isNested ? "" : "\n");
    }
    m_fillHeader = false;
    m_bufferValues << (m_isNested ? "" : "\n");
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << +v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << +v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << v << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << std::setprecision(7) << v << std::setprecision(6) << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << std::setprecision(11) << v << std::setprecision(6) << m_delimiter;
    }
}

inline void ToCSVVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)typeName;
    if ((0 == m_mask.count(id)) || m_mask[id]) {
        if (m_fillHeader) {
            m_bufferHeader << m_prefix << (!m_prefix.empty() ? "." : "") << name << m_delimiter;
        }
        m_bufferValues << '\"' << cluon::ToJSONVisitor::encodeBase64(v) << '\"' << m_delimiter;
    }
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for htonll
#endif
// clang-format on

//#include "cluon/ToLCMVisitor.hpp"

#include <cstring>

namespace cluon {

inline std::string ToLCMVisitor::encodedData(bool withHash) const noexcept {
    int64_t _hash = hash();
    _hash         = static_cast<int64_t>(htobe64(_hash));

    std::stringstream hashBuffer;
    hashBuffer.write(reinterpret_cast<const char *>(&_hash), sizeof(int64_t));

    const std::string s{(withHash ? hashBuffer.str() : "") + m_buffer.str()};
    return s;
}

////////////////////////////////////////////////////////////////////////////////

inline void ToLCMVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;
}

inline void ToLCMVisitor::postVisit() noexcept {}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("boolean");
    calculateHash(0);
    m_buffer.write(reinterpret_cast<char *>(&v), sizeof(bool));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int8_t");
    calculateHash(0);
    m_buffer.write(reinterpret_cast<char *>(&v), sizeof(char));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int8_t");
    calculateHash(0);
    m_buffer.write(reinterpret_cast<char *>(&v), sizeof(int8_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int8_t");
    calculateHash(0);
    m_buffer.write(reinterpret_cast<char *>(&v), sizeof(uint8_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int16_t");
    calculateHash(0);
    int16_t _v = static_cast<int16_t>(htobe16(v));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int16_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int16_t");
    calculateHash(0);
    int16_t _v = static_cast<int16_t>(htobe16(v));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int16_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int32_t");
    calculateHash(0);
    int32_t _v = static_cast<int32_t>(htobe32(v));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int32_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int32_t");
    calculateHash(0);
    int32_t _v = static_cast<int32_t>(htobe32(v));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int32_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int64_t");
    calculateHash(0);
    int64_t _v = static_cast<int64_t>(htobe64(v));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int64_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("int64_t");
    calculateHash(0);
    int64_t _v = static_cast<int64_t>(htobe64(v));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int64_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("float");
    calculateHash(0);
    int32_t _v{0};
    std::memmove(&_v, &v, sizeof(int32_t));
    _v = static_cast<int32_t>(htobe32(_v));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int32_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("double");
    calculateHash(0);
    int64_t _v{0};
    std::memmove(&_v, &v, sizeof(int64_t));
    _v = static_cast<int64_t>(htobe64(_v));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int64_t));
}

inline void ToLCMVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)id;
    (void)typeName;
    calculateHash(name);
    calculateHash("string");
    calculateHash(0);

    const std::size_t LENGTH = v.length();
    int32_t _v               = static_cast<int32_t>(htobe32(static_cast<uint32_t>(LENGTH + 1)));
    m_buffer.write(reinterpret_cast<char *>(&_v), sizeof(int32_t));
    m_buffer.write(v.c_str(), static_cast<std::streamsize>(LENGTH)); // LENGTH won't be negative.
    m_buffer << '\0';
}

////////////////////////////////////////////////////////////////////////////////

inline int64_t ToLCMVisitor::hash() const noexcept {
    // Apply ZigZag encoding for hash from this message's fields and depending
    // hashes for complex nested types.
    int64_t tmp{m_hash};
    for (int64_t v : m_hashes) { tmp += v; }

    const int64_t hash = (tmp << 1) + ((tmp >> 63) & 1);
    return hash;
}

inline void ToLCMVisitor::calculateHash(char c) noexcept {
    m_hash = ((m_hash << 8) ^ (m_hash >> 55)) + c;
}

inline void ToLCMVisitor::calculateHash(const std::string &s) noexcept {
    const std::string tmp{(s.length() > 255 ? s.substr(0, 255) : s)};
    const uint8_t length{static_cast<uint8_t>(tmp.length())};
    calculateHash(static_cast<char>(length));
    for (auto c : s) { calculateHash(c); }
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/LCMToGenericMessage.hpp"
//#include "cluon/FromLCMVisitor.hpp"
//#include "cluon/MessageParser.hpp"

#include <array>
#include <iostream>
#include <sstream>

// clang-format off
#ifdef WIN32
    #undef be32toh
    #define be32toh(x) __ntohl(x)

    #include <cstdint>
    #include <cstring>
    uint32_t __ntohl(const uint32_t v) {
        uint8_t d[4] = {};
        std::memmove(&d, &v, sizeof(d));

        return ((uint32_t) d[3] << 0)
             | ((uint32_t) d[2] << 8)
             | ((uint32_t) d[1] << 16)
             | ((uint32_t) d[0] << 24);
    }
#endif
// clang-format on

namespace cluon {

inline int32_t LCMToGenericMessage::setMessageSpecification(const std::string &ms) noexcept {
    int32_t retVal{-1};

    m_listOfMetaMessages.clear();
    m_scopeOfMetaMessages.clear();

    cluon::MessageParser mp;
    auto parsingResult = mp.parse(ms);
    if (cluon::MessageParser::MessageParserErrorCodes::NO_MESSAGEPARSER_ERROR == parsingResult.second) {
        m_listOfMetaMessages = parsingResult.first;
        for (const auto &mm : m_listOfMetaMessages) { m_scopeOfMetaMessages[mm.messageName()] = mm; }
        retVal = static_cast<int32_t>(m_listOfMetaMessages.size());
    }
    return retVal;
}

inline cluon::GenericMessage LCMToGenericMessage::getGenericMessage(const std::string &data) noexcept {
    cluon::GenericMessage gm;

    if (!m_listOfMetaMessages.empty()) {
        constexpr uint8_t LCM_HEADER_SIZE{4 /*magic number*/ + 4 /*sequence number*/ + 1 /*'\0' after channel name*/};
        if (LCM_HEADER_SIZE < data.size()) {
            // First, read magic number.
            constexpr uint32_t MAGIC_NUMBER_LCM2{0x4c433032};
            uint32_t offset{0};
            uint32_t magicNumber{0};
            {
                std::stringstream sstr{std::string(&data[offset], 4)};
                // clang-format off
                sstr.read(reinterpret_cast<char *>(&magicNumber), sizeof(uint32_t)); /* Flawfinder: ignore */ // NOLINT
                // clang-format on
                magicNumber = be32toh(magicNumber);
            }
            if (MAGIC_NUMBER_LCM2 == magicNumber) {
                offset += 4;

                // Next, read sequence number in case of fragmented data.
                uint32_t sequenceNumber{0};
                {
                    std::stringstream sstr{std::string(&data[offset], 4)};
                    // clang-format off
                    sstr.read(reinterpret_cast<char *>(&sequenceNumber), sizeof(uint32_t)); /* Flawfinder: ignore */ // NOLINT
                    // clang-format on
                    sequenceNumber = be32toh(sequenceNumber);
                }
                // Only support for non-fragmented messages.
                if (0 == sequenceNumber) {
                    offset += 4;

                    const std::string::size_type START_POSITION = offset;
                    std::string::size_type pos                  = data.find('\0', START_POSITION); // Extract channel name.
                    if (std::string::npos != pos) {
                        const std::string CHANNEL_NAME(data.substr(START_POSITION, (pos - START_POSITION)));

                        // Next, find the MetaMessage corresponding to the channel name
                        // and create a Message therefrom based on the decoded LCM data.
                        if ((0 < m_scopeOfMetaMessages.count(CHANNEL_NAME)) && (std::string::npos != (pos + 1))) {
                            // data[offset+i] marks now the beginning of the payload to be decoded.
                            std::stringstream sstr{data.substr(pos + 1)};

                            cluon::FromLCMVisitor fromLCM;
                            fromLCM.decodeFrom(sstr);

                            gm.createFrom(m_scopeOfMetaMessages[CHANNEL_NAME], m_listOfMetaMessages);
                            gm.accept(fromLCM);
                        }
                    }
                }
            }
        }
    }

    return gm;
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// clang-format off
#ifdef WIN32
    #include <Winsock2.h> // for htonl, htons
#endif
// clang-format on

//#include "cluon/ToMsgPackVisitor.hpp"

#include <cstring>
#include <limits>

namespace cluon {

inline std::string ToMsgPackVisitor::encodedData() const noexcept {
    std::stringstream prefix;
    if (m_numberOfFields <= 0xF) {
        const uint8_t pairs = static_cast<uint8_t>(MsgPackConstants::FIXMAP) | static_cast<uint8_t>(m_numberOfFields);
        prefix.write(reinterpret_cast<const char *>(&pairs), sizeof(uint8_t));
    } else if ((m_numberOfFields > 0xF) && (m_numberOfFields <= 0xFFFF)) {
        const uint8_t pairs = static_cast<uint8_t>(MsgPackConstants::MAP16);
        prefix.write(reinterpret_cast<const char *>(&pairs), sizeof(uint8_t));
        uint16_t n = htobe16(static_cast<uint16_t>(m_numberOfFields));
        prefix.write(reinterpret_cast<const char *>(&n), sizeof(uint16_t));
    } else if (m_numberOfFields > 0xFFFF) {                                    // LCOV_EXCL_LINE
        const uint8_t pairs = static_cast<uint8_t>(MsgPackConstants::MAP32);   // LCOV_EXCL_LINE
        prefix.write(reinterpret_cast<const char *>(&pairs), sizeof(uint8_t)); // LCOV_EXCL_LINE
        uint32_t n = htobe32(static_cast<uint32_t>(m_numberOfFields));         // LCOV_EXCL_LINE
        prefix.write(reinterpret_cast<const char *>(&n), sizeof(uint32_t));    // LCOV_EXCL_LINE
    }
    std::string s{prefix.str() + m_buffer.str()};
    return s;
}

inline void ToMsgPackVisitor::encode(std::ostream &o, const std::string &s) {
    const uint32_t LENGTH{static_cast<uint32_t>(s.size())};
    if (LENGTH < 32) {
        const uint8_t v = static_cast<uint8_t>(MsgPackConstants::FIXSTR) | static_cast<uint8_t>(LENGTH);
        o.write(reinterpret_cast<const char *>(&v), sizeof(uint8_t));
    } else if (LENGTH <= 0xFF) {
        const uint8_t v = static_cast<uint8_t>(MsgPackConstants::STR8);
        o.write(reinterpret_cast<const char *>(&v), sizeof(uint8_t));
        uint8_t len = static_cast<uint8_t>(LENGTH);
        o.write(reinterpret_cast<const char *>(&len), sizeof(uint8_t));
    } else if (LENGTH <= 0xFFFF) {
        const uint8_t v = static_cast<uint8_t>(MsgPackConstants::STR16);
        o.write(reinterpret_cast<const char *>(&v), sizeof(uint8_t));
        uint16_t len = htobe16(static_cast<uint16_t>(LENGTH));
        o.write(reinterpret_cast<const char *>(&len), sizeof(uint16_t));
    } else {
        const uint8_t v = static_cast<uint8_t>(MsgPackConstants::STR32);
        o.write(reinterpret_cast<const char *>(&v), sizeof(uint8_t));
        uint32_t len = htobe32(LENGTH);
        o.write(reinterpret_cast<const char *>(&len), sizeof(uint32_t));
    }
    o.write(s.c_str(), static_cast<std::streamsize>(LENGTH)); // LENGTH won't be negative.
}

inline void ToMsgPackVisitor::encodeUint(std::ostream &o, uint64_t v) {
    if (0x7f >= v) {
        const uint8_t _v = static_cast<uint8_t>(v);
        o.write(reinterpret_cast<const char *>(&_v), sizeof(uint8_t));
    } else if (0xFF >= v) {
        const uint8_t t = static_cast<uint8_t>(MsgPackConstants::UINT8);
        o.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
        const uint8_t _v = static_cast<uint8_t>(v);
        o.write(reinterpret_cast<const char *>(&_v), sizeof(uint8_t));
    } else if (0xFFFF >= v) {
        const uint8_t t = static_cast<uint8_t>(MsgPackConstants::UINT16);
        o.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
        uint16_t _v = static_cast<uint16_t>(v);
        _v          = htobe16(_v);
        o.write(reinterpret_cast<const char *>(&_v), sizeof(uint16_t));
    } else if (0xFFFFFFFF >= v) {
        const uint8_t t = static_cast<uint8_t>(MsgPackConstants::UINT32);
        o.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
        uint32_t _v = static_cast<uint32_t>(v);
        _v          = htobe32(_v);
        o.write(reinterpret_cast<const char *>(&_v), sizeof(uint32_t));
    } else {
        const uint8_t t = static_cast<uint8_t>(MsgPackConstants::UINT64);
        o.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
        uint64_t _v = v;
        _v          = htobe64(_v);
        o.write(reinterpret_cast<const char *>(&_v), sizeof(uint64_t));
    }
}

inline void ToMsgPackVisitor::encodeInt(std::ostream &o, int64_t v) {
    if (-31 <= v) {
        int8_t _v = static_cast<int8_t>(v);
        o.write(reinterpret_cast<const char *>(&_v), sizeof(int8_t));
    } else if (std::numeric_limits<int8_t>::lowest() <= v) {
        const uint8_t t = static_cast<uint8_t>(MsgPackConstants::INT8);
        o.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
        int8_t _v = static_cast<int8_t>(v);
        o.write(reinterpret_cast<const char *>(&_v), sizeof(int8_t));
    } else if (std::numeric_limits<int16_t>::lowest() <= v) {
        const uint8_t t = static_cast<uint8_t>(MsgPackConstants::INT16);
        o.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
        int16_t _v = static_cast<int16_t>(v);
        _v         = static_cast<int16_t>(htobe16(_v));
        o.write(reinterpret_cast<const char *>(&_v), sizeof(int16_t));
    } else if (std::numeric_limits<int32_t>::lowest() <= v) {
        const uint8_t t = static_cast<uint8_t>(MsgPackConstants::INT32);
        o.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
        int32_t _v = static_cast<int32_t>(v);
        _v         = static_cast<int32_t>(htobe32(_v));
        o.write(reinterpret_cast<const char *>(&_v), sizeof(int32_t));
    } else {
        const uint8_t t = static_cast<uint8_t>(MsgPackConstants::INT64);
        o.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
        int64_t _v = static_cast<int64_t>(v);
        _v         = static_cast<int64_t>(htobe64(_v));
        o.write(reinterpret_cast<const char *>(&_v), sizeof(int64_t));
    }
}

inline void ToMsgPackVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)id;
    (void)shortName;
    (void)longName;

    m_numberOfFields = 0;
    m_buffer.str("");
}

inline void ToMsgPackVisitor::postVisit() noexcept {}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    const uint8_t value = (v ? static_cast<uint8_t>(MsgPackConstants::IS_TRUE) : static_cast<uint8_t>(MsgPackConstants::IS_FALSE));
    m_buffer.write(reinterpret_cast<const char *>(&value), sizeof(uint8_t));
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    const std::string s(1, v);
    encode(m_buffer, s);
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    (v < 0) ? encodeInt(m_buffer, v) : encodeUint(m_buffer, static_cast<uint8_t>(v));
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    encodeUint(m_buffer, v);
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    (v < 0) ? encodeInt(m_buffer, v) : encodeUint(m_buffer, static_cast<uint16_t>(v));
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    encodeUint(m_buffer, v);
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    (v < 0) ? encodeInt(m_buffer, v) : encodeUint(m_buffer, static_cast<uint32_t>(v));
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    encodeUint(m_buffer, v);
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    (v < 0) ? encodeInt(m_buffer, v) : encodeUint(m_buffer, static_cast<uint64_t>(v));
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    encodeUint(m_buffer, v);
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    const uint8_t t = static_cast<uint8_t>(MsgPackConstants::FLOAT);
    m_buffer.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
    uint32_t _v{0};
    std::memmove(&_v, &v, sizeof(float));
    _v = htobe32(_v);
    m_buffer.write(reinterpret_cast<const char *>(&_v), sizeof(uint32_t));
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    const uint8_t t = static_cast<uint8_t>(MsgPackConstants::DOUBLE);
    m_buffer.write(reinterpret_cast<const char *>(&t), sizeof(uint8_t));
    uint64_t _v{0};
    std::memmove(&_v, &v, sizeof(double));
    _v = htobe64(_v);
    m_buffer.write(reinterpret_cast<const char *>(&_v), sizeof(double));
    m_numberOfFields++;
}

inline void ToMsgPackVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)id;
    (void)typeName;

    encode(m_buffer, name);
    encode(m_buffer, v);
    m_numberOfFields++;
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/OD4Session.hpp"
//#include "cluon/Envelope.hpp"
//#include "cluon/FromProtoVisitor.hpp"
//#include "cluon/TerminateHandler.hpp"
//#include "cluon/Time.hpp"

#include <iostream>
#include <sstream>
#include <thread>

namespace cluon {

inline OD4Session::OD4Session(uint16_t CID, std::function<void(cluon::data::Envelope &&envelope)> delegate) noexcept
    : m_receiver{nullptr}
    , m_sender{"225.0.0." + std::to_string(CID), 12175}
    , m_delegate(std::move(delegate))
    , m_mapOfDataTriggeredDelegatesMutex{}
    , m_mapOfDataTriggeredDelegates{} {
    m_receiver = std::make_unique<cluon::UDPReceiver>(
        "225.0.0." + std::to_string(CID),
        12175,
        [this](std::string &&data, std::string &&from, std::chrono::system_clock::time_point &&timepoint) {
            this->callback(std::move(data), std::move(from), std::move(timepoint));
        },
        m_sender.getSendFromPort() /* passing our local send from port to the UDPReceiver to filter out our own bytes */);
}

inline void OD4Session::timeTrigger(float freq, std::function<bool()> delegate) noexcept {
    if (nullptr != delegate) {
        bool delegateIsRunning{true};
        const int64_t TIME_SLICE_IN_MILLISECONDS{static_cast<uint32_t>(1000 / ((freq > 0) ? freq : 1.0f))};
        do {
            cluon::data::TimeStamp before{cluon::time::now()};
            try {
                delegateIsRunning = delegate();
            } catch (...) {
                delegateIsRunning = false; // delegate threw exception.
            }
            cluon::data::TimeStamp after{cluon::time::now()};

            const int64_t beforeInMicroseconds{cluon::time::toMicroseconds(before)};
            const int64_t afterInMicroseconds{cluon::time::toMicroseconds(after)};

            const int64_t timeSpent{(afterInMicroseconds > beforeInMicroseconds) ? (afterInMicroseconds - beforeInMicroseconds) / 1000 : 0};
            const int64_t timeToSleepInMilliseconds{TIME_SLICE_IN_MILLISECONDS - timeSpent};

            // Sleep the remaining time.
            if ((timeToSleepInMilliseconds > 0) && (timeToSleepInMilliseconds <= TIME_SLICE_IN_MILLISECONDS)) {
                std::this_thread::sleep_for(std::chrono::duration<int64_t, std::milli>(timeToSleepInMilliseconds));
            } else {
                std::cerr << "[cluon::OD4Session]: time-triggered delegate violated allocated time slice." << std::endl;
            }
        } while (delegateIsRunning && !TerminateHandler::instance().isTerminated.load());
    }
}

inline bool OD4Session::dataTrigger(int32_t messageIdentifier, std::function<void(cluon::data::Envelope &&envelope)> delegate) noexcept {
    bool retVal{false};
    if (nullptr == m_delegate) {
        try {
            std::lock_guard<std::mutex> lck{m_mapOfDataTriggeredDelegatesMutex};
            if ((nullptr == delegate) && (m_mapOfDataTriggeredDelegates.count(messageIdentifier) > 0)) {
                auto element = m_mapOfDataTriggeredDelegates.find(messageIdentifier);
                if (element != m_mapOfDataTriggeredDelegates.end()) {
                    m_mapOfDataTriggeredDelegates.erase(element);
                }
            } else {
                m_mapOfDataTriggeredDelegates[messageIdentifier] = delegate;
            }
            retVal = true;
        } catch (...) {} // LCOV_EXCL_LINE
    }
    return retVal;
}

inline void OD4Session::callback(std::string &&data, std::string && /*from*/, std::chrono::system_clock::time_point &&timepoint) noexcept {
    size_t numberOfDataTriggeredDelegates{0};
    {
        try {
            std::lock_guard<std::mutex> lck{m_mapOfDataTriggeredDelegatesMutex};
            numberOfDataTriggeredDelegates = m_mapOfDataTriggeredDelegates.size();
        } catch (...) {} // LCOV_EXCL_LINE
    }
    // Only unpack the envelope when it needs to be post-processed.
    if ((nullptr != m_delegate) || (0 < numberOfDataTriggeredDelegates)) {
        std::stringstream sstr(data);
        auto retVal = extractEnvelope(sstr);

        if (retVal.first) {
            cluon::data::Envelope env{retVal.second};
            env.received(cluon::time::convert(timepoint));

            // "Catch all"-delegate.
            if (nullptr != m_delegate) {
                m_delegate(std::move(env));
            } else {
                try {
                    // Data triggered-delegates.
                    std::lock_guard<std::mutex> lck{m_mapOfDataTriggeredDelegatesMutex};
                    if (m_mapOfDataTriggeredDelegates.count(env.dataType()) > 0) {
                        m_mapOfDataTriggeredDelegates[env.dataType()](std::move(env));
                    }
                } catch (...) {} // LCOV_EXCL_LINE
            }
        }
    }
}

inline void OD4Session::send(cluon::data::Envelope &&envelope) noexcept {
    sendInternal(cluon::serializeEnvelope(std::move(envelope)));
}

inline void OD4Session::sendInternal(std::string &&dataToSend) noexcept {
    m_sender.send(std::move(dataToSend));
}

inline bool OD4Session::isRunning() noexcept {
    return m_receiver->isRunning();
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/ToODVDVisitor.hpp"

#include <sstream>

namespace cluon {

inline std::string ToODVDVisitor::messageSpecification() const noexcept {
    std::stringstream tmp;
    for (const auto &e : m_forwardDeclarations) { tmp << e; }
    tmp << m_buffer.str();

    const std::string retVal{tmp.str()};
    return retVal;
}

inline void ToODVDVisitor::preVisit(int32_t id, const std::string &shortName, const std::string &longName) noexcept {
    (void)shortName;
    m_buffer << "message " << longName << " [ id = " << id << " ] {" << '\n';
}

inline void ToODVDVisitor::postVisit() noexcept {
    m_buffer << '}' << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, bool &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "bool"
             << " " << name << " [ default = false, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, char &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "char"
             << " " << name << " [ default = '0', id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int8_t &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "int8"
             << " " << name << " [ default = 0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint8_t &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "uint8"
             << " " << name << " [ default = 0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int16_t &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "int16"
             << " " << name << " [ default = 0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint16_t &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "uint16"
             << " " << name << " [ default = 0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int32_t &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "int32"
             << " " << name << " [ default = 0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint32_t &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "uint32"
             << " " << name << " [ default = 0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, int64_t &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "int64"
             << " " << name << " [ default = 0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, uint64_t &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "uint64"
             << " " << name << " [ default = 0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, float &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "float"
             << " " << name << " [ default = 0.0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, double &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "double"
             << " " << name << " [ default = 0.0, id = " << id << " ];" << '\n';
}

inline void ToODVDVisitor::visit(uint32_t id, std::string &&typeName, std::string &&name, std::string &v) noexcept {
    (void)typeName;
    (void)v;
    m_buffer << "    "
             << "string"
             << " " << name << " [ default = \"\", id = " << id << " ];" << '\n';
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/EnvelopeConverter.hpp"
//#include "cluon/Envelope.hpp"
//#include "cluon/FromJSONVisitor.hpp"
//#include "cluon/FromProtoVisitor.hpp"
//#include "cluon/GenericMessage.hpp"
//#include "cluon/MessageParser.hpp"
//#include "cluon/Time.hpp"
//#include "cluon/ToJSONVisitor.hpp"
//#include "cluon/ToProtoVisitor.hpp"
//#include "cluon/any/any.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace cluon {

inline int32_t EnvelopeConverter::setMessageSpecification(const std::string &ms) noexcept {
    int32_t retVal{-1};

    m_listOfMetaMessages.clear();
    m_scopeOfMetaMessages.clear();

    cluon::MessageParser mp;
    auto parsingResult = mp.parse(ms);
    if (cluon::MessageParser::MessageParserErrorCodes::NO_MESSAGEPARSER_ERROR == parsingResult.second) {
        m_listOfMetaMessages = parsingResult.first;
        for (const auto &mm : m_listOfMetaMessages) { m_scopeOfMetaMessages[mm.messageIdentifier()] = mm; }
        retVal = static_cast<int32_t>(m_listOfMetaMessages.size());
    }
    return retVal;
}

inline std::string EnvelopeConverter::getJSONFromProtoEncodedEnvelope(const std::string &protoEncodedEnvelope) noexcept {
    std::string retVal{"{}"};
    if (!m_listOfMetaMessages.empty()) {
        cluon::data::Envelope envelope;
        std::stringstream sstr(protoEncodedEnvelope);
        constexpr uint8_t OD4_HEADER_SIZE{5};
        if (OD4_HEADER_SIZE < protoEncodedEnvelope.size()) {
            // Try decoding complete OD4-encoded Envelope including header.
            constexpr uint8_t byte0{0x0D};
            constexpr uint8_t byte1{0xA4};
            if ((static_cast<uint8_t>(protoEncodedEnvelope.at(0)) == byte0) && (static_cast<uint8_t>(protoEncodedEnvelope.at(1)) == byte1)) {
                uint32_t length = (*reinterpret_cast<const uint32_t *>(protoEncodedEnvelope.data() + 1));
                length          = le32toh(length) >> 8;
                if ((OD4_HEADER_SIZE + length) == protoEncodedEnvelope.size()) {
                    auto result{extractEnvelope(sstr)};
                    if (result.first) {
                        envelope = result.second;
                    }
                }
            }
        }

        if (0 == envelope.dataType()) {
            // Directly decoding complete OD4 container failed, try decoding without header.
            cluon::FromProtoVisitor protoDecoder;
            protoDecoder.decodeFrom(sstr, envelope);
        }

        retVal = getJSONFromEnvelope(envelope);
    }
    return retVal;
}

inline std::string EnvelopeConverter::getJSONFromEnvelope(cluon::data::Envelope &envelope) noexcept {
    std::string retVal{"{}"};
    if (!m_listOfMetaMessages.empty()) {
        if (0 < m_scopeOfMetaMessages.count(envelope.dataType())) {
            // First, create JSON from Envelope.
            constexpr bool OUTER_CURLY_BRACES{false};
            // Ignore field 2 (= serializedData) as it will be replaced below.
            const std::map<uint32_t, bool> mask{{2, false}};
            ToJSONVisitor envelopeToJSON{OUTER_CURLY_BRACES, mask};
            envelope.accept(envelopeToJSON);

            std::stringstream sstr{envelope.serializedData()};
            cluon::FromProtoVisitor protoDecoder;
            protoDecoder.decodeFrom(sstr);

            // Now, create JSON from payload.
            cluon::MetaMessage payload{m_scopeOfMetaMessages[envelope.dataType()]};
            cluon::GenericMessage gm;

            // Create "empty" GenericMessage from this MetaMessage.
            gm.createFrom(payload, m_listOfMetaMessages);

            // Set values in the newly created GenericMessage from ProtoDecoder.
            gm.accept(protoDecoder);

            ToJSONVisitor payloadToJSON{OUTER_CURLY_BRACES};
            try {
                // Catch possible linb::any exception.
                gm.accept(payloadToJSON);
            } catch (const linb::bad_any_cast &) {} // LCOV_EXCL_LINE

            std::string tmp{payload.messageName()};
            std::replace(tmp.begin(), tmp.end(), '.', '_');

            const std::string strPayloadJSON{payloadToJSON.json() != "{}" ? payloadToJSON.json() : ""};

            retVal = '{' + envelopeToJSON.json() + ',' + '\n' + '"' + tmp + '"' + ':' + '{' + strPayloadJSON + '}' + '}';
        }
    }
    return retVal;
}

// clang-format off
inline std::string EnvelopeConverter::getProtoEncodedEnvelopeFromJSONWithoutTimeStamps(const std::string &json, int32_t messageIdentifier, uint32_t senderStamp) noexcept {
    // clang-format on
    return getProtoEncodedEnvelopeFromJSON(json, messageIdentifier, senderStamp, cluon::data::TimeStamp());
}

// clang-format off
inline std::string EnvelopeConverter::getProtoEncodedEnvelopeFromJSON(const std::string &json, int32_t messageIdentifier, uint32_t senderStamp) noexcept {
    // clang-format on
    return getProtoEncodedEnvelopeFromJSON(json, messageIdentifier, senderStamp, cluon::time::now());
}

// clang-format off
inline std::string EnvelopeConverter::getProtoEncodedEnvelopeFromJSON(const std::string &json, int32_t messageIdentifier, uint32_t senderStamp, cluon::data::TimeStamp sampleTimeStamp) noexcept {
    // clang-format on
    std::string retVal;
    if (0 < m_scopeOfMetaMessages.count(messageIdentifier)) {
        // Get specification for message to be created.
        cluon::MetaMessage message{m_scopeOfMetaMessages[messageIdentifier]};

        // Create "empty" instance for the required message as GenericMessage.
        cluon::GenericMessage gm;
        gm.createFrom(message, m_listOfMetaMessages);

        // Parse data from given JSON.
        std::stringstream sstr{json};
        cluon::FromJSONVisitor jsonDecoder;
        jsonDecoder.decodeFrom(sstr);

        // Set values in the newly created GenericMessage from JSONDecoder.
        gm.accept(jsonDecoder);

        // Finally, transform GenericMessage into Envelope.
        ToProtoVisitor protoEncoder;
        gm.accept(protoEncoder);

        cluon::data::Envelope env;
        env.dataType(messageIdentifier).serializedData(protoEncoder.encodedData()).senderStamp(senderStamp).sampleTimeStamp(sampleTimeStamp);

        retVal = cluon::serializeEnvelope(std::move(env));
    }
    return retVal;
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/Player.hpp"
//#include "cluon/Envelope.hpp"
//#include "cluon/Time.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#include <utility>

namespace cluon {

inline IndexEntry::IndexEntry(const int64_t &sampleTimeStamp, const uint64_t &filePosition) noexcept
    : m_sampleTimeStamp(sampleTimeStamp)
    , m_filePosition(filePosition)
    , m_available(false) {}

////////////////////////////////////////////////////////////////////////

inline Player::Player(const std::string &file, const bool &autoRewind, const bool &threading) noexcept
    : m_threading(threading)
    , m_file(file)
    , m_recFile()
    , m_recFileValid(false)
    , m_autoRewind(autoRewind)
    , m_indexMutex()
    , m_index()
    , m_previousPreviousEnvelopeAlreadyReplayed(m_index.end())
    , m_previousEnvelopeAlreadyReplayed(m_index.begin())
    , m_currentEnvelopeToReplay(m_index.begin())
    , m_nextEntryToReadFromRecFile(m_index.begin())
    , m_desiredInitialLevel(0)
    , m_firstTimePointReturningAEnvelope()
    , m_numberOfReturnedEnvelopesInTotal(0)
    , m_delay(0)
    , m_envelopeCacheFillingThreadIsRunningMutex()
    , m_envelopeCacheFillingThreadIsRunning(false)
    , m_envelopeCacheFillingThread()
    , m_envelopeCache()
    , m_playerListenerMutex()
    , m_playerListener(nullptr) {
    initializeIndex();
    computeInitialCacheLevelAndFillCache();

    if (m_threading) {
        // Start concurrent thread to manage cache.
        setEnvelopeCacheFillingRunning(true);
        m_envelopeCacheFillingThread = std::thread(&Player::manageCache, this);
    }
}

inline Player::~Player() {
    if (m_threading) {
        // Stop concurrent thread to manage cache.
        setEnvelopeCacheFillingRunning(false);
        m_envelopeCacheFillingThread.join();
    }

    m_recFile.close();
}

////////////////////////////////////////////////////////////////////////

inline void Player::setPlayerListener(std::function<void(cluon::data::PlayerStatus playerStatus)> playerListener) noexcept {
    std::lock_guard<std::mutex> lck(m_playerListenerMutex);
    m_playerListener = playerListener;
}

////////////////////////////////////////////////////////////////////////

inline void Player::initializeIndex() noexcept {
    m_recFile.open(m_file.c_str(), std::ios_base::in | std::ios_base::binary); /* Flawfinder: ignore */
    m_recFileValid = m_recFile.good();

    if (m_recFileValid) {
        // Determine file size to display progress.
        m_recFile.seekg(0, m_recFile.end);
        int64_t fileLength = m_recFile.tellg();
        m_recFile.seekg(0, m_recFile.beg);

        // Read complete file and store file positions to envelopes to create
        // index of available data. The actual reading of Envelopes is deferred.
        uint64_t totalBytesRead = 0;
        const cluon::data::TimeStamp BEFORE{cluon::time::now()};
        {
            int32_t oldPercentage = -1;
            while (m_recFile.good()) {
                const uint64_t POS_BEFORE = static_cast<uint64_t>(m_recFile.tellg());
                auto retVal               = extractEnvelope(m_recFile);
                const uint64_t POS_AFTER  = static_cast<uint64_t>(m_recFile.tellg());

                if (!m_recFile.eof() && retVal.first) {
                    totalBytesRead += (POS_AFTER - POS_BEFORE);

                    // Store mapping .rec file position --> index entry.
                    const int64_t microseconds = cluon::time::toMicroseconds(retVal.second.sampleTimeStamp());
                    m_index.emplace(std::make_pair(microseconds, IndexEntry(microseconds, POS_BEFORE)));

                    const int32_t percentage = static_cast<int32_t>((static_cast<float>(m_recFile.tellg()) * 100.0f) / static_cast<float>(fileLength));
                    if ((percentage % 5 == 0) && (percentage != oldPercentage)) {
                        std::clog << "[cluon::Player]: Indexed " << percentage << "% from " << m_file << "." << std::endl;
                        oldPercentage = percentage;
                    }
                }
            }
        }
        const cluon::data::TimeStamp AFTER{cluon::time::now()};

        std::clog << "[cluon::Player]: " << m_file << " contains " << m_index.size() << " entries; "
                  << "read " << totalBytesRead << " bytes "
                  << "in " << cluon::time::deltaInMicroseconds(AFTER, BEFORE) / static_cast<int64_t>(1000 * 1000) << "s." << std::endl;
    } else {
        std::clog << "[cluon::Player]: " << m_file << " could not be opened." << std::endl;
    }
}

inline void Player::resetCaches() noexcept {
    try {
        std::lock_guard<std::mutex> lck(m_indexMutex);
        m_delay                            = 0;
        m_numberOfReturnedEnvelopesInTotal = 0;
        m_envelopeCache.clear();
    } catch (...) {} // LCOV_EXCL_LINE
}

inline void Player::resetIterators() noexcept {
    try {
        std::lock_guard<std::mutex> lck(m_indexMutex);
        // Point to first entry in index.
        m_nextEntryToReadFromRecFile = m_previousEnvelopeAlreadyReplayed = m_currentEnvelopeToReplay = m_index.begin();
        // Invalidate iterator for erasing entries point.
        m_previousPreviousEnvelopeAlreadyReplayed = m_index.end();
    } catch (...) {} // LCOV_EXCL_LINE
}

inline void Player::computeInitialCacheLevelAndFillCache() noexcept {
    if (m_recFileValid && (m_index.size() > 0)) {
        int64_t smallestSampleTimePoint = (std::numeric_limits<int64_t>::max)();
        int64_t largestSampleTimePoint  = (std::numeric_limits<int64_t>::min)();
        for (auto it = m_index.begin(); it != m_index.end(); it++) {
            smallestSampleTimePoint = (std::min)(smallestSampleTimePoint, it->first);
            largestSampleTimePoint  = (std::max)(largestSampleTimePoint, it->first);
        }

        const uint32_t ENTRIES_TO_READ_PER_SECOND_FOR_REALTIME_REPLAY
            = static_cast<uint32_t>(std::ceil(static_cast<float>(m_index.size()) * (static_cast<float>(Player::ONE_SECOND_IN_MICROSECONDS))
                                              / static_cast<float>(largestSampleTimePoint - smallestSampleTimePoint)));
        m_desiredInitialLevel = (std::max<uint32_t>)(ENTRIES_TO_READ_PER_SECOND_FOR_REALTIME_REPLAY * Player::LOOK_AHEAD_IN_S, MIN_ENTRIES_FOR_LOOK_AHEAD);

        std::clog << "[cluon::Player]: Initializing cache with " << m_desiredInitialLevel << " entries." << std::endl;

        resetCaches();
        resetIterators();
        fillEnvelopeCache(m_desiredInitialLevel);
    }
}

inline uint32_t Player::fillEnvelopeCache(const uint32_t &maxNumberOfEntriesToReadFromFile) noexcept {
    uint32_t entriesReadFromFile = 0;
    if (m_recFileValid && (maxNumberOfEntriesToReadFromFile > 0)) {
        // Reset any fstream's error states.
        m_recFile.clear();

        while ((m_nextEntryToReadFromRecFile != m_index.end()) && (entriesReadFromFile < maxNumberOfEntriesToReadFromFile)) {
            // Move to corresponding position in the .rec file.
            m_recFile.seekg(static_cast<std::streamoff>(m_nextEntryToReadFromRecFile->second.m_filePosition));

            // Read the corresponding cluon::data::Envelope.
            auto retVal = extractEnvelope(m_recFile);
            if (retVal.first) {
                // Store the envelope in the envelope cache.
                try {
                    std::lock_guard<std::mutex> lck(m_indexMutex);
                    m_nextEntryToReadFromRecFile->second.m_available
                        = m_envelopeCache.emplace(std::make_pair(m_nextEntryToReadFromRecFile->second.m_filePosition, retVal.second)).second;
                } catch (...) {} // LCOV_EXCL_LINE

                m_nextEntryToReadFromRecFile++;
                entriesReadFromFile++;
            }
        }
    }

    return entriesReadFromFile;
}

inline std::pair<bool, cluon::data::Envelope> Player::getNextEnvelopeToBeReplayed() noexcept {
    bool hasEnvelopeToReturn{false};
    cluon::data::Envelope envelopeToReturn;

    // If at "EOF", either throw exception or autorewind.
    if (m_currentEnvelopeToReplay == m_index.end()) {
        if (!m_autoRewind) {
            return std::make_pair(hasEnvelopeToReturn, envelopeToReturn);
        } else {
            rewind();
        }
    }

    if (m_currentEnvelopeToReplay != m_index.end()) {
        checkAvailabilityOfNextEnvelopeToBeReplayed();

        try {
            {
                std::lock_guard<std::mutex> lck(m_indexMutex);

                cluon::data::Envelope &nextEnvelope = m_envelopeCache[m_currentEnvelopeToReplay->second.m_filePosition];
                envelopeToReturn                    = nextEnvelope;

                m_delay = static_cast<uint32_t>(m_currentEnvelopeToReplay->first - m_previousEnvelopeAlreadyReplayed->first);

                // TODO: Delegate deleting into own thread.
                if (m_previousPreviousEnvelopeAlreadyReplayed != m_index.end()) {
                    auto it = m_envelopeCache.find(m_previousEnvelopeAlreadyReplayed->second.m_filePosition);
                    if (it != m_envelopeCache.end()) {
                        m_envelopeCache.erase(it);
                    }
                }

                m_previousPreviousEnvelopeAlreadyReplayed = m_previousEnvelopeAlreadyReplayed;
                m_previousEnvelopeAlreadyReplayed         = m_currentEnvelopeToReplay++;

                m_numberOfReturnedEnvelopesInTotal++;
            }

            // TODO compensate for internal data processing.

            // If Player is non-threaded, read next entry sequentially.
            if (!m_threading) {
                fillEnvelopeCache(1);
            }

            // Store sample time stamp as int64 to avoid unnecessary copying of Envelopes.
            hasEnvelopeToReturn = true;
        } catch (...) {} // LCOV_EXCL_LINE
    }
    return std::make_pair(hasEnvelopeToReturn, envelopeToReturn);
}

inline void Player::checkAvailabilityOfNextEnvelopeToBeReplayed() noexcept {
    uint64_t numberOfEntries = 0;
    do {
        {
            try {
                std::lock_guard<std::mutex> lck(m_indexMutex);
                numberOfEntries = m_envelopeCache.size();
            } catch (...) {} // LCOV_EXCL_LINE
        }
        if (0 == numberOfEntries) {
            using namespace std::chrono_literals; // LCOV_EXCL_LINE
            std::this_thread::sleep_for(10ms);    // LCOV_EXCL_LINE
        }
    } while (0 == numberOfEntries);
}

////////////////////////////////////////////////////////////////////////

inline uint32_t Player::totalNumberOfEnvelopesInRecFile() const noexcept {
    std::lock_guard<std::mutex> lck(m_indexMutex);
    return static_cast<uint32_t>(m_index.size());
}

inline uint32_t Player::delay() const noexcept {
    std::lock_guard<std::mutex> lck(m_indexMutex);
    // Make sure that delay is not exceeding the specified maximum delay.
    return std::min<uint32_t>(m_delay, Player::MAX_DELAY_IN_MICROSECONDS);
}

inline void Player::rewind() noexcept {
    if (m_threading) {
        // Stop concurrent thread.
        setEnvelopeCacheFillingRunning(false);
        m_envelopeCacheFillingThread.join();
    }

    computeInitialCacheLevelAndFillCache();

    if (m_threading) {
        // Re-start concurrent thread.
        setEnvelopeCacheFillingRunning(true);
        m_envelopeCacheFillingThread = std::thread(&Player::manageCache, this);
    }
}

inline void Player::seekTo(float ratio) noexcept {
    if (!(ratio < 0) && !(ratio > 1)) {
        bool enableThreading = m_threading;
        if (m_threading) {
            // Stop concurrent thread.
            setEnvelopeCacheFillingRunning(false);
            m_envelopeCacheFillingThread.join();
        }

        // Read data sequentially.
        m_threading = false;

        resetCaches();
        resetIterators();

        uint32_t numberOfEntriesInIndex = 0;
        try {
            std::lock_guard<std::mutex> lck(m_indexMutex);
            numberOfEntriesInIndex = static_cast<uint32_t>(m_index.size());
        } catch (...) {} // LCOV_EXCL_LINE

        // Fast forward.
        m_numberOfReturnedEnvelopesInTotal = 0;
        std::clog << "[cluon::Player]: Seeking to " << static_cast<float>(numberOfEntriesInIndex) * ratio << "/" << numberOfEntriesInIndex << std::endl;
        if (0 < ratio) {
            for (m_numberOfReturnedEnvelopesInTotal = 0;
                 m_numberOfReturnedEnvelopesInTotal < static_cast<uint32_t>(static_cast<float>(numberOfEntriesInIndex) * ratio) - 1;
                 m_numberOfReturnedEnvelopesInTotal++) {
                m_currentEnvelopeToReplay++;
            }
        }
        try {
            std::lock_guard<std::mutex> lck(m_indexMutex);
            m_nextEntryToReadFromRecFile = m_previousEnvelopeAlreadyReplayed = m_currentEnvelopeToReplay;
        } catch (...) {} // LCOV_EXCL_LINE

        // Refill cache.
        m_envelopeCache.clear();
        fillEnvelopeCache(static_cast<uint32_t>(static_cast<float>(m_desiredInitialLevel) * .3f));

        // Correct iterators if not at the beginning.
        if ((0 < ratio) && (ratio < 1)) {
            getNextEnvelopeToBeReplayed();
        }
        std::clog << "[cluon::Player]: Seeking done." << std::endl;

        if (enableThreading) {
            m_threading = enableThreading;
            // Re-start concurrent thread.
            setEnvelopeCacheFillingRunning(true);
            m_envelopeCacheFillingThread = std::thread(&Player::manageCache, this);
        }
    }
}

inline bool Player::hasMoreData() const noexcept {
    std::lock_guard<std::mutex> lck(m_indexMutex);
    return hasMoreDataFromRecFile();
}

inline bool Player::hasMoreDataFromRecFile() const noexcept {
    // File must be successfully opened AND
    //  the Player must be configured as m_autoRewind OR
    //  some entries are left to replay.
    return (m_recFileValid && (m_autoRewind || (m_currentEnvelopeToReplay != m_index.end())));
}

////////////////////////////////////////////////////////////////////////

inline void Player::setEnvelopeCacheFillingRunning(const bool &running) noexcept {
    std::lock_guard<std::mutex> lck(m_envelopeCacheFillingThreadIsRunningMutex);
    m_envelopeCacheFillingThreadIsRunning = running;
}

inline bool Player::isEnvelopeCacheFillingRunning() const noexcept {
    std::lock_guard<std::mutex> lck(m_envelopeCacheFillingThreadIsRunningMutex);
    return m_envelopeCacheFillingThreadIsRunning;
}

inline void Player::manageCache() noexcept {
    uint8_t statisticsCounter = 0;
    float refillMultiplicator = 1.1f;
    uint32_t numberOfEntries  = 0;

    while (isEnvelopeCacheFillingRunning()) {
        try {
            std::lock_guard<std::mutex> lck(m_indexMutex);
            numberOfEntries = static_cast<uint32_t>(m_envelopeCache.size());
        } catch (...) {} // LCOV_EXCL_LINE

        // Check if refilling of the cache is needed.
        refillMultiplicator = checkRefillingCache(numberOfEntries, refillMultiplicator);
        (void)refillMultiplicator;

        // Manage cache at 10 Hz.
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(100ms);

        // Publish some statistics at 1 Hz.
        if (0 == ((++statisticsCounter) % 10)) {
            uint64_t numberOfReturnedEnvelopesInTotal = 0;
            uint32_t totalNumberOfEnvelopes           = 0;
            try {
                // m_numberOfReturnedEnvelopesInTotal is modified in a different thread.
                std::lock_guard<std::mutex> lck(m_indexMutex);
                numberOfReturnedEnvelopesInTotal = m_numberOfReturnedEnvelopesInTotal;
                totalNumberOfEnvelopes           = static_cast<uint32_t>(m_index.size());
            } catch (...) {} // LCOV_EXCL_LINE

            try {
                std::lock_guard<std::mutex> lck(m_playerListenerMutex);
                if (nullptr != m_playerListener) {
                    cluon::data::PlayerStatus ps;
                    ps.state(2); // State: "playback"
                    ps.numberOfEntries(totalNumberOfEnvelopes);
                    ps.currentEntryForPlayback(static_cast<uint32_t>(numberOfReturnedEnvelopesInTotal));
                    m_playerListener(ps);
                }
            } catch (...) {} // LCOV_EXCL_LINE

            statisticsCounter = 0;
        }
    }
}

inline float Player::checkRefillingCache(const uint32_t &numberOfEntries, float refillMultiplicator) noexcept {
    // If filling level is around 35%, pour in more from the recording.
    if (numberOfEntries < 0.35 * m_desiredInitialLevel) {
        const uint32_t entriesReadFromFile = fillEnvelopeCache(static_cast<uint32_t>(refillMultiplicator * static_cast<float>(m_desiredInitialLevel)));
        if (entriesReadFromFile > 0) {
            std::clog << "[cluon::Player]: Number of entries in cache: " << numberOfEntries << ". " << entriesReadFromFile << " added to cache. "
                      << m_envelopeCache.size() << " entries available." << std::endl;
            refillMultiplicator *= 1.25f;
        }
    }
    return refillMultiplicator;
}

} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/SharedMemory.hpp"

// clang-format off
#ifdef WIN32
    #include <limits>
#else
    #include <cstdlib>
    #include <fcntl.h>
    #include <sys/ipc.h>
    #include <sys/mman.h>
    #include <sys/sem.h>
    #include <sys/shm.h>
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif
// clang-format on

#include <cerrno>
#include <cstring>
#include <iostream>
#include <fstream>

#if !defined(__APPLE__) && !defined(__OpenBSD__) && (defined(_SEM_SEMUN_UNDEFINED) || !defined(__FreeBSD__))
union semun {
    int val;               /* for SETVAL */
    struct semid_ds *buf;  /* for IPC_STAT and IPC_SET */
    unsigned short *array; /* for GETALL and SETALL*/
};
#endif

namespace cluon {

inline SharedMemory::SharedMemory(const std::string &name, uint32_t size) noexcept
    : m_size(size) {
    if (!name.empty()) {
#ifdef WIN32
        constexpr int MAX_LENGTH_NAME{MAX_PATH};
#else
        constexpr int MAX_LENGTH_NAME{254};
#endif
        const std::string n{name.substr(0, (name.size() > MAX_LENGTH_NAME ? MAX_LENGTH_NAME : name.size()))};
        if ('/' != n[0]) {
            m_name = "/";
        }

#ifndef WIN32
#if defined(__NetBSD__) || defined(__OpenBSD__)
        std::clog << "[cluon::SharedMemory] Found NetBSD or OpenBSD; using SysV implementation." << std::endl;
        m_usePOSIX = false;
#else
        const char *CLUON_SHAREDMEMORY_POSIX = getenv("CLUON_SHAREDMEMORY_POSIX");
        m_usePOSIX                           = ((nullptr != CLUON_SHAREDMEMORY_POSIX) && (CLUON_SHAREDMEMORY_POSIX[0] == '1'));
        std::clog << "[cluon::SharedMemory] Using " << (m_usePOSIX ? "POSIX" : "SysV") << " implementation." << std::endl;
#endif
        // Define filename for timestamping.
        if (0 != n.find("/tmp")) {
            m_nameForTimeStamping = "/tmp" + m_name;

            // For NetBSD and OpenBSD or for the SysV-based implementation, we put all token files to /tmp.
            if (!m_usePOSIX) {
                m_name = m_nameForTimeStamping;
            }
        }
#endif

        // Name of the shared memory.
        {
            m_name += n;
            if (m_name.size() > MAX_LENGTH_NAME) {
                m_name = m_name.substr(0, MAX_LENGTH_NAME);
            }
        }

        // Name of the file for timestamping.
        {
            m_nameForTimeStamping += n;
            if (m_nameForTimeStamping.size() > MAX_LENGTH_NAME) {
                m_nameForTimeStamping = m_nameForTimeStamping.substr(0, MAX_LENGTH_NAME);
            }
        }

#ifdef WIN32
        initWIN32();
#else
        if (m_usePOSIX) {
            initPOSIX();
        } else {
            initSysV();
        }
#endif
    }
}

inline SharedMemory::~SharedMemory() noexcept {
#ifdef WIN32
    deinitWIN32();
#else
    if (m_usePOSIX) {
        deinitPOSIX();
    } else {
        deinitSysV();
    }
#endif
}

inline bool SharedMemory::isLocked() const noexcept {
    return m_isLocked.load();
}

inline void SharedMemory::lock() noexcept {
#ifdef WIN32
    lockWIN32();
#else
    if (m_usePOSIX) {
        lockPOSIX();
    } else {
        lockSysV();
    }
#endif
    m_isLocked.store(true);
}

inline void SharedMemory::unlock() noexcept {
#ifdef WIN32
    unlockWIN32();
#else
    if (m_usePOSIX) {
        unlockPOSIX();
    } else {
        unlockSysV();
    }
#endif
    m_isLocked.store(false);
}

inline void SharedMemory::wait() noexcept {
#ifdef WIN32
    waitWIN32();
#else
    if (m_usePOSIX) {
        waitPOSIX();
    } else {
        waitSysV();
    }
#endif
}

inline void SharedMemory::notifyAll() noexcept {
#ifdef WIN32
    notifyAllWIN32();
#else
    if (m_usePOSIX) {
        notifyAllPOSIX();
    } else {
        notifyAllSysV();
    }
#endif
}

inline bool SharedMemory::setTimeStamp(const cluon::data::TimeStamp &ts) noexcept {
    bool retVal{false};

#ifdef WIN32
    (void)ts;
#else
    if ((retVal = isLocked())) {
#ifdef __APPLE__
        struct timeval accessedTime;
        accessedTime.tv_sec = 0;
        accessedTime.tv_usec = 0;

        struct timeval modifiedTime;
        modifiedTime.tv_sec = ts.seconds();
        modifiedTime.tv_usec = ts.microseconds();

        struct timeval times[2]{accessedTime, modifiedTime};
        if (0 != futimes(m_fdForTimeStamping, times)) {
            std::cerr << "[cluon::SharedMemory] Failed to set time stamp: '" << strerror(errno) << "' (" << errno << "): " << std::endl;
            retVal = false;
        }
#else
        struct timespec accessedTime;
        accessedTime.tv_sec = 0;
        accessedTime.tv_nsec = UTIME_OMIT;

        struct timespec modifiedTime;
        modifiedTime.tv_sec = ts.seconds();
        modifiedTime.tv_nsec = ts.microseconds()*1000;

        struct timespec times[2]{accessedTime, modifiedTime};
        if (0 != futimens(m_fdForTimeStamping, times)) {
            std::cerr << "[cluon::SharedMemory] Failed to set time stamp: '" << strerror(errno) << "' (" << errno << "): " << std::endl; // LCOV_EXCL_LINE
            retVal = false; // LCOV_EXCL_LINE
        }
#endif
    }
#endif

    return retVal;
}

inline std::pair<bool, cluon::data::TimeStamp> SharedMemory::getTimeStamp() noexcept {
    bool retVal{false};
    cluon::data::TimeStamp sampleTimeStamp;

#ifndef WIN32
    if ((retVal = isLocked())) {
        struct stat fileStatus;
        auto r = fstat(m_fdForTimeStamping, &fileStatus);
        if (0 == r) {
#ifdef __APPLE__
            sampleTimeStamp.seconds(static_cast<int32_t>(fileStatus.st_mtimespec.tv_sec))
                           .microseconds(static_cast<int32_t>(fileStatus.st_mtimespec.tv_nsec/1000));
#else
            sampleTimeStamp.seconds(static_cast<int32_t>(fileStatus.st_mtim.tv_sec))
                           .microseconds(static_cast<int32_t>(fileStatus.st_mtim.tv_nsec/1000));
#endif
        }
    }
#endif

    return std::make_pair(retVal, sampleTimeStamp);
}

inline bool SharedMemory::valid() noexcept {
    bool valid{!m_broken.load()};
    valid &= (nullptr != m_sharedMemory);
    valid &= (0 < m_size);
#ifndef WIN32
    if (m_usePOSIX) {
        valid &= validPOSIX();
    } else {
        valid &= validSysV();
    }
#endif
    return valid;
}

inline char *SharedMemory::data() noexcept {
    return m_userAccessibleSharedMemory;
}

inline uint32_t SharedMemory::size() const noexcept {
    return m_size;
}

inline const std::string SharedMemory::name() const noexcept {
    return m_name;
}

////////////////////////////////////////////////////////////////////////////////
// Platform-dependent implementations.
#ifdef WIN32
inline void SharedMemory::initWIN32() noexcept {
    std::string mutexName = m_name;
    if (mutexName.size() > MAX_PATH) {
        mutexName = mutexName.substr(0, MAX_PATH - 6);
    }
    const std::string conditionEventName = mutexName + "_event";
    mutexName += "_mutex";

    if (0 < m_size) {
        // Create a shared memory area and semaphores.
        const LONG MUTEX_INITIAL_COUNT = 1;
        const LONG MUTEX_MAX_COUNT     = 1;
        const DWORD FLAGS              = 0; // Reserved.
        __mutex                        = CreateSemaphoreEx(NULL, MUTEX_INITIAL_COUNT, MUTEX_MAX_COUNT, mutexName.c_str(), FLAGS, SEMAPHORE_ALL_ACCESS);
        if (nullptr != __mutex) {
            __conditionEvent = CreateEvent(
                NULL /*use default security*/, TRUE /*manually resetting event*/, FALSE /*initial state is not signaled*/, conditionEventName.c_str());
            if (nullptr != __conditionEvent) {
                __sharedMemory = CreateFileMapping(INVALID_HANDLE_VALUE /*use paging file*/,
                                                   NULL /*use default security*/,
                                                   PAGE_READWRITE,
                                                   0,
                                                   m_size + sizeof(uint32_t) /*size + size-information (uint32_t)*/,
                                                   m_name.c_str());
                if (nullptr != __sharedMemory) {
                    m_sharedMemory = (char *)MapViewOfFile(__sharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, m_size + sizeof(uint32_t));
                    if (nullptr != m_sharedMemory) {
                        // Provide size information at the beginning of the shared memory.
                        *(uint32_t *)m_sharedMemory  = m_size;
                        m_userAccessibleSharedMemory = m_sharedMemory + sizeof(uint32_t);
                    } else {
                        std::cerr << "[cluon::SharedMemory] Failed to map shared memory '" << m_name << "': "
                                  << " (" << GetLastError() << ")" << std::endl;
                        CloseHandle(__sharedMemory);
                        __sharedMemory = nullptr;

                        CloseHandle(__conditionEvent);
                        __conditionEvent = nullptr;

                        CloseHandle(__mutex);
                        __mutex = nullptr;
                    }
                } else {
                    std::cerr << "[cluon::SharedMemory] Failed to request shared memory '" << m_name << "': "
                              << " (" << GetLastError() << ")" << std::endl;
                    CloseHandle(__conditionEvent);
                    __conditionEvent = nullptr;

                    CloseHandle(__mutex);
                    __mutex = nullptr;
                }
            } else {
                std::cerr << "[cluon::SharedMemory] Failed to request event '" << conditionEventName << "': "
                          << " (" << GetLastError() << ")" << std::endl;
                CloseHandle(__conditionEvent);
                __conditionEvent = nullptr;

                CloseHandle(__mutex);
                __mutex = nullptr;
            }
        } else {
            std::cerr << "[cluon::SharedMemory] Failed to create mutex '" << mutexName << "': "
                      << " (" << GetLastError() << ")" << std::endl;
            CloseHandle(__mutex);
            __mutex = nullptr;
        }
    } else {
        // Open a shared memory area and semaphores.
        m_hasOnlyAttachedToSharedMemory = true;
        const BOOL INHERIT_HANDLE       = FALSE;
        __mutex                         = OpenSemaphore(SEMAPHORE_ALL_ACCESS, INHERIT_HANDLE, mutexName.c_str());
        if (nullptr != __mutex) {
            __conditionEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE /*do not inherit the name*/, conditionEventName.c_str());
            if (nullptr != __conditionEvent) {
                __sharedMemory = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE /*do not inherit the name*/, m_name.c_str());
                if (nullptr != __sharedMemory) {
                    // Firstly, map only for the size of a uint32_t to read the entire size.
                    m_sharedMemory = (char *)MapViewOfFile(__sharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(uint32_t));
                    if (nullptr != m_sharedMemory) {
                        //  Now, read the real size...
                        m_size = *(uint32_t *)m_sharedMemory;
                        // ..unmap and re-map.
                        UnmapViewOfFile(m_sharedMemory);
                        m_sharedMemory = (char *)MapViewOfFile(__sharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, m_size + sizeof(uint32_t));
                        if (nullptr != m_sharedMemory) {
                            m_userAccessibleSharedMemory = m_sharedMemory + sizeof(uint32_t);
                        } else {
                            std::cerr << "[cluon::SharedMemory] Failed to finally map shared memory '" << m_name << "': "
                                      << " (" << GetLastError() << ")" << std::endl;
                            CloseHandle(__sharedMemory);
                            __sharedMemory = nullptr;

                            CloseHandle(__conditionEvent);
                            __conditionEvent = nullptr;

                            CloseHandle(__mutex);
                            __mutex = nullptr;
                        }
                    } else {
                        std::cerr << "[cluon::SharedMemory] Failed to temporarily map shared memory '" << m_name << "': "
                                  << " (" << GetLastError() << ")" << std::endl;
                        CloseHandle(__sharedMemory);
                        __sharedMemory = nullptr;

                        CloseHandle(__conditionEvent);
                        __conditionEvent = nullptr;

                        CloseHandle(__mutex);
                        __mutex = nullptr;
                    }
                } else {
                    std::cerr << "[cluon::SharedMemory] Failed to open shared memory '" << m_name << "': "
                              << " (" << GetLastError() << ")" << std::endl;
                    CloseHandle(__conditionEvent);
                    __conditionEvent = nullptr;

                    CloseHandle(__mutex);
                    __mutex = nullptr;
                }
            } else {
                std::cerr << "[cluon::SharedMemory] Failed to open event '" << conditionEventName << "': "
                          << " (" << GetLastError() << ")" << std::endl;
                CloseHandle(__conditionEvent);
                __conditionEvent = nullptr;

                CloseHandle(__mutex);
                __mutex = nullptr;
            }
        } else {
            std::cerr << "[cluon::SharedMemory] Failed to open mutex '" << mutexName << "': "
                      << " (" << GetLastError() << ")" << std::endl;
            CloseHandle(__mutex);
            __mutex = nullptr;
        }
    }
}

inline void SharedMemory::deinitWIN32() noexcept {
    if (nullptr != __conditionEvent) {
        SetEvent(__conditionEvent);
        CloseHandle(__conditionEvent);
    }
    if (nullptr != __mutex) {
        unlock();
        CloseHandle(__mutex);
    }
    if (nullptr != m_sharedMemory) {
        UnmapViewOfFile(m_sharedMemory);
    }
    if (nullptr != __sharedMemory) {
        CloseHandle(__sharedMemory);
    }
}

inline void SharedMemory::lockWIN32() noexcept {
    if (nullptr != __mutex) {
        if (0 != WaitForSingleObject(__mutex, INFINITE)) {
            m_broken.store(true);
        }
    }
}

inline void SharedMemory::unlockWIN32() noexcept {
    if (nullptr != __mutex) {
        const LONG RELEASE_COUNT = 1;
        if (/* Testing for equality with 0 is correct according to MSDN reference. */ 0 == ReleaseSemaphore(__mutex, RELEASE_COUNT, 0)) {
            m_broken.store(true);
        }
    }
}

inline void SharedMemory::waitWIN32() noexcept {
    if (nullptr != __conditionEvent) {
        if (0 != WaitForSingleObject(__conditionEvent, INFINITE)) {
            m_broken.store(true);
        }
    }
}

inline void SharedMemory::notifyAllWIN32() noexcept {
    if (nullptr != __conditionEvent) {
        if (/* Testing for equality with 0 is correct according to MSDN reference. */ 0 == SetEvent(__conditionEvent)) {
            m_broken.store(true);
        }
        if (/* Testing for equality with 0 is correct according to MSDN reference. */ 0 == ResetEvent(__conditionEvent)) {
            m_broken.store(true);
        }
    }
}

#else /* POSIX and SysV */

inline void SharedMemory::initPOSIX() noexcept {
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    // If size is greater than 0, the caller wants to create a new shared
    // memory area. Otherwise, the caller wants to open an existing shared memory.
    int flags = O_RDWR;
    if (0 < m_size) {
        flags |= O_CREAT | O_EXCL;
    }

    m_fd = ::shm_open(m_name.c_str(), flags, S_IRUSR | S_IWUSR);
    if (-1 == m_fd) {
// clang-format off
        std::cerr << "[cluon::SharedMemory (POSIX)] Failed to open shared memory '" << m_name << "': " << ::strerror(errno) << " (" << errno << ")" << std::endl;
// clang-format on
        // Try to remove existing shared memory segment and try again.
        if ((flags & O_CREAT) == O_CREAT) {
            std::clog << "[cluon::SharedMemory (POSIX)] Trying to remove existing shared memory '" << m_name << "' and trying again... ";
            if (0 == ::shm_unlink(m_name.c_str())) {
                m_fd = ::shm_open(m_name.c_str(), flags, S_IRUSR | S_IWUSR);
            }

            if (-1 == m_fd) {
                std::cerr << "failed: " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
            } else {
                std::cerr << "succeeded." << std::endl;
            }
        }
    }

    if (-1 != m_fd) {
        bool retVal{true};

        // When creating a shared memory segment, truncate it.
        if (0 < m_size) {
            retVal = (0 == ::ftruncate(m_fd, static_cast<off_t>(sizeof(SharedMemoryHeader) + m_size)));
            if (!retVal) {
// clang-format off // LCOV_EXCL_LINE
                std::cerr << "[cluon::SharedMemory (POSIX)] Failed to truncate '" << m_name << "': " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
            }
        }

        // Accessing shared memory segment.
        if (retVal) {
            // On opening (i.e., NOT creating) a shared memory segment, m_size is still 0 and we need to figure out the size first.
            m_sharedMemory = static_cast<char *>(::mmap(0, sizeof(SharedMemoryHeader) + m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0));
            if (MAP_FAILED != m_sharedMemory) {
                m_sharedMemoryHeader = reinterpret_cast<SharedMemoryHeader *>(m_sharedMemory);

                // On creating (i.e., NOT opening) a shared memory segment, setup the shared memory header.
                if (0 < m_size) {
                    // Store user accessible size in shared memory.
                    m_sharedMemoryHeader->__size = m_size;

                    // Create process-shared mutex (fastest approach, cf. Stevens & Rago: "Advanced Programming in the UNIX (R) Environment").
                    pthread_mutexattr_t mutexAttribute;
                    ::pthread_mutexattr_init(&mutexAttribute);
                    ::pthread_mutexattr_setpshared(&mutexAttribute, PTHREAD_PROCESS_SHARED); // Share between unrelated processes.
#ifndef __APPLE__
                    ::pthread_mutexattr_setrobust(&mutexAttribute, PTHREAD_MUTEX_ROBUST);    // Allow continuation of other processes waiting for this mutex
                                                                                             // when the currently holding process unexpectedly terminates.
#endif
                    ::pthread_mutexattr_settype(&mutexAttribute, PTHREAD_MUTEX_NORMAL);      // Using regular mutex with deadlock behavior.
                    ::pthread_mutex_init(&(m_sharedMemoryHeader->__mutex), &mutexAttribute);
                    ::pthread_mutexattr_destroy(&mutexAttribute);

                    // Create shared condition.
                    pthread_condattr_t conditionAttribute;
                    ::pthread_condattr_init(&conditionAttribute);
#ifndef __APPLE__
                    ::pthread_condattr_setclock(&conditionAttribute, CLOCK_MONOTONIC);          // Use realtime clock for timed waits with non-negative jumps.
#endif
                    ::pthread_condattr_setpshared(&conditionAttribute, PTHREAD_PROCESS_SHARED); // Share between unrelated processes.
                    ::pthread_cond_init(&(m_sharedMemoryHeader->__condition), &conditionAttribute);
                    ::pthread_condattr_destroy(&conditionAttribute);
                } else {
                    // Indicate that this instance is attaching to an existing shared memory segment.
                    m_hasOnlyAttachedToSharedMemory = true;

                    // Read size as we are attaching to an existing shared memory.
                    m_size = m_sharedMemoryHeader->__size;

                    // Now, as we know the real size, unmap the first mapping that did not know the size.
                    if (::munmap(m_sharedMemory, sizeof(SharedMemoryHeader))) {
// clang-format off // LCOV_EXCL_LINE
                        std::cerr << "[cluon::SharedMemory (POSIX)] Failed to unmap shared memory: " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                    }

                    // Invalidate all pointers.
                    m_sharedMemory = nullptr;
                    m_sharedMemoryHeader = nullptr;

                    // Re-map with the correct size parameter.
                    m_sharedMemory = static_cast<char *>(::mmap(0, sizeof(SharedMemoryHeader) + m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0));
                    if (MAP_FAILED != m_sharedMemory) {
                        m_sharedMemoryHeader = reinterpret_cast<SharedMemoryHeader *>(m_sharedMemory);
                    }
                }
            } else { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                std::cerr << "[cluon::SharedMemory (POSIX)] Failed to map '" << m_name << "': " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
            }

            // If the shared memory segment is correctly available, store the pointer for the user data.
            if (MAP_FAILED != m_sharedMemory) {
                m_userAccessibleSharedMemory = m_sharedMemory + sizeof(SharedMemoryHeader);

                // Lock the shared memory into RAM for performance reasons.
                if (-1 == ::mlock(m_sharedMemory, sizeof(SharedMemoryHeader) + m_size)) {
                    std::cerr << "[cluon::SharedMemory (POSIX)] Failed to mlock shared memory: " // LCOV_EXCL_LINE
                              << ::strerror(errno) << " (" << errno << ")" << std::endl;         // LCOV_EXCL_LINE
                }
            }
        } else { // LCOV_EXCL_LINE
            if (-1 != m_fd) { // LCOV_EXCL_LINE
                if (-1 == ::shm_unlink(m_name.c_str())) { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                    std::cerr << "[cluon::SharedMemory (POSIX)] Failed to unlink shared memory: " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                }
            }
            m_fd = -1; // LCOV_EXCL_LINE
        }
    }
#endif

#ifdef __linux__
    // On Linux, the POSIX shared memory lives in /dev/shm and we have a valid
    // file descriptor to use for timestamping.
    if (-1 != m_fd) {
        m_fdForTimeStamping = m_fd;
    }
#else
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    // On *BSDs, the POSIX shared memory lives not in /dev/shm and we have
    // need to use a separate file for timestamping.
    if (-1 != m_fd) {
        m_fdForTimeStamping = ::open(m_nameForTimeStamping.c_str(), O_CREAT|O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    }
#endif
#endif
}

inline void SharedMemory::deinitPOSIX() noexcept {
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    if ((nullptr != m_sharedMemoryHeader) && (!m_hasOnlyAttachedToSharedMemory)) {
        // Wake any waiting threads as we are going to end the shared memory session.
        ::pthread_cond_broadcast(&(m_sharedMemoryHeader->__condition));
        ::pthread_cond_destroy(&(m_sharedMemoryHeader->__condition));
        ::pthread_mutex_destroy(&(m_sharedMemoryHeader->__mutex));
    }
    if ((nullptr != m_sharedMemory) && ::munmap(m_sharedMemory, sizeof(SharedMemoryHeader) + m_size)) {
// clang-format off // LCOV_EXCL_LINE
        std::cerr << "[cluon::SharedMemory (POSIX)] Failed to unmap shared memory: " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
    }
    if (!m_hasOnlyAttachedToSharedMemory && (-1 != m_fd) && (-1 == ::shm_unlink(m_name.c_str()) && (ENOENT != errno))) {
// clang-format off // LCOV_EXCL_LINE
        std::cerr << "[cluon::SharedMemory (POSIX)] Failed to unlink shared memory: " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
    }
#endif

#ifndef __linux__
    // On *BSDs, the POSIX shared memory lives not in /dev/shm and we have
    // used a separate file for timestamping.
    if (-1 != m_fdForTimeStamping) {
        ::close(m_fdForTimeStamping);
        ::unlink(m_nameForTimeStamping.c_str());
    }
#endif
}

inline void SharedMemory::lockPOSIX() noexcept {
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    if (nullptr != m_sharedMemoryHeader) {
        auto retVal = ::pthread_mutex_lock(&(m_sharedMemoryHeader->__mutex));
        if (EOWNERDEAD == retVal) {
// clang-format off // LCOV_EXCL_LINE
            std::cerr << "[cluon::SharedMemory (POSIX)] pthread_mutex_lock returned for EOWNERDEAD for mutex in shared memory '" << m_name << "': " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
        } else if (0 != retVal) {
            m_broken.store(true); // LCOV_EXCL_LINE
        }
    }
#endif
}

inline void SharedMemory::unlockPOSIX() noexcept {
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    if (nullptr != m_sharedMemoryHeader) {
        if (0 != ::pthread_mutex_unlock(&(m_sharedMemoryHeader->__mutex))) {
            m_broken.store(true); // LCOV_EXCL_LINE
        }
    }
#endif
}

inline void SharedMemory::waitPOSIX() noexcept {
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    if (nullptr != m_sharedMemoryHeader) {
        lock();
        if (0 != ::pthread_cond_wait(&(m_sharedMemoryHeader->__condition), &(m_sharedMemoryHeader->__mutex))) {
            m_broken.store(true); // LCOV_EXCL_LINE
        }
        unlock();
    }
#endif
}

inline void SharedMemory::notifyAllPOSIX() noexcept {
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    if (nullptr != m_sharedMemoryHeader) {
        if (0 != ::pthread_cond_broadcast(&(m_sharedMemoryHeader->__condition))) {
            m_broken.store(true); // LCOV_EXCL_LINE
        }
    }
#endif
}

inline bool SharedMemory::validPOSIX() noexcept {
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
    return (-1 != m_fd) && (MAP_FAILED != m_sharedMemory);
#else
    return false;
#endif
}

////////////////////////////////////////////////////////////////////////////////

inline void SharedMemory::initSysV() noexcept {
    // If size is greater than 0, the caller wants to create a new shared
    // memory area. Otherwise, the caller wants to open an existing shared memory.

    // Create a key to identify the shared memory area.
    constexpr int32_t ID_SHM = 1;
    constexpr int32_t ID_SEM_AS_MUTEX = 2;
    constexpr int32_t ID_SEM_AS_CONDITION = 3;
    bool tokenFileExisting{false};

    if (0 < m_size) {
        // The file should not exist; otherwise, we need to clear an existing
        // set of semaphores and shared memory areas.
        std::fstream tokenFile(m_name.c_str(), std::ios::in);
        if (tokenFile.good()) {
            // Existing tokenFile found. Cleaning up will be tried in the code below.
        }
        tokenFile.close();

        tokenFile.open(m_name.c_str(), std::ios::out);
        tokenFileExisting = tokenFile.good();
        if (!tokenFileExisting) {
            std::cerr << "[cluon::SharedMemory (SysV)] Token file '" << m_name << "' could not be created; shared memory cannot be created." << std::endl;
        }
        tokenFile.close();
    } else {
        // Open an existing shared memory area indicated by an existing token file.
        m_hasOnlyAttachedToSharedMemory = true;

        std::fstream tokenFile(m_name.c_str(), std::ios::in);
        tokenFileExisting = tokenFile.good();
        if (!tokenFileExisting) {
            std::cerr << "[cluon::SharedMemory (SysV)] Token file '" << m_name << "' not found; shared memory cannot be created." << std::endl;
        }
        tokenFile.close();
    }

    // We have a token file to be used for the keys.
    if (tokenFileExisting) {
        m_shmKeySysV = ::ftok(m_name.c_str(), ID_SHM);
        if (-1 == m_shmKeySysV) {
// clang-format off // LCOV_EXCL_LINE
            std::cerr << "[cluon::SharedMemory (SysV)] Key for shared memory could not be created: " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
        } else {
            if (!m_hasOnlyAttachedToSharedMemory) {
                // The caller wants to create a shared memory segment.

                // First, try to clean up an orphaned shared memory segment.
                // Therefore, we try to open the shared memory area without the
                // IPC_CREAT flag. On a clean environment, this call must fail
                // as there should not be any shared memory segments left.
                {
                    int orphanedSharedMemoryIDSysV = ::shmget(m_shmKeySysV, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                    if (!(orphanedSharedMemoryIDSysV < 0)) {
                        if (::shmctl(orphanedSharedMemoryIDSysV, IPC_RMID, 0)) {
// clang-format off // LCOV_EXCL_LINE
                            std::cerr << "[cluon::SharedMemory (SysV)] Existing shared memory (0x" << std::hex << m_shmKeySysV << std::dec << ") found; removing failed." << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                        }
                    }
                }

                // Now, create the shared memory segment.
                m_sharedMemoryIDSysV = ::shmget(m_shmKeySysV, m_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                if (-1 != m_sharedMemoryIDSysV) {
                    m_sharedMemory = reinterpret_cast<char *>(::shmat(m_sharedMemoryIDSysV, nullptr, 0));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
                    if ((void *)-1 != m_sharedMemory) {
                        m_userAccessibleSharedMemory = m_sharedMemory;
                    } else { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                        std::cerr << "[cluon::SharedMemory (SysV)] Failed to attach to shared memory (0x" << std::hex << m_shmKeySysV << std::dec << "): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                    }
#pragma GCC diagnostic pop
                } else { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                    std::cerr << "[cluon::SharedMemory (SysV)] Failed to get to shared memory (0x" << std::hex << m_shmKeySysV << std::dec << "): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                }
            } else {
                // The caller wants to attach to an existing shared memory segment.
                m_sharedMemoryIDSysV = ::shmget(m_shmKeySysV, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                if (-1 != m_sharedMemoryIDSysV) {
                    struct shmid_ds info;
                    if (-1 != ::shmctl(m_sharedMemoryIDSysV, IPC_STAT, &info)) {
                        m_size = static_cast<uint32_t>(info.shm_segsz);
                        m_sharedMemory = reinterpret_cast<char *>(::shmat(m_sharedMemoryIDSysV, nullptr, 0));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
                        if ((void *)-1 != m_sharedMemory) {
                            m_userAccessibleSharedMemory = m_sharedMemory;
                        } else { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                            std::cerr << "[cluon::SharedMemory (SysV)] Failed to attach to shared memory (0x" << std::hex << m_shmKeySysV << std::dec << "): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                        }
#pragma GCC diagnostic pop
                    } else { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                        std::cerr << "[cluon::SharedMemory (SysV)] Could not read information about shared memory (0x" << std::hex << m_shmKeySysV << std::dec << "): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                    }
                } else { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                    std::cerr << "[cluon::SharedMemory (SysV)] Failed to get shared memory (0x" << std::hex << m_shmKeySysV << std::dec << "): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                }
            }
        }

        // Next, create the mutex (but only if the shared memory was acquired correctly.
        m_mutexKeySysV = ::ftok(m_name.c_str(), ID_SEM_AS_MUTEX);
        if (-1 == m_mutexKeySysV) {
// clang-format off // LCOV_EXCL_LINE
            std::cerr << "[cluon::SharedMemory (SysV)] Key for mutex could not be created: " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
        }
        if ((-1 != m_shmKeySysV) && (-1 != m_mutexKeySysV) && (nullptr != m_userAccessibleSharedMemory)) {
            if (!m_hasOnlyAttachedToSharedMemory) {
                // The caller has created the shared memory segment and thus,
                // we need the corresponding mutex.

                // First, try to remove the orphaned one.
                {
                    int orphanedMutexIDSysV = ::semget(m_mutexKeySysV, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                    if (!(orphanedMutexIDSysV < 0)) {
                        if (::semctl(orphanedMutexIDSysV, 0, IPC_RMID)) {
// clang-format off // LCOV_EXCL_LINE
                            std::cerr << "[cluon::SharedMemory (SysV)] Existing semaphore (0x" << std::hex << m_mutexKeySysV << std::dec << ", intended to use as mutex) found; removing failed." << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                        }
                    }
                }

                // Next, create the correct semaphore used as mutex.
                {
                    constexpr int NSEMS{1};
                    m_mutexIDSysV = ::semget(m_mutexKeySysV, NSEMS, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                    if (-1 != m_mutexIDSysV) {
                        constexpr int NUMBER_OF_SEMAPHORE_TO_CONTROL{0};
                        constexpr int INITIAL_VALUE{1};
                        union semun tmp;
                        tmp.val = INITIAL_VALUE;
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wclass-varargs"
#endif
                        if (-1 == ::semctl(m_mutexIDSysV, NUMBER_OF_SEMAPHORE_TO_CONTROL, SETVAL, tmp)) {
// clang-format off // LCOV_EXCL_LINE
                            std::cerr << "[cluon::SharedMemory (SysV)] Failed to initialize semaphore (0x" << std::hex << m_mutexKeySysV << std::dec << ", intended to use as mutex): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                        }
#pragma GCC diagnostic pop
                    } else { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                        std::cerr << "[cluon::SharedMemory (SysV)] Failed to create semaphore (0x" << std::hex << m_mutexKeySysV << std::dec << ", intended to use as mutex): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                    }
                }
            } else {
                m_mutexIDSysV = ::semget(m_mutexKeySysV, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                if (-1 == m_mutexIDSysV) {
// clang-format off // LCOV_EXCL_LINE
                    std::cerr << "[cluon::SharedMemory (SysV)] Failed to get semaphore (0x" << std::hex << m_mutexKeySysV << std::dec << ", intended to use as mutex): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                }
            }
        }

        // Next, create the condition variable (but only if the shared memory was acquired correctly.
        m_conditionKeySysV = ::ftok(m_name.c_str(), ID_SEM_AS_CONDITION);
        if (-1 == m_conditionKeySysV) {
// clang-format off // LCOV_EXCL_LINE
            std::cerr << "[cluon::SharedMemory (SysV)] Key for condition could not be created: " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
        }
        if ((-1 != m_shmKeySysV) && (-1 != m_mutexKeySysV) && (-1 != m_conditionKeySysV) && (nullptr != m_userAccessibleSharedMemory)) {
            if (!m_hasOnlyAttachedToSharedMemory) {
                // The caller has created the shared memory segment and thus,
                // we need the corresponding condition variable.

                // First, try to remove the orphaned one.
                {
                    int orphanedConditionIDSysV = ::semget(m_conditionKeySysV, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                    if (!(orphanedConditionIDSysV < 0)) {
                        if (::semctl(orphanedConditionIDSysV, 0, IPC_RMID)) {
// clang-format off // LCOV_EXCL_LINE
                            std::cerr << "[cluon::SharedMemory (SysV)] Existing semaphore (0x" << std::hex << m_conditionKeySysV << std::dec << ", intended to use as condition variable) found; removing failed." << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                        }
                    }
                }

                // Next, create the correct semaphore used as condition variable.
                {
                    constexpr int NSEMS{1};
                    m_conditionIDSysV = ::semget(m_conditionKeySysV, NSEMS, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                    if (-1 != m_conditionIDSysV) {
                        constexpr int NUMBER_OF_SEMAPHORE_TO_CONTROL{0};
                        constexpr int INITIAL_VALUE{1};
                        union semun tmp;
                        tmp.val = INITIAL_VALUE;
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wclass-varargs"
#endif
                        if (-1 == ::semctl(m_conditionIDSysV, NUMBER_OF_SEMAPHORE_TO_CONTROL, SETVAL, tmp)) {
// clang-format off // LCOV_EXCL_LINE
                            std::cerr << "[cluon::SharedMemory (SysV)] Failed to initialize semaphore (0x" << std::hex << m_conditionKeySysV << std::dec << ", intended to use as condition variable): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                        }
#pragma GCC diagnostic pop
                    } else { // LCOV_EXCL_LINE
// clang-format off // LCOV_EXCL_LINE
                        std::cerr << "[cluon::SharedMemory (SysV)] Failed to create semaphore (0x" << std::hex << m_conditionKeySysV << std::dec << ", intended to use as condition variable): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                    }
                }
            } else {
                m_conditionIDSysV = ::semget(m_conditionKeySysV, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                if (-1 == m_conditionIDSysV) {
// clang-format off // LCOV_EXCL_LINE
                    std::cerr << "[cluon::SharedMemory (SysV)] Failed to get semaphore (0x" << std::hex << m_conditionKeySysV << std::dec << ", intended to use as condition variable): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
                }
            }
        }
    }

    // If the shared memory is present, open the token file for the time stamping.
    if (nullptr != m_sharedMemory) {
        m_fdForTimeStamping = ::open(m_name.c_str(), O_RDONLY);
    }
}

inline void SharedMemory::deinitSysV() noexcept {
    if (nullptr != m_sharedMemory) {
        // Close token file.
        ::close(m_fdForTimeStamping);
        m_fdForTimeStamping = -1;

        if (-1 == ::shmdt(m_sharedMemory)) {
// clang-format off // LCOV_EXCL_LINE
            std::cerr << "[cluon::SharedMemory (SysV)] Could not detach shared memory (0x" << std::hex << m_shmKeySysV << std::dec << "): " << ::strerror(errno) << " (" << errno << ")" << std::endl; // LCOV_EXCL_LINE
// clang-format on // LCOV_EXCL_LINE
        }
    }

    if (!m_hasOnlyAttachedToSharedMemory) {
        notifyAllSysV();

        if (-1 != m_conditionIDSysV) {
            if (-1 == ::semctl(m_conditionIDSysV, 0, IPC_RMID)) {
                std::cerr << "[cluon::SharedMemory (SysV)] Semaphore (0x" << std::hex << m_conditionKeySysV << std::dec
                          << ") used as condition could not be removed: " << ::strerror(errno) << " (" << errno << ")" << std::endl;
            }
        }

        if (-1 != m_mutexIDSysV) {
            if (-1 == ::semctl(m_mutexIDSysV, 0, IPC_RMID)) {
                std::cerr << "[cluon::SharedMemory (SysV)] Semaphore (0x" << std::hex << m_mutexKeySysV << std::dec
                          << ") used as mutex could not be removed: " << ::strerror(errno) << " (" << errno << ")" << std::endl;
            }
        }
        if (-1 != m_sharedMemoryIDSysV) {
            if (-1 == ::shmctl(m_sharedMemoryIDSysV, IPC_RMID, 0)) {
                std::cerr << "[cluon::SharedMemory (SysV)] Shared memory (0x" << std::hex << m_shmKeySysV << std::dec
                          << ") could not be removed: " << ::strerror(errno) << " (" << errno << ")" << std::endl;
            }
        }

        if (-1 == ::unlink(m_name.c_str())) {
            std::cerr << "[cluon::SharedMemory (SysV)] Token file '" << m_name << "' could not be removed: " << ::strerror(errno) << " (" << errno << ")"
                      << std::endl;
        }
    }
}

inline void SharedMemory::lockSysV() noexcept {
    if (-1 != m_mutexIDSysV) {
        constexpr int NUMBER_OF_SEMAPHORE_TO_CONTROL{0};
        constexpr int VALUE{-1};

        struct sembuf tmp;
        tmp.sem_num = NUMBER_OF_SEMAPHORE_TO_CONTROL;
        tmp.sem_op = VALUE;
        tmp.sem_flg = SEM_UNDO; // When the caller terminates unexpectedly, let the kernel restore the original value.
        if (-1 == ::semop(m_mutexIDSysV, &tmp, 1)) {
            std::cerr << "[cluon::SharedMemory (SysV)] Failed to lock semaphore (0x" << std::hex << m_mutexKeySysV << std::dec << "): " << ::strerror(errno)
                      << " (" << errno << ")" << std::endl;
            m_broken.store(true);
        }
    }
}

inline void SharedMemory::unlockSysV() noexcept {
    if (-1 != m_mutexIDSysV) {
        constexpr int NUMBER_OF_SEMAPHORE_TO_CONTROL{0};
        constexpr int VALUE{+1};

        struct sembuf tmp;
        tmp.sem_num = NUMBER_OF_SEMAPHORE_TO_CONTROL;
        tmp.sem_op = VALUE;
        tmp.sem_flg = SEM_UNDO; // When the caller terminates unexpectedly, let the kernel restore the original value.
        if (-1 == ::semop(m_mutexIDSysV, &tmp, 1)) {
            std::cerr << "[cluon::SharedMemory (SysV)] Failed to unlock semaphore (0x" << std::hex << m_mutexKeySysV << std::dec << "): " << ::strerror(errno)
                      << " (" << errno << ")" << std::endl;
            m_broken.store(true);
        }
    }
}

inline void SharedMemory::waitSysV() noexcept {
    if (-1 != m_conditionIDSysV) {
        constexpr int NUMBER_OF_SEMAPHORE_TO_CONTROL{0};
        constexpr int VALUE{0}; // Wait for this semaphore to become 0.

        struct sembuf tmp;
        tmp.sem_num = NUMBER_OF_SEMAPHORE_TO_CONTROL;
        tmp.sem_op = VALUE;
        tmp.sem_flg = 0;
        if (-1 == ::semop(m_conditionIDSysV, &tmp, 1)) {
            std::cerr << "[cluon::SharedMemory (SysV)] Failed to wait on semaphore (0x" << std::hex << m_conditionKeySysV << std::dec
                      << "): " << ::strerror(errno) << " (" << errno << ")" << std::endl;
            m_broken.store(true);
        }
    }
}

inline void SharedMemory::notifyAllSysV() noexcept {
    if (-1 != m_conditionIDSysV) {
        {
            constexpr int NUMBER_OF_SEMAPHORE_TO_CONTROL{0};
            constexpr int WAKEUP_VALUE{0};

            union semun tmp;
            tmp.val = WAKEUP_VALUE;
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wclass-varargs"
#endif
            if (-1 == ::semctl(m_conditionIDSysV, NUMBER_OF_SEMAPHORE_TO_CONTROL, SETVAL, tmp)) {
                std::cerr << "[cluon::SharedMemory (SysV)] Failed to notify semaphore (0x" << std::hex << m_conditionKeySysV << std::dec
                          << ", intended to use as condition variable): " << ::strerror(errno) << " (" << errno << ")" << std::endl;
                m_broken.store(true);
            }
#pragma GCC diagnostic pop
        }
        {
            constexpr int NUMBER_OF_SEMAPHORE_TO_CONTROL{0};
            constexpr int SLEEPING_VALUE{1};

            union semun tmp;
            tmp.val = SLEEPING_VALUE;
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wclass-varargs"
#endif
            if (-1 == ::semctl(m_conditionIDSysV, NUMBER_OF_SEMAPHORE_TO_CONTROL, SETVAL, tmp)) {
                std::cerr << "[cluon::SharedMemory (SysV)] Failed to reset semaphore for notification (0x" << std::hex << m_conditionKeySysV << std::dec
                          << ", intended to use as condition variable): " << ::strerror(errno) << " (" << errno << ")" << std::endl;
                m_broken.store(true);
            }
#pragma GCC diagnostic pop
        }
    }
}

inline bool SharedMemory::validSysV() noexcept {
    return (-1 != m_sharedMemoryIDSysV) && (nullptr != m_sharedMemory) && (0 < m_size) && (-1 != m_mutexIDSysV) && (-1 != m_conditionIDSysV);
}
#endif

} // namespace cluon
#endif
#ifdef HAVE_CLUON_MSC
/*
 * Boost Software License - Version 1.0
 *
 * Mustache v4.1
 * Copyright 2015-2020 Kevin Wojniak
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef KAINJOW_MUSTACHE_HPP
#define KAINJOW_MUSTACHE_HPP

#include <cassert>
#include <cctype>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace kainjow {
namespace mustache {

template <typename string_type>
string_type trim(const string_type& s) {
    auto it = s.begin();
    while (it != s.end() && std::isspace(*it)) {
        it++;
    }
    auto rit = s.rbegin();
    while (rit.base() != it && std::isspace(*rit)) {
        rit++;
    }
    return {it, rit.base()};
}

template <typename string_type>
string_type html_escape(const string_type& s) {
    string_type ret;
    ret.reserve(s.size()*2);
    for (const auto ch : s) {
        switch (ch) {
            case '&':
                ret.append({'&','a','m','p',';'});
                break;
            case '<':
                ret.append({'&','l','t',';'});
                break;
            case '>':
                ret.append({'&','g','t',';'});
                break;
            case '\"':
                ret.append({'&','q','u','o','t',';'});
                break;
            case '\'':
                ret.append({'&','a','p','o','s',';'});
                break;
            default:
                ret.append(1, ch);
                break;
        }
    }
    return ret;
}

template <typename string_type>
std::vector<string_type> split(const string_type& s, typename string_type::value_type delim) {
    std::vector<string_type> elems;
    std::basic_stringstream<typename string_type::value_type> ss(s);
    string_type item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

template <typename string_type>
class basic_renderer {
public:
    using type1 = std::function<string_type(const string_type&)>;
    using type2 = std::function<string_type(const string_type&, bool escaped)>;

    string_type operator()(const string_type& text) const {
        return type1_(text);
    }

    string_type operator()(const string_type& text, bool escaped) const {
        return type2_(text, escaped);
    }

private:
    basic_renderer(const type1& t1, const type2& t2)
        : type1_(t1)
        , type2_(t2)
    {}

    const type1& type1_;
    const type2& type2_;

    template <typename StringType>
    friend class basic_mustache;
};

template <typename string_type>
class basic_lambda_t {
public:
    using type1 = std::function<string_type(const string_type&)>;
    using type2 = std::function<string_type(const string_type&, const basic_renderer<string_type>& render)>;

    basic_lambda_t(const type1& t) : type1_(new type1(t)) {}
    basic_lambda_t(const type2& t) : type2_(new type2(t)) {}

    bool is_type1() const { return static_cast<bool>(type1_); }
    bool is_type2() const { return static_cast<bool>(type2_); }

    const type1& type1_value() const { return *type1_; }
    const type2& type2_value() const { return *type2_; }

    // Copying
    basic_lambda_t(const basic_lambda_t& l) {
        if (l.type1_) {
            type1_.reset(new type1(*l.type1_));
        } else if (l.type2_) {
            type2_.reset(new type2(*l.type2_));
        }
    }

    string_type operator()(const string_type& text) const {
        return (*type1_)(text);
    }

    string_type operator()(const string_type& text, const basic_renderer<string_type>& render) const {
        return (*type2_)(text, render);
    }

private:
    std::unique_ptr<type1> type1_;
    std::unique_ptr<type2> type2_;
};

template <typename string_type>
class basic_data;
template <typename string_type>
using basic_object = std::unordered_map<string_type, basic_data<string_type>>;
template <typename string_type>
using basic_list = std::vector<basic_data<string_type>>;
template <typename string_type>
using basic_partial = std::function<string_type()>;
template <typename string_type>
using basic_lambda = typename basic_lambda_t<string_type>::type1;
template <typename string_type>
using basic_lambda2 = typename basic_lambda_t<string_type>::type2;

template <typename string_type>
class basic_data {
public:
    enum class type {
        object,
        string,
        list,
        bool_true,
        bool_false,
        partial,
        lambda,
        lambda2,
        invalid,
    };

    // Construction
    basic_data() : basic_data(type::object) {
    }
    basic_data(const string_type& string) : type_{type::string} {
        str_.reset(new string_type(string));
    }
    basic_data(const typename string_type::value_type* string) : type_{type::string} {
        str_.reset(new string_type(string));
    }
    basic_data(const basic_object<string_type>& obj) : type_{type::object} {
        obj_.reset(new basic_object<string_type>(obj));
    }
    basic_data(const basic_list<string_type>& l) : type_{type::list} {
        list_.reset(new basic_list<string_type>(l));
    }
    basic_data(type t) : type_{t} {
        switch (type_) {
            case type::object:
                obj_.reset(new basic_object<string_type>);
                break;
            case type::string:
                str_.reset(new string_type);
                break;
            case type::list:
                list_.reset(new basic_list<string_type>);
                break;
            default:
                break;
        }
    }
    basic_data(const string_type& name, const basic_data& var) : basic_data{} {
        set(name, var);
    }
    basic_data(const basic_partial<string_type>& p) : type_{type::partial} {
        partial_.reset(new basic_partial<string_type>(p));
    }
    basic_data(const basic_lambda<string_type>& l) : type_{type::lambda} {
        lambda_.reset(new basic_lambda_t<string_type>(l));
    }
    basic_data(const basic_lambda2<string_type>& l) : type_{type::lambda2} {
        lambda_.reset(new basic_lambda_t<string_type>(l));
    }
    basic_data(const basic_lambda_t<string_type>& l) {
        if (l.is_type1()) {
            type_ = type::lambda;
        } else if (l.is_type2()) {
            type_ = type::lambda2;
        }
        lambda_.reset(new basic_lambda_t<string_type>(l));
    }
    basic_data(bool b) : type_{b ? type::bool_true : type::bool_false} {
    }

    // Copying
    basic_data(const basic_data& dat) : type_(dat.type_) {
        if (dat.obj_) {
            obj_.reset(new basic_object<string_type>(*dat.obj_));
        } else if (dat.str_) {
            str_.reset(new string_type(*dat.str_));
        } else if (dat.list_) {
            list_.reset(new basic_list<string_type>(*dat.list_));
        } else if (dat.partial_) {
            partial_.reset(new basic_partial<string_type>(*dat.partial_));
        } else if (dat.lambda_) {
            lambda_.reset(new basic_lambda_t<string_type>(*dat.lambda_));
        }
    }

    // Move
    basic_data(basic_data&& dat) : type_{dat.type_} {
        if (dat.obj_) {
            obj_ = std::move(dat.obj_);
        } else if (dat.str_) {
            str_ = std::move(dat.str_);
        } else if (dat.list_) {
            list_ = std::move(dat.list_);
        } else if (dat.partial_) {
            partial_ = std::move(dat.partial_);
        } else if (dat.lambda_) {
            lambda_ = std::move(dat.lambda_);
        }
        dat.type_ = type::invalid;
    }
    basic_data& operator= (basic_data&& dat) {
        if (this != &dat) {
            obj_.reset();
            str_.reset();
            list_.reset();
            partial_.reset();
            lambda_.reset();
            if (dat.obj_) {
                obj_ = std::move(dat.obj_);
            } else if (dat.str_) {
                str_ = std::move(dat.str_);
            } else if (dat.list_) {
                list_ = std::move(dat.list_);
            } else if (dat.partial_) {
                partial_ = std::move(dat.partial_);
            } else if (dat.lambda_) {
                lambda_ = std::move(dat.lambda_);
            }
            type_ = dat.type_;
            dat.type_ = type::invalid;
        }
        return *this;
    }

    // Type info
    bool is_object() const {
        return type_ == type::object;
    }
    bool is_string() const {
        return type_ == type::string;
    }
    bool is_list() const {
        return type_ == type::list;
    }
    bool is_bool() const {
        return is_true() || is_false();
    }
    bool is_true() const {
        return type_ == type::bool_true;
    }
    bool is_false() const {
        return type_ == type::bool_false;
    }
    bool is_partial() const {
        return type_ == type::partial;
    }
    bool is_lambda() const {
        return type_ == type::lambda;
    }
    bool is_lambda2() const {
        return type_ == type::lambda2;
    }
    bool is_invalid() const {
        return type_ == type::invalid;
    }

    // Object data
    bool is_empty_object() const {
        return is_object() && obj_->empty();
    }
    bool is_non_empty_object() const {
        return is_object() && !obj_->empty();
    }
    void set(const string_type& name, const basic_data& var) {
        if (is_object()) {
            auto it = obj_->find(name);
            if (it != obj_->end()) {
                obj_->erase(it);
            }
            obj_->insert(std::pair<string_type,basic_data>{name, var});
        }
    }
    const basic_data* get(const string_type& name) const {
        if (!is_object()) {
            return nullptr;
        }
        const auto& it = obj_->find(name);
        if (it == obj_->end()) {
            return nullptr;
        }
        return &it->second;
    }

    // List data
    void push_back(const basic_data& var) {
        if (is_list()) {
            list_->push_back(var);
        }
    }
    const basic_list<string_type>& list_value() const {
        return *list_;
    }
    bool is_empty_list() const {
        return is_list() && list_->empty();
    }
    bool is_non_empty_list() const {
        return is_list() && !list_->empty();
    }
    basic_data& operator<< (const basic_data& data) {
        push_back(data);
        return *this;
    }

    // String data
    const string_type& string_value() const {
        return *str_;
    }

    basic_data& operator[] (const string_type& key) {
        return (*obj_)[key];
    }

    const basic_partial<string_type>& partial_value() const {
        return (*partial_);
    }

    const basic_lambda<string_type>& lambda_value() const {
        return lambda_->type1_value();
    }

    const basic_lambda2<string_type>& lambda2_value() const {
        return lambda_->type2_value();
    }

private:
    type type_;
    std::unique_ptr<basic_object<string_type>> obj_;
    std::unique_ptr<string_type> str_;
    std::unique_ptr<basic_list<string_type>> list_;
    std::unique_ptr<basic_partial<string_type>> partial_;
    std::unique_ptr<basic_lambda_t<string_type>> lambda_;
};

template <typename string_type>
class delimiter_set {
public:
    string_type begin;
    string_type end;
    delimiter_set()
        : begin(default_begin)
        , end(default_end)
    {}
    bool is_default() const { return begin == default_begin && end == default_end; }
    static const string_type default_begin;
    static const string_type default_end;
};

template <typename string_type>
const string_type delimiter_set<string_type>::default_begin(2, '{');
template <typename string_type>
const string_type delimiter_set<string_type>::default_end(2, '}');

template <typename string_type>
class basic_context {
public:
    virtual ~basic_context() = default;
    virtual void push(const basic_data<string_type>* data) = 0;
    virtual void pop() = 0;

    virtual const basic_data<string_type>* get(const string_type& name) const = 0;
    virtual const basic_data<string_type>* get_partial(const string_type& name) const = 0;
};

template <typename string_type>
class context : public basic_context<string_type> {
public:
    context(const basic_data<string_type>* data) {
        push(data);
    }

    context() {
    }

    virtual void push(const basic_data<string_type>* data) override {
        items_.insert(items_.begin(), data);
    }

    virtual void pop() override {
        items_.erase(items_.begin());
    }

    virtual const basic_data<string_type>* get(const string_type& name) const override {
        // process {{.}} name
        if (name.size() == 1 && name.at(0) == '.') {
            return items_.front();
        }
        if (name.find('.') == string_type::npos) {
            // process normal name without having to split which is slower
            for (const auto& item : items_) {
                const auto var = item->get(name);
                if (var) {
                    return var;
                }
            }
            return nullptr;
        }
        // process x.y-like name
        const auto names = split(name, '.');
        for (const auto& item : items_) {
            auto var = item;
            for (const auto& n : names) {
                var = var->get(n);
                if (!var) {
                    break;
                }
            }
            if (var) {
                return var;
            }
        }
        return nullptr;
    }

    virtual const basic_data<string_type>* get_partial(const string_type& name) const override {
        for (const auto& item : items_) {
            const auto var = item->get(name);
            if (var) {
                return var;
            }
        }
        return nullptr;
    }

    context(const context&) = delete;
    context& operator= (const context&) = delete;

private:
    std::vector<const basic_data<string_type>*> items_;
};

template <typename string_type>
class line_buffer_state {
public:
    string_type data;
    bool contained_section_tag = false;

    bool is_empty_or_contains_only_whitespace() const {
        for (const auto ch : data) {
            // don't look at newlines
            if (ch != ' ' && ch != '\t') {
                return false;
            }
        }
        return true;
    }

    void clear() {
        data.clear();
        contained_section_tag = false;
    }
};

template <typename string_type>
class context_internal {
public:
    basic_context<string_type>& ctx;
    delimiter_set<string_type> delim_set;
    line_buffer_state<string_type> line_buffer;

    context_internal(basic_context<string_type>& a_ctx)
        : ctx(a_ctx)
    {
    }
};

enum class tag_type {
    text,
    variable,
    unescaped_variable,
    section_begin,
    section_end,
    section_begin_inverted,
    comment,
    partial,
    set_delimiter,
};

template <typename string_type>
class mstch_tag /* gcc doesn't allow "tag tag;" so rename the class :( */ {
public:
    string_type name;
    tag_type type = tag_type::text;
    std::shared_ptr<string_type> section_text;
    std::shared_ptr<delimiter_set<string_type>> delim_set;
    bool is_section_begin() const {
        return type == tag_type::section_begin || type == tag_type::section_begin_inverted;
    }
    bool is_section_end() const {
        return type == tag_type::section_end;
    }
};

template <typename string_type>
class context_pusher {
public:
    context_pusher(context_internal<string_type>& ctx, const basic_data<string_type>* data)
        : ctx_(ctx)
    {
        ctx.ctx.push(data);
    }
    ~context_pusher() {
        ctx_.ctx.pop();
    }
    context_pusher(const context_pusher&) = delete;
    context_pusher& operator= (const context_pusher&) = delete;
private:
    context_internal<string_type>& ctx_;
};

template <typename string_type>
class component {
private:
    using string_size_type = typename string_type::size_type;

public:
    string_type text;
    mstch_tag<string_type> tag;
    std::vector<component> children;
    string_size_type position = string_type::npos;

    enum class walk_control {
        walk, // "continue" is reserved :/
        stop,
        skip,
    };
    using walk_callback = std::function<walk_control(component&)>;

    component() {}
    component(const string_type& t, string_size_type p) : text(t), position(p) {}

    bool is_text() const {
        return tag.type == tag_type::text;
    }

    bool is_newline() const {
        return is_text() && ((text.size() == 2 && text[0] == '\r' && text[1] == '\n') ||
        (text.size() == 1 && (text[0] == '\n' || text[0] == '\r')));
    }

    bool is_non_newline_whitespace() const {
        return is_text() && !is_newline() && text.size() == 1 && (text[0] == ' ' || text[0] == '\t');
    }

    void walk_children(const walk_callback& callback) {
        for (auto& child : children) {
            if (child.walk(callback) != walk_control::walk) {
                break;
            }
        }
    }

private:
    walk_control walk(const walk_callback& callback) {
        walk_control control{callback(*this)};
        if (control == walk_control::stop) {
            return control;
        } else if (control == walk_control::skip) {
            return walk_control::walk;
        }
        for (auto& child : children) {
            control = child.walk(callback);
            if (control == walk_control::stop) {
                return control;
            }
        }
        return control;
    }
};

template <typename string_type>
class parser {
public:
    parser(const string_type& input, context_internal<string_type>& ctx, component<string_type>& root_component, string_type& error_message)
    {
        parse(input, ctx, root_component, error_message);
    }

private:
    void parse(const string_type& input, context_internal<string_type>& ctx, component<string_type>& root_component, string_type& error_message) const {
        using string_size_type = typename string_type::size_type;
        using streamstring = std::basic_ostringstream<typename string_type::value_type>;

        const string_type brace_delimiter_end_unescaped(3, '}');
        const string_size_type input_size{input.size()};

        bool current_delimiter_is_brace{ctx.delim_set.is_default()};

        std::vector<component<string_type>*> sections{&root_component};
        std::vector<string_size_type> section_starts;
        string_type current_text;
        string_size_type current_text_position = -1;

        current_text.reserve(input_size);

        const auto process_current_text = [&current_text, &current_text_position, &sections]() {
            if (!current_text.empty()) {
                const component<string_type> comp{current_text, current_text_position};
                sections.back()->children.push_back(comp);
                current_text.clear();
                current_text_position = -1;
            }
        };

        const std::vector<string_type> whitespace{
            string_type(1, '\r') + string_type(1, '\n'),
            string_type(1, '\n'),
            string_type(1, '\r'),
            string_type(1, ' '),
            string_type(1, '\t'),
        };

        for (string_size_type input_position = 0; input_position != input_size;) {
            bool parse_tag = false;

            if (input.compare(input_position, ctx.delim_set.begin.size(), ctx.delim_set.begin) == 0) {
                process_current_text();

                // Tag start delimiter
                parse_tag = true;
            } else {
                bool parsed_whitespace = false;
                for (const auto& whitespace_text : whitespace) {
                    if (input.compare(input_position, whitespace_text.size(), whitespace_text) == 0) {
                        process_current_text();

                        const component<string_type> comp{whitespace_text, input_position};
                        sections.back()->children.push_back(comp);
                        input_position += whitespace_text.size();

                        parsed_whitespace = true;
                        break;
                    }
                }

                if (!parsed_whitespace) {
                    if (current_text.empty()) {
                        current_text_position = input_position;
                    }
                    current_text.append(1, input[input_position]);
                    input_position++;
                }
            }

            if (!parse_tag) {
                continue;
            }

            // Find the next tag start delimiter
            const string_size_type tag_location_start = input_position;

            // Find the next tag end delimiter
            string_size_type tag_contents_location{tag_location_start + ctx.delim_set.begin.size()};
            const bool tag_is_unescaped_var{current_delimiter_is_brace && tag_location_start != (input_size - 2) && input.at(tag_contents_location) == ctx.delim_set.begin.at(0)};
            const string_type& current_tag_delimiter_end{tag_is_unescaped_var ? brace_delimiter_end_unescaped : ctx.delim_set.end};
            const auto current_tag_delimiter_end_size = current_tag_delimiter_end.size();
            if (tag_is_unescaped_var) {
                ++tag_contents_location;
            }
            const string_size_type tag_location_end{input.find(current_tag_delimiter_end, tag_contents_location)};
            if (tag_location_end == string_type::npos) {
                streamstring ss;
                ss << "Unclosed tag at " << tag_location_start;
                error_message.assign(ss.str());
                return;
            }

            // Parse tag
            const string_type tag_contents{trim(string_type{input, tag_contents_location, tag_location_end - tag_contents_location})};
            component<string_type> comp;
            if (!tag_contents.empty() && tag_contents[0] == '=') {
                if (!parse_set_delimiter_tag(tag_contents, ctx.delim_set)) {
                    streamstring ss;
                    ss << "Invalid set delimiter tag at " << tag_location_start;
                    error_message.assign(ss.str());
                    return;
                }
                current_delimiter_is_brace = ctx.delim_set.is_default();
                comp.tag.type = tag_type::set_delimiter;
                comp.tag.delim_set.reset(new delimiter_set<string_type>(ctx.delim_set));
            }
            if (comp.tag.type != tag_type::set_delimiter) {
                parse_tag_contents(tag_is_unescaped_var, tag_contents, comp.tag);
            }
            comp.position = tag_location_start;
            sections.back()->children.push_back(comp);

            // Start next search after this tag
            input_position = tag_location_end + current_tag_delimiter_end_size;

            // Push or pop sections
            if (comp.tag.is_section_begin()) {
                sections.push_back(&sections.back()->children.back());
                section_starts.push_back(input_position);
            } else if (comp.tag.is_section_end()) {
                if (sections.size() == 1) {
                    streamstring ss;
                    ss << "Unopened section \"" << comp.tag.name << "\" at " << comp.position;
                    error_message.assign(ss.str());
                    return;
                }
                sections.back()->tag.section_text.reset(new string_type(input.substr(section_starts.back(), tag_location_start - section_starts.back())));
                sections.pop_back();
                section_starts.pop_back();
            }
        }

        process_current_text();

        // Check for sections without an ending tag
        root_component.walk_children([&error_message](component<string_type>& comp) -> typename component<string_type>::walk_control {
            if (!comp.tag.is_section_begin()) {
                return component<string_type>::walk_control::walk;
            }
            if (comp.children.empty() || !comp.children.back().tag.is_section_end() || comp.children.back().tag.name != comp.tag.name) {
                streamstring ss;
                ss << "Unclosed section \"" << comp.tag.name << "\" at " << comp.position;
                error_message.assign(ss.str());
                return component<string_type>::walk_control::stop;
            }
            comp.children.pop_back(); // remove now useless end section component
            return component<string_type>::walk_control::walk;
        });
        if (!error_message.empty()) {
            return;
        }
    }

    bool is_set_delimiter_valid(const string_type& delimiter) const {
        // "Custom delimiters may not contain whitespace or the equals sign."
        for (const auto ch : delimiter) {
            if (ch == '=' || std::isspace(ch)) {
                return false;
            }
        }
        return true;
    }

    bool parse_set_delimiter_tag(const string_type& contents, delimiter_set<string_type>& delimiter_set) const {
        // Smallest legal tag is "=X X="
        if (contents.size() < 5) {
            return false;
        }
        if (contents.back() != '=') {
            return false;
        }
        const auto contents_substr = trim(contents.substr(1, contents.size() - 2));
        const auto spacepos = contents_substr.find(' ');
        if (spacepos == string_type::npos) {
            return false;
        }
        const auto nonspace = contents_substr.find_first_not_of(' ', spacepos + 1);
        assert(nonspace != string_type::npos);
        const string_type begin = contents_substr.substr(0, spacepos);
        const string_type end = contents_substr.substr(nonspace, contents_substr.size() - nonspace);
        if (!is_set_delimiter_valid(begin) || !is_set_delimiter_valid(end)) {
            return false;
        }
        delimiter_set.begin = begin;
        delimiter_set.end = end;
        return true;
    }

    void parse_tag_contents(bool is_unescaped_var, const string_type& contents, mstch_tag<string_type>& tag) const {
        if (is_unescaped_var) {
            tag.type = tag_type::unescaped_variable;
            tag.name = contents;
        } else if (contents.empty()) {
            tag.type = tag_type::variable;
            tag.name.clear();
        } else {
            switch (contents.at(0)) {
                case '#':
                    tag.type = tag_type::section_begin;
                    break;
                case '^':
                    tag.type = tag_type::section_begin_inverted;
                    break;
                case '/':
                    tag.type = tag_type::section_end;
                    break;
                case '>':
                    tag.type = tag_type::partial;
                    break;
                case '&':
                    tag.type = tag_type::unescaped_variable;
                    break;
                case '!':
                    tag.type = tag_type::comment;
                    break;
                default:
                    tag.type = tag_type::variable;
                    break;
            }
            if (tag.type == tag_type::variable) {
                tag.name = contents;
            } else {
                string_type name{contents};
                name.erase(name.begin());
                tag.name = trim(name);
            }
        }
    }
};

template <typename StringType>
class basic_mustache {
public:
    using string_type = StringType;

    basic_mustache(const string_type& input)
        : basic_mustache() {
        context<string_type> ctx;
        context_internal<string_type> context{ctx};
        parser<string_type> parser{input, context, root_component_, error_message_};
    }

    bool is_valid() const {
        return error_message_.empty();
    }

    const string_type& error_message() const {
        return error_message_;
    }

    using escape_handler = std::function<string_type(const string_type&)>;
    void set_custom_escape(const escape_handler& escape_fn) {
        escape_ = escape_fn;
    }

    template <typename stream_type>
    stream_type& render(const basic_data<string_type>& data, stream_type& stream) {
        render(data, [&stream](const string_type& str) {
            stream << str;
        });
        return stream;
    }

    string_type render(const basic_data<string_type>& data) {
        std::basic_ostringstream<typename string_type::value_type> ss;
        return render(data, ss).str();
    }

    template <typename stream_type>
    stream_type& render(basic_context<string_type>& ctx, stream_type& stream) {
        context_internal<string_type> context{ctx};
        render([&stream](const string_type& str) {
            stream << str;
        }, context);
        return stream;
    }

    string_type render(basic_context<string_type>& ctx) {
        std::basic_ostringstream<typename string_type::value_type> ss;
        return render(ctx, ss).str();
    }

    using render_handler = std::function<void(const string_type&)>;
    void render(const basic_data<string_type>& data, const render_handler& handler) {
        if (!is_valid()) {
            return;
        }
        context<string_type> ctx{&data};
        context_internal<string_type> context{ctx};
        render(handler, context);
    }

private:
    using string_size_type = typename string_type::size_type;

    basic_mustache()
        : escape_(html_escape<string_type>)
    {
    }

    basic_mustache(const string_type& input, context_internal<string_type>& ctx)
        : basic_mustache() {
        parser<string_type> parser{input, ctx, root_component_, error_message_};
    }

    string_type render(context_internal<string_type>& ctx) {
        std::basic_ostringstream<typename string_type::value_type> ss;
        render([&ss](const string_type& str) {
            ss << str;
        }, ctx);
        return ss.str();
    }

    void render(const render_handler& handler, context_internal<string_type>& ctx, bool root_renderer = true) {
        root_component_.walk_children([&handler, &ctx, this](component<string_type>& comp) -> typename component<string_type>::walk_control {
            return render_component(handler, ctx, comp);
        });
        // process the last line, but only for the top-level renderer
        if (root_renderer) {
            render_current_line(handler, ctx, nullptr);
        }
    }

    void render_current_line(const render_handler& handler, context_internal<string_type>& ctx, const component<string_type>* comp) const {
        // We're at the end of a line, so check the line buffer state to see
        // if the line had tags in it, and also if the line is now empty or
        // contains whitespace only. if this situation is true, skip the line.
        bool output = true;
        if (ctx.line_buffer.contained_section_tag && ctx.line_buffer.is_empty_or_contains_only_whitespace()) {
            output = false;
        }
        if (output) {
            handler(ctx.line_buffer.data);
            if (comp) {
                handler(comp->text);
            }
        }
        ctx.line_buffer.clear();
    }

    void render_result(context_internal<string_type>& ctx, const string_type& text) const {
        ctx.line_buffer.data.append(text);
    }

    typename component<string_type>::walk_control render_component(const render_handler& handler, context_internal<string_type>& ctx, component<string_type>& comp) {
        if (comp.is_text()) {
            if (comp.is_newline()) {
                render_current_line(handler, ctx, &comp);
            } else {
                render_result(ctx, comp.text);
            }
            return component<string_type>::walk_control::walk;
        }

        const mstch_tag<string_type>& tag{comp.tag};
        const basic_data<string_type>* var = nullptr;
        switch (tag.type) {
            case tag_type::variable:
            case tag_type::unescaped_variable:
                if ((var = ctx.ctx.get(tag.name)) != nullptr) {
                    if (!render_variable(handler, var, ctx, tag.type == tag_type::variable)) {
                        return component<string_type>::walk_control::stop;
                    }
                }
                break;
            case tag_type::section_begin:
                if ((var = ctx.ctx.get(tag.name)) != nullptr) {
                    if (var->is_lambda() || var->is_lambda2()) {
                        if (!render_lambda(handler, var, ctx, render_lambda_escape::optional, *comp.tag.section_text, true)) {
                            return component<string_type>::walk_control::stop;
                        }
                    } else if (!var->is_false() && !var->is_empty_list()) {
                        render_section(handler, ctx, comp, var);
                    }
                }
                return component<string_type>::walk_control::skip;
            case tag_type::section_begin_inverted:
                if ((var = ctx.ctx.get(tag.name)) == nullptr || var->is_false() || var->is_empty_list()) {
                    render_section(handler, ctx, comp, var);
                }
                return component<string_type>::walk_control::skip;
            case tag_type::partial:
                if ((var = ctx.ctx.get_partial(tag.name)) != nullptr && (var->is_partial() || var->is_string())) {
                    const auto& partial_result = var->is_partial() ? var->partial_value()() : var->string_value();
                    basic_mustache tmpl{partial_result};
                    tmpl.set_custom_escape(escape_);
                    if (!tmpl.is_valid()) {
                        error_message_ = tmpl.error_message();
                    } else {
                        tmpl.render(handler, ctx, false);
                        if (!tmpl.is_valid()) {
                            error_message_ = tmpl.error_message();
                        }
                    }
                    if (!tmpl.is_valid()) {
                        return component<string_type>::walk_control::stop;
                    }
                }
                break;
            case tag_type::set_delimiter:
                ctx.delim_set = *comp.tag.delim_set;
                break;
            default:
                break;
        }

        return component<string_type>::walk_control::walk;
    }

    enum class render_lambda_escape {
        escape,
        unescape,
        optional,
    };

    bool render_lambda(const render_handler& handler, const basic_data<string_type>* var, context_internal<string_type>& ctx, render_lambda_escape escape, const string_type& text, bool parse_with_same_context) {
        const typename basic_renderer<string_type>::type2 render2 = [this, &ctx, parse_with_same_context, escape](const string_type& text, bool escaped) {
            const auto process_template = [this, &ctx, escape, escaped](basic_mustache& tmpl) -> string_type {
                if (!tmpl.is_valid()) {
                    error_message_ = tmpl.error_message();
                    return {};
                }
                context_internal<string_type> render_ctx{ctx.ctx}; // start a new line_buffer
                const auto str = tmpl.render(render_ctx);
                if (!tmpl.is_valid()) {
                    error_message_ = tmpl.error_message();
                    return {};
                }
                bool do_escape = false;
                switch (escape) {
                    case render_lambda_escape::escape:
                        do_escape = true;
                        break;
                    case render_lambda_escape::unescape:
                        do_escape = false;
                        break;
                    case render_lambda_escape::optional:
                        do_escape = escaped;
                        break;
                }
                return do_escape ? escape_(str) : str;
            };
            if (parse_with_same_context) {
                basic_mustache tmpl{text, ctx};
                tmpl.set_custom_escape(escape_);
                return process_template(tmpl);
            }
            basic_mustache tmpl{text};
            tmpl.set_custom_escape(escape_);
            return process_template(tmpl);
        };
        const typename basic_renderer<string_type>::type1 render = [&render2](const string_type& text) {
            return render2(text, false);
        };
        if (var->is_lambda2()) {
            const basic_renderer<string_type> renderer{render, render2};
            render_result(ctx, var->lambda2_value()(text, renderer));
        } else {
            render_current_line(handler, ctx, nullptr);
            render_result(ctx, render(var->lambda_value()(text)));
        }
        return error_message_.empty();
    }

    bool render_variable(const render_handler& handler, const basic_data<string_type>* var, context_internal<string_type>& ctx, bool escaped) {
        if (var->is_string()) {
            const auto& varstr = var->string_value();
            render_result(ctx, escaped ? escape_(varstr) : varstr);
        } else if (var->is_lambda()) {
            const render_lambda_escape escape_opt = escaped ? render_lambda_escape::escape : render_lambda_escape::unescape;
            return render_lambda(handler, var, ctx, escape_opt, {}, false);
        } else if (var->is_lambda2()) {
            using streamstring = std::basic_ostringstream<typename string_type::value_type>;
            streamstring ss;
            ss << "Lambda with render argument is not allowed for regular variables";
            error_message_ = ss.str();
            return false;
        }
        return true;
    }

    void render_section(const render_handler& handler, context_internal<string_type>& ctx, component<string_type>& incomp, const basic_data<string_type>* var) {
        const auto callback = [&handler, &ctx, this](component<string_type>& comp) -> typename component<string_type>::walk_control {
            return render_component(handler, ctx, comp);
        };
        if (var && var->is_non_empty_list()) {
            for (const auto& item : var->list_value()) {
                // account for the section begin tag
                ctx.line_buffer.contained_section_tag = true;

                const context_pusher<string_type> ctxpusher{ctx, &item};
                incomp.walk_children(callback);

                // ctx may have been cleared. account for the section end tag
                ctx.line_buffer.contained_section_tag = true;
            }
        } else if (var) {
            // account for the section begin tag
            ctx.line_buffer.contained_section_tag = true;

            const context_pusher<string_type> ctxpusher{ctx, var};
            incomp.walk_children(callback);

            // ctx may have been cleared. account for the section end tag
            ctx.line_buffer.contained_section_tag = true;
        } else {
            // account for the section begin tag
            ctx.line_buffer.contained_section_tag = true;

            incomp.walk_children(callback);

            // ctx may have been cleared. account for the section end tag
            ctx.line_buffer.contained_section_tag = true;
        }
    }

private:
    string_type error_message_;
    component<string_type> root_component_;
    escape_handler escape_;
};

using mustache = basic_mustache<std::string>;
using data = basic_data<mustache::string_type>;
using object = basic_object<mustache::string_type>;
using list = basic_list<mustache::string_type>;
using partial = basic_partial<mustache::string_type>;
using renderer = basic_renderer<mustache::string_type>;
using lambda = basic_lambda<mustache::string_type>;
using lambda2 = basic_lambda2<mustache::string_type>;
using lambda_t = basic_lambda_t<mustache::string_type>;

using mustachew = basic_mustache<std::wstring>;
using dataw = basic_data<mustachew::string_type>;

} // namespace mustache
} // namespace kainjow

#endif // KAINJOW_MUSTACHE_HPP
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_METAMESSAGETOCPPTRANSFORMATOR_HPP
#define CLUON_METAMESSAGETOCPPTRANSFORMATOR_HPP

//#include "Mustache/mustache.hpp"
//#include "cluon/MetaMessage.hpp"
//#include "cluon/cluon.hpp"

#include <string>

namespace cluon {
/**
This class transforms a given MetaMessage to a C++ header and source content.

Transformation is using https://github.com/kainjow/Mustache.
*/
class LIBCLUON_API MetaMessageToCPPTransformator {
   private:
    MetaMessageToCPPTransformator(MetaMessageToCPPTransformator &&) = delete;
    MetaMessageToCPPTransformator &operator=(const MetaMessageToCPPTransformator &) = delete;
    MetaMessageToCPPTransformator &operator=(MetaMessageToCPPTransformator &&) = delete;

   public:
    MetaMessageToCPPTransformator()                                      = default;
    MetaMessageToCPPTransformator(const MetaMessageToCPPTransformator &) = default;

    /**
     * The method is called from MetaMessage to visit itself using this transformator.
     *
     * @param mm MetaMessage to visit.
     */
    void visit(const MetaMessage &mm) noexcept;

    /**
     * @return Content of the C++ header.
     */
    std::string content() noexcept;

   private:
    kainjow::mustache::data m_dataToBeRendered{};
    kainjow::mustache::data m_fields{kainjow::mustache::data::type::list};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_METAMESSAGETOPROTOTRANSFORMATOR_HPP
#define CLUON_METAMESSAGETOPROTOTRANSFORMATOR_HPP

//#include "Mustache/mustache.hpp"
//#include "cluon/MetaMessage.hpp"
//#include "cluon/cluon.hpp"

#include <string>

namespace cluon {
/**
This class transforms a given MetaMessage to a .proto file.

Transformation is using https://github.com/kainjow/Mustache.
*/
class LIBCLUON_API MetaMessageToProtoTransformator {
   private:
    MetaMessageToProtoTransformator(MetaMessageToProtoTransformator &&) = delete;
    MetaMessageToProtoTransformator &operator=(const MetaMessageToProtoTransformator &) = delete;
    MetaMessageToProtoTransformator &operator=(MetaMessageToProtoTransformator &&) = delete;

   public:
    MetaMessageToProtoTransformator()                                        = default;
    MetaMessageToProtoTransformator(const MetaMessageToProtoTransformator &) = default;

    /**
     * The method is called from MetaMessage to visit itself using this transformator.
     *
     * @param mm MetaMessage to visit.
     */
    void visit(const MetaMessage &mm) noexcept;

    /**
     * @return Content of the .proto file.
     */
    std::string content(bool withProtoHeader) noexcept;

   private:
    kainjow::mustache::data m_dataToBeRendered;
    kainjow::mustache::data m_fields{kainjow::mustache::data::type::list};
};
} // namespace cluon

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/MetaMessageToCPPTransformator.hpp"
//#include "cluon/MetaMessage.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>

//#include "Mustache/mustache.hpp"

namespace cluon {

const char *headerFileTemplate = R"(
/*
 * THIS IS AN AUTO-GENERATED FILE. DO NOT MODIFY AS CHANGES MIGHT BE OVERWRITTEN!
 */

#ifndef VISITABLE_TYPE_TRAIT
#define VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct visitorSelector {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct visitorSelector<true> {
    template<typename T, class Visitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
        visitor.visit(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<typename T>
struct isVisitable {
    static const bool value = false;
};

template<typename T, class Visitor>
void doVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, Visitor &visitor) {
    visitorSelector<isVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, visitor);
}
#endif

#ifndef TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#define TRIPLET_FORWARD_VISITABLE_TYPE_TRAIT
#include <cstdint>
#include <string>
#include <utility>

template<bool b>
struct tripletForwardVisitorSelector {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)preVisit;
        (void)postVisit;
        std::forward<Visitor>(visit)(fieldIdentifier, std::move(typeName), std::move(name), value);
    }
};

template<>
struct tripletForwardVisitorSelector<true> {
    template<typename T, class PreVisitor, class Visitor, class PostVisitor>
    static void impl(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
        (void)fieldIdentifier;
        (void)typeName;
        (void)name;
        // Apply preVisit, visit, and postVisit on value.
        value.accept(preVisit, visit, postVisit);
    }
};

template<typename T>
struct isTripletForwardVisitable {
    static const bool value = false;
};

template< typename T, class PreVisitor, class Visitor, class PostVisitor>
void doTripletForwardVisit(uint32_t fieldIdentifier, std::string &&typeName, std::string &&name, T &value, PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
    tripletForwardVisitorSelector<isTripletForwardVisitable<T>::value >::impl(fieldIdentifier, std::move(typeName), std::move(name), value, std::move(preVisit), std::move(visit), std::move(postVisit)); // NOLINT
}
#endif


#ifndef {{%HEADER_GUARD%}}_HPP
#define {{%HEADER_GUARD%}}_HPP

#ifdef WIN32
    // Export symbols if compile flags "LIB_SHARED" and "LIB_EXPORTS" are set on Windows.
    #ifdef LIB_SHARED
        #ifdef LIB_EXPORTS
            #define LIB_API __declspec(dllexport)
        #else
            #define LIB_API __declspec(dllimport)
        #endif
    #else
        // Disable definition if linking statically.
        #define LIB_API
    #endif
#else
    // Disable definition for non-Win32 systems.
    #define LIB_API
#endif

#include <string>
#include <utility>
{{%NAMESPACE_OPENING%}}
using namespace std::string_literals; // NOLINT
class LIB_API {{%MESSAGE%}} {
    private:
        static constexpr const char* TheShortName = "{{%MESSAGE%}}";
        static constexpr const char* TheLongName = "{{%COMPLETEPACKAGENAME%}}{{%MESSAGE%}}";

    public:
        inline static int32_t ID() {
            return {{%IDENTIFIER%}};
        }
        inline static const std::string ShortName() {
            return TheShortName;
        }
        inline static const std::string LongName() {
            return TheLongName;
        }

    public:
        {{%MESSAGE%}}() = default;
        {{%MESSAGE%}}(const {{%MESSAGE%}}&) = default;
        {{%MESSAGE%}}& operator=(const {{%MESSAGE%}}&) = default;
        {{%MESSAGE%}}({{%MESSAGE%}}&&) = default;
        {{%MESSAGE%}}& operator=({{%MESSAGE%}}&&) = default;
        ~{{%MESSAGE%}}() = default;

    public:
        {{#%FIELDS%}}
        inline {{%MESSAGE%}}& {{%NAME%}}(const {{%TYPE%}} &v) noexcept {
            m_{{%NAME%}} = v;
            return *this;
        }
        inline {{%TYPE%}} {{%NAME%}}() const noexcept {
            return m_{{%NAME%}};
        }
        {{/%FIELDS%}}

    public:
        template<class Visitor>
        inline void accept(uint32_t fieldId, Visitor &visitor) {
            (void)fieldId;
            (void)visitor;
//            visitor.preVisit(ID(), ShortName(), LongName());
            {{#%FIELDS%}}
            if ({{%FIELDIDENTIFIER%}} == fieldId) {
                doVisit({{%FIELDIDENTIFIER%}}, std::move("{{%TYPE%}}"s), std::move("{{%NAME%}}"s), m_{{%NAME%}}, visitor);
                return;
            }
            {{/%FIELDS%}}
//            visitor.postVisit();
        }

        template<class Visitor>
        inline void accept(Visitor &visitor) {
            visitor.preVisit(ID(), ShortName(), LongName());
            {{#%FIELDS%}}
            doVisit({{%FIELDIDENTIFIER%}}, std::move("{{%TYPE%}}"s), std::move("{{%NAME%}}"s), m_{{%NAME%}}, visitor);
            {{/%FIELDS%}}
            visitor.postVisit();
        }

        template<class PreVisitor, class Visitor, class PostVisitor>
        inline void accept(PreVisitor &&preVisit, Visitor &&visit, PostVisitor &&postVisit) {
            (void)visit; // Prevent warnings from empty messages.
            std::forward<PreVisitor>(preVisit)(ID(), ShortName(), LongName());
            {{#%FIELDS%}}
            doTripletForwardVisit({{%FIELDIDENTIFIER%}}, std::move("{{%TYPE%}}"s), std::move("{{%NAME%}}"s), m_{{%NAME%}}, preVisit, visit, postVisit);
            {{/%FIELDS%}}
            std::forward<PostVisitor>(postVisit)();
        }

    private:
        {{#%FIELDS%}}
        {{%TYPE%}} m_{{%NAME%}}{ {{%FIELD_DEFAULT_INITIALIZATION_VALUE%}}{{%INITIALIZER_SUFFIX%}} }; // field identifier = {{%FIELDIDENTIFIER%}}.
        {{/%FIELDS%}}
};
{{%NAMESPACE_CLOSING%}}

template<>
struct isVisitable<{{%COMPLETEPACKAGENAME_WITH_COLON_SEPARATORS%}}{{%MESSAGE%}}> {
    static const bool value = true;
};
template<>
struct isTripletForwardVisitable<{{%COMPLETEPACKAGENAME_WITH_COLON_SEPARATORS%}}{{%MESSAGE%}}> {
    static const bool value = true;
};
#endif
)";

std::string MetaMessageToCPPTransformator::content() noexcept {
    m_dataToBeRendered.set("%FIELDS%", m_fields);

    kainjow::mustache::mustache tmpl{headerFileTemplate};
    // Reset Mustache's default string-escaper.
    tmpl.set_custom_escape([](const std::string &s) { return s; });
    std::stringstream sstr;
    sstr << tmpl.render(m_dataToBeRendered);
    const std::string str(sstr.str());
    return str;
}

void MetaMessageToCPPTransformator::visit(const MetaMessage &mm) noexcept {
    kainjow::mustache::data dataToBeRendered;
    kainjow::mustache::data fields{kainjow::mustache::data::type::list};

    try {
        std::map<MetaMessage::MetaField::MetaFieldDataTypes, std::string> typeToTypeStringMap = {
            {MetaMessage::MetaField::BOOL_T, "bool"},
            {MetaMessage::MetaField::CHAR_T, "char"},
            {MetaMessage::MetaField::UINT8_T, "uint8_t"},
            {MetaMessage::MetaField::INT8_T, "int8_t"},
            {MetaMessage::MetaField::UINT16_T, "uint16_t"},
            {MetaMessage::MetaField::INT16_T, "int16_t"},
            {MetaMessage::MetaField::UINT32_T, "uint32_t"},
            {MetaMessage::MetaField::INT32_T, "int32_t"},
            {MetaMessage::MetaField::UINT64_T, "uint64_t"},
            {MetaMessage::MetaField::INT64_T, "int64_t"},
            {MetaMessage::MetaField::FLOAT_T, "float"},
            {MetaMessage::MetaField::DOUBLE_T, "double"},
            {MetaMessage::MetaField::STRING_T, "std::string"},
            {MetaMessage::MetaField::BYTES_T, "std::string"},
        };

        std::map<MetaMessage::MetaField::MetaFieldDataTypes, std::string> typeToDefaultInitizationValueMap = {
            {MetaMessage::MetaField::BOOL_T, "false"},
            {MetaMessage::MetaField::CHAR_T, "'\\0'"},
            {MetaMessage::MetaField::UINT8_T, "0"},
            {MetaMessage::MetaField::INT8_T, "0"},
            {MetaMessage::MetaField::UINT16_T, "0"},
            {MetaMessage::MetaField::INT16_T, "0"},
            {MetaMessage::MetaField::UINT32_T, "0"},
            {MetaMessage::MetaField::INT32_T, "0"},
            {MetaMessage::MetaField::UINT64_T, "0"},
            {MetaMessage::MetaField::INT64_T, "0"},
            {MetaMessage::MetaField::FLOAT_T, "0.0"},
            {MetaMessage::MetaField::DOUBLE_T, "0.0"},
            {MetaMessage::MetaField::STRING_T, R"("")"},
            {MetaMessage::MetaField::BYTES_T, R"("")"},
        };

        std::string namespacePrefix;
        std::string messageName{mm.messageName()};
        const auto pos = mm.messageName().find_last_of('.');
        if (std::string::npos != pos) {
            namespacePrefix = mm.messageName().substr(0, pos);
            messageName     = mm.messageName().substr(pos + 1);
        }

        const std::string completePackageName = mm.packageName() + (!mm.packageName().empty() && !namespacePrefix.empty() ? "." : "") + namespacePrefix;
        const std::string completePackageNameWithColonSeparators{std::regex_replace(completePackageName, std::regex("\\."), "::")}; // NOLINT
        const std::string namespaceHeader{std::regex_replace(completePackageName, std::regex("\\."), " { namespace ")};             // NOLINT
        const std::string namespaceFooter(
            static_cast<uint32_t>(std::count(std::begin(namespaceHeader), std::end(namespaceHeader), '{')) + (!namespaceHeader.empty() ? 1 : 0), '}');
        std::string headerGuard{std::regex_replace(completePackageName, std::regex("\\."), "_")}; // NOLINT
        headerGuard += (!headerGuard.empty() ? +"_" : "") + messageName;
        std::transform(std::begin(headerGuard), std::end(headerGuard), std::begin(headerGuard), [](unsigned char c) { return ::toupper(c); });

        dataToBeRendered.set("%HEADER_GUARD%", headerGuard);
        dataToBeRendered.set("%NAMESPACE_OPENING%", (!namespaceHeader.empty() ? "namespace " + namespaceHeader + " {" : ""));
        dataToBeRendered.set("%COMPLETEPACKAGENAME%", completePackageName + (!completePackageName.empty() ? "." : ""));
        dataToBeRendered.set("%COMPLETEPACKAGENAME_WITH_COLON_SEPARATORS%",
                             completePackageNameWithColonSeparators + (!completePackageNameWithColonSeparators.empty() ? "::" : ""));
        dataToBeRendered.set("%MESSAGE%", messageName);
        dataToBeRendered.set("%NAMESPACE_CLOSING%", namespaceFooter);
        dataToBeRendered.set("%IDENTIFIER%", std::to_string(mm.messageIdentifier()));

        for (const auto &e : mm.listOfMetaFields()) {
            std::string fieldName{std::regex_replace(e.fieldName(), std::regex("\\."), "_")}; // NOLINT
            kainjow::mustache::data fieldEntry;
            fieldEntry.set("%NAME%", fieldName);
            if (MetaMessage::MetaField::MESSAGE_T != e.fieldDataType()) {
                fieldEntry.set("%TYPE%", typeToTypeStringMap[e.fieldDataType()]);

                const std::string defaultInitializatioValue{
                    (e.defaultInitializationValue().empty() ? typeToDefaultInitizationValueMap[e.fieldDataType()] : e.defaultInitializationValue())};
                fieldEntry.set("%FIELD_DEFAULT_INITIALIZATION_VALUE%", defaultInitializatioValue);

                std::string initializerSuffix;
                if (e.fieldDataType() == MetaMessage::MetaField::FLOAT_T) {
                    initializerSuffix = "f"; // suffix for float types.
                } else if (e.fieldDataType() == MetaMessage::MetaField::STRING_T || e.fieldDataType() == MetaMessage::MetaField::BYTES_T) {
                    initializerSuffix = "s"; // suffix to enforce std::string initialization.
                }
                fieldEntry.set("%INITIALIZER_SUFFIX%", initializerSuffix);
            } else {
                const std::string tmp{mm.packageName() + (!mm.packageName().empty() ? "." : "") + e.fieldDataTypeName()};
                const std::string completeDataTypeNameWithDoubleColons{std::regex_replace(tmp, std::regex("\\."), "::")}; // NOLINT

                fieldEntry.set("%TYPE%", completeDataTypeNameWithDoubleColons);
            }
            fieldEntry.set("%FIELDIDENTIFIER%", std::to_string(e.fieldIdentifier()));

            fields.push_back(fieldEntry);
        }
    } catch (std::regex_error &) { // LCOV_EXCL_LINE
    }

    m_dataToBeRendered = std::move(dataToBeRendered);
    m_fields           = std::move(fields);
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//#include "cluon/MetaMessageToProtoTransformator.hpp"
//#include "cluon/MetaMessage.hpp"

#include <algorithm>
#include <map>
#include <regex>
#include <sstream>

//#include "Mustache/mustache.hpp"

namespace cluon {

const char *proto2FileTemplate = R"(
//
// THIS IS AN AUTO-GENERATED FILE. DO NOT MODIFY AS CHANGES MIGHT BE OVERWRITTEN!
//

{{%PROTO2_HEADER%}}

// Message identifier: {{%MESSAGE_IDENTIFIER%}}.
message {{%MESSAGE%}} {
    {{#%FIELDS%}}
    optional {{%TYPE%}} {{%NAME%}} = {{%FIELD_IDENTIFIER%}};
    {{/%FIELDS%}}
}
)";

std::string MetaMessageToProtoTransformator::content(bool withProtoHeader) noexcept {
    if (withProtoHeader) {
        m_dataToBeRendered.set("%PROTO2_HEADER%", R"(
// This line is only needed when using Google Protobuf 3.
syntax = "proto2";
)");
    }
    m_dataToBeRendered.set("%FIELDS%", m_fields);

    kainjow::mustache::mustache tmpl{proto2FileTemplate};
    // Reset Mustache's default string-escaper.
    tmpl.set_custom_escape([](const std::string &s) { return s; });
    std::stringstream sstr;
    sstr << tmpl.render(m_dataToBeRendered);
    const std::string str(sstr.str());
    return str;
}

void MetaMessageToProtoTransformator::visit(const MetaMessage &mm) noexcept {
    kainjow::mustache::data dataToBeRendered;
    kainjow::mustache::data fields{kainjow::mustache::data::type::list};

    std::map<MetaMessage::MetaField::MetaFieldDataTypes, std::string> typeToTypeStringMap = {
        {MetaMessage::MetaField::BOOL_T, "bool"},
        {MetaMessage::MetaField::CHAR_T, "sint32"},
        {MetaMessage::MetaField::UINT8_T, "uint32"},
        {MetaMessage::MetaField::INT8_T, "sint32"},
        {MetaMessage::MetaField::UINT16_T, "uint32"},
        {MetaMessage::MetaField::INT16_T, "sint32"},
        {MetaMessage::MetaField::UINT32_T, "uint32"},
        {MetaMessage::MetaField::INT32_T, "sint32"},
        {MetaMessage::MetaField::UINT64_T, "uint64"},
        {MetaMessage::MetaField::INT64_T, "sint64"},
        {MetaMessage::MetaField::FLOAT_T, "float"},
        {MetaMessage::MetaField::DOUBLE_T, "double"},
        {MetaMessage::MetaField::STRING_T, "string"},
        {MetaMessage::MetaField::BYTES_T, "bytes"},
    };

    std::string namespacePrefix;
    std::string messageName{mm.messageName()};
    const auto pos = mm.messageName().find_last_of('.');
    if (std::string::npos != pos) {
        namespacePrefix = mm.messageName().substr(0, pos);
        messageName     = mm.messageName().substr(pos + 1);
    }

    std::string packageNameWithUnderscores{mm.packageName()};
    std::replace(packageNameWithUnderscores.begin(), packageNameWithUnderscores.end(), '.', '_');
    const std::string completePackageNameWithNamespacePrefix
        = packageNameWithUnderscores + (!packageNameWithUnderscores.empty() && !namespacePrefix.empty() ? "." : "") + namespacePrefix;

    std::string completePackageNameWithNamespacePrefixWithUnderscores{completePackageNameWithNamespacePrefix};
    std::replace(completePackageNameWithNamespacePrefixWithUnderscores.begin(), completePackageNameWithNamespacePrefixWithUnderscores.end(), '.', '_');
    const std::string completeMessageNameWithUnderscores
        = completePackageNameWithNamespacePrefixWithUnderscores + (!completePackageNameWithNamespacePrefixWithUnderscores.empty() ? +"_" : "") + messageName;

    dataToBeRendered.set("%MESSAGE_IDENTIFIER%", std::to_string(mm.messageIdentifier()));
    dataToBeRendered.set("%MESSAGE%", completeMessageNameWithUnderscores);

    for (const auto &f : mm.listOfMetaFields()) {
        std::string fieldName{f.fieldName()};
        std::replace(fieldName.begin(), fieldName.end(), '.', '_');

        kainjow::mustache::data fieldEntry;
        fieldEntry.set("%NAME%", fieldName);
        if (MetaMessage::MetaField::MESSAGE_T != f.fieldDataType()) {
            fieldEntry.set("%TYPE%", typeToTypeStringMap[f.fieldDataType()]);
        } else {
            std::string dataTypeNameWithUnderscores{f.fieldDataTypeName()};
            std::replace(dataTypeNameWithUnderscores.begin(), dataTypeNameWithUnderscores.end(), '.', '_');
            std::string tmp{packageNameWithUnderscores + (!packageNameWithUnderscores.empty() ? "_" : "")};
            tmp += dataTypeNameWithUnderscores;
            fieldEntry.set("%TYPE%", tmp);
        }
        fieldEntry.set("%FIELD_IDENTIFIER%", std::to_string(f.fieldIdentifier()));
        fields.push_back(fieldEntry);
    }

    m_dataToBeRendered = std::move(dataToBeRendered);
    m_fields           = std::move(fields);
}
} // namespace cluon
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_MSC_HPP
#define CLUON_MSC_HPP

//#include "cluon/MessageParser.hpp"
//#include "cluon/MetaMessage.hpp"
//#include "cluon/MetaMessageToCPPTransformator.hpp"
//#include "cluon/MetaMessageToProtoTransformator.hpp"

//#include "argh/argh.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

inline int32_t cluon_msc(int32_t argc, char **argv) {
    const std::string PROGRAM{argv[0]}; // NOLINT
    argh::parser commandline(argc, argv);

    std::string inputFilename = commandline.pos_args().back();
    if (std::string::npos != inputFilename.find(PROGRAM)) {
        std::cerr << PROGRAM
                  << " transforms a given message specification file in .odvd format into C++." << std::endl;
        std::cerr << "Usage:   " << PROGRAM << " [--cpp] [--proto] [--out=<file>] <odvd file>" << std::endl;
        std::cerr << "         " << PROGRAM << " --cpp:   Generate C++14-compliant, self-contained header file." << std::endl;
        std::cerr << "         " << PROGRAM << " --proto: Generate Proto version2-compliant file." << std::endl;
        std::cerr << std::endl;
        std::cerr << "Example: " << PROGRAM << " --cpp --out=/tmp/myOutput.hpp myFile.odvd" << std::endl;
        return 1;
    }

    std::string outputFilename;
    commandline({"--out"}) >> outputFilename;

    const bool generateCPP = commandline[{"--cpp"}];
    const bool generateProto = commandline[{"--proto"}];

    int retVal = 1;
    std::ifstream inputFile(inputFilename, std::ios::in);
    if (inputFile.good()) {
        bool addHeaderForFirstProtoFile = true;
        std::string input(static_cast<std::stringstream const&>(std::stringstream() << inputFile.rdbuf()).str()); // NOLINT

        cluon::MessageParser mp;
        auto result = mp.parse(input);
        retVal = result.second;

        // Delete the content of a potentially existing file.
        if (!outputFilename.empty()) {
            std::ofstream outputFile(outputFilename, std::ios::out | std::ios::trunc);
            outputFile.close();
        }
        for (auto e : result.first) {
            std::string content;
            if (generateCPP) {
                cluon::MetaMessageToCPPTransformator transformation;
                e.accept([&trans = transformation](const cluon::MetaMessage &_mm){ trans.visit(_mm); });
                content = transformation.content();
            }
            if (generateProto) {
                cluon::MetaMessageToProtoTransformator transformation;
                e.accept([&trans = transformation](const cluon::MetaMessage &_mm){ trans.visit(_mm); });
                content = transformation.content(addHeaderForFirstProtoFile);
                addHeaderForFirstProtoFile = false;
            }

            if (!outputFilename.empty()) {
                std::ofstream outputFile(outputFilename, std::ios::out | std::ios::app);
                outputFile << content << std::endl;
                outputFile.close();
            }
            else { // LCOV_EXCL_LINE
                std::cout << content << std::endl; // LCOV_EXCL_LINE
            }
        }
    }
    else {
        std::cerr << "[" << PROGRAM << "] Could not find '" << inputFilename << "'." << std::endl;
    }

    return retVal;
}

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// This test for a compiler definition is necessary to preserve single-file, header-only compability.
#ifndef HAVE_CLUON_MSC
#include "cluon-msc.hpp"
#endif

#include <cstdint>

int32_t main(int32_t argc, char **argv) {
    return cluon_msc(argc, argv);
}
#endif
#ifdef HAVE_CLUON_REPLAY
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_REPLAY_HPP
#define CLUON_REPLAY_HPP

//#include "cluon/cluon.hpp"
//#include "cluon/Envelope.hpp"
//#include "cluon/OD4Session.hpp"
//#include "cluon/ToProtoVisitor.hpp"
//#include "cluon/Player.hpp"
//#include "cluon/cluonDataStructures.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

inline int32_t cluon_replay(int32_t argc, char **argv) {
    int32_t retCode{0};
    const std::string PROGRAM{argv[0]}; // NOLINT
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if (1 == argc) {
        std::cerr << PROGRAM << " replays a .rec file into an OpenDaVINCI session or to stdout; if playing back to an OD4Session using parameter --cid, you can specify the optional parameter --stdout to also playback to stdout; --keeprunning keeps " << PROGRAM << " open at the end of a recording file." << std::endl;
        std::cerr << "Usage:   " << PROGRAM << " [--cid=<OpenDaVINCI session> [--stdout] [--keeprunning]] recording.rec" << std::endl;
        std::cerr << "Example: " << PROGRAM << " --cid=111 file.rec" << std::endl;
        std::cerr << "         " << PROGRAM << " --cid=111 --stdout file.rec" << std::endl;
        std::cerr << "         " << PROGRAM << " file.rec" << std::endl;
        retCode = 1;
    }
    else {
        const bool playBackToStdout = ( (0 != commandlineArguments.count("stdout")) || (0 == commandlineArguments.count("cid")) );
        const bool keepRunning = (0 != commandlineArguments.count("keeprunning"));

        std::string recFile;
        for (auto e : commandlineArguments) {
            if (recFile.empty() && e.second.empty() && e.first != PROGRAM) {
                recFile = e.first;
                break;
            }
        }

        std::fstream fin(recFile, std::ios::in|std::ios::binary);
        if (fin.good()) {
            std::atomic<bool> playCommandUpdate{false};
            std::mutex playerCommandMutex;
            cluon::data::PlayerCommand playerCommand;

            // Create an OD4Session to relay the.
            std::unique_ptr<cluon::OD4Session> od4;
            if (0 != commandlineArguments.count("cid")) {
                // Interface to a running OpenDaVINCI session and listening for PlayerCommands.
                od4 = std::make_unique<cluon::OD4Session>(static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))); // LCOV_EXCL_LINE
                if (od4) {
                    od4->dataTrigger(cluon::data::PlayerCommand::ID(), [&playCommandUpdate, &playerCommandMutex, &playerCommand](cluon::data::Envelope &&env){
                        cluon::data::PlayerCommand pc = cluon::extractMessage<cluon::data::PlayerCommand>(std::move(env));
                        {
                            std::lock_guard<std::mutex> lck(playerCommandMutex);
                            playerCommand = pc;
                        }
                        playCommandUpdate = true;
                    });
                }
            }

            // Listen for PlayerStatus updates.
            std::atomic<bool> playerStatusUpdate{false};
            std::mutex playerStatusMutex;
            cluon::data::PlayerStatus playerStatus;
            {
                std::string s;
                playerStatus.state(1); // Report: "loading file"
                {
                    std::lock_guard<std::mutex> lck(playerStatusMutex);

                    cluon::ToProtoVisitor protoEncoder;
                    playerStatus.accept(protoEncoder);
                    s = protoEncoder.encodedData();
                }
                cluon::data::Envelope env;
                env.dataType(playerStatus.ID())
                   .sent(cluon::time::now())
                   .sampleTimeStamp(cluon::time::now())
                   .serializedData(s);

                if (od4 && od4->isRunning()) {
                    od4->send(std::move(env));
                }
                else {
                    std::cout << cluon::serializeEnvelope(std::move(env));
                    std::cout.flush();
                }
            }
            constexpr bool AUTOREWIND{false};
            constexpr bool THREADING{true};
            cluon::Player player(recFile, AUTOREWIND, THREADING);
            player.setPlayerListener([&playerStatusUpdate, &playerStatusMutex, &playerStatus](cluon::data::PlayerStatus &&ps){
                {
                    std::lock_guard<std::mutex> lck(playerStatusMutex);
                    playerStatus = ps;
                }
                playerStatusUpdate = true;
            });

            {
                std::string s;
                playerStatus.numberOfEntries(player.totalNumberOfEnvelopesInRecFile());
                playerStatus.state(2); // playback file
                {
                    std::lock_guard<std::mutex> lck(playerStatusMutex);

                    cluon::ToProtoVisitor protoEncoder;
                    playerStatus.accept(protoEncoder);
                    s = protoEncoder.encodedData();
                }
                cluon::data::Envelope env;
                env.dataType(playerStatus.ID())
                   .sent(cluon::time::now())
                   .sampleTimeStamp(cluon::time::now())
                   .serializedData(s);

                if (od4 && od4->isRunning()) {
                    od4->send(std::move(env));
                }
                else {
                    std::cout << cluon::serializeEnvelope(std::move(env));
                    std::cout.flush();
                }
            }

            bool play = true;
            bool step = false;
            while ( (player.hasMoreData() || keepRunning) ) {
                // Stop execution in case of a running OD4Session.
                if (od4 && !od4->isRunning()) {
                    break;
                }
                // If we are at the end of the playback file, simply wait a little to avoid excessive system load.
                if (!player.hasMoreData() && keepRunning) {
                    std::this_thread::sleep_for(std::chrono::duration<int32_t, std::milli>(200)); // LCOV_EXCL_LINE
                }
                // Check for broadcasting status updates.
                if (playerStatusUpdate) {
                    std::string s;
                    {
                        std::lock_guard<std::mutex> lck(playerStatusMutex);

                        cluon::ToProtoVisitor protoEncoder;
                        playerStatus.accept(protoEncoder);
                        s = protoEncoder.encodedData();
                    }
                    cluon::data::Envelope env;
                    env.dataType(playerStatus.ID())
                       .sent(cluon::time::now())
                       .sampleTimeStamp(cluon::time::now())
                       .serializedData(s);

                    if (od4 && od4->isRunning()) {
                        cluon::data::Envelope e = env;
                        od4->send(std::move(e));
                    }
                    if (playBackToStdout) {
                        cluon::data::Envelope e = env;
                        std::cout << cluon::serializeEnvelope(std::move(e));
                        std::cout.flush();
                    }
                    playerStatusUpdate = false;
                }
                // Check for remotely controlling the player.
                if (playCommandUpdate) {
                    std::lock_guard<std::mutex> lck(playerCommandMutex);
                    if ( (playerCommand.command() == 1) || (playerCommand.command() == 2) ) {
                        play = !(2 == playerCommand.command()); // LCOV_EXCL_LINE
                        std::cerr << PROGRAM << ": Change state: " << +playerCommand.command() << ", play = " << play << std::endl;
                    }

                    if (3 == playerCommand.command()) {
                        std::cerr << PROGRAM << ": Change state: " << +playerCommand.command() << ", seekTo: " << playerCommand.seekTo() << std::endl;
                        player.seekTo(playerCommand.seekTo());
                    }

                    if (4 == playerCommand.command()) {
                        play = false;
                        step = true;
                        std::cerr << PROGRAM << ": Change state: " << +playerCommand.command() << ", play = " << play << std::endl;
                    }

                    playCommandUpdate = false;
                }
                // If playback is desired, relay the Envelope to the OD4Session.
                if (play || step) {
                    auto next = player.getNextEnvelopeToBeReplayed();
                    if (next.first) {
                        if (od4 && od4->isRunning()) {
                            cluon::data::Envelope e = next.second;
                            od4->send(std::move(e));
                        }
                        if (playBackToStdout) {
                            cluon::data::Envelope e = next.second;
                            std::cout << cluon::serializeEnvelope(std::move(e));
                            std::cout.flush();
                        }
                        std::this_thread::sleep_for(std::chrono::duration<int32_t, std::micro>(player.delay()));
                    }
                }
                else {
                    std::this_thread::sleep_for(std::chrono::duration<int32_t, std::milli>(100)); // LCOV_EXCL_LINE
                } // LCOV_EXCL_LINE

                // Reset step.
                step = false;
            }
            retCode = 0;
        }
        else {
            std::cerr << PROGRAM << ": file '" << recFile << "' not found." << std::endl;
            retCode = 1;
        }
    }
    return retCode;
}

#endif

/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// This test for a compiler definition is necessary to preserve single-file, header-only compability.
#ifndef HAVE_CLUON_REPLAY
#include "cluon-replay.hpp"
#endif

#include <cstdint>

int32_t main(int32_t argc, char **argv) {
    return cluon_replay(argc, argv);
}
#endif
#ifdef HAVE_CLUON_LIVEFEED
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_LIVEFEED_HPP
#define CLUON_LIVEFEED_HPP

//#include "cluon/cluon.hpp"
//#include "cluon/MetaMessage.hpp"
//#include "cluon/MessageParser.hpp"
//#include "cluon/OD4Session.hpp"
//#include "cluon/TerminateHandler.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

enum Color {
    RED     = 31,
    GREEN   = 32,
    YELLOW  = 33,
    DEFAULT = 39,
};

inline void clearScreen() {
    std::cout << "\033[2J" << std::endl;
}

inline void writeText(Color c, uint8_t y, uint8_t x, const std::string &text) {
    std::cout << "\033[" << +y << ";" << +x << "H" << "\033[0;" << +c << "m" << text << "\033[0m" << std::endl;
}

inline std::string formatTimeStamp(const cluon::data::TimeStamp &ts) {
    std::time_t temp = static_cast<std::time_t>(ts.seconds());
    std::tm* t = std::gmtime(&temp);
    std::stringstream sstr;
    sstr << std::put_time(t, "%H:%M:%S") << "." << std::setfill('0') << std::setw(6) << ts.microseconds() << std::setw(0);
    const std::string str{sstr.str()};
    return str;
}

inline int32_t cluon_livefeed(int32_t argc, char **argv) {
    int retVal{1};
    const std::string PROGRAM{argv[0]}; // NOLINT
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if (0 == commandlineArguments.count("cid")) {
        std::cerr << PROGRAM
                  << " displays any Envelopes received from an OpenDaVINCI v4 session to stdout with optional data type resolving using a .odvd message specification." << std::endl;
        std::cerr << "Usage:    " << PROGRAM << " [--odvd=<ODVD message specification file>] --cid=<OpenDaVINCI session>" << std::endl;
        std::cerr << "Examples: " << PROGRAM << " --cid=111" << std::endl;
        std::cerr << "          " << PROGRAM << " --odvd=MyMessages.odvd --cid=111" << std::endl;
    } else {
        std::map<int32_t, cluon::MetaMessage> scopeOfMetaMessages{};

        // Try parsing a supplied .odvd file to resolve numerical data types to human readable message names.
        {
            std::string odvdFile{commandlineArguments["odvd"]};
            if (!odvdFile.empty()) {
                std::fstream fin{odvdFile, std::ios::in};
                if (fin.good()) {
                    const std::string s{static_cast<std::stringstream const&>(std::stringstream() << fin.rdbuf()).str()}; // NOLINT

                    cluon::MessageParser mp;
                    auto parsingResult = mp.parse(s);
                    if (!parsingResult.first.empty()) {
                        for (const auto &mm : parsingResult.first) { scopeOfMetaMessages[mm.messageIdentifier()] = mm; }
                        std::clog << "Parsed " << parsingResult.first.size() << " message(s)." << std::endl;
                    }
                }
            }
        }

        std::mutex mapOfLastEnvelopesMutex;
        std::unordered_map<int32_t, std::unordered_map<uint32_t, cluon::data::Envelope, cluon::UseUInt32ValueAsHashKey>, cluon::UseUInt32ValueAsHashKey> mapOfLastEnvelopes;
        std::unordered_map<int32_t, std::unordered_map<uint32_t, float>, cluon::UseUInt32ValueAsHashKey> mapOfUpdateRates;

        cluon::OD4Session od4Session(static_cast<uint16_t>(std::stoi(commandlineArguments["cid"])),
            [&mapOfLastEnvelopesMutex, &mapOfLastEnvelopes, &mapOfUpdateRates](cluon::data::Envelope &&envelope) noexcept {
            std::lock_guard<std::mutex> lck(mapOfLastEnvelopesMutex);

            int64_t lastTimeStamp{0};
            int64_t currentTimeStamp{0};
            {
                // Update mapping for tupel (dataType, senderStamp) --> Envelope.
                auto entry = mapOfLastEnvelopes[envelope.dataType()];
                if (0 != entry.count(envelope.senderStamp())) {
                    lastTimeStamp = cluon::time::toMicroseconds(entry[envelope.senderStamp()].sampleTimeStamp()); // LCOV_EXCL_LINE
                }
                currentTimeStamp = cluon::time::toMicroseconds(envelope.sampleTimeStamp()); // LCOV_EXCL_LINE
                if (currentTimeStamp != lastTimeStamp) {
                    entry[envelope.senderStamp()] = envelope; // LCOV_EXCL_LINE
                    mapOfLastEnvelopes[envelope.dataType()] = entry; // LCOV_EXCL_LINE
                }
            }
            if (currentTimeStamp != lastTimeStamp) {
                // Update mapping for tupel (dataType, senderStamp) --> deltaToLastEnvelope.
                auto entry = mapOfUpdateRates[envelope.dataType()];

                float average{0};
                if (0 != entry.count(envelope.senderStamp())) {
                    average = entry[envelope.senderStamp()]; // LCOV_EXCL_LINE
                    float freq = (static_cast<float>(currentTimeStamp - lastTimeStamp))/(1000.0f*1000.0f); // LCOV_EXCL_LINE
                    average = (1.0f/freq)*0.1f + 0.9f*average; // LCOV_EXCL_LINE
                }
                entry[envelope.senderStamp()] = average;
                mapOfUpdateRates[envelope.dataType()] = entry;
            }
        });

        if (od4Session.isRunning()) {
            od4Session.timeTrigger(5, [&mapOfLastEnvelopesMutex, &mapOfLastEnvelopes, &mapOfUpdateRates, &scopeOfMetaMessages, &od4Session](){
                std::lock_guard<std::mutex> lck(mapOfLastEnvelopesMutex);

                if (!mapOfLastEnvelopes.empty()) {
                    clearScreen();

                    uint8_t y = 1;
                    const uint8_t x = 1;
                    for (auto e : mapOfLastEnvelopes) {
                        for (auto ee : e.second) {
                            auto env = ee.second;
                            std::stringstream sstr;

                            float freq{0};
                            if ( (0 < mapOfUpdateRates.count(ee.second.dataType())) && (0 < mapOfUpdateRates[ee.second.dataType()].count(ee.second.senderStamp())) ) {
                                freq = mapOfUpdateRates[ee.second.dataType()][ee.second.senderStamp()];
                            }

                            sstr << "Envelope: " << std::setfill(' ') << std::setw(5) << env.dataType() << std::setw(0) << "/" << env.senderStamp() << "; " << (static_cast<float>(static_cast<uint32_t>(freq*100.0f))/100.f) << " Hz; " << "sent: " << formatTimeStamp(env.sent()) << "; sample: " << formatTimeStamp(env.sampleTimeStamp());
                            if (scopeOfMetaMessages.count(env.dataType()) > 0) {
                                sstr << "; " << scopeOfMetaMessages[env.dataType()].messageName();
                            }
                            else {
                                sstr << "; unknown data type";
                            }
                            sstr << std::endl;

                            const auto AGE{cluon::time::deltaInMicroseconds(cluon::time::now(), env.received())};

                            Color c = Color::DEFAULT;
                            if (AGE <= 2 * 1000 * 1000) { c = Color::GREEN; }
                            if (AGE > 2 * 1000 * 1000 && AGE <= 5 * 1000 * 1000) { c = Color::YELLOW; }
                            if (AGE > 5 * 1000 * 1000) { c = Color::RED; }

                            writeText(c, y++, x, sstr.str());
                        }
                    }
                }
                return od4Session.isRunning();
            });

            retVal = 0;
        }
    }
    return retVal;
}

#endif

/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// This test for a compiler definition is necessary to preserve single-file, header-only compability.
#ifndef HAVE_CLUON_LIVEFEED
#include "cluon-livefeed.hpp"
#endif

#include <cstdint>

int32_t main(int32_t argc, char **argv) {
    return cluon_livefeed(argc, argv);
}
#endif
#ifdef HAVE_CLUON_REC2CSV
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CLUON_REC2CSV_HPP
#define CLUON_REC2CSV_HPP

//#include "cluon/cluon.hpp"
//#include "cluon/GenericMessage.hpp"
//#include "cluon/MessageParser.hpp"
//#include "cluon/MetaMessage.hpp"
//#include "cluon/Player.hpp"
//#include "cluon/ToCSVVisitor.hpp"
//#include "cluon/stringtoolbox.hpp"
//#include "cluon/cluonDataStructures.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>

inline int32_t cluon_rec2csv(int32_t argc, char **argv) {
    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("rec")) || (0 == commandlineArguments.count("odvd")) ) {
        std::cerr << argv[0] << " extracts the content from a given .rec file using a provided .odvd message specification into separate .csv files." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --rec=<Recording from an OD4Session> --odvd=<ODVD Message Specification>" << std::endl;
        std::cerr << "Example: " << argv[0] << " --rec=myRecording.rec --odvd=myMessages.odvd" << std::endl;
        retCode = 1;
    } else {
        // Maps of container-ID & sender-stamp.
        std::map<std::string, std::string> mapOfFilenames;
        std::map<std::string, std::string> mapOfEntries;
        std::map<std::string, size_t> mapOfEntriesSizes;
        std::map<std::string, bool> mapOfFilenamesThatHaveBeenReset;

        cluon::MessageParser mp;
        std::pair<std::vector<cluon::MetaMessage>, cluon::MessageParser::MessageParserErrorCodes> messageParserResult;
        {
            std::ifstream fin(commandlineArguments["odvd"], std::ios::in|std::ios::binary);
            if (fin.good()) {
                std::string input(static_cast<std::stringstream const&>(std::stringstream() << fin.rdbuf()).str()); // NOLINT
                fin.close();
                messageParserResult = mp.parse(input);
                std::clog << "Found " << messageParserResult.first.size() << " messages." << std::endl;
            }
            else {
                std::cerr << argv[0] << ": Message specification '" << commandlineArguments["odvd"] << "' not found." << std::endl;
                return retCode = 1;
            }
        }

        std::fstream fin(commandlineArguments["rec"], std::ios::in|std::ios::binary);
        if (fin.good()) {
            fin.close();

            auto fileWriter = [argv, &mapOfFilenames, &mapOfEntries, &mapOfEntriesSizes, &mapOfFilenamesThatHaveBeenReset](){
              for(auto entries : mapOfFilenames) {
                  std::cerr << argv[0] << " writing '" << entries.second << ".csv'...";
                  // Reset files on first access.
                  std::ios_base::openmode openMode = std::ios::out|std::ios::binary|(mapOfFilenamesThatHaveBeenReset.count(entries.second) == 0 ? std::ios::trunc : std::ios::app);
                  std::fstream fout(entries.second + ".csv", openMode);
                  if (fout.good() && mapOfEntries.count(entries.first)) {
                      const std::string tmp{mapOfEntries[entries.first]};
                      fout.write(tmp.c_str(), static_cast<std::streamsize>(tmp.size()));
                      // Reset memory.
                      mapOfEntries[entries.first] = "";
                      mapOfEntriesSizes[entries.first] = 0;
                  }
                  fout.close();
                  mapOfFilenamesThatHaveBeenReset[entries.second] = true;
                  std::cerr << " done." << std::endl;
              }
            };

            std::map<int32_t, cluon::MetaMessage> scope;
            for (const auto &e : messageParserResult.first) { scope[e.messageIdentifier()] = e; }

            constexpr const bool AUTOREWIND{false};
            constexpr const bool THREADING{false};
            cluon::Player player(commandlineArguments["rec"], AUTOREWIND, THREADING);

            constexpr const size_t TEN_MB{10*1024*1024};
            uint32_t envelopeCounter{0};
            int32_t oldPercentage = -1;
            while (player.hasMoreData()) {
                auto next = player.getNextEnvelopeToBeReplayed();
                if (next.first) {
                    {
                        envelopeCounter++;
                        const int32_t percentage = static_cast<int32_t>((static_cast<float>(envelopeCounter)*100.0f)/static_cast<float>(player.totalNumberOfEnvelopesInRecFile()));
                        if ( (percentage % 5 == 0) && (percentage != oldPercentage) ) {
                            std::cerr << argv[0] << ": Processed " << percentage << "%." << std::endl;
                            oldPercentage = percentage;
                        }
                    }
                    cluon::data::Envelope env{std::move(next.second)};
                    if (scope.count(env.dataType()) > 0) {
                        cluon::FromProtoVisitor protoDecoder;
                        std::stringstream sstr(env.serializedData());
                        protoDecoder.decodeFrom(sstr);

                        cluon::MetaMessage m = scope[env.dataType()];
                        cluon::GenericMessage gm;
                        gm.createFrom(m, messageParserResult.first);
                        gm.accept(protoDecoder);

                        std::stringstream sstrKey;
                        sstrKey << env.dataType() << "/" << env.senderStamp();
                        const std::string KEY = sstrKey.str();

                        std::stringstream sstrFilename;
                        sstrFilename << m.messageName() << "-" << env.senderStamp();
                        const std::string __FILENAME = sstrFilename.str();
                        mapOfFilenames[KEY] = __FILENAME;

                        if (mapOfEntries.count(KEY) > 0) {
                            // Extract timestamps.
                            std::string timeStamps;
                            {
                                cluon::ToCSVVisitor csv(';', false, { {1,false}, {2,false}, {3,true}, {4,true}, {5,true}, {6,false} });
                                env.accept(csv);
                                timeStamps = csv.csv();
                            }

                            cluon::ToCSVVisitor csv(';', false);
                            gm.accept(csv);

                            std::string entry{stringtoolbox::split(timeStamps, '\n')[0] + csv.csv()};
                            mapOfEntries[KEY] += entry;
                            mapOfEntriesSizes[KEY] += entry.size();
                       }
                        else {
                            // Extract timestamps.
                            std::vector<std::string> timeStampsWithHeader;
                            {
                                // Skip senderStamp (as it is in file name) and serializedData.
                                cluon::ToCSVVisitor csv(';', true, { {1,false}, {2,false}, {3,true}, {4,true}, {5,true}, {6,false} });
                                env.accept(csv);
                                timeStampsWithHeader = stringtoolbox::split(csv.csv(), '\n');
                            }

                            cluon::ToCSVVisitor csv(';', true);
                            gm.accept(csv);

                            std::vector<std::string> valuesWithHeader = stringtoolbox::split(csv.csv(), '\n');
                            std::string entry{timeStampsWithHeader.at(0) + valuesWithHeader.at(0) + '\n' + timeStampsWithHeader.at(1) + valuesWithHeader.at(1) + '\n'};
                            mapOfEntries[KEY] += entry;
                            mapOfEntriesSizes[KEY] += entry.size();
                        }

                        // Keep track of buffer sizes.
                        if (mapOfEntriesSizes[KEY] > TEN_MB) {
                            std::cerr << argv[0] << ": Buffer for '" << KEY << "' has consumed " << mapOfEntriesSizes[KEY] << "/" << TEN_MB << " bytes; dumping data to disk."<< std::endl; // LCOV_EXCL_LINE
                            fileWriter(); // LCOV_EXCL_LINE
                        }
                    }
                }
            }
            // Clear buffer at the end.
            fileWriter();
        }
        else {
            std::cerr << argv[0] << ": Recording '" << commandlineArguments["rec"] << "' not found." << std::endl;
            retCode = 1;
        }
    }
    return retCode;
}

#endif
/*
 * Copyright (C) 2017-2018  Christian Berger
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// This test for a compiler definition is necessary to preserve single-file, header-only compability.
#ifndef HAVE_CLUON_REC2CSV
#include "cluon-rec2csv.hpp"
#endif

#include <cstdint>

int32_t main(int32_t argc, char **argv) {
    return cluon_rec2csv(argc, argv);
}
#endif
