//
// detail/type_traits.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2020 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_TYPE_TRAITS_HPP
#define BOOST_ASIO_DETAIL_TYPE_TRAITS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>

#if defined(BOOST_ASIO_HAS_STD_TYPE_TRAITS)
# include <type_traits>
#else // defined(BOOST_ASIO_HAS_STD_TYPE_TRAITS)
# include <boost/type_traits/add_const.hpp>
# include <boost/type_traits/add_lvalue_reference.hpp>
# include <boost/type_traits/aligned_storage.hpp>
# include <boost/type_traits/alignment_of.hpp>
# include <boost/type_traits/conditional.hpp>
# include <boost/type_traits/decay.hpp>
# include <boost/type_traits/has_nothrow_copy.hpp>
# include <boost/type_traits/has_nothrow_destructor.hpp>
# include <boost/type_traits/integral_constant.hpp>
# include <boost/type_traits/is_base_of.hpp>
# include <boost/type_traits/is_class.hpp>
# include <boost/type_traits/is_const.hpp>
# include <boost/type_traits/is_convertible.hpp>
# include <boost/type_traits/is_constructible.hpp>
# include <boost/type_traits/is_copy_constructible.hpp>
# include <boost/type_traits/is_destructible.hpp>
# include <boost/type_traits/is_function.hpp>
# include <boost/type_traits/is_object.hpp>
# include <boost/type_traits/is_same.hpp>
# include <boost/type_traits/remove_cv.hpp>
# include <boost/type_traits/remove_pointer.hpp>
# include <boost/type_traits/remove_reference.hpp>
# include <boost/utility/declval.hpp>
# include <boost/utility/enable_if.hpp>
# include <boost/utility/result_of.hpp>
#endif // defined(BOOST_ASIO_HAS_STD_TYPE_TRAITS)

namespace lslboost {
namespace asio {

#if defined(BOOST_ASIO_HAS_STD_TYPE_TRAITS)
using std::add_const;
using std::add_lvalue_reference;
using std::aligned_storage;
using std::alignment_of;
using std::conditional;
using std::decay;
using std::declval;
using std::enable_if;
using std::false_type;
using std::integral_constant;
using std::is_base_of;
using std::is_class;
using std::is_const;
using std::is_constructible;
using std::is_convertible;
using std::is_copy_constructible;
using std::is_destructible;
using std::is_function;
using std::is_move_constructible;
using std::is_nothrow_copy_constructible;
using std::is_nothrow_destructible;
using std::is_object;
using std::is_reference;
using std::is_same;
using std::is_scalar;
using std::remove_cv;
template <typename T>
struct remove_cvref : remove_cv<typename std::remove_reference<T>::type> {};
using std::remove_pointer;
using std::remove_reference;
#if defined(BOOST_ASIO_HAS_STD_INVOKE_RESULT)
template <typename> struct result_of;
template <typename F, typename... Args>
struct result_of<F(Args...)> : std::invoke_result<F, Args...> {};
#else // defined(BOOST_ASIO_HAS_STD_INVOKE_RESULT)
using std::result_of;
#endif // defined(BOOST_ASIO_HAS_STD_INVOKE_RESULT)
using std::true_type;
#else // defined(BOOST_ASIO_HAS_STD_TYPE_TRAITS)
using lslboost::add_const;
using lslboost::add_lvalue_reference;
using lslboost::aligned_storage;
using lslboost::alignment_of;
template <bool Condition, typename Type = void>
struct enable_if : lslboost::enable_if_c<Condition, Type> {};
using lslboost::conditional;
using lslboost::decay;
using lslboost::declval;
using lslboost::false_type;
using lslboost::integral_constant;
using lslboost::is_base_of;
using lslboost::is_class;
using lslboost::is_const;
using lslboost::is_constructible;
using lslboost::is_convertible;
using lslboost::is_copy_constructible;
using lslboost::is_destructible;
using lslboost::is_function;
#if defined(BOOST_ASIO_HAS_MOVE)
template <typename T>
struct is_move_constructible : false_type {};
#else // defined(BOOST_ASIO_HAS_MOVE)
template <typename T>
struct is_move_constructible : is_copy_constructible<T> {};
#endif // defined(BOOST_ASIO_HAS_MOVE)
template <typename T>
struct is_nothrow_copy_constructible : lslboost::has_nothrow_copy<T> {};
template <typename T>
struct is_nothrow_destructible : lslboost::has_nothrow_destructor<T> {};
using lslboost::is_object;
using lslboost::is_reference;
using lslboost::is_same;
using lslboost::is_scalar;
using lslboost::remove_cv;
template <typename T>
struct remove_cvref : remove_cv<typename lslboost::remove_reference<T>::type> {};
using lslboost::remove_pointer;
using lslboost::remove_reference;
using lslboost::result_of;
using lslboost::true_type;
#endif // defined(BOOST_ASIO_HAS_STD_TYPE_TRAITS)

template <typename> struct void_type { typedef void type; };

#if defined(BOOST_ASIO_HAS_VARIADIC_TEMPLATES)

template <typename...> struct conjunction : true_type {};
template <typename T> struct conjunction<T> : T {};
template <typename Head, typename... Tail> struct conjunction<Head, Tail...> :
  conditional<Head::value, conjunction<Tail...>, Head>::type {};

#endif // defined(BOOST_ASIO_HAS_VARIADIC_TEMPLATES)

} // namespace asio
} // namespace lslboost

#endif // BOOST_ASIO_DETAIL_TYPE_TRAITS_HPP
