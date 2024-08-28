#pragma once

#include <unordered_map>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

#define MSG_REQ_ARG_MAX_LENGTH 16
#define MSG_RESP_MAX_LENGTH 128
#define UNKNOWN_ERROR_LENGTH 64

namespace comms
{
    /**
     * @brief Enum representing different message types.
     */
    enum class msg_type : uint8_t
    {
        MSG_ECHO = 0,   ///< Echo message
        MSG_ID,         ///< ID message
        MSG_STATUS,     ///< Status message
        MSG_TARGET,     ///< Target message
        MSG_PULSE,      ///< Pulse message
        MSG_SER,        ///< SER message
        UNKNOWN         ///< Unknown message type
    };

    /**
     * @brief Enum representing different message categories.
     */
    enum class msg_category : uint8_t
    {
        REQUEST = 0,            ///< Request message
        RESPONSE_PARTIAL,       ///< Partial response message
        RESPONSE_COMPLETE      ///< Complete response message
    };

    /**
     * @brief Map to associate string representations with message types.
     */
    static const std::unordered_map<std::string, msg_type> msg_type_map = 
    {
        {"ECHO", msg_type::MSG_ECHO},
        {"ID", msg_type::MSG_ID},
        {"STATUS", msg_type::MSG_STATUS},
        {"STA", msg_type::MSG_STATUS},
        {"TARGET", msg_type::MSG_TARGET},
        {"TAR", msg_type::MSG_TARGET},
        {"PULSE", msg_type::MSG_PULSE},
        {"PUL", msg_type::MSG_PULSE},
        {"SER", msg_type::MSG_SER}
    };

    /**
     * @brief Converts a string to uppercase.
     * @param str The input string.
     * @return The uppercase version of the input string.
     */
    std::string to_upper(const std::string& str);

    /**
     * @brief Splits a message string into a vector of words.
     * @param message The input message string.
     * @return A vector of words extracted from the message.
     */
    std::vector<std::string> split(const std::string& message);

    /**
     * @brief Parses a word to determine its message type.
     * @param word The input word.
     * @return The corresponding message type.
     */
    msg_type parse_msg_type(const std::string& word);

    /**
     * @brief Union representing different types of message arguments.
     */
    typedef union msg_arg
    {
        char sArg[MSG_REQ_ARG_MAX_LENGTH]; ///< String argument
        uint16_t uArg;                     ///< Unsigned integer argument
        float fArg;                        ///< Float argument
        bool bArg;                         ///< Boolean argument
    } msg_arg;

    /**
     * @brief Struct representing a generic message.
     */
    typedef struct msg
    {
    public:
        /**
         * @brief Constructor for the msg struct.
         * @param type The type of the message.
         * @param category The category of the message.
         */
        msg(msg_type type, msg_category category);

        /**
         * @brief Retrieves the type of the message.
         * @return The type of the message.
         */
        msg_type GetType() const;

        /**
         * @brief Retrieves the category of the message.
         * @return The categroy of the message.
         */
        msg_category GetCategory() const;

    protected:
        msg_type type;         ///< The type of the message
        msg_category category; ///< The category of the message
    } msg;

#pragma pack(push, 1)
    /**
     * @brief Struct representing a request message.
     */
    typedef struct msg_req : public msg
    {
    public:
        /**
         * @brief Constructor for the msg_req struct with no arguments.
         * @param type The type of the message.
         */
        msg_req(msg_type type);

    protected:
        msg_arg args[4]; ///< Array of message arguments
    } msg_req;
#pragma pack(pop)

#pragma pack(push, 1)
    /**
     * @brief Struct representing an echo request message.
     */
    typedef struct msg_req_echo : public msg_req
    {
    public:
        /**
         * @brief Constructor for the msg_req_echo struct.
         * @param type The type of the message.
         * @param echo The echo message string.
         */
        msg_req_echo(std::string& echo);

        /**
         * @brief Retrieves the echo message from the request.
         * @return The echo message string.
         */
        std::string GetEcho() const;
    } msg_req_echo;
#pragma pack(pop)

#pragma pack(push, 1)
    /**
     * @brief Struct representing a target request message.
     */
    typedef struct msg_req_target : public msg_req
    {
    public:
        /**
         * @brief Constructor for the msg_req_target struct.
         * @param type The type of the message.
         * @param elementName The name of the target element.
         * @param outputSlot The output slot number.
         * @param repetition The number of repetitions.
         * @param delayMS The delay in milliseconds.
         */
        msg_req_target(const std::string& elementName, uint8_t outputSlot, uint8_t repetition, uint16_t delayMS);

        /**
         * @brief Retrieves the name of the target element.
         * @return The name of the target element.
         */
        std::string GetElementName() const;

        /**
         * @brief Retrieves the output slot number.
         * @return The output slot number.
         */
        uint8_t GetOutputSlot() const;

        /**
         * @brief Retrieves the number of repetitions.
         * @return The number of repetitions.
         */
        uint8_t GetRepetition() const;

        /**
         * @brief Retrieves the delay in milliseconds.
         * @return The delay in milliseconds.
         */
        uint16_t GetDelayMS() const;
    } msg_req_target;
#pragma pack(pop)

#pragma pack(push, 1)
    /**
     * @brief Struct representing a pulse request message.
     */
    typedef struct msg_req_pulse : public msg_req
    {
    public:
        /**
         * @brief Constructor for the msg_req_pulse struct.
         * @param type The type of the message.
         * @param elementName The name of the target element.
         * @param value The pulse value.
         * @param duration The duration of the pulse.
         */
        msg_req_pulse(const std::string& elementName, float value, float duration);

        /**
         * @brief Retrieves the name of the target element.
         * @return The name of the target element.
         */
        std::string GetElementName() const;

        /**
         * @brief Retrieves the pulse value.
         * @return The pulse value.
         */
        float GetValue() const;

        /**
         * @brief Retrieves the duration of the pulse.
         * @return The duration of the pulse.
         */
        float GetDuration() const;
    } msg_req_pulse;
#pragma pack(pop)

#pragma pack(push, 1)
    /**
     * @brief Struct representing a SER request message.
     */
    typedef struct msg_req_ser : public msg_req
    {
    public:
        /**
         * @brief Constructor for the msg_req_ser struct.
         * @param numSers The number of SERS.
         */
        msg_req_ser(uint16_t numSers);

        /**
         * @brief Retrieves the number of SERS.
         * @return The number of SERS.
         */
        uint16_t GetNumSERs() const;
    } msg_req_ser;
#pragma pack(pop)

#pragma pack(push, 1)
    /**
     * @brief Struct representing an unknown request message.
     */
    typedef struct msg_req_unknown : public msg_req
    {
    public:
        /**
         * @brief Constructor for the msg_req_unknown struct.
         * @param errorMajor The major error message.
         * @param errorMinor The minor error message.
         */
        msg_req_unknown(std::string errorMajor, std::string errorMinor);

        /**
         * @brief Function to return a full error string.
         * @return A string in the format "Majorerror: Minorerror".
         */
        std::string GetFullError() const;

    protected:
        std::string errorMajor; ///< Major error message
        std::string errorMinor; ///< Minor error message
    } msg_req_unknown;
#pragma pack(pop)

#pragma pack(push, 1)
    /**
     * @brief Struct representing a response message.
     */
    typedef struct msg_resp : public msg
    {
        uint16_t length; ///< Length of the response message
        char buffer[MSG_RESP_MAX_LENGTH]; ///< Buffer to store the response message
        bool badResponse; ///< Flag indicating if the response is bad

        /**
         * @brief Constructor for the msg_resp struct.
         * @param type The type of the message.
         * @param buffer The buffer containing the response message.
         * @param length The length of the response message.
         * @param badResponse Flag indicating if the response is bad.
         */
        msg_resp(msg_type type, char* buffer, uint16_t length, bool badResponse);

        /**
         * @brief Static function to split a message into partial responses.
         * @param type The type of the message.
         * @param buffer The buffer containing the response message.
         * @param length The length of the response message.
         * @param badResponse Flag indicating if the response is bad.
         * @return A vector of partial response messages.
         */
        static std::vector<msg_resp> partialize_msg(msg_type type, char* buffer, uint16_t length, bool badResponse);
    } msg_resp;
#pragma pack(pop)

    /**
     * @brief Parses a vector of strings to create a request message.
     * @param args The vector of strings representing the message arguments.
     * @return The parsed request message.
     */
    std::unique_ptr<msg_req> parse_msg_req_command(std::string& line);
}