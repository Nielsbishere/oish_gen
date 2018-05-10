#pragma once

#include <chrono>

#include "types/generic.h"
#include "types/string.h"

namespace oi {

	typedef std::chrono::high_resolution_clock::time_point CTP;

	struct TimePoint {
		CTP point;
		String name;

		TimePoint(CTP time, String n);
	};

	class Timer {

	public:

		Timer();
		void lap(String lap);
		void stop();

		void print() const;
		flp getDuration() const;

		flp count(flp previous) const;

		static const Timer &getGlobalTimer();
		static CTP getTime();

	private:

		static Timer global;

		static flp duration(CTP t0, CTP t1);

		std::vector<TimePoint> timePoints;
		CTP start;
		CTP end;
		bool isActive;
	};
}