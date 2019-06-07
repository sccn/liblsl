#ifndef BOOST_ENDIAN_DETAIL_IS_TRIVIALLY_COPYABLE_HPP_INCLUDED
#define BOOST_ENDIAN_DETAIL_IS_TRIVIALLY_COPYABLE_HPP_INCLUDED

// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>
#include <boost/type_traits/has_trivial_copy.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#if !defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)
# include <type_traits>
#endif

namespace lslboost
{
namespace endian
{
namespace detail
{

#if !defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)

using std::is_trivially_copyable;

#else

template<class T> struct is_trivially_copyable: lslboost::integral_constant<bool,
    lslboost::has_trivial_copy<T>::value && lslboost::has_trivial_assign<T>::value && lslboost::has_trivial_destructor<T>::value> {};

#endif

} // namespace detail
} // namespace endian
} // namespace lslboost

#endif  // BOOST_ENDIAN_DETAIL_IS_TRIVIALLY_COPYABLE_HPP_INCLUDED
