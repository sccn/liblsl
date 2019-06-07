// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PARAMETER_AUX_PACK_MAKE_PARAMETER_SPEC_ITEMS_HPP
#define BOOST_PARAMETER_AUX_PACK_MAKE_PARAMETER_SPEC_ITEMS_HPP

namespace lslboost { namespace parameter { namespace aux {

    // This recursive metafunction forwards successive elements of
    // parameters::parameter_spec to make_deduced_items<>.
    // -- Cromwell D. Enage
    template <typename SpecSeq>
    struct make_deduced_list;

    // Helper for match_parameters_base_cond<...>, below.
    template <typename ArgumentPackAndError, typename SpecSeq>
    struct match_parameters_base_cond_helper;

    // Helper metafunction for make_parameter_spec_items<...>, below.
    template <typename SpecSeq, typename ...Args>
    struct make_parameter_spec_items_helper;
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/void.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename SpecSeq>
    struct make_parameter_spec_items_helper<SpecSeq>
    {
        typedef ::lslboost::parameter::void_ type;
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/pack/make_deduced_items.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/list.hpp>
#else
#include <boost/mpl/front.hpp>
#include <boost/mpl/pop_front.hpp>
#endif

namespace lslboost { namespace parameter { namespace aux {

    template <typename SpecSeq>
    struct make_deduced_list_not_empty
      : ::lslboost::parameter::aux::make_deduced_items<
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            ::lslboost::mp11::mp_front<SpecSeq>
#else
            typename ::lslboost::mpl::front<SpecSeq>::type
#endif
          , ::lslboost::parameter::aux::make_deduced_list<
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
                ::lslboost::mp11::mp_pop_front<SpecSeq>
#else
                typename ::lslboost::mpl::pop_front<SpecSeq>::type
#endif
            >
        >
    {
    };
}}} // namespace lslboost::parameter::aux

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/utility.hpp>
#else
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/empty.hpp>
#include <boost/mpl/identity.hpp>
#endif

namespace lslboost { namespace parameter { namespace aux {

    template <typename SpecSeq>
    struct make_deduced_list
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
      : ::lslboost::mp11::mp_if<
            ::lslboost::mp11::mp_empty<SpecSeq>
          , ::lslboost::mp11::mp_identity< ::lslboost::parameter::void_>
#else
      : ::lslboost::mpl::eval_if<
            ::lslboost::mpl::empty<SpecSeq>
          , ::lslboost::mpl::identity< ::lslboost::parameter::void_>
#endif
          , ::lslboost::parameter::aux::make_deduced_list_not_empty<SpecSeq>
        >
    {
    };
}}} // namespace lslboost::parameter::aux

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_same.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename ArgumentPackAndError>
    struct is_arg_pack_error_void
      : ::lslboost::mpl::if_<
            ::lslboost::is_same<
                typename ::lslboost::mpl::second<ArgumentPackAndError>::type
              , ::lslboost::parameter::void_
            >
          , ::lslboost::mpl::true_
          , ::lslboost::mpl::false_
        >::type
    {
    };
}}} // namespace lslboost::parameter::aux

#endif  // BOOST_PARAMETER_CAN_USE_MP11

namespace lslboost { namespace parameter { namespace aux {

    // Checks if the arguments match the criteria of overload resolution.
    // If NamedList satisfies the PS0, PS1, ..., this is a metafunction
    // returning parameters.  Otherwise it has no nested ::type.
    template <typename ArgumentPackAndError, typename SpecSeq>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    using match_parameters_base_cond = ::lslboost::mp11::mp_if<
        ::lslboost::mp11::mp_empty<SpecSeq>
      , ::std::is_same<
            ::lslboost::mp11::mp_at_c<ArgumentPackAndError,1>
          , ::lslboost::parameter::void_
        >
      , ::lslboost::parameter::aux::match_parameters_base_cond_helper<
            ArgumentPackAndError
          , SpecSeq
        >
    >;
#else
    struct match_parameters_base_cond
      : ::lslboost::mpl::eval_if<
            ::lslboost::mpl::empty<SpecSeq>
          , ::lslboost::parameter::aux
            ::is_arg_pack_error_void<ArgumentPackAndError>
          , ::lslboost::parameter::aux::match_parameters_base_cond_helper<
                ArgumentPackAndError
              , SpecSeq
            >
        >
    {
    };
#endif  // BOOST_PARAMETER_CAN_USE_MP11
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/pack/satisfies.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename ArgumentPackAndError, typename SpecSeq>
    struct match_parameters_base_cond_helper
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
      : ::lslboost::mp11::mp_if<
#else
      : ::lslboost::mpl::eval_if<
#endif
            ::lslboost::parameter::aux::satisfies_requirements_of<
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
                ::lslboost::mp11::mp_at_c<ArgumentPackAndError,0>
              , ::lslboost::mp11::mp_front<SpecSeq>
#else
                typename ::lslboost::mpl::first<ArgumentPackAndError>::type
              , typename ::lslboost::mpl::front<SpecSeq>::type
#endif
            >
          , ::lslboost::parameter::aux::match_parameters_base_cond<
                ArgumentPackAndError
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
              , ::lslboost::mp11::mp_pop_front<SpecSeq>
#else
              , typename ::lslboost::mpl::pop_front<SpecSeq>::type
#endif
            >
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
          , ::lslboost::mp11::mp_false
#else
          , ::lslboost::mpl::false_
#endif
        >
    {
    };

    // This parameters item chaining metafunction class does not require
    // the lengths of the SpecSeq and of Args parameter pack to match.
    // Used by argument_pack to build the items in the resulting arg_list.
    // -- Cromwell D. Enage
    template <typename SpecSeq, typename ...Args>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
    using make_parameter_spec_items = ::lslboost::mp11::mp_if<
        ::lslboost::mp11::mp_empty<SpecSeq>
      , ::lslboost::mp11::mp_identity< ::lslboost::parameter::void_>
      , ::lslboost::parameter::aux
        ::make_parameter_spec_items_helper<SpecSeq,Args...>
    >;
#else
    struct make_parameter_spec_items
      : ::lslboost::mpl::eval_if<
            ::lslboost::mpl::empty<SpecSeq>
          , ::lslboost::mpl::identity< ::lslboost::parameter::void_>
          , ::lslboost::parameter::aux
            ::make_parameter_spec_items_helper<SpecSeq,Args...>
        >
    {
    };
#endif
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/pack/make_items.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename SpecSeq, typename A0, typename ...Args>
    struct make_parameter_spec_items_helper<SpecSeq,A0,Args...>
      : ::lslboost::parameter::aux::make_items<
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            ::lslboost::mp11::mp_front<SpecSeq>
#else
            typename ::lslboost::mpl::front<SpecSeq>::type
#endif
          , A0
          , ::lslboost::parameter::aux::make_parameter_spec_items<
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
                ::lslboost::mp11::mp_pop_front<SpecSeq>
#else
                typename ::lslboost::mpl::pop_front<SpecSeq>::type
#endif
              , Args...
            >
        >
    {
    };
}}} // namespace lslboost::parameter::aux

#endif  // include guard

