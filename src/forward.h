#pragma once

#include <boost/version.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>

// In Boost>=1.66, io_service is typedef'ed
namespace lslboost {
class thread;
namespace system { class error_code; }
namespace asio {
namespace ip { class tcp; class udp;}
#if BOOST_VERSION >= 106600
	class io_context;
	typedef io_context io_service;
#else
	class io_service;
#endif
} }

namespace eos {
	class portable_oarchive;
	class portable_iarchive;
}

namespace lsl {
	/// shared pointers to various classes
	typedef lslboost::shared_ptr<class consumer_queue> consumer_queue_p;
	typedef lslboost::shared_ptr<class factory> factory_p;
	typedef lslboost::shared_ptr<class resolve_attempt_udp> resolve_attempt_udp_p;
	typedef lslboost::intrusive_ptr<class sample> sample_p;
	typedef lslboost::shared_ptr<class send_buffer> send_buffer_p;
	typedef lslboost::shared_ptr<class stream_info_impl> stream_info_impl_p;
	typedef lslboost::shared_ptr<lslboost::asio::io_service> io_service_p;
	typedef lslboost::shared_ptr<class tcp_server> tcp_server_p;
	typedef lslboost::shared_ptr<class udp_server> udp_server_p;
}
