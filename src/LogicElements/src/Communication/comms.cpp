#include "Communication/comms.hpp"

#define DEFAULT_DELAY_MS 1000
#define DEFAULT_REPETITION 1

namespace comms
{

/**
 * @brief Converts a string to uppercase.
 * @param str The input string.
 * @return The uppercase version of the input string.
 */
std::string to_upper(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

/**
 * @brief Splits a message string into a vector of words.
 * @param message The input message string.
 * @return A vector of words extracted from the message.
 */
std::vector<std::string> split(const std::string& message)
{
    std::stringstream ss(message);
    std::string word;
    std::vector<std::string> args;

    while (ss >> word)
    {
        args.push_back(word);
    }

    return args;
}

/**
 * @brief Parses a word to determine its message type.
 * @param word The input word.
 * @return The corresponding message type.
 */
msg_type parse_msg_type(const std::string& word)
{
    std::string upWord = comms::to_upper(word);
    auto it = msg_type_map.find(upWord);
    if (it != msg_type_map.end())
    {
        return it->second;
    }
    else
    {
        return msg_type::UNKNOWN;
    }
}

/**
 * @brief Constructor for the msg struct.
 * @param type The type of the message.
 * @param category The category of the message.
 */
msg::msg(msg_type type, msg_category category) : type(type), category(category) {}

/**
 * @brief Retrieves the type of the message.
 * @return The type of the message.
 */
msg_type msg::GetType() const
{
    return type;
}

/**
 * @brief Retrieves the type of the message.
 * @return The type of the message.
 */
msg_category msg::GetCategory() const
{
    return category;
}

/**
 * @brief Constructor for the msg_req struct with no arguments.
 * @param type The type of the message.
 */
msg_req::msg_req(msg_type type) : msg(type, msg_category::REQUEST) {}

/**
 * @brief Constructor for the msg_req_echo struct.
 * @param echo The echo message string.
 */
msg_req_echo::msg_req_echo(std::string& echo) : msg_req(msg_type::MSG_ECHO)
{
    size_t len = echo.length();
    if (len >= MSG_REQ_ARG_MAX_LENGTH) len = MSG_REQ_ARG_MAX_LENGTH - 1;
    memcpy(args[0].sArg, echo.c_str(), len);
    args[0].sArg[len] = '\0';
}

/**
 * @brief Retrieves the echo message from the request.
 * @return The echo message string.
 */
std::string msg_req_echo::GetEcho() const
{
    return std::string(args[0].sArg);
}

/**
 * @brief Constructor for the msg_req_target struct.
 * @param elementName The name of the target element.
 * @param outputSlot The output slot number.
 * @param repetition The number of repetitions.
 * @param delayMS The delay in milliseconds.
 */
msg_req_target::msg_req_target(const std::string& elementName, uint8_t outputSlot, uint8_t repetition, uint16_t delayMS) : msg_req(msg_type::MSG_TARGET)
{
    size_t len = elementName.length();
    if (len >= MSG_REQ_ARG_MAX_LENGTH) len = MSG_REQ_ARG_MAX_LENGTH - 1;
    memcpy(args[0].sArg, elementName.c_str(), len);
    args[0].sArg[len] = '\0';
    args[1].uArg = outputSlot;
    args[2].uArg = repetition;
    args[3].uArg = delayMS;
}

/**
 * @brief Retrieves the name of the target element.
 * @return The name of the target element.
 */
std::string msg_req_target::GetElementName() const
{
    return std::string(args[0].sArg);
}

/**
 * @brief Retrieves the output slot number.
 * @return The output slot number.
 */
uint8_t msg_req_target::GetOutputSlot() const
{
    return args[1].uArg;
}

/**
 * @brief Retrieves the number of repetitions.
 * @return The number of repetitions.
 */
uint8_t msg_req_target::GetRepetition() const
{
    return args[2].uArg;
}

/**
 * @brief Retrieves the delay in milliseconds.
 * @return The delay in milliseconds.
 */
uint16_t msg_req_target::GetDelayMS() const
{
    return args[3].uArg;
}

/**
 * @brief Constructor for the msg_req_pulse struct.
 * @param elementName The name of the target element.
 * @param value The pulse value.
 * @param duration The duration of the pulse.
 */
msg_req_pulse::msg_req_pulse(const std::string& elementName, float value, float duration) : msg_req(msg_type::MSG_PULSE)
{
    size_t len = elementName.length();
    if (len >= MSG_REQ_ARG_MAX_LENGTH) len = MSG_REQ_ARG_MAX_LENGTH - 1;
    memcpy(args[0].sArg, elementName.c_str(), len);
    args[0].sArg[len] = '\0';
    args[1].fArg = value;
    args[2].fArg = duration;
}

/**
 * @brief Retrieves the name of the target element.
 * @return The name of the target element.
 */
std::string msg_req_pulse::GetElementName() const
{
    return std::string(args[0].sArg);
}

/**
 * @brief Retrieves the pulse value.
 * @return The pulse value.
 */
float msg_req_pulse::GetValue() const
{
    return args[1].bArg;
}

/**
 * @brief Retrieves the duration of the pulse.
 * @return The duration of the pulse.
 */
float msg_req_pulse::GetDuration() const
{
    return args[2].fArg;
}

/**
 * @brief Constructor for the msg_req_ser struct.
 * @param numSers The number of SERS.
 */
msg_req_ser::msg_req_ser(uint16_t numSers) : msg_req(msg_type::MSG_SER)
{
    args[0].uArg = numSers;
}

/**
 * @brief Retrieves the number of SERS.
 * @return The number of SERS.
 */
uint16_t msg_req_ser::GetNumSERs() const
{
    return args[0].uArg;
}

/**
 * @brief Constructor for the msg_req_unknown struct.
 * @param errorMajor The major error message.
 * @param errorMinor The minor error message.
 */
msg_req_unknown::msg_req_unknown(std::string errorMajor, std::string errorMinor = "") : msg_req(msg_type::UNKNOWN), errorMajor(errorMajor), errorMinor(errorMinor) {}

/**
 * @brief Function to return a full error string.
 * @return A string in the format "Majorerror: Minorerror".
 */
std::string msg_req_unknown::GetFullError() const 
{
    if (!errorMinor.empty())
        return errorMajor + ": " + errorMinor;
    else
        return errorMajor;
}

/**
 * @brief Constructor for the msg_resp struct.
 * @param type The type of the message.
 * @param buffer The buffer containing the response message.
 * @param length The length of the response message.
 * @param badResponse Flag indicating if the response is bad.
 */
msg_resp::msg_resp(msg_type type, char* buffer, uint16_t length, bool badResponse)
    : msg(type, msg_category::RESPONSE_PARTIAL), badResponse(badResponse)
{
    memset(this->buffer, '\0', MSG_RESP_MAX_LENGTH);

    if (length < MSG_RESP_MAX_LENGTH)
    {
        memcpy(this->buffer, buffer, length);
        this->length = length;
        this->SetCategory(msg_category::RESPONSE_COMPLETE);
    }
    else
    {
        memcpy(this->buffer, buffer, MSG_RESP_MAX_LENGTH);
        this->length = MSG_RESP_MAX_LENGTH;
    }
}

/**
 * @brief Static function to split a message into partial responses.
 * @param type The type of the message.
 * @param buffer The buffer containing the response message.
 * @param length The length of the response message.
 * @param badResponse Flag indicating if the response is bad.
 * @return A vector of partial response messages.
 */
std::vector<msg_resp> msg_resp::partialize_msg(msg_type type, char* buffer, uint16_t length, bool badResponse)
{
    uint16_t cnt = 0;
    std::vector<msg_resp> partials;

    while (cnt < length)
    {
        partials.push_back(msg_resp(type, &buffer[cnt], length - cnt, badResponse));
        msg_resp* partial = &partials.back();
        cnt += partial->length;
    }

    return partials;
}

/**
 * @brief Parses a vector of strings to create a request message.
 * @param args The vector of strings representing the message arguments.
 * @return The parsed request message.
 */
std::unique_ptr<msg_req> parse_msg_req_command(std::string& line)
{
    // Split the input line into a vector of arguments
    std::vector<std::string> args = comms::split(line);

    if(args.size() == 0)
        return nullptr;

    // Convert the first argument (command) to uppercase
    std::string command = comms::to_upper(args[0]);

    // Move all arguments down so that the zeroth index is the first argument
    args.erase(args.begin());

    // Parse the command to determine the message type
    msg_type type = comms::parse_msg_type(command);

    switch (type)
    {
    case msg_type::UNKNOWN:
        return std::make_unique<msg_req_unknown>("Invalid command", command);

    case msg_type::MSG_ECHO:
    {
        if (args.size() > 1)
        {
            return std::make_unique<msg_req_echo>(args[0]);
        }
        else
            return std::make_unique<msg_req_unknown>("No echo message provided");
    }

    case msg_type::MSG_ID:
        return std::make_unique<msg_req>(type);

    case msg_type::MSG_STATUS:
        return std::make_unique<msg_req>(type);

    case msg_type::MSG_TARGET:
    {
        if (args.size() < 2)
            return std::make_unique<msg_req_unknown>("Not enough arguments provided"); // Not enough arguments
        if (args.size() > 4)
            return std::make_unique<msg_req_unknown>("Too many arguments provided"); // Too many arguments

        std::string elementName = args[0];
        uint8_t outputSlot, repetition = DEFAULT_REPETITION; // Default repetition
        uint16_t delayMS = DEFAULT_DELAY_MS; // Default delay
        
        try {
            outputSlot = static_cast<uint8_t>(std::stoi(args[1]));
        }
        catch (const std::exception&) {
            return std::make_unique<msg_req_unknown>("Invalid output slot argument", args[1]); // Invalid argument parsing
        }

        if (args.size() > 2) {
            try {
                repetition = static_cast<uint8_t>(std::stoi(args[2]));
            }
            catch (const std::exception&) {
                return std::make_unique<msg_req_unknown>("Invalid repetition argument", args[2]); // Invalid argument parsing
            }
        }

        if (args.size() > 3) {
            try {
                delayMS = static_cast<uint16_t>(std::stoi(args[3]));
            }
            catch (const std::exception&) {
                return std::make_unique<msg_req_unknown>("Invalid delay argument", args[3]); // Invalid argument parsing
            }
        }

        return std::make_unique<msg_req_target>(elementName, outputSlot, repetition, delayMS);
    }

    case msg_type::MSG_PULSE:
    {
        if (args.size() < 2)
            return std::make_unique<msg_req_unknown>("Not enough arguments provided"); // Not enough arguments
        if (args.size() > 3)
            return std::make_unique<msg_req_unknown>("Too many arguments provided"); // Too many arguments

        std::string elementName = args[0];
        std::string valueStr = args[1];
        float value;
        float duration = 1;

        try {
            if (valueStr == "true" || valueStr == "false") {
                value = valueStr == "true" ? 1.0f : 0.0f;
            }
            else {
                value = std::stof(valueStr);
            }
        }
        catch (const std::exception&) {
            return std::make_unique<msg_req_unknown>("Invalid value argument", valueStr); // Invalid argument parsing
        }

        if (args.size() > 2) {
            try {
                duration = std::stof(args[2]);
            }
            catch (const std::exception&) {
                return std::make_unique<msg_req_unknown>("Invalid duration argument", args[2]); // Invalid argument parsing
            }
        }

        return std::make_unique<msg_req_pulse>(elementName, value, duration);
    }

    case msg_type::MSG_SER:
    {
        if (args.size() != 1)
            return std::make_unique<msg_req_unknown>("Invalid number of arguments provided"); // Invalid number of arguments

        std::string numSersStr = args[0];
        uint16_t numSers;

        try {
            numSers = static_cast<uint16_t>(std::stoi(numSersStr));
        }
        catch (const std::exception&) {
            return std::make_unique<msg_req_unknown>("Invalid number of SERS argument", numSersStr); // Invalid argument parsing
        }

        return std::make_unique<msg_req_ser>(numSers);
    }

    default:
        return std::make_unique<msg_req_unknown>("Invalid command");
    }
}

} // namespace comms
