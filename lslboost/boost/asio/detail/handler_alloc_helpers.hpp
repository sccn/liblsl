//
// detail/handler_alloc_helpers.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2020 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_HANDLER_ALLOC_HELPERS_HPP
#define BOOST_ASIO_DETAIL_HANDLER_ALLOC_HELPERS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/memory.hpp>
#include <boost/asio/detail/noncopyable.hpp>
#include <boost/asio/detail/recycling_allocator.hpp>
#include <boost/asio/detail/thread_info_base.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/handler_alloc_hook.hpp>

#include <boost/asio/detail/push_options.hpp>

// Calls to asio_handler_allocate and asio_handler_deallocate must be made from
// a namespace that does not contain any overloads of these functions. The
// boost_asio_handler_alloc_helpers namespace is defined here for that purpose.
namespace lslboost_asio_handler_alloc_helpers {

#if defined(BOOST_ASIO_NO_DEPRECATED)
template <typename Handler>
inline void error_if_hooks_are_defined(Handler& h)
{
  using lslboost::asio::asio_handler_allocate;
  // If you get an error here it is because some of your handlers still
  // overload asio_handler_allocate, but this hook is no longer used.
  (void)static_cast<lslboost::asio::asio_handler_allocate_is_no_longer_used>(
    asio_handler_allocate(static_cast<std::size_t>(0),
      lslboost::asio::detail::addressof(h)));

  using lslboost::asio::asio_handler_deallocate;
  // If you get an error here it is because some of your handlers still
  // overload asio_handler_deallocate, but this hook is no longer used.
  (void)static_cast<lslboost::asio::asio_handler_deallocate_is_no_longer_used>(
    asio_handler_deallocate(static_cast<void*>(0),
      static_cast<std::size_t>(0), lslboost::asio::detail::addressof(h)));
}
#endif // defined(BOOST_ASIO_NO_DEPRECATED)

template <typename Handler>
inline void* allocate(std::size_t s, Handler& h)
{
#if !defined(BOOST_ASIO_HAS_HANDLER_HOOKS)
  return ::operator new(s);
#elif defined(BOOST_ASIO_NO_DEPRECATED)
  // The asio_handler_allocate hook is no longer used to obtain memory.
  (void)&error_if_hooks_are_defined<Handler>;
  (void)h;
#if !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
  return lslboost::asio::detail::thread_info_base::allocate(
      lslboost::asio::detail::thread_context::thread_call_stack::top(), s);
#else // !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
  return ::operator new(size);
#endif // !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
#else
  using lslboost::asio::asio_handler_allocate;
  return asio_handler_allocate(s, lslboost::asio::detail::addressof(h));
#endif
}

template <typename Handler>
inline void deallocate(void* p, std::size_t s, Handler& h)
{
#if !defined(BOOST_ASIO_HAS_HANDLER_HOOKS)
  ::operator delete(p);
#elif defined(BOOST_ASIO_NO_DEPRECATED)
  // The asio_handler_allocate hook is no longer used to obtain memory.
  (void)&error_if_hooks_are_defined<Handler>;
  (void)h;
#if !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
  lslboost::asio::detail::thread_info_base::deallocate(
      lslboost::asio::detail::thread_context::thread_call_stack::top(), p, s);
#else // !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
  (void)s;
  ::operator delete(p);
#endif // !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
#else
  using lslboost::asio::asio_handler_deallocate;
  asio_handler_deallocate(p, s, lslboost::asio::detail::addressof(h));
#endif
}

} // namespace lslboost_asio_handler_alloc_helpers

namespace lslboost {
namespace asio {
namespace detail {

template <typename Handler, typename T>
class hook_allocator
{
public:
  typedef T value_type;

  template <typename U>
  struct rebind
  {
    typedef hook_allocator<Handler, U> other;
  };

  explicit hook_allocator(Handler& h)
    : handler_(h)
  {
  }

  template <typename U>
  hook_allocator(const hook_allocator<Handler, U>& a)
    : handler_(a.handler_)
  {
  }

  T* allocate(std::size_t n)
  {
    return static_cast<T*>(
        lslboost_asio_handler_alloc_helpers::allocate(sizeof(T) * n, handler_));
  }

  void deallocate(T* p, std::size_t n)
  {
    lslboost_asio_handler_alloc_helpers::deallocate(p, sizeof(T) * n, handler_);
  }

//private:
  Handler& handler_;
};

template <typename Handler>
class hook_allocator<Handler, void>
{
public:
  typedef void value_type;

  template <typename U>
  struct rebind
  {
    typedef hook_allocator<Handler, U> other;
  };

  explicit hook_allocator(Handler& h)
    : handler_(h)
  {
  }

  template <typename U>
  hook_allocator(const hook_allocator<Handler, U>& a)
    : handler_(a.handler_)
  {
  }

//private:
  Handler& handler_;
};

template <typename Handler, typename Allocator>
struct get_hook_allocator
{
  typedef Allocator type;

  static type get(Handler&, const Allocator& a)
  {
    return a;
  }
};

template <typename Handler, typename T>
struct get_hook_allocator<Handler, std::allocator<T> >
{
  typedef hook_allocator<Handler, T> type;

  static type get(Handler& handler, const std::allocator<T>&)
  {
    return type(handler);
  }
};

} // namespace detail
} // namespace asio
} // namespace lslboost

#define BOOST_ASIO_DEFINE_HANDLER_PTR(op) \
  struct ptr \
  { \
    Handler* h; \
    op* v; \
    op* p; \
    ~ptr() \
    { \
      reset(); \
    } \
    static op* allocate(Handler& handler) \
    { \
      typedef typename ::lslboost::asio::associated_allocator< \
        Handler>::type associated_allocator_type; \
      typedef typename ::lslboost::asio::detail::get_hook_allocator< \
        Handler, associated_allocator_type>::type hook_allocator_type; \
      BOOST_ASIO_REBIND_ALLOC(hook_allocator_type, op) a( \
            ::lslboost::asio::detail::get_hook_allocator< \
              Handler, associated_allocator_type>::get( \
                handler, ::lslboost::asio::get_associated_allocator(handler))); \
      return a.allocate(1); \
    } \
    void reset() \
    { \
      if (p) \
      { \
        p->~op(); \
        p = 0; \
      } \
      if (v) \
      { \
        typedef typename ::lslboost::asio::associated_allocator< \
          Handler>::type associated_allocator_type; \
        typedef typename ::lslboost::asio::detail::get_hook_allocator< \
          Handler, associated_allocator_type>::type hook_allocator_type; \
        BOOST_ASIO_REBIND_ALLOC(hook_allocator_type, op) a( \
              ::lslboost::asio::detail::get_hook_allocator< \
                Handler, associated_allocator_type>::get( \
                  *h, ::lslboost::asio::get_associated_allocator(*h))); \
        a.deallocate(static_cast<op*>(v), 1); \
        v = 0; \
      } \
    } \
  } \
  /**/

#define BOOST_ASIO_DEFINE_TAGGED_HANDLER_ALLOCATOR_PTR(purpose, op) \
  struct ptr \
  { \
    const Alloc* a; \
    void* v; \
    op* p; \
    ~ptr() \
    { \
      reset(); \
    } \
    static op* allocate(const Alloc& a) \
    { \
      typedef typename ::lslboost::asio::detail::get_recycling_allocator< \
        Alloc, purpose>::type recycling_allocator_type; \
      BOOST_ASIO_REBIND_ALLOC(recycling_allocator_type, op) a1( \
            ::lslboost::asio::detail::get_recycling_allocator< \
              Alloc, purpose>::get(a)); \
      return a1.allocate(1); \
    } \
    void reset() \
    { \
      if (p) \
      { \
        p->~op(); \
        p = 0; \
      } \
      if (v) \
      { \
        typedef typename ::lslboost::asio::detail::get_recycling_allocator< \
          Alloc, purpose>::type recycling_allocator_type; \
        BOOST_ASIO_REBIND_ALLOC(recycling_allocator_type, op) a1( \
              ::lslboost::asio::detail::get_recycling_allocator< \
                Alloc, purpose>::get(*a)); \
        a1.deallocate(static_cast<op*>(v), 1); \
        v = 0; \
      } \
    } \
  } \
  /**/

#define BOOST_ASIO_DEFINE_HANDLER_ALLOCATOR_PTR(op) \
  BOOST_ASIO_DEFINE_TAGGED_HANDLER_ALLOCATOR_PTR( \
      ::lslboost::asio::detail::thread_info_base::default_tag, op ) \
  /**/

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_HANDLER_ALLOC_HELPERS_HPP
