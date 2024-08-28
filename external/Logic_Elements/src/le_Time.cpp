#include "le_Time.hpp"

#ifdef _WIN32
#include <chrono>
#elif defined(__linux__)
#include <chrono>
#else
#include <mach/mach_time.h>
#include <ctime> // Add this line
#endif

/**
 * @brief Array containing the number of days in each month for a non-leap year.
 */
const uint8_t le_Time::_daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/**
 * @brief Constructor to initialize the le_Time object with the specified date and time parameters.
 * @param subSecondFraction The sub-second fraction part of the time.
 * @param subSecond The sub-second part of the time.
 * @param second The second part of the time.
 * @param minute The minute part of the time.
 * @param hour The hour part of the time.
 * @param day The day part of the time (day of the year).
 * @param year The year part of the time (years since 1970).
 */
le_Time::le_Time(uint32_t subSecondFraction, uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year)
{
    // Set extrinsic variables
    this->uSubSecondFraction = subSecondFraction;
    this->uSubSecond = subSecond;
    this->uSecond = second;
    this->uMinute = minute;
    this->uHour = hour;
    this->uDay = day;
    this->uYear = year;

    this->Update(0);
}

 /**
  * @brief Default constructor to initialize the le_Time object with zero values.
  */
 le_Time::le_Time() : le_Time(1000000000, 0, 0, 0, 0, 0, 0) {}

/**
 * @brief Gets a future le_Time object based on the current time plus the specified seconds.
 * @param seconds The number of seconds to add to the current time.
 * @return A new le_Time object representing the future time.
 */
le_Time le_Time::GetFuture(float seconds)
{
    uint32_t subSeconds = (uint32_t)(seconds * this->uSubSecondFraction);
    le_Time futureTime = *this;
    futureTime.Update(subSeconds);
    return futureTime;
}

/**
 * @brief Updates the current le_Time object by adding the specified number of sub-seconds.
 * @param subSeconds The number of sub-seconds to add to the current time.
 */
void le_Time::Update(uint32_t subSeconds)
{
    uint16_t year = this->uYear;
    uint16_t day = this->uDay;
    uint32_t hour = this->uHour;
    uint32_t minute = this->uMinute;
    uint32_t second = this->uSecond;
    uint32_t subSecond = this->uSubSecond;
    uint32_t subSecondFraction = this->uSubSecondFraction;

    // Increment sub-second
    subSecond += subSeconds;

    // Adjust seconds, minutes, hours, and days as needed
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
        daysInYear = GetDaysInYear(year + 1970);
    }

    this->uYear = (uint16_t)year;
    this->uDay = (uint16_t)day;
    this->uHour = (uint8_t)hour;
    this->uMinute = (uint8_t)minute;
    this->uSecond = (uint8_t)second;
    this->uSubSecond = (uint8_t)subSecond;
}

/**
 * @brief Subtracts the specified le_Time object from the current le_Time object.
 * @param other The reference le_Time object to subtract.
 * @return The elapsed time in microseconds.
 */
int32_t le_Time::operator-(le_Time& other)
{
    int32_t totalTime = 0;
    totalTime += ((int32_t)this->uSubSecond - (int32_t)other.uSubSecond) * 1000000 / (int32_t)this->uSubSecondFraction;
    totalTime += ((int32_t)this->uSecond - (int32_t)other.uSecond) * 1000000;
    totalTime += ((int32_t)this->uMinute - (int32_t)other.uMinute) * 60 * 1000000;
    totalTime += ((int32_t)this->uHour - (int32_t)other.uHour) * 3600 * 1000000;
    totalTime += ((int32_t)this->uDay - (int32_t)other.uDay) * 86400 * 1000000;
    return totalTime;
}

/**
 * @brief Adds the specified le_Time object to the current le_Time object.
 * @param other The reference le_Time object to add.
 * @return A new le_Time object representing the sum of the two times.
 */
le_Time le_Time::operator+(le_Time& other)
{
    le_Time result = *this;
    result.uSubSecond += other.uSubSecond;
    if (result.uSubSecond >= result.uSubSecondFraction)
    {
        result.uSecond += result.uSubSecond / result.uSubSecondFraction;
        result.uSubSecond %= result.uSubSecondFraction;
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
 * @brief Aligns the current le_Time object to the specified time components and calculates the drift.
 * @param subSecond The new sub-second part of the time.
 * @param second The new second part of the time.
 * @param minute The new minute part of the time.
 * @param hour The new hour part of the time.
 * @param day The new day part of the time.
 * @param year The new year part of the time.
 * @return The drift in microseconds.
 */
int32_t le_Time::Align(uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year)
{
    le_Time time(this->uSubSecondFraction, subSecond, second, minute, hour, day, year);
    int32_t drift = time - (*this);

    this->uSubSecond = subSecond;
    this->uSecond = second;
    this->uMinute = minute;
    this->uHour = hour;
    this->uDay = day;
    this->uYear = year;

    return drift;
}

/**
 * @brief Prints a short representation of the current time into the provided buffer.
 * @param buffer The buffer to print the time into.
 * @param length The length of the buffer.
 */
uint16_t le_Time::PrintShortTime(char* buffer, uint32_t length)
{
    uint8_t month;
    uint8_t monthDay;
    ConvertDayOfYearToMonthDay(this->uYear + 1970, this->uDay, &month, &monthDay);

    return snprintf(buffer, length, "%4u-%02u-%02u %02u:%02u:%02u", this->uYear + 1970, month + 1, monthDay + 1, this->uHour, this->uMinute, this->uSecond);
}

/**
 * @brief Gets the current time as an le_Time object.
 * @return A new le_Time object representing the current time.
 */
le_Time le_Time::GetTime()
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    uint32_t subSecondFraction = 1000000000; // Nanoseconds
    uint32_t subSecond = nanoseconds % subSecondFraction;
    time_t seconds = nanoseconds / subSecondFraction;
    struct tm* tm_info = gmtime(&seconds);
    uint8_t second = tm_info->tm_sec;
    uint8_t minute = tm_info->tm_min;
    uint8_t hour = tm_info->tm_hour;
    uint16_t day = tm_info->tm_yday + 1;
    uint16_t year = tm_info->tm_year + 1900 - 1970;

    return le_Time(subSecondFraction, subSecond, second, minute, hour, day, year);
}

/**
 * @brief Determines if the specified year is a leap year.
 * @param year The year to check.
 * @return True if the year is a leap year, false otherwise.
 */
inline bool le_Time::IsLeapYear(uint16_t year) const
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @brief Gets the number of days in the specified month of the specified year.
 * @param year The year.
 * @param month The month (0 = January, 11 = December).
 * @return The number of days in the month.
 */
inline uint8_t le_Time::GetDaysInMonth(uint16_t year, uint8_t month) const
{
    if (month == 1 && IsLeapYear(year)) return 29;
    return _daysInMonth[month];
}

/**
 * @brief Gets the number of days in the specified year.
 * @param year The year.
 * @return The number of days in the year.
 */
inline uint16_t le_Time::GetDaysInYear(uint16_t year) const
{
    return IsLeapYear(year) ? 366 : 365;
}

/**
 * @brief Converts the day of the year to the corresponding month and day of the month.
 * @param year The year.
 * @param dayOfYear The day of the year.
 * @param month Pointer to store the resulting month.
 * @param day Pointer to store the resulting day of the month.
 */
inline void le_Time::ConvertDayOfYearToMonthDay(uint16_t year, uint16_t dayOfYear, uint8_t* month, uint8_t* day)
{
    if (dayOfYear < 1 || dayOfYear > GetDaysInYear(year))
    {
        // Invalid day of the year
        *month = 0;
        *day = 0;
        return;
    }

    *month = 0;
    while (dayOfYear > GetDaysInMonth(year, *month))
    {
        dayOfYear -= GetDaysInMonth(year, *month);
        (*month)++;
    }
    *day = (uint8_t)dayOfYear;
}

/**
 * @brief Checks if the specified time has elapsed compared to the current time.
 * @param other The reference le_Time object to compare.
 * @return True if the specified time has elapsed, false otherwise.
 */
bool le_Time::HasElapsed(const le_Time& other) const
{
    return ((le_Time&)*this - (le_Time&)other) >= 0;
}

/**
 * @brief Converts the le_Time object to microseconds since the epoch (1970-01-01 00:00:00).
 * @return The number of microseconds since the epoch.
 */
uint64_t le_Time::ToMicrosecondsSinceEpoch() const
{
    uint64_t totalMicroseconds = 0;

    // Calculate the number of days since the epoch
    for (uint16_t year = 1970; year < this->uYear + 1970; ++year)
    {
        totalMicroseconds += GetDaysInYear(year) * 86400ULL * 1000000ULL;
    }

    // Add the number of days in the current year
    totalMicroseconds += this->uDay * 86400ULL * 1000000ULL;

    // Add the number of hours, minutes, seconds, and sub-seconds
    totalMicroseconds += this->uHour * 3600ULL * 1000000ULL;
    totalMicroseconds += this->uMinute * 60ULL * 1000000ULL;
    totalMicroseconds += this->uSecond * 1000000ULL;
    totalMicroseconds += this->uSubSecond * (1000000ULL / this->uSubSecondFraction);

    return totalMicroseconds;
}

/**
 * @brief Converts the le_Time object to nanoseconds since the epoch (1970-01-01 00:00:00).
 * @return The number of nanoseconds since the epoch.
 */
uint64_t le_Time::ToNanosecondsSinceEpoch() const
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    return nanoseconds;
}
