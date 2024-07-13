#pragma once

#include <cstdint>
#include <cstdio>

class le_Time
{
public:
	enum class le_Time_Day : uint8_t
	{
		SUNDAY = 0,
		MONDAY = 1,
		TUESDAY = 2,
		WEDNESDAY = 3,
		THURSDAY = 4,
		FRIDAY = 5,
		SATURDAY = 6
	};

	enum class le_Time_Month : uint8_t
	{
		JANUARY = 0,
		FEBRUARY = 1,
		MARCH = 2,
		APRIL = 3,
		MAY = 4,
		JUNE = 5,
		JULY = 6,
		AUGUST = 7,
		SEPTEMBER = 8,
		OCTOBER = 9,
		NOVEMBER = 10,
		DECEMBER = 11
	};

	le_Time(uint32_t subSecondFraction, uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year)
	{
		// Set extrinsic variables
		this->uSubSecondFraction = subSecondFraction;
		this->uSubSecond = subSecond;
		this->uSecond = second;
		this->uMinute = minute;
		this->uHour = hour;
		this->uDay = day;
		this->uYear = year;
	}

	le_Time GetFuture(float seconds)
	{
		uint32_t subSeconds = (uint32_t)(seconds * this->uSubSecondFraction);
		le_Time futureTime = *this;
		futureTime.Update(subSeconds);
		return futureTime;
	}

	void Update(uint32_t subSeconds)
	{
		uint16_t year = this->uYear;
		uint16_t day = this->uDay;
		uint32_t hour = this->uHour;
		uint32_t minute = this->uMinute;
		uint32_t second = this->uSecond;
		uint32_t subSecond = this->uSubSecond;
		uint32_t subSecondFraction = this->uSubSecondFraction;

		// Increment
		subSecond += subSeconds;

		if (subSecond >= subSecondFraction)
		{
			second += subSecond / subSecondFraction;
			subSecond %= subSecondFraction;
		}

		if (second >= 60)
		{
			minute += second / 60;
			second %= 60;
		}

		if (minute >= 60)
		{
			hour += minute / 60;
			minute %= 60;
		}

		if (hour >= 24)
		{
			day += hour / 24;
			hour %= 24;
		}

		uint16_t daysInYear = GetDaysInYear(year + 1970);

		while (day >= daysInYear)
		{
			year++;
			day -= daysInYear;

			// Get new days in month
			daysInYear = GetDaysInYear(year + 1970);
		}

		this->uYear = (uint16_t)year;
		this->uDay = (uint16_t)day;
		this->uHour = (uint8_t)hour;
		this->uMinute = (uint8_t)minute;
		this->uSecond = (uint8_t)second;
		this->uSubSecond = (uint8_t)subSecond;
	}

	int32_t GetElapsedTimeSince(le_Time& other)
	{
		int32_t totalTime = 0;
		totalTime += ((int32_t)this->uSubSecond - (int32_t)other.uSubSecond) * 1000000 / (int32_t)this->uSubSecondFraction;
		totalTime += ((int32_t)this->uSecond - (int32_t)other.uSecond) * 1000000;
		totalTime += ((int32_t)this->uMinute - (int32_t)other.uMinute) * 60 * 1000000;
		totalTime += ((int32_t)this->uHour - (int32_t)other.uHour) * 3600 * 1000000;
		totalTime += ((int32_t)this->uDay - (int32_t)other.uDay) * 86400 * 1000000;
		return totalTime;
	}	

	int32_t Align(uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint8_t day, uint8_t year)
	{
		le_Time time(this->uSubSecondFraction, subSecond, second, minute, hour, day, year);
		
		int32_t drift = time.GetElapsedTimeSince(*this);

		this->uSubSecond = subSecond;
		this->uSecond = second;
		this->uMinute = minute;
		this->uHour = hour;
		this->uDay = day;
		this->uYear = year;

		return drift;
	}

	void PrintShortTime(char* buffer, uint32_t length)
	{
		uint8_t month;
		uint8_t monthDay;
		ConvertDayOfYearToMonthDay(this->uYear + 1970, this->uDay, &month, &monthDay);

		snprintf(buffer, length, "%4u-%02u-%02u %02u:%02u:%02u", this->uYear + 1970, month + 1, monthDay + 1, this->uHour, this->uMinute, this->uSecond);
	}

protected:
	inline bool IsLeapYear(uint16_t year) const
	{
		return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
	}

	inline uint8_t GetDaysInMonth(uint16_t year, uint8_t month) const
	{
		if (month == 1 && IsLeapYear(year)) return 29;
		return _daysInMonth[month];
	}

	inline uint16_t GetDaysInYear(uint16_t year) const
	{
		return IsLeapYear(year) ? 366 : 365;
	}

	// Function to convert day of the year to month and day of the month
	inline void ConvertDayOfYearToMonthDay(uint16_t year, uint16_t dayOfYear, uint8_t* month, uint8_t* day) {
		if (dayOfYear < 1 || dayOfYear > GetDaysInYear(year)) {
			// Invalid day of the year
			*month = 0;
			*day = 0;
			return;
		}

		*month = 0;
		while (dayOfYear > GetDaysInMonth(year, *month)) {
			dayOfYear -= GetDaysInMonth(year, *month);
			(*month)++;
		}
		*day = dayOfYear;
	}

	uint16_t uYear; // Year since 1970
	uint16_t uDay; // Year day
	uint8_t uHour;
	uint8_t uMinute;
	uint8_t uSecond;
	uint32_t uSubSecond;
	uint32_t uSubSecondFraction;

	static const uint8_t _daysInMonth[12];
};

const uint8_t le_Time::_daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
