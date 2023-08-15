#include "data_receiver.h"
#include "api_config.h"
#include "cancellable_streambuf.h"
#include "inlet_connection.h"
#include "sample.h"
#include "socket_utils.h"
#include "util/cast.hpp"
#include "util/endian.hpp"
#include "util/strfuns.hpp"
#include <chrono>
#include <exception>
#include <iostream>
#include <loguru.hpp>
#include <memory>
#include <string>
#include <vector>

// a convention that applies when including portable_oarchive.h in multiple .cpp files.
// otherwise, the templates are instantiated in this file and sample.cpp which leads
// to errors like "multiple definition of `typeinfo name"
#define NO_EXPLICIT_TEMPLATE_INSTANTIATION
#include "portable_archive/portable_iarchive.hpp"

namespace lsl {

data_receiver::data_receiver(inlet_connection &conn, int max_buflen, int max_chunklen)
	: conn_(conn),
	  sample_factory_(
		  new factory(conn.type_info().channel_format(), conn.type_info().channel_count(),
			  conn.type_info().nominal_srate()
				  ? static_cast<int>(conn.type_info().nominal_srate() *
									 api_config::get_instance()->inlet_buffer_reserve_ms() / 1000)
				  : api_config::get_instance()->inlet_buffer_reserve_samples())),
	  check_thread_start_(true), closing_stream_(false), connected_(false),
	  sample_queue_(max_buflen), max_buflen_(max_buflen), max_chunklen_(max_chunklen) {
	if (max_buflen < 0)
		throw std::invalid_argument("The max_buflen argument must not be smaller than 0.");
	if (max_chunklen < 0)
		throw std::invalid_argument("The max_chunklen argument must not be smaller than 0.");
	conn_.register_onlost(this, &connected_upd_);
}

data_receiver::~data_receiver() {
	try {
		conn_.unregister_onlost(this);
		if (data_thread_.joinable()) data_thread_.join();
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error during destruction of a data_receiver: %s", e.what());
	} catch (...) { LOG_F(ERROR, "Severe error during data receiver shutdown."); }
}


// === external access ===

void data_receiver::open_stream(double timeout) {
	closing_stream_ = false;
	std::unique_lock<std::mutex> lock(connected_mut_);
	auto connection_completed = [this]() { return connected_ || conn_.lost(); };
	if (!connection_completed()) {
		// start thread if not yet running
		if (check_thread_start_ && !data_thread_.joinable()) {
			data_thread_ = std::thread(&data_receiver::data_thread, this);
			check_thread_start_ = false;
		}
		// wait until the connection attempt completes (or we time out)
		if (timeout >= FOREVER)
			connected_upd_.wait(lock, connection_completed);
		else if (!connected_upd_.wait_for(
					 lock, std::chrono::duration<double>(timeout), connection_completed))
			throw timeout_error("The open_stream() operation timed out.");
	}
	if (conn_.lost())
		throw lost_error("The stream read by this inlet has been lost. To recover, you need to "
						 "re-resolve the source and re-create the inlet.");
}

void data_receiver::close_stream() {
	check_thread_start_ = true;
	closing_stream_ = true;
	cancel_all_registered();
}

sample_p lsl::data_receiver::try_get_next_sample(double timeout) {
	if (conn_.lost())
		throw lost_error("The stream read by this outlet has been lost. To recover, you need to "
						 "re-resolve the source and re-create the inlet.");
	// start data thread implicitly if necessary
	if (check_thread_start_ && !data_thread_.joinable()) {
		data_thread_ = std::thread(&data_receiver::data_thread, this);
		check_thread_start_ = false;
	}
	// get the sample with timeout
	if (sample_p s = sample_queue_.pop_sample(timeout))
		return s;
	if (conn_.lost())
		throw lost_error("The stream read by this inlet has been lost. To recover, you need to "
						 "re-resolve the source and re-create the inlet.");
	return nullptr;
}


template <class T>
double data_receiver::pull_sample_typed(T *buffer, uint32_t buffer_elements, double timeout) {
	if(sample_p s = try_get_next_sample(timeout))
	{
		if (buffer_elements != conn_.type_info().channel_count())
			throw std::range_error("The number of buffer elements provided does not match the "
								   "number of channels in the sample.");
		s->retrieve_typed(buffer);
		return s->timestamp();
	}
	return 0.0;
}

template double data_receiver::pull_sample_typed<char>(char *, uint32_t, double);
template double data_receiver::pull_sample_typed<int16_t>(int16_t *, uint32_t, double);
template double data_receiver::pull_sample_typed<int32_t>(int32_t *, uint32_t, double);
template double data_receiver::pull_sample_typed<int64_t>(int64_t *, uint32_t, double);
template double data_receiver::pull_sample_typed<float>(float *, uint32_t, double);
template double data_receiver::pull_sample_typed<double>(double *, uint32_t, double);
template double data_receiver::pull_sample_typed<std::string>(std::string *, uint32_t, double);

double data_receiver::pull_sample_untyped(void *buffer, int buffer_bytes, double timeout) {
	if(sample_p s = try_get_next_sample(timeout)) {
		if (buffer_bytes != conn_.type_info().sample_bytes())
			throw std::range_error("The size of the provided buffer does not match the number of "
								   "bytes in the sample.");
		s->retrieve_untyped(buffer);
		return s->timestamp();
	}
	return 0.0;
}


// === internal processing ===

void data_receiver::data_thread() {
	conn_.acquire_watchdog();
	loguru::set_thread_name((std::string("R_") += conn_.type_info().name().substr(0, 12)).c_str());
	// ensure that the sample factory persists for the lifetime of this thread
	factory_p factory(sample_factory_);
	try {
		while (!conn_.lost() && !conn_.shutdown() && !closing_stream_) {
			try {
				// --- connection setup ---

				// make a new stream buffer and a stream on top of it
				cancellable_streambuf buffer;
				buffer.register_at(&conn_);
				buffer.register_at(this);
				std::iostream server_stream(&buffer);
				std::unique_ptr<eos::portable_iarchive> inarch;
				// connect to endpoint
				buffer.connect(conn_.get_tcp_endpoint());
				if (buffer.error()) throw buffer.error();

				// --- protocol negotiation ---

				bool reverse_byte_order = false; // perform little <-> big endian conversion?
				int data_protocol_version = 100;  // which protocol version we shall use for data
												  // transmission (100=version 1.00)
				bool suppress_subnormals = false; // whether we shall suppress subnormal numbers

				// propose to use the highest protocol version supported by both parties
				int proposed_protocol_version =
					std::min(api_config::get_instance()->use_protocol_version(),
						conn_.type_info().version());
				if (proposed_protocol_version >= 110) {
					// request line LSL:streamfeed/[ProtocolVersion] [UID]\r\n
					server_stream << "LSL:streamfeed/" << proposed_protocol_version << " "
								  << conn_.current_uid() << "\r\n";
					// transmit request parameters
					server_stream << "Native-Byte-Order: " << LSL_BYTE_ORDER << "\r\n";
					server_stream << "Endian-Performance: "
								  << std::floor(measure_endian_performance()) << "\r\n";
					server_stream << "Has-IEEE754-Floats: "
								  << (format_ieee754[cft_float32] && format_ieee754[cft_double64])
								  << "\r\n";
					server_stream << "Supports-Subnormals: "
								  << format_subnormal[conn_.type_info().channel_format()] << "\r\n";
					server_stream << "Value-Size: " << conn_.type_info().channel_bytes()
								  << "\r\n"; // 0 for strings
					server_stream << "Data-Protocol-Version: " << proposed_protocol_version
								  << "\r\n";
					server_stream << "Max-Buffer-Length: " << max_buflen_ << "\r\n";
					server_stream << "Max-Chunk-Length: " << max_chunklen_ << "\r\n";
					server_stream << "Hostname: " << conn_.type_info().hostname() << "\r\n";
					server_stream << "Source-Id: " << conn_.type_info().source_id() << "\r\n";
					server_stream << "Session-Id: " << conn_.type_info().session_id() << "\r\n";
					server_stream << "\r\n" << std::flush;

					// check server response line (LSL/[Version] [StatusCode] [Message])
					char buf[16384] = {0};
					if (!server_stream.getline(buf, sizeof(buf)))
						throw lost_error("Connection lost.");
					std::vector<std::string> parts = splitandtrim(buf, ' ', false);
					if (parts.size() < 3 || parts[0].compare(0, 4, "LSL/") != 0)
						throw std::runtime_error("Received a malformed response.");
					if (std::stoi(parts[0].substr(4)) / 100 >
						api_config::get_instance()->use_protocol_version() / 100)
						throw std::runtime_error(
							"The other party's protocol version is too new for this client; please "
							"upgrade your LSL library.");
					int status_code = std::stoi(parts[1]);
					if (status_code == 404)
						throw lost_error("The given address does not serve the resolved stream "
										 "(likely outdated).");
					if (status_code >= 400)
						throw std::runtime_error(
							"The other party sent an error: " + std::string(buf));
					if (status_code >= 300)
						throw lost_error("The other party requested a redirect.");

					// receive response parameters
					while (server_stream.getline(buf, sizeof(buf)) && (buf[0] != '\r')) {
						std::string hdrline(buf);
						std::size_t colon = hdrline.find_first_of(':');
						if (colon != std::string::npos) {
							// strip off comments
							auto semicolon = hdrline.find_first_of(';');
							if (semicolon != std::string::npos) hdrline.erase(semicolon);
							// convert to lowercase
							for (auto &c : hdrline) c = ::tolower(c);
							// extract key & value
							std::string type = trim(hdrline.substr(0, colon)),
										rest = trim(hdrline.substr(colon + 1));
							// get the header information
							if (type == "byte-order") {
								int use_byte_order = std::stoi(rest);
								// needed for interoperability with liblsl ~1.13 and data protocol 100
								if(use_byte_order == 0) use_byte_order = LSL_BYTE_ORDER;
								auto value_size = format_sizes[conn_.type_info().channel_format()];
								if (!lsl::can_convert_endian(use_byte_order, value_size))
									throw std::runtime_error(
										"The byte order conversion requested by the other party is "
										"not supported.");
								reverse_byte_order = use_byte_order != LSL_BYTE_ORDER;
							}
							if (type == "suppress-subnormals")
								suppress_subnormals = lsl::from_string<bool>(rest);
							if (type == "uid" && rest != conn_.current_uid())
								throw lost_error("The received UID does not match the current "
												 "connection's UID.");
							if (type == "data-protocol-version") {
								data_protocol_version = std::stoi(rest);
								if (data_protocol_version >
									api_config::get_instance()->use_protocol_version())
									throw std::runtime_error(
										"The protocol version requested by the other party is not "
										"supported by this client.");
							}
						}
					}
					if (!server_stream) throw lost_error("Server connection lost.");
				} else {
					// version 1.00: send request line and feed parameters
					server_stream << "LSL:streamfeed\r\n";
					server_stream << max_buflen_ << " " << max_chunklen_ << "\r\n" << std::flush;
				}

				if (data_protocol_version == 100) {
					// portable binary archive (parse archive header)
					inarch = std::make_unique<eos::portable_iarchive>(*server_stream.rdbuf());
					// receive stream_info message from server
					std::string infomsg;
					*inarch >> infomsg;
					stream_info_impl info;
					info.from_shortinfo_message(infomsg);
					// confirm that the UID matches, otherwise reconnect
					if (info.uid() != conn_.current_uid())
						throw lost_error(
							"The received UID does not match the current connection's UID.");
				}

				// --- format validation ---
				{
					// receive and parse two subsequent test-pattern samples and check if they are
					// formatted as expected
					lsl::factory fac(
						conn_.type_info().channel_format(), conn_.type_info().channel_count(), 4);

					for (int test_pattern : {4, 2}) {
						lsl::sample_p expected(fac.new_sample(0.0, false)),
							received(fac.new_sample(0.0, false));
						expected->assign_test_pattern(test_pattern);
						if (data_protocol_version >= 110)
							received->load_streambuf(buffer, data_protocol_version,
								reverse_byte_order, suppress_subnormals);
						else
							*inarch >> *received;

						if (*expected.get() != *received.get())
							throw std::runtime_error(
								"The received test-pattern samples do not match the specification."
								" The protocol formats are likely incompatible.");
					}
				}

				// signal to accessor functions on other threads that the protocol negotiation has
				// been successful, so we're now connected (and remain to be even if we later
				// recover silently)
				{
					std::lock_guard<std::mutex> lock(connected_mut_);
					connected_ = true;
				}
				connected_upd_.notify_all();

				// --- transmission loop ---

				double last_timestamp = 0.0;
				double srate = conn_.current_srate();
				for (int k = 0; !conn_.lost() && !conn_.shutdown() && !closing_stream_; k++) {
					// allocate and fetch a new sample
					sample_p samp(factory->new_sample(0.0, false));
					if (data_protocol_version >= 110)
						samp->load_streambuf(
							buffer, data_protocol_version, reverse_byte_order, suppress_subnormals);
					else
						*inarch >> *samp;
					// deduce timestamp if necessary
					if (samp->timestamp() == DEDUCED_TIMESTAMP) {
						samp->timestamp() = last_timestamp;
						if (srate != IRREGULAR_RATE) samp->timestamp() += 1.0 / srate;
					}
					last_timestamp = samp->timestamp();
					// push it into the sample queue
					sample_queue_.push_sample(samp);
					// periodically update the last receive time to keep the watchdog happy
					if (srate <= 16 || (k & 0xF) == 0) conn_.update_receive_time(lsl_clock());
				}
			} catch (err_t) {
				// connection-level error: closed, reset, refused, etc.
				conn_.try_recover_from_error();
			} catch (lost_error &) {
				// another type of connection error
				conn_.try_recover_from_error();
			} catch (shutdown_error &) {
				// termination due to connection shutdown
				throw lost_error("The inlet has been disengaged.");
			} catch (std::exception &e) {
				// some perhaps more serious transmission or parsing error (could be indicative of a
				// protocol issue)
				if (!conn_.shutdown())
					LOG_F(ERROR, "Stream transmission broke off (%s); re-connecting...", e.what());
				conn_.try_recover_from_error();
			}
			// wait for a few msec so as to not spam the provider with reconnects
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	} catch (lost_error &) {
		// the connection was irrecoverably lost: since the pull_sample() function may
		// be waiting for the next sample we need to wake it up by passing a sentinel
		sample_queue_.push_sample(sample_p());
	}
	conn_.release_watchdog();
}

} // namespace lsl
