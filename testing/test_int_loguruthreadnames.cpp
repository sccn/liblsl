#include <loguru.hpp>
#include <thread>

#include <catch2/catch.hpp>

TEST_CASE("loguru_thread_local_storage", "[threading]") {
	char name[2] = "0";
	loguru::set_thread_name("1");
	std::thread([&](){
		loguru::set_thread_name("2");
		loguru::get_thread_name(name,2,false);
	}).join();
	REQUIRE(name[0] == '2');
	loguru::get_thread_name(name,2,false);
	REQUIRE(name[0] == '1');
}
