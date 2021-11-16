#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/signal_set.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <list>
#include <signal.h>
#include <string>

// LSL outlet stress tester. Run with `./blackhole --help` for more information.

using namespace std::chrono_literals;
using Endpoint = asio::ip::tcp::endpoint;

using socket_t = asio::basic_stream_socket<asio::ip::tcp, asio::io_context::executor_type>;
using Clock = std::chrono::high_resolution_clock;

static char buf[1024 * 1024];
const char handshake[] = "LSL:streamfeed/110 \nMax-buffer-length: 360\r\n\r\n";

class inlet_socket {
private:
	socket_t sock;

public:
	std::size_t bytes_read{0};

	inlet_socket(asio::io_context &ctx, Endpoint endpoint) : sock(ctx) {
		sock.async_connect(endpoint, [this](const asio::error_code &ec) {
			switch (ec.value()) {
			case 0:
				asio::write(sock, asio::const_buffer(handshake, sizeof(handshake) - 1));
				schedule_receive(0);
				break;

			case asio::error::interrupted:
				std::cout << "Connect was interrupted…" << std::endl;
				break;
			case asio::error::connection_refused:
			default: throw std::runtime_error("Connection refused");
			}
		});
	}
	void schedule_receive(std::size_t read) {
		bytes_read += read;
		sock.async_read_some(
			asio::buffer(buf, sizeof(buf)), [this](const asio::error_code &ec, std::size_t read) {
				if (!ec) this->schedule_receive(read);
			});
	}
};

class BlackHoleContainer {
	asio::io_context &ctx;
	asio::signal_set signals;
	Clock::time_point last_print;
	std::list<inlet_socket> sockets;
	Endpoint endpoint;

public:
	BlackHoleContainer(asio::io_context &ctx, Endpoint endpoint)
		: ctx(ctx), signals(ctx), last_print(Clock::now()), endpoint(endpoint) {
		for (int signal : {SIGUSR1, SIGUSR2, SIGTERM, SIGINT, SIGCONT}) signals.add(signal);

		add_socket();
		handle_signal(0);
	}
	void add_socket() { sockets.emplace_back(ctx, endpoint); }
	void handle_signal(int signal) {
		auto t = std::chrono::high_resolution_clock::now();
		if (signal) {
			auto time_passed =
				std::chrono::duration_cast<std::chrono::milliseconds>(t - last_print);
			std::cout.precision(1);
			std::cout.setf(std::ios::fixed, std::ios::floatfield);
			std::cout << time_passed.count() << ' ';
			double total_bandwidth = 0;
			for (auto &sock : sockets) {
				double inlet_bandwidth = (sock.bytes_read / 1.024 / 1024 / time_passed.count());
				total_bandwidth += inlet_bandwidth;
				std::cout << inlet_bandwidth << ' ';
				sock.bytes_read = 0;
			}
			std::cout << total_bandwidth << std::endl;
			if (signal == SIGUSR2) add_socket();
			if (signal == SIGTERM || signal == SIGINT) exit(0);
		}

		last_print = t;
		signals.async_wait(
			[this](const asio::error_code &ec, int signal) { this->handle_signal(signal); });
	}
};

/// Parse an address specification like "10.0.0.1:16572", "::1:16574"
/// or (for link-local addresses with scope IDs) "
/// or just a port (16573) into an endpoint
Endpoint parse_addr(std::string addr,
	asio::ip::address default_address = asio::ip::address_v4::loopback(),
	uint16_t default_port = 16572) {
	if (!addr.empty()) {
		auto pos = addr.find_last_of(':');
		if (pos == std::string::npos)
			default_port = std::stoi(addr);
		else {
			asio::error_code ec;
			auto addr_part = addr.substr(0, pos);
			default_address = asio::ip::make_address(addr_part, ec);
			if (ec) throw std::invalid_argument("Invalid IP address " + addr_part);
			default_port = std::stoi(addr.substr(pos + 1));
		}
	}
	return {default_address, default_port};
}

int main(int argc, char **argv) {
	if (argc > 1 && (argv[1] == std::string("-h") || argv[1] == std::string("--help"))) {
		std::cout
			<< "LSL outlet stress tester\n"
			<< "Usage: " << argv[0] << " [address=16572] [inlet_count=1]\n\n"
			<< "address can be either a port or an address and port (127.0.0.1:16572)\n"
			<< "Link-local addresses need a scope id, e.g. fe80::264b:feff:fe2d:f4bc%3:16573\n\n"
			<< "While running, several signals are handled:"
			<< "SIGCONT (Ctrl+Q)\n"
			<< "\tSIGUSR1\t\tPrint current stats\n"
			<< "\tSIGUSR2\t\tAdd another inlet and print current stats\n"
			<< "\tSIGINT (Ctrl+C)\tPrint stats & quit\n";
	}
	Endpoint endpoint = parse_addr(argc > 1 ? argv[1] : "");
	const int startnum = argc > 2 ? std::stoi(argv[2]) : 1;

	std::cout << "Connecting to " << endpoint << std::endl;

	asio::io_context ctx;
	std::list<inlet_socket> sockets;

	BlackHoleContainer cnt(ctx, endpoint);

	for (int i = 1; i < startnum; ++i) cnt.add_socket();

	ctx.run();
}
