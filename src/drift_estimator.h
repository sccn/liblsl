#ifndef DRIFT_ESTIMATOR_H
#define DRIFT_ESTIMATOR_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <vector>

namespace lsl {

/**
 * Robust linear model of the clock offset over the recent time-sync rounds (opt-in drift
 * compensation, `[tuning] TimeDriftWindow`).
 *
 * Each round (one min-rtt probe) adds (local time, offset, rtt). Over the rounds of the last
 * `window` seconds the offset is fit with a Theil-Sen line (slope = median of the pairwise slopes,
 * intercept = median of the residuals), which ignores single outlier rounds, and predict()
 * extrapolates that line to the query time. Until the fit is usable (at least kMinRounds rounds
 * spanning kMinSpan seconds, |slope| <= kMaxSlope) predict() returns the latest round, i.e. the
 * behaviour without drift compensation.
 *
 * A median line resists a change of the clock rate for half a window. So when at least
 * kResetRounds consecutive rounds, spanning at least kResetSpan seconds, miss the prediction in
 * the same direction by more than max(kResetSigmas * sigma, kResetFloor) + (rtt - best rtt in
 * window) / 2 (sigma: robust spread of the fit residuals; the rtt term because a round with x
 * more rtt can be off by x/2 from asymmetric delays), the line no longer describes the clock pair
 * and the rounds before the first miss are dropped. Shorter excursions (a few rounds of a
 * different network path, seen for seconds at a time between macOS and Linux) are not a rate
 * change and are left to the median.
 * See docs/internals/time_sync.md for how the constants were chosen.
 *
 * Not thread-safe; the time_receiver guards it with its mutex.
 */
class drift_estimator {
public:
	static constexpr std::size_t kMinRounds = 5;
	static constexpr double kMinSpan = 6.0;		  // s
	static constexpr double kMaxSlope = 500e-6;	  // s/s
	static constexpr double kMinPairDt = 0.25;	  // s, closer pairs give too noisy a slope
	static constexpr double kResetSigmas = 5.0;	  //
	static constexpr double kResetFloor = 30e-6;  // s
	static constexpr int kResetRounds = 3;		  // consecutive misses that drop the history ...
	static constexpr double kResetSpan = 3.0;	  // s, ... if they also span this long
	static constexpr std::size_t kMaxRounds = 256; // bounds the O(n^2) slope computation

	explicit drift_estimator(double window) : window_(window) {}

	/// Add one round: local time t, offset (any consistent sign convention), round trip time.
	void add(double t, double offset, double rtt) {
		if (fitted_) {
			const double miss = offset - predict(t);
			const double threshold =
				std::max(kResetSigmas * sigma_, kResetFloor) + std::max(rtt - min_rtt_, 0.0) / 2;
			if (std::abs(miss) > threshold) {
				const bool positive = miss > 0;
				misses_ = (misses_ > 0 && positive == miss_positive_) ? misses_ + 1 : 1;
				miss_positive_ = positive;
				if (misses_ == 1) first_miss_t_ = t;
			} else
				misses_ = 0;
		} else
			misses_ = 0;
		rounds_.push_back({t, offset, rtt});
		while (!rounds_.empty() && (rounds_.front().t <= t - window_ || rounds_.size() > kMaxRounds))
			rounds_.pop_front();
		if (misses_ >= kResetRounds && t - first_miss_t_ >= kResetSpan) {
			while (rounds_.front().t < first_miss_t_) rounds_.pop_front();
			misses_ = 0;
		}
		fit();
	}

	/// The offset at local time t (the latest round's offset while no fit is usable).
	/// Only valid after at least one add() since the last clear().
	double predict(double t) const {
		return fitted_ ? intercept_ + slope_ * (t - t_ref_) : rounds_.back().offset;
	}

	bool empty() const { return rounds_.empty(); }
	bool fitted() const { return fitted_; }
	double slope() const { return slope_; }

	/// Forget everything, e.g. when the connection switched to another host.
	void clear() {
		rounds_.clear();
		fitted_ = false;
		misses_ = 0;
	}

private:
	struct round {
		double t, offset, rtt;
	};

	static double median(std::vector<double> &v) {
		const auto mid = v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2);
		std::nth_element(v.begin(), mid, v.end());
		if (v.size() % 2) return *mid;
		return (*mid + *std::max_element(v.begin(), mid)) / 2;
	}

	void fit() {
		fitted_ = false;
		const std::size_t n = rounds_.size();
		if (n < kMinRounds || rounds_.back().t - rounds_.front().t < kMinSpan) return;
		std::vector<double> v;
		v.reserve(n * (n - 1) / 2);
		for (std::size_t i = 0; i < n; i++)
			for (std::size_t j = i + 1; j < n; j++) {
				const double dt = rounds_[j].t - rounds_[i].t;
				if (dt >= kMinPairDt) v.push_back((rounds_[j].offset - rounds_[i].offset) / dt);
			}
		if (v.empty()) return;
		const double slope = median(v);
		if (!(std::abs(slope) <= kMaxSlope)) return;
		const double t_ref = rounds_.back().t;
		v.clear();
		for (const auto &r : rounds_) v.push_back(r.offset - slope * (r.t - t_ref));
		const double intercept = median(v);
		for (auto &x : v) x = std::abs(x - intercept);
		sigma_ = 1.4826 * median(v);
		min_rtt_ = std::min_element(rounds_.begin(), rounds_.end(), [](const round &a,
														const round &b) { return a.rtt < b.rtt; })
					   ->rtt;
		slope_ = slope;
		intercept_ = intercept;
		t_ref_ = t_ref;
		fitted_ = true;
	}

	double window_;
	std::deque<round> rounds_;
	bool fitted_{false};
	double slope_{0}, intercept_{0}, t_ref_{0}, sigma_{0}, min_rtt_{0}, first_miss_t_{0};
	int misses_{0};
	bool miss_positive_{false};
};

} // namespace lsl

#endif
