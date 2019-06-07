// Copyright David Abrahams, Daniel Wallin 2003.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PARAMETER_AUX_PACK_PREDICATE_HPP
#define BOOST_PARAMETER_AUX_PACK_PREDICATE_HPP

namespace lslboost { namespace parameter { namespace aux {

    // helper for get_predicate<...>, below
    template <typename T>
    struct get_predicate_or_default
    {
        typedef T type;
    };

    // helper for predicate<...>, below
    template <typename T>
    struct get_predicate
      : ::lslboost::parameter::aux
        ::get_predicate_or_default<typename T::predicate>
    {
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/use_default.hpp>
#include <boost/parameter/aux_/always_true_predicate.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <>
    struct get_predicate_or_default< ::lslboost::parameter::aux::use_default>
    {
        typedef ::lslboost::parameter::aux::always_true_predicate type;
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/required.hpp>
#include <boost/parameter/optional.hpp>
#include <boost/parameter/config.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/integral.hpp>
#include <boost/mp11/utility.hpp>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/identity.hpp>
#endif

namespace lslboost { namespace parameter { namespace aux {

    template <typename T>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    using predicate = ::lslboost::mp11::mp_if<
        ::lslboost::mp11::mp_if<
            ::lslboost::parameter::aux::is_optional<T>
          , ::lslboost::mp11::mp_true
          , ::lslboost::parameter::aux::is_required<T>
        >
      , ::lslboost::parameter::aux::get_predicate<T>
      , ::lslboost::mp11::mp_identity<
            ::lslboost::parameter::aux::always_true_predicate
        >
    >;
#else
    struct predicate
      : ::lslboost::mpl::eval_if<
            typename ::lslboost::mpl::if_<
                ::lslboost::parameter::aux::is_optional<T>
              , ::lslboost::mpl::true_
              , ::lslboost::parameter::aux::is_required<T>
            >::type
          , ::lslboost::parameter::aux::get_predicate<T>
          , ::lslboost::mpl::identity<
                ::lslboost::parameter::aux::always_true_predicate
            >
        >
    {
    };
#endif  // BOOST_PARAMETER_CAN_USE_MP11
}}} // namespace lslboost::parameter::aux

#endif  // include guard

