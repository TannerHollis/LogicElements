#pragma once

#include <cstdint>
#include <cstdio>

/**
 * @brief Class representing a point in time with various time manipulation methods.
 */
class le_Time
{
public:
    /**
     * @brief Enumeration for days of the week.
     */
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

    /**
     * @brief Enumeration for months of the year.
     */
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
    le_Time(uint32_t subSecondFraction, uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year);

    /**
     * @brief Default constructor to initialize the le_Time object with zero values.
     */
    le_Time();

    /**
     * @brief Gets a future le_Time object based on the current time plus the specified seconds.
     * @param seconds The number of seconds to add to the current time.
     * @return A new le_Time object representing the future time.
     */
    le_Time GetFuture(float seconds);

    /**
     * @brief Updates the current le_Time object by adding the specified number of sub-seconds.
     * @param subSeconds The number of sub-seconds to add to the current time.
     */
    void Update(uint32_t subSeconds);

    /**
     * @brief Subtracts the specified le_Time object from the current le_Time object.
     * @param other The reference le_Time object to subtract.
     * @return The elapsed time in microseconds.
     */
    int32_t operator-(le_Time& other);

    /**
     * @brief Adds the specified le_Time object to the current le_Time object.
     * @param other The reference le_Time object to add.
     * @return A new le_Time object representing the sum of the two times.
     */
    le_Time operator+(le_Time& other);

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
    int32_t Align(uint32_t subSecond, uint8_t second, uint8_t minute, uint8_t hour, uint16_t day, uint16_t year);

    /**
     * @brief Prints a short representation of the current time into the provided buffer.
     * @param buffer The buffer to print the time into.
     * @param length The length of the buffer.
     */
    uint16_t PrintShortTime(char* buffer, uint32_t length);

    /**
     * @brief Gets the current time as an le_Time object.
     * @return A new le_Time object representing the current time.
     */
    static le_Time GetTime();

    /**
     * @brief Checks if the specified time has elapsed compared to the current time.
     * @param other The reference le_Time object to compare.
     * @return True if the specified time has elapsed, false otherwise.
     */
    bool HasElapsed(const le_Time& other) const;

    /**
     * @brief Determines if the specified year is a leap year.
     * @param year The year to check.
     * @return True if the year is a leap year, false otherwise.
     */
    inline bool IsLeapYear(uint16_t year) const;

    /**
     * @brief Gets the number of days in the specified month of the specified year.
     * @param year The year.
     * @param month The month (0 = January, 11 = December).
     * @return The number of days in the month.
     */
    inline uint8_t GetDaysInMonth(uint16_t year, uint8_t month) const;

    /**
     * @brief Gets the number of days in the specified year.
     * @param year The year.
     * @return The number of days in the year.
     */
    inline uint16_t GetDaysInYear(uint16_t year) const;

    /**
     * @brief Converts the day of the year to the corresponding month and day of the month.
     * @param year The year.
     * @param dayOfYear The day of the year.
     * @param month Pointer to store the resulting month.
     * @param day Pointer to store the resulting day of the month.
     */
    inline void ConvertDayOfYearToMonthDay(uint16_t year, uint16_t dayOfYear, uint8_t* month, uint8_t* day);

    uint16_t uYear; ///< Year since 1970
    uint16_t uDay; ///< Year day
    uint8_t uHour;
    uint8_t uMinute;
    uint8_t uSecond;
    uint32_t uSubSecond;
    uint32_t uSubSecondFraction;

    static const uint8_t _daysInMonth[12];

    /**
     * @brief Converts the le_Time object to microseconds since the epoch (1970-01-01 00:00:00).
     * @return The number of microseconds since the epoch.
     */
    uint64_t ToMicrosecondsSinceEpoch() const;

    /**
     * @brief Converts the le_Time object to nanoseconds since the epoch (1970-01-01 00:00:00).
     * @return The number of nanoseconds since the epoch.
     */
    uint64_t ToNanosecondsSinceEpoch() const;
};