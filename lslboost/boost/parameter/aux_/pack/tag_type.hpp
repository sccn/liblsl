// Copyright David Abrahams, Daniel Wallin 2003.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PARAMETER_AUX_PACK_TAG_TYPE_HPP
#define BOOST_PARAMETER_AUX_PACK_TAG_TYPE_HPP

namespace lslboost { namespace parameter { namespace aux {

    // helper for tag_type<...>, below.
    template <typename T>
    struct get_tag_type0
    {
        typedef typename T::key_type type;
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/deduced.hpp>
#include <boost/parameter/config.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/utility.hpp>
#else
#include <boost/mpl/eval_if.hpp>
#endif

namespace lslboost { namespace parameter { namespace aux {

    template <typename T>
    struct get_tag_type
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
      : ::lslboost::mp11::mp_if<
#else
      : ::lslboost::mpl::eval_if<
#endif
            ::lslboost::parameter::aux::is_deduced0<T>
          , ::lslboost::parameter::aux::get_tag_type0<typename T::key_type>
          , ::lslboost::parameter::aux::get_tag_type0<T>
        >
    {
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/required.hpp>
#include <boost/parameter/optional.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/integral.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename T>
    using tag_type = ::lslboost::mp11::mp_if<
        ::lslboost::mp11::mp_if<
            ::lslboost::parameter::aux::is_optional<T>
          , ::lslboost::mp11::mp_true
          , ::lslboost::parameter::aux::is_required<T>
        >
      , ::lslboost::parameter::aux::get_tag_type<T>
      , ::lslboost::mp11::mp_identity<T>
    >;
}}} // namespace lslboost::parameter::aux

#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/identity.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename T>
    struct tag_type
      : ::lslboost::mpl::eval_if<
            typename ::lslboost::mpl::if_<
                ::lslboost::parameter::aux::is_optional<T>
              , ::lslboost::mpl::true_
              , ::lslboost::parameter::aux::is_required<T>
            >::type
          , ::lslboost::parameter::aux::get_tag_type<T>
          , ::lslboost::mpl::identity<T>
        >
    {
    };
}}} // namespace lslboost::parameter::aux

#endif  // BOOST_PARAMETER_CAN_USE_MP11
#endif  // include guard

