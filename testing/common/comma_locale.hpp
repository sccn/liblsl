#pragma once
#include <clocale>
#include <string>

/// Switches LC_NUMERIC to a locale with a decimal comma for the lifetime of the object, as a
/// GUI toolkit calling setlocale(LC_ALL, "") does on such systems.
class CommaLocale {
	std::string previous_;
	bool active_{false};

public:
	CommaLocale() {
		if (const char *prev = std::setlocale(LC_NUMERIC, nullptr)) previous_ = prev;
		for (const char *name : {"de_DE.UTF-8", "da_DK.UTF-8", "fr_FR.UTF-8", "de_DE", "German"}) {
			if (!std::setlocale(LC_NUMERIC, name)) continue;
			if (*std::localeconv()->decimal_point == ',') {
				active_ = true;
				return;
			}
		}
		std::setlocale(LC_NUMERIC, previous_.c_str());
	}
	~CommaLocale() { std::setlocale(LC_NUMERIC, previous_.c_str()); }
	CommaLocale(const CommaLocale &) = delete;
	CommaLocale &operator=(const CommaLocale &) = delete;

	/// False if no such locale is installed.
	bool active() const { return active_; }
};
