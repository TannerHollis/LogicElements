/*!
\file    serialib.h
\brief   Header file of the class serialib. This class is used for communication over a serial device.
\author  Philippe Lucidarme (University of Angers)
\version 2.0
\date    December 27th, 2019
This Serial library is used to communicate through serial port.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

This is a licence-free software, it can be used by anyone who tries to build a better world.
*/

#ifndef SERIALIB_H
#define SERIALIB_H

#include <cstdint>

#if defined(__CYGWIN__)
    #include <sys/time.h>
#endif

#if defined (_WIN32) || defined (_WIN64)
    #if defined(__GNUC__)
        #include <sys/time.h>
    #else
        #define NO_POSIX_TIME
    #endif
    #include <windows.h>
#endif

#if defined (__linux__) || defined(__APPLE__)
    #include <stdlib.h>
    #include <sys/types.h>
    #include <sys/shm.h>
    #include <termios.h>
    #include <string.h>
    #include <iostream>
    #include <sys/time.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
#endif

#define UNUSED(x) (void)(x)

/**
 * @enum SerialDataBits
 * @brief Number of serial data bits
 */
enum SerialDataBits {
    SERIAL_DATABITS_5, /**< 5 databits */
    SERIAL_DATABITS_6, /**< 6 databits */
    SERIAL_DATABITS_7, /**< 7 databits */
    SERIAL_DATABITS_8, /**< 8 databits */
    SERIAL_DATABITS_16 /**< 16 databits */
};

/**
 * @enum SerialStopBits
 * @brief Number of serial stop bits
 */
enum SerialStopBits {
    SERIAL_STOPBITS_1, /**< 1 stop bit */
    SERIAL_STOPBITS_1_5, /**< 1.5 stop bits */
    SERIAL_STOPBITS_2 /**< 2 stop bits */
};

/**
 * @enum SerialParity
 * @brief Type of serial parity bits
 */
enum SerialParity {
    SERIAL_PARITY_NONE, /**< no parity bit */
    SERIAL_PARITY_EVEN, /**< even parity bit */
    SERIAL_PARITY_ODD, /**< odd parity bit */
    SERIAL_PARITY_MARK, /**< mark parity */
    SERIAL_PARITY_SPACE /**< space bit */
};

/**
 * @class serialib
 * @brief This class is used for communication over a serial device.
 */
/**
 * @class serialib
 * @brief This class is used for communication over a serial device.
 */
class serialib {
public:
    /**
     * @brief Constructor for the serialib class.
     */
    serialib();

    /**
     * @brief Destructor for the serialib class.
     */
    ~serialib();

    /**
     * @brief Opens the serial device.
     * @param Device The name of the serial device.
     * @param Bauds The baud rate for the serial communication.
     * @param Databits The number of data bits (default is 8).
     * @param Parity The type of parity bit (default is none).
     * @param Stopbits The number of stop bits (default is 1).
     * @param timeOut_ms The timeout value in milliseconds (default is UINT16_MAX).
     * @return A character indicating the success or failure of the operation.
     */
    char openDevice(const char *Device, const unsigned int Bauds,
                    SerialDataBits Databits = SERIAL_DATABITS_8,
                    SerialParity Parity = SERIAL_PARITY_NONE,
                    SerialStopBits Stopbits = SERIAL_STOPBITS_1,
                    uint16_t timeOut_ms = UINT16_MAX);

    /**
     * @brief Checks if the serial device is open.
     * @return True if the device is open, false otherwise.
     */
    bool isDeviceOpen();

    /**
     * @brief Closes the serial device.
     */
    void closeDevice();

    /**
     * @brief Writes a character to the serial device.
     * @param pByte The character to write.
     * @return The number of bytes written.
     */
    int writeChar(char);

    /**
     * @brief Reads a character from the serial device.
     * @param pByte Pointer to store the read character.
     * @param timeOut_ms The timeout value in milliseconds (default is 0).
     * @return The number of bytes read.
     */
    int readChar(char *pByte, const unsigned int timeOut_ms = 0);

    /**
     * @brief Writes a string to the serial device.
     * @param String The string to write.
     * @return The number of bytes written.
     */
    int writeString(const char *String);

    /**
     * @brief Reads a string from the serial device.
     * @param receivedString Pointer to store the read string.
     * @param finalChar The final character to stop reading.
     * @param maxNbBytes The maximum number of bytes to read.
     * @param timeOut_ms The timeout value in milliseconds (default is 0).
     * @return The number of bytes read.
     */
    int readString(char *receivedString, char finalChar, unsigned int maxNbBytes, const unsigned int timeOut_ms = 0);

    /**
     * @brief Writes an array of bytes to the serial device.
     * @param Buffer The buffer containing the data to write.
     * @param NbBytes The number of bytes to write.
     * @return The number of bytes written.
     */
    int writeBytes(const void *Buffer, const unsigned int NbBytes);

    /**
     * @brief Reads an array of bytes from the serial device.
     * @param buffer The buffer to store the read data.
     * @param maxNbBytes The maximum number of bytes to read.
     * @param timeOut_ms The timeout value in milliseconds (default is 0).
     * @param sleepDuration_us The sleep duration in microseconds (default is 100).
     * @return The number of bytes read.
     */
    int readBytes(void *buffer, unsigned int maxNbBytes, const unsigned int timeOut_ms = 0, unsigned int sleepDuration_us = 100);

    /**
     * @brief Flushes the receiver buffer.
     * @return A character indicating the success or failure of the operation.
     */
    char flushReceiver();

    /**
     * @brief Checks the number of bytes available for reading.
     * @return The number of bytes available.
     */
    int available();

    /**
     * @brief Sets the DTR (Data Terminal Ready) signal.
     * @param status The status to set (true to set, false to clear).
     * @return True if the operation was successful, false otherwise.
     */
    bool DTR(bool status);

    /**
     * @brief Sets the DTR (Data Terminal Ready) signal.
     * @return True if the operation was successful, false otherwise.
     */
    bool setDTR();

    /**
     * @brief Clears the DTR (Data Terminal Ready) signal.
     * @return True if the operation was successful, false otherwise.
     */
    bool clearDTR();

    /**
     * @brief Sets the RTS (Request to Send) signal.
     * @param status The status to set (true to set, false to clear).
     * @return True if the operation was successful, false otherwise.
     */
    bool RTS(bool status);

    /**
     * @brief Sets the RTS (Request to Send) signal.
     * @return True if the operation was successful, false otherwise.
     */
    bool setRTS();

    /**
     * @brief Clears the RTS (Request to Send) signal.
     * @return True if the operation was successful, false otherwise.
     */
    bool clearRTS();

    /**
     * @brief Checks the RI (Ring Indicator) signal.
     * @return True if the signal is active, false otherwise.
     */
    bool isRI();

    /**
     * @brief Checks the DCD (Data Carrier Detect) signal.
     * @return True if the signal is active, false otherwise.
     */
    bool isDCD();

    /**
     * @brief Checks the CTS (Clear to Send) signal.
     * @return True if the signal is active, false otherwise.
     */
    bool isCTS();

    /**
     * @brief Checks the DSR (Data Set Ready) signal.
     * @return True if the signal is active, false otherwise.
     */
    bool isDSR();

    /**
     * @brief Checks the RTS (Request to Send) signal.
     * @return True if the signal is active, false otherwise.
     */
    bool isRTS();

    /**
     * @brief Checks the DTR (Data Terminal Ready) signal.
     * @return True if the signal is active, false otherwise.
     */
    bool isDTR();

private:
    /**
     * @brief Reads a string from the serial device without timeout.
     * @param String Pointer to store the read string.
     * @param FinalChar The final character to stop reading.
     * @param MaxNbBytes The maximum number of bytes to read.
     * @return The number of bytes read.
     */
    int readStringNoTimeOut(char *String, char FinalChar, unsigned int MaxNbBytes);

    bool currentStateRTS; /**< Current state of the RTS signal */
    bool currentStateDTR; /**< Current state of the DTR signal */

#if defined (_WIN32) || defined(_WIN64)
    HANDLE hSerial; /**< Handle for the serial device */
    COMMTIMEOUTS timeouts; /**< COMMTIMEOUTS structure for timeouts */
#endif
#if defined (__linux__) || defined(__APPLE__)
    int fd; /**< File descriptor for the serial device */
#endif
};

/**
 * @class timeOut
 * @brief This class can manage a timer which is used as a timeout.
 */
class timeOut {
public:
    timeOut();
    void initTimer();
    unsigned long int elapsedTime_ms();

private:
#if defined (NO_POSIX_TIME)
    LONGLONG counterFrequency;
    LONGLONG previousTime;
#else
    struct timeval previousTime;
#endif
};

#endif // SERIALIB_H
