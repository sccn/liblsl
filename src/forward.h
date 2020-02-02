#pragma once

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/version.hpp>
#include <memory>

namespace lslboost {
	class thread;
	namespace system {
		class error_code;
	}
	namespace asio {
		namespace ip {
			class tcp;
			class udp;
		} // namespace ip
		class io_context;
	} // namespace asio
} // namespace lslboost

namespace eos {
	class portable_oarchive;
	class portable_iarchive;
} // namespace eos

namespace lsl {
	/// shared pointers to various classes
using consumer_queue_p = std::shared_ptr<class consumer_queue>;
using factory_p = std::shared_ptr<class factory>;
using resolve_attempt_udp_p = std::shared_ptr<class resolve_attempt_udp>;
using sample_p = lslboost::intrusive_ptr<class sample>;
using send_buffer_p = std::shared_ptr<class send_buffer>;
using stream_info_impl_p = std::shared_ptr<class stream_info_impl>;
using io_context_p = std::shared_ptr<lslboost::asio::io_context>;
using tcp_server_p = std::shared_ptr<class tcp_server>;
using udp_server_p = std::shared_ptr<class udp_server>;
} // namespace lsl
