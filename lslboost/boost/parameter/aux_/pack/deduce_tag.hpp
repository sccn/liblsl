// Copyright David Abrahams, Daniel Wallin 2003.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PARAMETER_AUX_PACK_DEDUCE_TAG_HPP
#define BOOST_PARAMETER_AUX_PACK_DEDUCE_TAG_HPP

namespace lslboost { namespace parameter { namespace aux {

    template <
        typename Argument
      , typename ArgumentPack
      , typename DeducedArgs
      , typename UsedArgs
      , typename TagFn
      , typename EmitsErrors
    >
    struct deduce_tag;
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/lambda_tag.hpp>
#include <boost/parameter/config.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/lambda.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)

namespace lslboost { namespace parameter { namespace aux {

    template <typename Predicate, typename Argument, typename ArgumentPack>
    struct deduce_tag_condition_mpl
      : ::lslboost::mpl::apply_wrap2<
            typename ::lslboost::mpl::lambda<
                Predicate
              , ::lslboost::parameter::aux::lambda_tag
            >::type
          , Argument
          , ArgumentPack
        >
    {
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/has_nested_template_fn.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename Predicate, typename Argument, typename ArgumentPack>
    struct deduce_tag_condition_mp11
    {
        using type = ::lslboost::mp11::mp_apply_q<
            Predicate
          , ::lslboost::mp11::mp_list<Argument,ArgumentPack>
        >;
    };

    template <typename Predicate, typename Argument, typename ArgumentPack>
    using deduce_tag_condition = ::lslboost::mp11::mp_if<
        ::lslboost::parameter::aux::has_nested_template_fn<Predicate>
      , ::lslboost::parameter::aux
        ::deduce_tag_condition_mp11<Predicate,Argument,ArgumentPack>
      , ::lslboost::parameter::aux
        ::deduce_tag_condition_mpl<Predicate,Argument,ArgumentPack>
    >;
}}} // namespace lslboost::parameter::aux

#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/mpl/assert.hpp>
#endif  // BOOST_PARAMETER_CAN_USE_MP11

#include <boost/parameter/aux_/set.hpp>
#include <boost/parameter/aux_/pack/tag_type.hpp>
#include <boost/parameter/aux_/pack/tag_deduced.hpp>

namespace lslboost { namespace parameter { namespace aux {

    // Helper for deduce_tag<...>, below.
    template <
        typename Argument
      , typename ArgumentPack
      , typename DeducedArgs
      , typename UsedArgs
      , typename TagFn
      , typename EmitsErrors
    >
    class deduce_tag0
    {
        typedef typename DeducedArgs::spec _spec;

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        typedef typename ::lslboost::parameter::aux::deduce_tag_condition<
            typename _spec::predicate
#else
        typedef typename ::lslboost::mpl::apply_wrap2<
            typename ::lslboost::mpl::lambda<
                typename _spec::predicate
              , ::lslboost::parameter::aux::lambda_tag
            >::type
#endif
          , Argument
          , ArgumentPack
        >::type _condition;

#if !defined(BOOST_PARAMETER_CAN_USE_MP11)
        // Deduced parameter matches several arguments.
        BOOST_MPL_ASSERT((
            typename ::lslboost::mpl::eval_if<
                typename ::lslboost::parameter::aux::has_key_<
                    UsedArgs
                  , typename ::lslboost::parameter::aux::tag_type<_spec>::type
                >::type
              , ::lslboost::mpl::eval_if<
                    _condition
                  , ::lslboost::mpl::if_<
                        EmitsErrors
                      , ::lslboost::mpl::false_
                      , ::lslboost::mpl::true_
                    >
                  , ::lslboost::mpl::true_
                >
              , ::lslboost::mpl::true_
            >::type
        ));
#endif  // BOOST_PARAMETER_CAN_USE_MP11

     public:
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        using type = typename ::lslboost::mp11::mp_if<
#else
        typedef typename ::lslboost::mpl::eval_if<
#endif
            _condition
          , ::lslboost::parameter::aux
            ::tag_deduced<UsedArgs,_spec,Argument,TagFn>
          , ::lslboost::parameter::aux::deduce_tag<
                Argument
              , ArgumentPack
              , typename DeducedArgs::tail
              , UsedArgs
              , TagFn
              , EmitsErrors
            >
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        >::type;
#else
        >::type type;
#endif
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/void.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <type_traits>
#else
#include <boost/mpl/pair.hpp>
#include <boost/type_traits/is_same.hpp>
#endif

namespace lslboost { namespace parameter { namespace aux {

    // Tries to deduced a keyword tag for a given Argument.
    // Returns an mpl::pair<> consisting of the tagged_argument<>,
    // and an mpl::set<> where the new tag has been inserted.
    //
    //  Argument:     The argument type to be tagged.
    //
    //  ArgumentPack: The ArgumentPack built so far.
    //
    //  DeducedArgs:  A specialization of deduced_item<> (see below).
    //                A list containing only the deduced ParameterSpecs.
    //
    //  UsedArgs:     An mpl::set<> containing the keyword tags used so far.
    //
    //  TagFn:        A metafunction class used to tag positional or deduced
    //                arguments with a keyword tag.
    template <
        typename Argument
      , typename ArgumentPack
      , typename DeducedArgs
      , typename UsedArgs
      , typename TagFn
      , typename EmitsErrors
    >
    struct deduce_tag
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
      : ::lslboost::mp11::mp_if<
            ::std::is_same<DeducedArgs,::lslboost::parameter::void_>
          , ::lslboost::mp11::mp_identity<
                ::lslboost::mp11::mp_list< ::lslboost::parameter::void_,UsedArgs>
            >
#else
      : ::lslboost::mpl::eval_if<
            ::lslboost::is_same<DeducedArgs,::lslboost::parameter::void_>
          , ::lslboost::mpl::pair< ::lslboost::parameter::void_,UsedArgs>
#endif
          , ::lslboost::parameter::aux::deduce_tag0<
                Argument
              , ArgumentPack
              , DeducedArgs
              , UsedArgs
              , TagFn
              , EmitsErrors
            >
        >
    {
    };
}}} // namespace lslboost::parameter::aux

#endif  // include guard

