// Copyright Cromwell D. Enage 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PARAMETER_AUGMENT_PREDICATE_HPP
#define BOOST_PARAMETER_AUGMENT_PREDICATE_HPP

#include <boost/parameter/keyword_fwd.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/type_traits/is_lvalue_reference.hpp>
#include <boost/type_traits/is_scalar.hpp>
#include <boost/type_traits/is_same.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename V, typename R, typename Tag>
    struct augment_predicate_check_consume_ref
      : ::lslboost::mpl::eval_if<
            ::lslboost::is_scalar<V>
          , ::lslboost::mpl::true_
          , ::lslboost::mpl::eval_if<
                ::lslboost::is_same<
                    typename Tag::qualifier
                  , ::lslboost::parameter::consume_reference
                >
              , ::lslboost::mpl::if_<
                    ::lslboost::is_lvalue_reference<R>
                  , ::lslboost::mpl::false_
                  , ::lslboost::mpl::true_
                >
              , lslboost::mpl::true_
            >
        >::type
    {
    };
}}} // namespace lslboost::parameter::aux

#include <boost/type_traits/is_const.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <typename V, typename R, typename Tag>
    struct augment_predicate_check_out_ref
      : ::lslboost::mpl::eval_if<
            ::lslboost::is_same<
                typename Tag::qualifier
              , ::lslboost::parameter::out_reference
            >
          , ::lslboost::mpl::eval_if<
                ::lslboost::is_lvalue_reference<R>
              , ::lslboost::mpl::if_<
                    ::lslboost::is_const<V>
                  , ::lslboost::mpl::false_
                  , ::lslboost::mpl::true_
                >
              , ::lslboost::mpl::false_
            >
          , ::lslboost::mpl::true_
        >::type
    {
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/lambda_tag.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/lambda.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <
        typename Predicate
      , typename R
      , typename Tag
      , typename T
      , typename Args
    >
    class augment_predicate
    {
        typedef typename ::lslboost::mpl::lambda<
            Predicate
          , ::lslboost::parameter::aux::lambda_tag
        >::type _actual_predicate;

     public:
        typedef typename ::lslboost::mpl::eval_if<
            typename ::lslboost::mpl::if_<
                ::lslboost::parameter::aux
                ::augment_predicate_check_consume_ref<T,R,Tag>
              , ::lslboost::parameter::aux
                ::augment_predicate_check_out_ref<T,R,Tag>
              , ::lslboost::mpl::false_
            >::type
          , ::lslboost::mpl::apply_wrap2<_actual_predicate,T,Args>
          , ::lslboost::mpl::false_
        >::type type;
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/config.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/integral.hpp>
#include <boost/mp11/utility.hpp>
#include <type_traits>

namespace lslboost { namespace parameter { namespace aux {

    template <typename V, typename R, typename Tag>
    using augment_predicate_check_consume_ref_mp11 = ::lslboost::mp11::mp_if<
        ::std::is_scalar<V>
      , ::lslboost::mp11::mp_true
      , ::lslboost::mp11::mp_if<
            ::std::is_same<
                typename Tag::qualifier
              , ::lslboost::parameter::consume_reference
            >
          , ::lslboost::mp11::mp_if<
                ::std::is_lvalue_reference<R>
              , ::lslboost::mp11::mp_false
              , ::lslboost::mp11::mp_true
            >
          , lslboost::mp11::mp_true
        >
    >;

    template <typename V, typename R, typename Tag>
    using augment_predicate_check_out_ref_mp11 = ::lslboost::mp11::mp_if<
        ::std::is_same<
            typename Tag::qualifier
          , ::lslboost::parameter::out_reference
        >
      , ::lslboost::mp11::mp_if<
            ::std::is_lvalue_reference<R>
          , ::lslboost::mp11::mp_if<
                ::std::is_const<V>
              , ::lslboost::mp11::mp_false
              , ::lslboost::mp11::mp_true
            >
          , ::lslboost::mp11::mp_false
        >
      , ::lslboost::mp11::mp_true
    >;
}}} // namespace lslboost::parameter::aux

#include <boost/mp11/list.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <
        typename Predicate
      , typename R
      , typename Tag
      , typename T
      , typename Args
    >
    struct augment_predicate_mp11_impl
    {
        using type = ::lslboost::mp11::mp_if<
            ::lslboost::mp11::mp_if<
                ::lslboost::parameter::aux
                ::augment_predicate_check_consume_ref_mp11<T,R,Tag>
              , ::lslboost::parameter::aux
                ::augment_predicate_check_out_ref_mp11<T,R,Tag>
              , ::lslboost::mp11::mp_false
            >
          , ::lslboost::mp11
            ::mp_apply_q<Predicate,::lslboost::mp11::mp_list<T,Args> >
          , ::lslboost::mp11::mp_false
        >;
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/has_nested_template_fn.hpp>

namespace lslboost { namespace parameter { namespace aux {

    template <
        typename Predicate
      , typename R
      , typename Tag
      , typename T
      , typename Args
    >
    using augment_predicate_mp11 = ::lslboost::mp11::mp_if<
        ::lslboost::parameter::aux::has_nested_template_fn<Predicate>
      , ::lslboost::parameter::aux
        ::augment_predicate_mp11_impl<Predicate,R,Tag,T,Args>
      , ::lslboost::parameter::aux
        ::augment_predicate<Predicate,R,Tag,T,Args>
    >;
}}} // namespace lslboost::parameter::aux

#endif  // BOOST_PARAMETER_CAN_USE_MP11
#endif  // include guard

