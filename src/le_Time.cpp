#include "le_Time.hpp"

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
}

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
 * @brief Calculates the elapsed time in microseconds since the specified le_Time object.
 * @param other The reference le_Time object to compare against.
 * @return The elapsed time in microseconds.
 */
int32_t le_Time::GetElapsedTimeSince(le_Time& other)
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
 * @brief Aligns the current le_Time object to the specified time components and calculates the drift.
 * @param subSecond The new sub-second part of the time.
 * @param second The new second part of the time.
 * @param minute The new minute part of the time.
 * @param hour The new hour part of the time.
 * @param day The new day part of the time.
 * @param year The new year part of the time.
 * @return The drift in microseconds.
 */
int32_t le_Time::Align(uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint8_t year)
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

/**
 * @brief Prints a short representation of the current time into the provided buffer.
 * @param buffer The buffer to print the time into.
 * @param length The length of the buffer.
 */
void le_Time::PrintShortTime(char* buffer, uint32_t length)
{
    uint8_t month;
    uint8_t monthDay;
    ConvertDayOfYearToMonthDay(this->uYear + 1970, this->uDay, &month, &monthDay);

    snprintf(buffer, length, "%4u-%02u-%02u %02u:%02u:%02u", this->uYear + 1970, month + 1, monthDay + 1, this->uHour, this->uMinute, this->uSecond);
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
