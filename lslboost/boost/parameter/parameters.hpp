// Copyright David Abrahams, Daniel Wallin 2003.
// Copyright Cromwell D. Enage 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_PARAMETERS_031014_HPP
#define BOOST_PARAMETERS_031014_HPP

#include <boost/parameter/config.hpp>

#if defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)

namespace lslboost { namespace parameter { namespace aux {

    // The make_arg_list<> metafunction produces a reversed arg_list,
    // so pass the arguments to the arg_list constructor reversed in turn.
    template <typename ArgList, typename ...Args>
    struct arg_list_factory;
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/arg_list.hpp>
#include <utility>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/utility.hpp>
#include <type_traits>
#else
#include <boost/mpl/if.hpp>
#include <boost/type_traits/is_same.hpp>
#endif

namespace lslboost { namespace parameter { namespace aux {

    // TODO: Reduce template code bloat. -- Cromwell D. Enage
    template <typename ArgList>
    struct arg_list_factory<ArgList>
    {
        template <typename ...ReversedArgs>
        static inline BOOST_CONSTEXPR ArgList
            reverse(ReversedArgs&&... reversed_args)
        {
            return ArgList(
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
                ::lslboost::mp11::mp_if<
                    ::std::is_same<
#else
                typename ::lslboost::mpl::if_<
                    ::lslboost::is_same<
#endif
                        typename ArgList::tagged_arg::value_type
                      , ::lslboost::parameter::void_
                    >
                  , ::lslboost::parameter::aux::value_type_is_void
                  , ::lslboost::parameter::aux::value_type_is_not_void
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
                >()
#else
                >::type()
#endif
              , ::std::forward<ReversedArgs>(reversed_args)...
            );
        }
    };

    template <typename ArgList, typename A0, typename ...Args>
    struct arg_list_factory<ArgList,A0,Args...>
    {
        template <typename ...ReversedArgs>
        static inline BOOST_CONSTEXPR ArgList
            reverse(A0&& a0, Args&&... args, ReversedArgs&&... reversed_args)
        {
            return ::lslboost::parameter::aux
            ::arg_list_factory<ArgList,Args...>::reverse(
                ::std::forward<Args>(args)...
              , ::std::forward<A0>(a0)
              , ::std::forward<ReversedArgs>(reversed_args)...
            );
        }
    };
}}} // namespace lslboost::parameter::aux

#include <boost/parameter/aux_/void.hpp>
#include <boost/parameter/aux_/pack/make_arg_list.hpp>
#include <boost/parameter/aux_/pack/make_parameter_spec_items.hpp>
#include <boost/parameter/aux_/pack/tag_keyword_arg.hpp>
#include <boost/parameter/aux_/pack/tag_template_keyword_arg.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <boost/mp11/integral.hpp>
#include <boost/mp11/list.hpp>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/identity.hpp>
#endif

#if !defined(BOOST_PARAMETER_VARIADIC_MPL_SEQUENCE)
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
//#include <boost/mp11/mpl.hpp>
#define BOOST_PARAMETER_VARIADIC_MPL_SEQUENCE ::lslboost::mp11::mp_list
#else
#include <boost/fusion/container/list/list_fwd.hpp>

// Newer versions of MSVC fail on the evaluate_category and
// preprocessor_eval_category test programs when parameters uses
// lslboost::fusion::list.
// -- Cromwell D. Enage
#if defined(BOOST_FUSION_HAS_VARIADIC_LIST) && ( \
        !defined(BOOST_MSVC) || (BOOST_MSVC < 1800) \
    )
#include <boost/fusion/container/list.hpp>
#include <boost/fusion/mpl.hpp>
#define BOOST_PARAMETER_VARIADIC_MPL_SEQUENCE ::lslboost::fusion::list
#else
#include <boost/fusion/container/deque/deque_fwd.hpp>

#if defined(BOOST_FUSION_HAS_VARIADIC_DEQUE)
#include <boost/fusion/container/deque.hpp>
#include <boost/fusion/mpl.hpp>
#define BOOST_PARAMETER_VARIADIC_MPL_SEQUENCE ::lslboost::fusion::deque
#else
#include <boost/mpl/vector.hpp>
#define BOOST_PARAMETER_VARIADIC_MPL_SEQUENCE ::lslboost::mpl::vector
#endif  // BOOST_FUSION_HAS_VARIADIC_DEQUE
#endif  // BOOST_FUSION_HAS_VARIADIC_LIST
#endif  // BOOST_PARAMETER_CAN_USE_MP11
#endif  // BOOST_PARAMETER_VARIADIC_MPL_SEQUENCE

namespace lslboost { namespace parameter {

    template <typename ...Spec>
    struct parameters
    {
        typedef BOOST_PARAMETER_VARIADIC_MPL_SEQUENCE<Spec...> parameter_spec;

        typedef typename ::lslboost::parameter::aux
        ::make_deduced_list<parameter_spec>::type deduced_list;

        // If the elements of NamedList match the criteria of overload
        // resolution, returns a type which can be constructed from
        // parameters.  Otherwise, this is not a valid metafunction
        // (no nested ::type).
        template <typename ArgumentPackAndError>
        struct match_base
#if !defined(BOOST_PARAMETER_CAN_USE_MP11)
          : ::lslboost::mpl::if_<
                typename ::lslboost::parameter::aux::match_parameters_base_cond<
                    ArgumentPackAndError
                  , parameter_spec
                >::type
              , ::lslboost::mpl::identity<
                    ::lslboost::parameter::parameters<Spec...>
                >
              , ::lslboost::parameter::void_
            >
#endif
        {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            using type = ::lslboost::mp11::mp_if<
                typename ::lslboost::parameter::aux::match_parameters_base_cond<
                    ArgumentPackAndError
                  , parameter_spec
                >::type
              , ::lslboost::mp11::mp_identity<
                    ::lslboost::parameter::parameters<Spec...>
                >
              , ::lslboost::parameter::void_
            >;
#endif
        };

        // Specializations are to be used as an optional argument
        // to eliminate overloads via SFINAE.
        template <typename ...Args>
        struct match
          : ::lslboost::parameter::parameters<Spec...>
            ::BOOST_NESTED_TEMPLATE match_base<
                typename ::lslboost::parameter::aux::make_arg_list<
                    typename ::lslboost::parameter::aux
                    ::make_parameter_spec_items<parameter_spec,Args...>::type
                  , deduced_list
                  , ::lslboost::parameter::aux::tag_keyword_arg
                    // Don't emit errors when doing SFINAE.
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
                  , ::lslboost::mp11::mp_false
#else
                  , ::lslboost::mpl::false_
#endif
                >::type
            >::type
        {
        };

        // Metafunction that returns an ArgumentPack.
        template <typename ...Args>
        struct bind
#if !defined(BOOST_PARAMETER_CAN_USE_MP11)
          : ::lslboost::mpl::first<
                typename ::lslboost::parameter::aux::make_arg_list<
                    typename ::lslboost::parameter::aux
                    ::make_parameter_spec_items<parameter_spec,Args...>::type
                  , deduced_list
                  , ::lslboost::parameter::aux::tag_template_keyword_arg
                >::type
            >
#endif
        {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            using type = ::lslboost::mp11::mp_at_c<
                typename ::lslboost::parameter::aux::make_arg_list<
                    typename ::lslboost::parameter::aux
                    ::make_parameter_spec_items<parameter_spec,Args...>::type
                  , deduced_list
                  , ::lslboost::parameter::aux::tag_template_keyword_arg
                >::type
              , 0
            >;
#endif
        };

        // The function call operator is used to build an arg_list that
        // labels the positional parameters and maintains whatever other
        // tags may have been specified by the caller.
        inline ::lslboost::parameter::aux::empty_arg_list operator()() const
        {
            return ::lslboost::parameter::aux::empty_arg_list();
        }

        template <typename A0, typename ...Args>
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        inline ::lslboost::mp11::mp_at_c<
#else
        inline typename ::lslboost::mpl::first<
#endif
            typename ::lslboost::parameter::aux::make_arg_list<
                typename ::lslboost::parameter::aux
                ::make_parameter_spec_items<parameter_spec,A0,Args...>::type
              , deduced_list
              , ::lslboost::parameter::aux::tag_keyword_arg
            >::type
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
          , 0
        >
#else
        >::type
#endif
            operator()(A0&& a0, Args&& ...args) const
        {
            typedef typename ::lslboost::parameter::aux::make_arg_list<
                typename ::lslboost::parameter::aux
                ::make_parameter_spec_items<parameter_spec,A0,Args...>::type
              , deduced_list
              , ::lslboost::parameter::aux::tag_keyword_arg
            >::type list_error_pair;

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
            using result_type = ::lslboost::mp11::mp_at_c<list_error_pair,0>;

            using error = ::lslboost::mp11::mp_at_c<list_error_pair,1>;
#else
            typedef typename ::lslboost::mpl
            ::first<list_error_pair>::type result_type;

            typedef typename ::lslboost::mpl
            ::second<list_error_pair>::type error;
#endif

            error();

            return ::lslboost::parameter::aux
            ::arg_list_factory<result_type,A0,Args...>::reverse(
                ::std::forward<A0>(a0)
              , ::std::forward<Args>(args)...
            );
        }
    };
}} // namespace lslboost::parameter

#else   // !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING)

#include <boost/parameter/aux_/void.hpp>
#include <boost/parameter/aux_/arg_list.hpp>
#include <boost/parameter/aux_/pack/make_arg_list.hpp>
#include <boost/parameter/aux_/pack/make_items.hpp>
#include <boost/parameter/aux_/pack/make_deduced_items.hpp>
#include <boost/parameter/aux_/pack/tag_template_keyword_arg.hpp>
#include <boost/parameter/aux_/preprocessor/binary_seq_for_each.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/repetition/enum_shifted.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/selection/min.hpp>

#if ( \
        BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY < \
        BOOST_PARAMETER_MAX_ARITY \
    )
#include <boost/parameter/aux_/pack/tag_keyword_arg_ref.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/preprocessor/arithmetic/dec.hpp>
#include <boost/preprocessor/arithmetic/sub.hpp>
#include <boost/preprocessor/facilities/intercept.hpp>
#include <boost/preprocessor/iteration/iterate.hpp>
#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/repetition/enum_trailing_params.hpp>
#endif

#if !defined(BOOST_NO_SFINAE) && \
    !BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x592))
#include <boost/parameter/aux_/pack/tag_keyword_arg.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/identity.hpp>
#include <boost/type_traits/is_same.hpp>
#endif

#if BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x564))
#include <boost/preprocessor/repetition/enum_params.hpp>
#else
#include <boost/preprocessor/repetition/enum_binary_params.hpp>
#endif

#include <boost/parameter/aux_/preprocessor/no_perfect_forwarding_begin.hpp>

namespace lslboost { namespace parameter {

    template <
        typename PS0
      , BOOST_PP_ENUM_SHIFTED(
            BOOST_PARAMETER_MAX_ARITY
          , BOOST_PARAMETER_template_args
          , PS
        )
    >
    struct parameters
    {
        typedef typename BOOST_PARAMETER_build_deduced_list(
            BOOST_PARAMETER_MAX_ARITY
          , ::lslboost::parameter::aux::make_deduced_items
          , PS
        )::type deduced_list;

        // If the elements of NamedList match the criteria of overload
        // resolution, returns a type which can be constructed from
        // parameters.  Otherwise, this is not a valid metafunction
        // (no nested ::type).
#if !defined(BOOST_NO_SFINAE) && \
    !BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x592))
        // If NamedList satisfies the PS0, PS1, ..., this is a metafunction
        // returning parameters.  Otherwise it has no nested ::type.
        template <typename ArgumentPackAndError>
        struct match_base
          : ::lslboost::mpl::if_<
                // ::lslboost::mpl::and_<
                //    aux::satisfies_requirements_of<NamedList,PS0>
                //  , ::lslboost::mpl::and_<
                //       aux::satisfies_requirements_of<NamedList,PS1>...
                //           ..., ::lslboost::mpl::true_
                // ...> >
                typename BOOST_PP_REPEAT(
                    BOOST_PARAMETER_MAX_ARITY
                  , BOOST_PARAMETER_satisfies_begin
                  , PS
                )
                ::lslboost::is_same<
                    typename ::lslboost::mpl
                    ::second<ArgumentPackAndError>::type
                  , ::lslboost::parameter::void_
                >
                BOOST_PP_REPEAT(
                    BOOST_PARAMETER_MAX_ARITY
                  , BOOST_PARAMETER_satisfies_end
                  , ::lslboost::mpl::false_
                )::type
              , ::lslboost::mpl::identity<
                    ::lslboost::parameter::parameters<
                        BOOST_PP_ENUM_PARAMS(BOOST_PARAMETER_MAX_ARITY, PS)
                    >
                >
              , ::lslboost::parameter::void_
            >
        {
        };
#endif  // SFINAE enabled, not Borland

        // Specializations are to be used as an optional argument
        // to eliminate overloads via SFINAE.
        template <
#if BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x564))
            // Borland simply can't handle default arguments in member
            // class templates.  People wishing to write portable code can
            // explicitly specify BOOST_PARAMETER_MAX_ARITY arguments.
            BOOST_PP_ENUM_PARAMS(BOOST_PARAMETER_MAX_ARITY, typename A)
#else
            BOOST_PP_ENUM_BINARY_PARAMS(
                BOOST_PARAMETER_MAX_ARITY
              , typename A
              , = ::lslboost::parameter::void_ BOOST_PP_INTERCEPT
            )
#endif
        >
        struct match
#if !defined(BOOST_NO_SFINAE) && \
    !BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x592))
          : ::lslboost::parameter::parameters<
                BOOST_PP_ENUM_PARAMS(BOOST_PARAMETER_MAX_ARITY, PS)
            >::BOOST_NESTED_TEMPLATE match_base<
                typename ::lslboost::parameter::aux::make_arg_list<
                    typename BOOST_PARAMETER_build_arg_list(
                        BOOST_PARAMETER_MAX_ARITY
                      , ::lslboost::parameter::aux::make_items
                      , PS
                      , A
                    )::type
                  , deduced_list
                  , ::lslboost::parameter::aux::tag_keyword_arg
                    // Don't emit errors when doing SFINAE.
                  , ::lslboost::mpl::false_
                >::type
            >::type
        {
        };
#else
        {
            typedef ::lslboost::parameter::parameters<
                BOOST_PP_ENUM_PARAMS(BOOST_PARAMETER_MAX_ARITY, PS)
            > type;
        };
#endif  // SFINAE enabled, not Borland

        // Metafunction that returns an ArgumentPack.

        // TODO, bind has to instantiate the error type in the result
        // of make_arg_list.

        template <
#if BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x564))
            // Borland simply can't handle default arguments in member
            // class templates.  People wishing to write portable code can
            // explicitly specify BOOST_PARAMETER_MAX_ARITY arguments.
            BOOST_PP_ENUM_PARAMS(BOOST_PARAMETER_MAX_ARITY, typename A)
#else
            BOOST_PP_ENUM_BINARY_PARAMS(
                BOOST_PARAMETER_MAX_ARITY
              , typename A
              , = ::lslboost::parameter::void_ BOOST_PP_INTERCEPT
            )
#endif
        >
        struct bind
        {
            typedef typename ::lslboost::parameter::aux::make_arg_list<
                typename BOOST_PARAMETER_build_arg_list(
                    BOOST_PARAMETER_MAX_ARITY
                  , ::lslboost::parameter::aux::make_items
                  , PS
                  , A
                )::type
              , deduced_list
              , ::lslboost::parameter::aux::tag_template_keyword_arg
            >::type result;

            typedef typename ::lslboost::mpl::first<result>::type type;
        };

        BOOST_PP_REPEAT(
            BOOST_PARAMETER_MAX_ARITY
          , BOOST_PARAMETER_forward_typedef
          , (PS)(parameter_spec)
        )

        // The function call operator is used to build an arg_list that
        // labels the positional parameters and maintains whatever other
        // tags may have been specified by the caller.
        //
        // !!!NOTE!!!
        //
        // The make_arg_list<> metafunction produces a reversed arg_list,
        // so pass the arguments to the arg_list constructor reversed in turn.
        inline ::lslboost::parameter::aux::empty_arg_list operator()() const
        {
            return ::lslboost::parameter::aux::empty_arg_list();
        }

#if (0 < BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY)
        BOOST_PP_REPEAT(
            BOOST_PP_MIN(
                BOOST_PP_INC(BOOST_PARAMETER_MAX_ARITY)
              , BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY
            )
          , BOOST_PARAMETER_AUX_PP_BINARY_SEQ_FOR_EACH_Z
          , (BOOST_PARAMETER_function_call_op_overload_R)(_)
        )
#if ( \
        BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY < \
        BOOST_PARAMETER_MAX_ARITY \
    )
#define BOOST_PP_ITERATION_PARAMS_1 \
    (3,( \
        BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY \
      , BOOST_PARAMETER_MAX_ARITY \
      , <boost/parameter/aux_/preprocessor/overloads.hpp> \
    ))
#include BOOST_PP_ITERATE()
#endif
#else   // (0 == BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY)
        template <typename A0>
        inline typename ::lslboost::mpl::first<
            typename ::lslboost::parameter::aux::make_arg_list<
                ::lslboost::parameter::aux::item<
                    PS0,A0
                >
              , deduced_list
              , ::lslboost::parameter::aux::tag_keyword_arg_ref
            >::type
        >::type
            operator()(A0& a0) const
        {
            typedef typename ::lslboost::parameter::aux::make_arg_list<
                ::lslboost::parameter::aux::item<
                    PS0,A0
                >
              , deduced_list
              , ::lslboost::parameter::aux::tag_keyword_arg_ref
            >::type result;

            typedef typename ::lslboost::mpl::first<result>::type result_type;
            typedef typename ::lslboost::mpl::second<result>::type error;
            error();

            return result_type(
                a0
                // , void_(), void_(), void_() ...
                BOOST_PP_ENUM_TRAILING_PARAMS(
                    BOOST_PP_SUB(BOOST_PARAMETER_COMPOSE_MAX_ARITY, 1)
                  , ::lslboost::parameter::aux::void_reference() BOOST_PP_INTERCEPT
                )
            );
        }

        template <typename A0, typename A1>
        inline typename ::lslboost::mpl::first<
            typename ::lslboost::parameter::aux::make_arg_list<
                ::lslboost::parameter::aux::item<
                    PS0,A0
                  , ::lslboost::parameter::aux::item<
                        PS1,A1
                    >
                >
              , deduced_list
              , ::lslboost::parameter::aux::tag_keyword_arg_ref
            >::type
        >::type
            operator()(A0& a0, A1& a1) const
        {
            typedef typename ::lslboost::parameter::aux::make_arg_list<
                ::lslboost::parameter::aux::item<
                    PS0,A0
                  , ::lslboost::parameter::aux::item<
                        PS1,A1
                    >
                >
              , deduced_list
              , ::lslboost::parameter::aux::tag_keyword_arg
            >::type result;

            typedef typename ::lslboost::mpl::first<result>::type result_type;
            typedef typename ::lslboost::mpl::second<result>::type error;
            error();

            return result_type(
                a1
              , a0
                // , void_(), void_() ...
                BOOST_PP_ENUM_TRAILING_PARAMS(
                    BOOST_PP_SUB(BOOST_PARAMETER_COMPOSE_MAX_ARITY, 2)
                  , ::lslboost::parameter::aux::void_reference() BOOST_PP_INTERCEPT
                )
            );
        }

#if (2 < BOOST_PARAMETER_MAX_ARITY)
        // Higher arities are handled by the preprocessor
#define BOOST_PP_ITERATION_PARAMS_1 \
    (3,( \
        3 \
      , BOOST_PARAMETER_MAX_ARITY \
      , <boost/parameter/aux_/preprocessor/overloads.hpp> \
    ))
#include BOOST_PP_ITERATE()
#endif
#endif  // exponential overloads
    };
}} // namespace lslboost::parameter

#include <boost/parameter/aux_/preprocessor/no_perfect_forwarding_end.hpp>

#endif  // BOOST_PARAMETER_HAS_PERFECT_FORWARDING
#endif  // include guard

