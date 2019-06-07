// Copyright David Abrahams 2005.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PARAMETER_BINDING_DWA200558_HPP
#define BOOST_PARAMETER_BINDING_DWA200558_HPP

#include <boost/parameter/aux_/void.hpp>
#include <boost/parameter/config.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/integral.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/identity.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_same.hpp>
#endif

namespace lslboost { namespace parameter { 

    // A metafunction that, given an argument pack, returns the reference type
    // of the parameter identified by the given keyword.  If no such parameter
    // has been specified, returns Default

    template <typename Parameters, typename Keyword, typename Default>
    struct binding0
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        using type = ::lslboost::mp11::mp_apply_q<
            typename Parameters::binding
          , ::lslboost::mp11::mp_list<Keyword,Default,::lslboost::mp11::mp_true>
        >;

        static_assert(
            ::lslboost::mp11::mp_if<
                ::std::is_same<Default,::lslboost::parameter::void_>
              , ::lslboost::mp11::mp_if<
                    ::std::is_same<type,::lslboost::parameter::void_>
                  , ::lslboost::mp11::mp_false
                  , ::lslboost::mp11::mp_true
                >
              , ::lslboost::mp11::mp_true
            >::value
          , "required parameters must not result in void_ type"
        );
#else   // !defined(BOOST_PARAMETER_CAN_USE_MP11)
        typedef typename ::lslboost::mpl::apply_wrap3<
            typename Parameters::binding
          , Keyword
          , Default
          , ::lslboost::mpl::true_
        >::type type;

        BOOST_MPL_ASSERT((
            typename ::lslboost::mpl::eval_if<
                ::lslboost::is_same<Default,::lslboost::parameter::void_>
              , ::lslboost::mpl::if_<
                    ::lslboost::is_same<type,::lslboost::parameter::void_>
                  , ::lslboost::mpl::false_
                  , ::lslboost::mpl::true_
                >
              , ::lslboost::mpl::true_
            >::type
        ));
#endif  // BOOST_PARAMETER_CAN_USE_MP11
    };

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    template <typename Placeholder, typename Keyword, typename Default>
    struct binding1
    {
        using type = ::lslboost::mp11::mp_apply_q<
            Placeholder
          , ::lslboost::mp11::mp_list<Keyword,Default,::lslboost::mp11::mp_true>
        >;

        static_assert(
            ::lslboost::mp11::mp_if<
                ::std::is_same<Default,::lslboost::parameter::void_>
              , ::lslboost::mp11::mp_if<
                    ::std::is_same<type,::lslboost::parameter::void_>
                  , ::lslboost::mp11::mp_false
                  , ::lslboost::mp11::mp_true
                >
              , ::lslboost::mp11::mp_true
            >::value
          , "required parameters must not result in void_ type"
        );
    };
#endif  // BOOST_PARAMETER_CAN_USE_MP11
}} // namespace lslboost::parameter

#include <boost/parameter/aux_/is_placeholder.hpp>

namespace lslboost { namespace parameter { 

    template <
        typename Parameters
      , typename Keyword
      , typename Default = ::lslboost::parameter::void_
    >
    struct binding
#if !defined(BOOST_PARAMETER_CAN_USE_MP11)
      : ::lslboost::mpl::eval_if<
            ::lslboost::parameter::aux::is_mpl_placeholder<Parameters>
          , ::lslboost::mpl::identity<int>
          , ::lslboost::parameter::binding0<Parameters,Keyword,Default>
        >
#endif
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        using type = typename ::lslboost::mp11::mp_if<
            ::lslboost::parameter::aux::is_mpl_placeholder<Parameters>
          , ::lslboost::mp11::mp_identity<int>
          , ::lslboost::mp11::mp_if<
                ::lslboost::parameter::aux::is_mp11_placeholder<Parameters>
              , ::lslboost::parameter::binding1<Parameters,Keyword,Default>
              , ::lslboost::parameter::binding0<Parameters,Keyword,Default>
            >
        >::type;
#endif
    };
}} // namespace lslboost::parameter

#include <boost/parameter/aux_/result_of0.hpp>

namespace lslboost { namespace parameter { 

    // A metafunction that, given an argument pack, returns the reference type
    // of the parameter identified by the given keyword.  If no such parameter
    // has been specified, returns the type returned by invoking DefaultFn
    template <typename Parameters, typename Keyword, typename DefaultFn>
    struct lazy_binding
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        using type = ::lslboost::mp11::mp_apply_q<
            typename Parameters::binding
          , ::lslboost::mp11::mp_list<
                Keyword
              , typename ::lslboost::parameter::aux::result_of0<DefaultFn>::type
              , ::lslboost::mp11::mp_true
            >
        >;
#else
        typedef typename ::lslboost::mpl::apply_wrap3<
            typename Parameters::binding
          , Keyword
          , typename ::lslboost::parameter::aux::result_of0<DefaultFn>::type
          , ::lslboost::mpl::true_
        >::type type;
#endif  // BOOST_PARAMETER_CAN_USE_MP11
    };
}} // namespace lslboost::parameter

#endif  // include guard

