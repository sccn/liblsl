#include "sample.h"
#include <catch2/catch.hpp>
#include <fstream>
#include <sstream>
#include <type_traits>

// clazy:excludeall=non-pod-global-static

#define NO_EXPLICIT_TEMPLATE_INSTANTIATION
#include "portable_archive/portable_iarchive.hpp"
#include "portable_archive/portable_oarchive.hpp"

static lsl::factory doublefac{cft_double64, 4, 1}, strfac{cft_string, 4, 1};

struct Testclass {
	std::string teststr;
	double testdouble{0};
	uint64_t testbigint{0};
	int32_t negativeint{0}, testint{0};
	lsl::sample_p s1, s2;
	char testchar{0};

	template <typename Archive> void serialize_(Archive &a) {
		a &testchar &testint &negativeint &testbigint &testdouble &teststr &*s1 &*s2;
	}
	void serialize(eos::portable_iarchive &a, uint32_t) { serialize_(a); }

	void serialize(eos::portable_oarchive &a, uint32_t) const {
		const_cast<Testclass *>(this)->serialize_(a);
	}

	Testclass() : s1(doublefac.new_sample(0.0, false)), s2(strfac.new_sample(0.0, false)) {}
	Testclass(bool /*dummy*/)
		: teststr("String\x00with\x00nulls"), testdouble(17.3), testbigint(0xff), negativeint(-1),
		  testint(0x00abcdef), s1(doublefac.new_sample(17.3, true)),
		  s2(strfac.new_sample(18.3, true)), testchar('a') {
		s1->assign_test_pattern(2);
		s2->assign_test_pattern(4);
	}
};

struct Testclass2 {
	uint64_t i;

	void serialize(eos::portable_iarchive &a, uint32_t) { a &i; }
	void serialize(eos::portable_oarchive &a, uint32_t) const { a &i; }
};

TEST_CASE("v100 protocol serialization", "[basic][serialization]") {
	Testclass out1(true);
	Testclass2 out2, out3;
	out2.i = out3.i = 0x00ab00cd00ef0012u;
	std::stringbuf osb(std::ios::out);
	{
		eos::portable_oarchive outarch(osb);
		outarch << out2 << out3;
		outarch << std::string("Testclass") << out1;
	}

	// Preserialized, generate with
	// python -c "import re;print(re.sub('(?<=x[0-9a-f]{2})[0-9a-f]', r'\"\\n\"\\g<0>',
	// str(open('serialization_found.bin','rb').read())))"
	static const char binpacket[] =
		"\x7f\x01\t\x00\x00\x07\x12\x00\xef\x00\xcd\x00\xab\x07\x12\x00\xef"
		"\x00\xcd\x00\xab\x01\t"
		"Testclass\x00\x00\x01"
		"a\x03\xef\xcd\xab\xff\xff\x01\xff\x08\xcd\xcc\xcc\xcc\xccL1@\x01\x06"
		"String\x00\x00\x01\x02\x08\xc9v\xbe\x9f\x0c$\xfe@\x08\x00\x00\x00"
		"0\x00\x00pA\x08\x00\x00\x00@\x00\x00p\xc1\x08\x00\x00\x00P\x00\x00pA"
		"\x08\x00\x00\x00`\x00\x00p\xc1\x01\x02\x08\xc9v\xbe\x9f\x0c$\xfe@\x01\x02"
		"10\x01\x03-11\x01\x02"
		"12\x01\x03-13";
	const std::string preserialized(binpacket, sizeof(binpacket) - 1);
	std::string res(osb.str());

	if (res.size() != preserialized.size() || res != preserialized) {
		// dump mismatching data for further analysis
		std::ofstream("serialization_expected.bin", std::ios::binary) << preserialized;
		std::ofstream("serialization_found.bin", std::ios::binary) << res;
		FAIL();
	}

	std::stringbuf isb(preserialized, std::ios::in);
	try {
		eos::portable_iarchive inarch(isb);
		Testclass in1;
		Testclass2 in2, in3;
		inarch >> in2 >> in3;
		REQUIRE(in2.i == 0x00ab00cd00ef0012u);
		REQUIRE(in3.i == 0x00ab00cd00ef0012u);

		std::string teststr;
		inarch >> teststr >> in1;
		REQUIRE(teststr == std::string("Testclass"));
		REQUIRE(in1.testdouble == Approx(out1.testdouble));
		REQUIRE(in1.testbigint == out1.testbigint);
		REQUIRE(in1.testchar == out1.testchar);
		REQUIRE(in1.negativeint == out1.negativeint);
		REQUIRE(in1.testint == out1.testint);
		REQUIRE(in1.teststr == out1.teststr);

		if (*in1.s1 != *out1.s1) FAIL("Sample 1 serialization mismatch");
		if (*in1.s2 != *out1.s2) FAIL("Sample 2 serialization mismatch");
	} catch (std::exception &e) { FAIL(e.what()); }
}
