// This puts most asio functions in this object file instead of everywhere
// it is called when compiled with BOOST_ASIO_SEPARATE_COMPILATION in lslboost
// Normally, one would just compile boost/asio/impl/src.hpp but some compilers
// think they should generate a precompiled header.

#include <asio/impl/src.hpp>
