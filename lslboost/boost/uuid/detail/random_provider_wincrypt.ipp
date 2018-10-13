/* boost uuid/detail/random_provider_wincrypt implementation
*
* Copyright Jens Maurer 2000
* Copyright 2007 Andy Tompkins.
* Copyright Steven Watanabe 2010-2011
* Copyright 2017 James E. King III
*
* Distributed under the Boost Software License, Version 1.0. (See
* accompanying file LICENSE_1_0.txt or copy at
* http://www.boost.org/LICENSE_1_0.txt)
*
* $Id$
*/

#include <cstddef>
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/move/core.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/winapi/crypt.hpp>
#include <boost/winapi/get_last_error.hpp>
#include <boost/throw_exception.hpp>

#if defined(BOOST_UUID_FORCE_AUTO_LINK) || (!defined(BOOST_ALL_NO_LIB) && !defined(BOOST_UUID_RANDOM_PROVIDER_NO_LIB))
#   if defined(_WIN32_WCE)
#      define BOOST_LIB_NAME "coredll"
#   else
#      define BOOST_LIB_NAME "advapi32"
#   endif
#   define BOOST_AUTO_LINK_NOMANGLE
#   include <boost/config/auto_link.hpp>
#   undef BOOST_AUTO_LINK_NOMANGLE
#endif

namespace lslboost {
namespace uuids {
namespace detail {

class random_provider_base
{
    BOOST_MOVABLE_BUT_NOT_COPYABLE(random_provider_base)

public:
    random_provider_base()
        : hProv_(0)
    {
        lslboost::winapi::BOOL_ res = lslboost::winapi::CryptAcquireContextW(
            &hProv_,
            NULL,
            NULL,
            lslboost::winapi::PROV_RSA_FULL_,
            lslboost::winapi::CRYPT_VERIFYCONTEXT_ | lslboost::winapi::CRYPT_SILENT_);
        if (BOOST_UNLIKELY(!res))
        {
            lslboost::winapi::DWORD_ err = lslboost::winapi::GetLastError();
            BOOST_THROW_EXCEPTION(entropy_error(err, "CryptAcquireContext"));
        }
    }

    random_provider_base(BOOST_RV_REF(random_provider_base) that) BOOST_NOEXCEPT : hProv_(that.hProv_)
    {
        that.hProv_ = 0;
    }

    random_provider_base& operator= (BOOST_RV_REF(random_provider_base) that) BOOST_NOEXCEPT
    {
        destroy();
        hProv_ = that.hProv_;
        that.hProv_ = 0;
        return *this;
    }

    ~random_provider_base() BOOST_NOEXCEPT
    {
        destroy();
    }

    //! Obtain entropy and place it into a memory location
    //! \param[in]  buf  the location to write entropy
    //! \param[in]  siz  the number of bytes to acquire
    void get_random_bytes(void *buf, std::size_t siz)
    {
        lslboost::winapi::BOOL_ res = lslboost::winapi::CryptGenRandom(
            hProv_,
            lslboost::numeric_cast<lslboost::winapi::DWORD_>(siz),
            static_cast<lslboost::winapi::BYTE_ *>(buf));
        if (BOOST_UNLIKELY(!res))
        {
            lslboost::winapi::DWORD_ err = lslboost::winapi::GetLastError();
            BOOST_THROW_EXCEPTION(entropy_error(err, "CryptGenRandom"));
        }
    }

private:
    void destroy() BOOST_NOEXCEPT
    {
        if (hProv_)
        {
            lslboost::ignore_unused(lslboost::winapi::CryptReleaseContext(hProv_, 0));
        }
    }

private:
    lslboost::winapi::HCRYPTPROV_ hProv_;
};

} // detail
} // uuids
} // boost
