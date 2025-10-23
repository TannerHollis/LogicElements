#pragma once

#include <cstdint>
#include <cstdio>

namespace LogicElements {

/**
 * @brief Class representing a point in time with various time manipulation methods.
 */
class Time
{
public:
    /**
     * @brief Enumeration for days of the week.
     */
    enum class Day : uint8_t
    {
        SUNDAY = 0,
        MONDAY = 1,
        TUESDAY = 2,
        WEDNESDAY = 3,
        THURSDAY = 4,
        FRIDAY = 5,
        SATURDAY = 6
    };

    /**
     * @brief Enumeration for months of the year.
     */
    enum class Month : uint8_t
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
    Time(uint32_t subSecondFraction, uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year);

    /**
     * @brief Default constructor to initialize the Time object with zero values.
     */
    Time();

    /**
     * @brief Gets a future Time object based on the current time plus the specified seconds.
     * @param seconds The number of seconds to add to the current time.
     * @return A new Time object representing the future time.
     */
    Time GetFuture(float seconds);

    /**
     * @brief Updates the current Time object by adding the specified number of sub-seconds.
     * @param subSeconds The number of sub-seconds to add to the current time.
     */
    void Update(uint32_t subSeconds);

    /**
     * @brief Subtracts the specified Time object from the current Time object.
     * @param other The reference Time object to subtract.
     * @return The elapsed time in microseconds.
     */
    int64_t operator-(const Time& other) const;

    /**
     * @brief Adds the specified Time object to the current Time object.
     * @param other The reference Time object to add.
     * @return A new Time object representing the sum of the two times.
     */
    Time operator+(const Time& other) const;

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
    int32_t Align(uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year);

    /**
     * @brief Prints a short representation of the current time into the provided buffer.
     * @param buffer The buffer to print the time into.
     * @param length The length of the buffer.
     */
    uint16_t PrintShortTime(char* buffer, uint32_t length);

    /**
     * @brief Gets the current time as an Time object.
     * @return A new Time object representing the current time.
     */
    static Time GetTime();

    /**
     * @brief Checks if the specified time has elapsed compared to the current time.
     * @param other The reference Time object to compare.
     * @return True if the specified time has elapsed, false otherwise.
     */
    bool HasElapsed(const Time& other) const;

    /**
     * @brief Determines if the specified year is a leap year.
     * @param year The year to check.
     * @return True if the year is a leap year, false otherwise.
     * 
     * PERFORMANCE: Inline implementation for zero-overhead calls
     */
    inline bool IsLeapYear(uint16_t year) const {
        // Optimized: Most common case first (not century year)
        if (year % 4 != 0) return false;
        if (year % 100 != 0) return true;
        return (year % 400 == 0);
    }

    /**
     * @brief Gets the number of days in the specified month of the specified year.
     * @param year The year.
     * @param month The month (0 = January, 11 = December).
     * @return The number of days in the month.
     * 
     * PERFORMANCE: Inline implementation for zero-overhead calls
     */
    inline uint8_t GetDaysInMonth(uint16_t year, uint8_t month) const {
        // Fast path: most months aren't February
        if (month != 1) return _daysInMonth[month];
        return IsLeapYear(year) ? 29 : 28;
    }

    /**
     * @brief Gets the number of days in the specified year.
     * @param year The year.
     * @return The number of days in the year.
     * 
     * PERFORMANCE: Inline implementation for zero-overhead calls
     */
    inline uint16_t GetDaysInYear(uint16_t year) const {
        return IsLeapYear(year) ? 366 : 365;
    }

    /**
     * @brief Converts the day of the year to the corresponding month and day of the month.
     * @param year The year.
     * @param dayOfYear The day of the year.
     * @param month Pointer to store the resulting month.
     * @param day Pointer to store the resulting day of the month.
     */
    inline void ConvertDayOfYearToMonthDay(uint16_t year, uint16_t dayOfYear, uint8_t* month, uint8_t* day) const;

    // Getters for time components
    uint16_t GetYear() const { return uYear; }
    uint16_t GetDay() const { return uDay; }
    uint8_t GetHour() const { return uHour; }
    uint8_t GetMinute() const { return uMinute; }
    uint8_t GetSecond() const { return uSecond; }
    uint32_t GetSubSecond() const { return uSubSecond; }
    
    static constexpr uint32_t GetSubSecondFraction() { return SUBSECOND_FRACTION; }

    /**
     * @brief Converts the Time object to microseconds since the epoch (1970-01-01 00:00:00).
     * @return The number of microseconds since the epoch.
     */
    uint64_t ToMicrosecondsSinceEpoch() const;

    /**
     * @brief Converts the Time object to nanoseconds since the epoch (1970-01-01 00:00:00).
     * @return The number of nanoseconds since the epoch.
     */
    uint64_t ToNanosecondsSinceEpoch() const;

private:
    uint16_t uYear; ///< Year since 1970
    uint16_t uDay; ///< Year day (0-365)
    uint8_t uHour;
    uint8_t uMinute;
    uint8_t uSecond;
    uint32_t uSubSecond;

    static constexpr uint32_t SUBSECOND_FRACTION = 1000000000; ///< Nanoseconds per second
    static const uint8_t _daysInMonth[12];
};

} // namespace LogicElements