#include <algorithm>
#include <chrono>
#include <iostream>
#include <lsl_cpp.h>
#include <map>
#include <thread>

/**
 * This example program shows how all streams that are currently present on the lab network can be
 * resolved and displayed. This is useful for browsing applications.
 */

int main(int argc, char *argv[]) {
	try {
		std::cout << "Here is a one-shot resolve of all current streams:" << std::endl;

		// discover all streams on the network
		std::vector<lsl::stream_info> results = lsl::resolve_streams();

		std::map<std::string, lsl::stream_info> found_streams;
		// display them
		for (auto &stream : results) {
			found_streams.emplace(std::make_pair(stream.uid(), stream));
			std::cout << stream.as_xml() << "\n\n";
		}

		std::cout << "Press any key to switch to the continuous resolver test: " << std::endl;
		std::cin.get();

		lsl::continuous_resolver r;
		while (true) {
			auto results = r.results();
			for (auto &stream : results) {
				auto uid = stream.uid();
				if (found_streams.find(uid) == found_streams.end()) {
					found_streams.emplace(std::make_pair(uid, stream));
					std::cout << "Found " << stream.name() << '@' << stream.hostname() << std::endl;
				}
			}
			auto n_missing = found_streams.size() - results.size();
			if (n_missing) {
				std::vector<std::string> missing;
				missing.reserve(n_missing);
				for (const auto &pair : found_streams)
					if (std::none_of(results.begin(), results.end(),
							[uid = pair.first](
								const lsl::stream_info &info) { return info.uid() == uid; })) {
						missing.push_back(pair.first);
						std::cout << "Lost " << pair.second.name() << '@' << pair.second.hostname()
								  << std::endl;
					}
				for (const auto &uid : missing) found_streams.erase(uid);
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	std::cout << "Press any key to exit. " << std::endl;
	std::cin.get();
	return 0;
}
