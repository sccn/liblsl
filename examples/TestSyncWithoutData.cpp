#include <chrono> // std::chrono::seconds
#include <iostream>
#include <lsl_cpp.h>
#include <thread> // std::this_thread::sleep_for

int main(int argc, char *argv[]) {
	try {
		lsl::stream_info info("SyncTest", "Test", 1, lsl::IRREGULAR_RATE, lsl::cf_int16, "id23443");

		// make a new outlet
		lsl::stream_outlet outlet(info);

		auto found_stream_info = lsl::resolve_stream("name", "SyncTest");
		if (found_stream_info.empty()) throw std::runtime_error("Sender outlet not found!");
		lsl::stream_info si = found_stream_info[0];
		std::cout << "Found " << si.name() << '@' << si.hostname() << std::endl;

		lsl::stream_inlet inlet(si);

		// inlet.open_stream(2);
		// outlet.wait_for_consumers(2);

		std::thread push_thread{[&]() {
			std::this_thread::sleep_for(std::chrono::seconds(10));
			std::cout << "Pushing data now" << std::endl;
			for (int16_t i = 0; i < 10; ++i) {
				outlet.push_sample(&i);
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}};
		for (auto end = lsl::local_clock() + 20; lsl::local_clock() < end;) {
			try {
				std::cout << "Got time correction: " << inlet.time_correction(1) << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			} catch (std::exception &e) {
				std::cout << "Error getting time correction data: " << e.what() << std::endl;
			}
		}
		push_thread.join();
	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	return 0;
}
