#pragma once

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <memory>
#include <string>

namespace asio {
namespace ip {
class tcp;
class udp;
} // namespace ip
class io_context;
} // namespace asio

namespace eos {
class portable_oarchive;
class portable_iarchive;
} // namespace eos

namespace lsl {
/// shared pointers to various classes
using factory_p = std::shared_ptr<class factory>;
using sample_p = lslboost::intrusive_ptr<class sample>;
using send_buffer_p = std::shared_ptr<class send_buffer>;
using stream_info_impl_p = std::shared_ptr<class stream_info_impl>;
using io_context_p = std::shared_ptr<asio::io_context>;
using string_p = std::shared_ptr<std::string>;
using tcp_server_p = std::shared_ptr<class tcp_server>;
using udp_server_p = std::shared_ptr<class udp_server>;
} // namespace lsl
