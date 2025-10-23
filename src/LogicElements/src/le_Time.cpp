#include "le_Time.hpp"

#ifdef _WIN32
#include <chrono>
#elif defined(__linux__)
#include <chrono>
#else
#include <mach/mach_time.h>
#include <ctime>
#endif

namespace LogicElements {

/**
 * @brief Array containing the number of days in each month for a non-leap year.
 */
const uint8_t Time::_daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
 * @brief Constructor to initialize the Time object with the specified date and time parameters.
 * @param subSecondFraction The sub-second fraction part of the time.
 * @param subSecond The sub-second part of the time.
 * @param second The second part of the time.
 * @param minute The minute part of the time.
 * @param hour The hour part of the time.
 * @param day The day part of the time (day of the year).
 * @param year The year part of the time (years since 1970).
 */
Time::Time(uint32_t subSecondFraction, uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year)
{
    // Note: subSecondFraction is now static, but we keep the parameter for API compatibility
    // and use it to initialize subSecond if needed
    this->uSubSecond = subSecond;
    this->uSecond = second;
    this->uMinute = minute;
    this->uHour = hour;
    this->uDay = day;
    this->uYear = year;

    this->Update(0);
}

 /**
  * @brief Default constructor to initialize the Time object with zero values.
  */
 Time::Time() : Time(SUBSECOND_FRACTION, 0, 0, 0, 0, 0, 0) {}

/**
 * @brief Gets a future Time object based on the current time plus the specified seconds.
 * @param seconds The number of seconds to add to the current time.
 * @return A new Time object representing the future time.
 */
Time Time::GetFuture(float seconds)
{
    uint32_t subSeconds = (uint32_t)(seconds * SUBSECOND_FRACTION);
    Time futureTime = *this;
    futureTime.Update(subSeconds);
    return futureTime;
}

/**
 * @brief Updates the current Time object by adding the specified number of sub-seconds.
 * @param subSeconds The number of sub-seconds to add to the current time.
 */
void Time::Update(uint32_t subSeconds)
{
    uint16_t year = this->uYear;
    uint16_t day = this->uDay;
    uint32_t hour = this->uHour;
    uint32_t minute = this->uMinute;
    uint32_t second = this->uSecond;
    uint32_t subSecond = this->uSubSecond;

    // Increment sub-second
    subSecond += subSeconds;

    // Adjust seconds, minutes, hours, and days as needed
    if (subSecond >= SUBSECOND_FRACTION)
    {
        second += subSecond / SUBSECOND_FRACTION;
        subSecond %= SUBSECOND_FRACTION;
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
        daysInYear = GetDaysInYear(year + 1970);
    }

    this->uYear = (uint16_t)year;
    this->uDay = (uint16_t)day;
    this->uHour = (uint8_t)hour;
    this->uMinute = (uint8_t)minute;
    this->uSecond = (uint8_t)second;
    this->uSubSecond = subSecond; // FIX: Don't truncate to uint8_t!
}

/**
 * @brief Subtracts the specified Time object from the current Time object.
 * @param other The reference Time object to subtract.
 * @return The elapsed time in microseconds.
 */
int64_t Time::operator-(const Time& other) const
{
    // Convert both times to microseconds since epoch and subtract
    int64_t thisMicros = static_cast<int64_t>(this->ToMicrosecondsSinceEpoch());
    int64_t otherMicros = static_cast<int64_t>(other.ToMicrosecondsSinceEpoch());
    return thisMicros - otherMicros;
}

/**
 * @brief Adds the specified Time object to the current Time object.
 * @param other The reference Time object to add.
 * @return A new Time object representing the sum of the two times.
 */
Time Time::operator+(const Time& other) const
{
    Time result = *this;
    result.uSubSecond += other.uSubSecond;
    if (result.uSubSecond >= SUBSECOND_FRACTION)
    {
        result.uSecond += result.uSubSecond / SUBSECOND_FRACTION;
        result.uSubSecond %= SUBSECOND_FRACTION;
    }

    result.uSecond += other.uSecond;
    if (result.uSecond >= 60)
    {
        result.uMinute += result.uSecond / 60;
        result.uSecond %= 60;
    }

    result.uMinute += other.uMinute;
    if (result.uMinute >= 60)
    {
        result.uHour += result.uMinute / 60;
        result.uMinute %= 60;
    }

    result.uHour += other.uHour;
    if (result.uHour >= 24)
    {
        result.uDay += result.uHour / 24;
        result.uHour %= 24;
    }

    result.uDay += other.uDay;
    uint16_t daysInYear = result.GetDaysInYear(result.uYear + 1970);
    while (result.uDay >= daysInYear)
    {
        result.uYear++;
        result.uDay -= daysInYear;
        daysInYear = result.GetDaysInYear(result.uYear + 1970);
    }

    result.uYear += other.uYear;

    return result;
}

/**
 * @brief Aligns the current Time object to the specified time components and calculates the drift.
 * @param subSecond The new sub-second part of the time.
 * @param second The new second part of the time.
 * @param minute The new minute part of the time.
 * @param hour The new hour part of the time.
 * @param day The new day part of the time.
 * @param year The new year part of the time.
 * @return The drift in microseconds.
 */
int32_t Time::Align(uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year)
{
    Time time(SUBSECOND_FRACTION, subSecond, second, minute, hour, day, year);
    int64_t drift = time - (*this);

    this->uSubSecond = subSecond;
    this->uSecond = second;
    this->uMinute = minute;
    this->uHour = hour;
    this->uDay = day;
    this->uYear = year;

    return static_cast<int32_t>(drift);
}

/**
 * @brief Prints a short representation of the current time into the provided buffer.
 * @param buffer The buffer to print the time into.
 * @param length The length of the buffer.
 * 
 * OPTIMIZED: Minimized function calls and improved formatting
 */
uint16_t Time::PrintShortTime(char* buffer, uint32_t length)
{
    uint8_t month;
    uint8_t monthDay;
    ConvertDayOfYearToMonthDay(this->uYear + 1970, this->uDay, &month, &monthDay);

    return snprintf(buffer, length, "%4u-%02u-%02u %02u:%02u:%02u", 
                    this->uYear + 1970, month + 1, monthDay + 1, 
                    this->uHour, this->uMinute, this->uSecond);
}

/**
 * @brief Gets the current time as an Time object.
 * @return A new Time object representing the current time.
 */
Time Time::GetTime()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    uint32_t subSecond = static_cast<uint32_t>(nanoseconds % SUBSECOND_FRACTION);
    time_t seconds = nanoseconds / SUBSECOND_FRACTION;
    
    // Use thread-safe time conversion
    struct tm tm_info;
#ifdef _WIN32
    gmtime_s(&tm_info, &seconds);
#else
    gmtime_r(&seconds, &tm_info);
#endif
    
    uint8_t second = static_cast<uint8_t>(tm_info.tm_sec);
    uint8_t minute = static_cast<uint8_t>(tm_info.tm_min);
    uint8_t hour = static_cast<uint8_t>(tm_info.tm_hour);
    uint16_t day = static_cast<uint16_t>(tm_info.tm_yday);
    uint16_t year = static_cast<uint16_t>(tm_info.tm_year + 1900 - 1970);

    return Time(SUBSECOND_FRACTION, subSecond, second, minute, hour, day, year);
}

// NOTE: IsLeapYear(), GetDaysInMonth(), and GetDaysInYear() are now implemented
// inline in the header file for maximum performance (zero function call overhead)

/**
 * @brief Converts the day of the year to the corresponding month and day of the month.
 * @param year The year.
 * @param dayOfYear The day of the year.
 * @param month Pointer to store the resulting month.
 * @param day Pointer to store the resulting day of the month.
 */
inline void Time::ConvertDayOfYearToMonthDay(uint16_t year, uint16_t dayOfYear, uint8_t* month, uint8_t* day) const
{
    if (dayOfYear >= GetDaysInYear(year))
    {
        // Invalid day of the year
        *month = 0;
        *day = 0;
        return;
    }

    *month = 0;
    uint16_t remainingDays = dayOfYear;
    while (remainingDays >= GetDaysInMonth(year, *month))
    {
        remainingDays -= GetDaysInMonth(year, *month);
        (*month)++;
    }
    *day = (uint8_t)remainingDays;
}

/**
 * @brief Checks if the specified time has elapsed compared to the current time.
 * @param other The reference Time object to compare.
 * @return True if the specified time has elapsed, false otherwise.
 */
bool Time::HasElapsed(const Time& other) const
{
    return (*this - other) >= 0;
}

/**
 * @brief Converts the Time object to microseconds since the epoch (1970-01-01 00:00:00).
 * @return The number of microseconds since the epoch.
 * 
 * OPTIMIZED: O(1) complexity using mathematical formula instead of O(n) loop
 */
uint64_t Time::ToMicrosecondsSinceEpoch() const
{
    // Fast calculation of days since epoch using leap year formula
    // Leap years: divisible by 4, except century years (unless divisible by 400)
    
    uint32_t fullYear = 1970 + this->uYear;
    
    // Calculate total days from 1970 to start of current year
    // Formula: (years * 365) + leap_days - adjustment_for_century_years
    uint64_t yearsSince1970 = this->uYear;
    
    // Calculate leap years from 1970 to current year (not inclusive of current year)
    // Every 4 years is a leap year, except centuries (unless divisible by 400)
    uint32_t leapYears = 0;
    if (yearsSince1970 > 0) {
        uint32_t year1 = 1970;
        uint32_t year2 = fullYear - 1; // Up to but not including current year
        
        // Count leap years in range [year1, year2]
        auto countLeaps = [](uint32_t y) -> uint32_t {
            return (y / 4) - (y / 100) + (y / 400);
        };
        
        leapYears = countLeaps(year2) - countLeaps(year1 - 1);
    }
    
    uint64_t totalDays = (yearsSince1970 * 365ULL) + static_cast<uint64_t>(leapYears) + static_cast<uint64_t>(this->uDay);
    
    // Convert to microseconds (avoid overflow by casting each component)
    uint64_t totalMicroseconds = totalDays * 86400ULL * 1000000ULL;
    totalMicroseconds += this->uHour * 3600ULL * 1000000ULL;
    totalMicroseconds += this->uMinute * 60ULL * 1000000ULL;
    totalMicroseconds += this->uSecond * 1000000ULL;
    totalMicroseconds += this->uSubSecond / (SUBSECOND_FRACTION / 1000000ULL);

    return totalMicroseconds;
}

/**
 * @brief Converts the Time object to nanoseconds since the epoch (1970-01-01 00:00:00).
 * @return The number of nanoseconds since the epoch.
 * 
 * OPTIMIZED: O(1) complexity using mathematical formula instead of O(n) loop
 */
uint64_t Time::ToNanosecondsSinceEpoch() const
{
    // Fast calculation of days since epoch using leap year formula
    // Leap years: divisible by 4, except century years (unless divisible by 400)
    
    uint32_t fullYear = 1970 + this->uYear;
    
    // Calculate total days from 1970 to start of current year
    uint64_t yearsSince1970 = this->uYear;
    
    // Calculate leap years from 1970 to current year (not inclusive of current year)
    uint32_t leapYears = 0;
    if (yearsSince1970 > 0) {
        uint32_t year1 = 1970;
        uint32_t year2 = fullYear - 1; // Up to but not including current year
        
        // Count leap years in range [year1, year2]
        auto countLeaps = [](uint32_t y) -> uint32_t {
            return (y / 4) - (y / 100) + (y / 400);
        };
        
        leapYears = countLeaps(year2) - countLeaps(year1 - 1);
    }
    
    uint64_t totalDays = (yearsSince1970 * 365ULL) + static_cast<uint64_t>(leapYears) + static_cast<uint64_t>(this->uDay);
    
    // Convert to nanoseconds (avoid overflow by casting each component)
    uint64_t totalNanoseconds = totalDays * 86400ULL * 1000000000ULL;
    totalNanoseconds += this->uHour * 3600ULL * 1000000000ULL;
    totalNanoseconds += this->uMinute * 60ULL * 1000000000ULL;
    totalNanoseconds += this->uSecond * 1000000000ULL;
    totalNanoseconds += this->uSubSecond;

    return totalNanoseconds;
}

} // namespace LogicElements
