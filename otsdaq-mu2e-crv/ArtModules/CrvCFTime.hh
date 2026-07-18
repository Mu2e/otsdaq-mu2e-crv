// Constant-fraction timing for CRV waveforms
// Baseline from first sample, amplitude = peak - baseline,
// threshold = baseline + fraction * amplitude.
// Linearly interpolates between samples on the leading edge.
// Returns time within the waveform in ns (fractional_sample * digitizationPeriod).
// Caller adds startTDC * digitizationPeriod for the absolute time.

#ifndef CRV_CF_TIME_HH
#define CRV_CF_TIME_HH

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace crv
{

constexpr double kDigitizationPeriodNs = 12.5;  // ns per TDC count / ADC sample

struct CFResult
{
	double time_ns{
	    std::numeric_limits<double>::quiet_NaN()};  // time within waveform [ns]
	bool    valid{false};
	int16_t baseline{0};
	int16_t peak{0};
};

inline CFResult cfTime(const std::vector<int16_t>& adcs,
                       double                      fraction     = 0.20,
                       int                         minAmplitude = 0,
                       double digitizationPeriod                = kDigitizationPeriodNs)
{
	CFResult r;
	if(adcs.size() < 3)
		return r;

	r.baseline = adcs[0];

	auto it = std::max_element(adcs.begin(), adcs.end());
	r.peak  = *it;

	double amplitude = static_cast<double>(r.peak) - r.baseline;
	if(amplitude <= 0 || amplitude < minAmplitude)
		return r;

	double threshold = r.baseline + fraction * amplitude;

	std::size_t peakIdx = static_cast<std::size_t>(std::distance(adcs.begin(), it));

	for(std::size_t i = 1; i <= peakIdx; ++i)
	{
		if(adcs[i] >= threshold && adcs[i - 1] < threshold)
		{
			double denom = static_cast<double>(adcs[i]) - adcs[i - 1];
			double frac  = (denom != 0.0) ? (threshold - adcs[i - 1]) / denom : 0.0;
			r.time_ns    = ((i - 1) + frac) * digitizationPeriod;
			r.valid      = true;
			return r;
		}
	}

	return r;
}

}  // namespace crv

#endif  // CRV_CF_TIME_HH
