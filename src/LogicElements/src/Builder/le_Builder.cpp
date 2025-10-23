#include "Builder/le_Builder.hpp"
#include <fstream>

namespace LogicElements {

// Static member initialization
Builder::MajorError Builder::majorError = Builder::MajorError::NONE;
Builder::MinorError Builder::minorError = Builder::MinorError::NONE;
char Builder::sErrorMessage[MAX_ERROR_LENGTH] = "";
char Builder::sErroneousJson[MAX_ERROR_LENGTH] = "";

// Load configuration from file
bool Builder::LoadFromFile(const std::string& filePath, Board*& board)
{
    ClearErrors();
    std::string fileContent = readFile(filePath);
    return fileContent.empty() ? false : LoadConfig(fileContent.c_str(), board);
}

// Load configuration from JSON string
bool Builder::LoadConfig(const char* jsonString, Board*& board)
{
    // Clear any previous errors
    ClearErrors();
    nlohmann::json json;
    try {
        // Parse the JSON string
        json = nlohmann::json::parse(jsonString);
    } catch (const nlohmann::json::parse_error&) {
        // Set error if JSON parsing fails
        return SetError(MajorError::INV_JSON_FILE, MinorError::NONE, jsonString), false;
    }

    // Check if the JSON contains a valid "name" field
    if (!json.contains("name") || !json["name"].is_string()) {
        // Set error if "name" field is missing or invalid
        return SetError(MajorError::INV_ENGINE_NAME, MinorError::NONE, jsonString), false;
    }

    // Create a new engine with the name from the JSON
    Engine* engine = new Engine(json["name"].get<std::string>());

    // Attach the engine to the board
    board->AttachEngine(engine);

    // Parse elements and nets from the JSON
    if (!ParseElements(board, json["elements"]) || !ParseNets(board, json["nets"])) {
        // Delete the engine and return false if parsing fails
        delete engine;
        engine = nullptr;
        return false;
    }

    // Check if the JSON contains "ser" field and parse it
    if (json.contains("ser")) {
        if (!ParseSER(board, json["ser"])) {
            // Delete the engine and return false if parsing fails
            delete engine;
            engine = nullptr;
            return false;
        }
    }

#ifdef LE_DNP3
    // Check if the JSON contains "dnp3" field and parse it
    if (json.contains("dnp3")) {
        DNP3OutstationConfig dnp3Config = DNP3OutstationConfig();
        if (!ParseOutstationConfig(&dnp3Config, json["dnp3"]["outstation"])) {
            return false;
        }
        // Create and attach the DNP3 outstation to the board
        DNP3Outstation* dnp3 = new DNP3Outstation(dnp3Config);
        board->AttachDNP3Outstation(dnp3);
    }
#endif

    // Return true if configuration is successfully loaded
    return true;
}

// Get error information
uint16_t Builder::GetErrorString(char* buffer, uint16_t length)
{
    if (buffer == nullptr || length == 0) {
        return 0;
    }

    // Retrieve the static error message
    const char* errorMessage = GetErrorMessage();

    // Copy the static error message to the buffer
    uint16_t copiedLength = snprintf(buffer, length, "%s", errorMessage ? errorMessage : "None");

    // Return the length of the copied characters
    return copiedLength;
}

// Retrieve the last major error
Builder::MajorError Builder::GetMajorError()
{
    return majorError;
}

// Retrieve the last minor error
Builder::MinorError Builder::GetMinorError()
{
    return minorError;
}

// Retrieve the last error message
const char* Builder::GetErrorMessage()
{
    return sErrorMessage;
}

// Retrieve the erroneous JSON string
const char* Builder::GetErroneousJson()
{
    return sErroneousJson;
}

// Clear error state
void Builder::ClearErrors()
{
    majorError = MajorError::NONE;
    minorError = MinorError::NONE;
    sErrorMessage[0] = '\0';
    sErroneousJson[0] = '\0';
}

// Set error state and return nullptr
void Builder::SetError(MajorError major, MinorError minor, const char* erroneousJson)
{
    majorError = major;
    minorError = minor;

    const char* majorErrorStr = MajorErrorToString(major);
    const char* minorErrorStr = MinorErrorToString(minor);

    snprintf(sErrorMessage, MAX_ERROR_LENGTH, "Major Error: %s, Minor Error: %s", majorErrorStr, minorErrorStr);
    if (erroneousJson && erroneousJson[0] != '\0')
        snprintf(sErroneousJson, MAX_ERROR_LENGTH, "Erroneous JSON: %.500s", erroneousJson);
}

// Read file into string
std::string Builder::readFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) return SetError(MajorError::INV_FILE, MinorError::NONE, nullptr), "";

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string buffer(size, '\0');
    if (!file.read(&buffer[0], size)) return SetError(MajorError::INV_FILE, MinorError::NONE, nullptr), "";

    return buffer;
}

// Parse elements section
bool Builder::ParseElements(Board* board, const nlohmann::json& elementsField)
{
    // Check if elementsField is an array
    if (!elementsField.is_array())
    {
        SetError(MajorError::INV_ENGINE_COMPONENTS, MinorError::NONE, elementsField.dump().c_str());
        return false;
    }

    // Iterate through each element in the elementsField array
    for (const auto& element : elementsField)
    {
        // Check if element contains "name" and "type" fields and they are strings
        if (!element.contains("name") || !element["name"].is_string() ||
            !element.contains("type") || !element["type"].is_string())
        {
            SetError(MajorError::INV_ENGINE_COMPONENTS, MinorError::INV_COMPONENTS_OUTPUT, element.dump().c_str());
            return false;
        }

        // Parse the element type
        std::string compTypeString = element["type"].get<std::string>();
        ElementType compType = Engine::ParseElementType(compTypeString);
        if (compType == ElementType::Invalid)
        {
            SetError(MajorError::INV_ENGINE_COMPONENTS, MinorError::INV_COMPONENTS_OUTPUT, element["type"].get<std::string>().c_str());
            return false;
        }

        // Create a new element and parse its arguments
        Engine::ElementTypeDef comp(element["name"].get<std::string>(), compType);
        ParseElementArguments(&comp, element["args"]);

        // Finally, add the element to the engine
        board->GetEngine()->AddElement(&comp);
    }
    return true;
}

// Parse element arguments
void Builder::ParseElementArguments(Engine::ElementTypeDef* comp, const nlohmann::json& args)
{
    uint8_t i = 0;
    for (const auto& arg : args)
    {
        if (i >= 5) break;
        if (arg.is_number_integer()) comp->args[i].uArg = arg.get<uint16_t>();
        else if (arg.is_number_float()) comp->args[i].fArg = arg.get<float>();
        else if (arg.is_boolean()) comp->args[i].bArg = arg.get<bool>();
        else if (arg.is_string()) {
            std::string str = arg.get<std::string>();
            size_t len = str.length();
            if (len >= LE_ELEMENT_ARGUMENT_LENGTH) len = LE_ELEMENT_ARGUMENT_LENGTH - 1;
            memcpy(comp->args[i].sArg, str.c_str(), len);
            comp->args[i].sArg[len] = '\0';
        }
        else continue;
        ++i;
    }
}

// Parse nets section
bool Builder::ParseNets(Board* board, const nlohmann::json& netsField)
{
    // Check if netsField is an array
    if (!netsField.is_array())
    {
        // Set error if netsField is not an array
        SetError(MajorError::INV_ENGINE_NETS, MinorError::NONE, netsField.dump().c_str());
        return false;
    }

    // Iterate through each net in the netsField array
    for (const auto& net : netsField)
    {
        // Initialize net definition
        Engine::ElementNetTypeDef netDef("", "");

        // Parse the output connection of the net
        if (!net.contains("output") || !net["output"].is_object())
        {
            SetError(MajorError::INV_ENGINE_NETS, MinorError::NONE, net.dump().c_str());
            return false;
        }

        if (!ParseNetConnection(&netDef.output, net["output"]))
            return false;

        // Check if net contains "inputs" field and it is an array
        if (!net.contains("inputs") || !net["inputs"].is_array())
        {
            // Set error if "inputs" field is missing or not an array
            SetError(MajorError::INV_ENGINE_NETS, MinorError::NONE, net.dump().c_str());
            return false;
        }

        // Iterate through each input in the "inputs" array
        for (const auto& input : net["inputs"])
        {
            // Initialize net connection
            Engine::ElementNetConnectionTypeDef connection;

            // Parse the input connection and add to net definition
            if (ParseNetConnection(&connection, input))
                netDef.inputs.push_back(connection);
            else
                return false;
        }

        // Add the net definition to the engine
        board->GetEngine()->AddNet(&netDef);
    }
    return true;
}

// Parse a single net connection
bool Builder::ParseNetConnection(Engine::ElementNetConnectionTypeDef* connection, const nlohmann::json& j)
{
    if (!j.contains("name") || !j["name"].is_string() || !j.contains("port") || !j["port"].is_string())
    {
        SetError(MajorError::INV_ENGINE_NETS, MinorError::INV_ENGINE_NETS, j.dump().c_str());
        return false;
    }

    Engine::CopyAndClampString(j["name"].get<std::string>(), connection->name, LE_ELEMENT_NAME_LENGTH);
    Engine::CopyAndClampString(j["port"].get<std::string>(), connection->port, LE_ELEMENT_NAME_LENGTH);

    return true;
}

bool Builder::ParseSER(Board* board, const nlohmann::json& serField)
{
    // Check if the serField is an array
    if (!serField.is_array())
    {
        // Set error if serField is not an array
        SetError(MajorError::INV_SER, MinorError::NONE, serField.dump().c_str());
        return false;
    }

    uint8_t serElementCount = 0; // Counter for SER elements
    std::vector<Engine::ElementNetTypeDef> nets; // Vector to store net definitions

    // Iterate through each SER element in the array
    for (const auto& serElement : serField)
    {
        // Check if the SER element contains "name" and "slot" fields
        if (!serElement.contains("name") || !serElement["name"].is_string() ||
            !serElement.contains("slot") || !serElement["slot"].is_number_integer())
        {
            // Set error if required fields are missing or invalid
            SetError(MajorError::INV_SER, MinorError::INV_SER_POINT, serElement.dump().c_str());
            return false;
        }

        // Extract the name and slot of the SER element
        std::string elementName = serElement["name"].get<std::string>();
        uint8_t outputSlot = serElement["slot"].get<int>();

        // Create a new net definition and add the input
        Engine::ElementNetTypeDef net(elementName, "output");
        net.AddInput(DEFAULT_SER_NAME, "input_" + std::to_string(serElementCount));

        // Add the net definition to the vector
        nets.push_back(net);

        serElementCount++; // Increment the SER element counter
    }

    // Create the SER element and set the number of inputs
    Engine::ElementTypeDef ser(DEFAULT_SER_NAME, ElementType::SER);
    ser.args[0].uArg = serElementCount;

    // Add the SER element to the engine
    board->GetEngine()->AddElement(&ser);

    // Add all net definitions to the engine
    for (Engine::ElementNetTypeDef net : nets)
    {
        board->GetEngine()->AddNet(&net);
    }

    return true;
}


#ifdef LE_DNP3
// Parse outstation configuration
bool Builder::ParseOutstationConfig(DNP3OutstationConfig* config, const nlohmann::json& outstationField)
{
    // Check if the outstationField is an object
    if (!outstationField.is_object())
    {
        // Set error if outstationField is not an object
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::NONE, outstationField.dump().c_str());
        return false;
    }

    // Parse the name of the outstation
    if (outstationField.contains("name") && outstationField["name"].is_string())
    {
        config->name = outstationField["name"].get<std::string>();
    }
    else
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::NONE, outstationField.dump().c_str());
        return false;
    }

    // Parse the DNP3 address of the outstation
    if (!ParseDNP3Address(config->outstation, outstationField["address"]))
    {
        return false;
    };

    // Check if the outstationField contains "sessions" field and it is an array
    if (!outstationField.contains("sessions") || !outstationField["sessions"].is_array())
    {
        // Set error if "sessions" field is missing or not an array
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::INV_DNP3_SESSION, outstationField.dump().c_str());
        return false;
    }

    // Iterate through each session in the "sessions" array
    for (const auto& sessionField : outstationField["sessions"])
    {
        // Initialize session configuration
        DNP3OutstationSessionConfig session;

        // Parse the name of the session
        session.name = sessionField["name"].get<std::string>();

        // Parse the DNP3 address of the session client
        if (!ParseDNP3Address(session.client, sessionField["address"]))
            return false;

        // Parse the points configuration of the session
        if (!ParsePoints(session, sessionField["points"]))
            return false;
        
        // Add session to outstation
        config->AddSession(session);
    }

    return true;
}

// Parse DNP3 address configuration
bool Builder::ParseDNP3Address(DNP3Address& address, const nlohmann::json& addressField)
{
    // Check if the addressField is an object
    if (!addressField.is_object())
    {
        // Set error if addressField is not an object
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::NONE, addressField.dump().c_str());
        return false;
    }

    // Check if the "ip" field exists and is a string
    if (!addressField.contains("ip") || !addressField["ip"].is_string())
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::NONE, addressField.dump().c_str());
        return false;
    }

    // Check if the "dnp" field exists and is an integer
    if (!addressField.contains("dnp") || !addressField["dnp"].is_number_integer())
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::NONE, addressField.dump().c_str());
        return false;
    }

    // Check if the "port" field exists and is an integer
    if (!addressField.contains("port") || !addressField["port"].is_number_integer())
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::NONE, addressField.dump().c_str());
        return false;
    }

    // Parse the IP address
    address.ip = addressField["ip"].get<std::string>();
    
    // Parse the DNP3 address
    address.dnp = addressField["dnp"].get<int>();
    
    // Parse the port number
    address.port = addressField["port"].get<int>();

    return true;
}

// Parse points for a DNP3 session
bool Builder::ParsePoints(DNP3OutstationSessionConfig& session, const nlohmann::json& pointsField)
{
    if (!pointsField.is_object())
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::INV_DNP3_POINT, pointsField.dump().c_str());
        return false;
    }

    // Parse Binary Inputs
    if (pointsField.contains("binary_inputs") && pointsField["binary_inputs"].is_array())
    {
        for (const auto& pointField : pointsField["binary_inputs"])
        {
            if (!ParseBinaryInput(session, pointField))
                return false;
        }
    }

    // Parse Binary Outputs
    if (pointsField.contains("binary_outputs") && pointsField["binary_outputs"].is_array())
    {
        for (const auto& pointField : pointsField["binary_outputs"])
        {
            if (!ParseBinaryOutput(session, pointField))
                return false;
        }
    }

    // Parse Analog Inputs
    if (pointsField.contains("analog_inputs") && pointsField["analog_inputs"].is_array())
    {
        for (const auto& pointField : pointsField["analog_inputs"])
        {
            if (!ParseAnalogInput(session, pointField))
                return false;
        }
    }

    // Parse Analog Outputs
    if (pointsField.contains("analog_outputs") && pointsField["analog_outputs"].is_array())
    {
        for (const auto& pointField : pointsField["analog_outputs"])
        {
            if (!ParseAnalogOutput(session, pointField))
                return false;
        }
    }

    return true;
}

bool Builder::ParseBinaryInput(DNP3OutstationSessionConfig& session, const nlohmann::json& pointField)
{
    if (!pointField.contains("index") || !pointField["index"].is_number_integer() ||
        !pointField.contains("name") || !pointField["name"].is_string() ||
        !pointField.contains("class") || !pointField["class"].is_string() ||
        !pointField.contains("sVar") || !pointField["sVar"].is_string() ||
        !pointField.contains("eVar") || !pointField["eVar"].is_string())
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::INV_DNP3_POINT, pointField.dump().c_str());
        return false;
    }

    int index = pointField["index"].get<int>();
    const std::string& name = pointField["name"].get<std::string>();
    PointClass pointClass = ParsePointClass(pointField["class"].get<std::string>());
    StaticBinaryVariation sVar = StringToStaticBinaryVariation(pointField["sVar"].get<std::string>().c_str());
    EventBinaryVariation eVar = StringToEventBinaryVariation(pointField["eVar"].get<std::string>().c_str());

    session.AddBinaryInput(index, name.c_str(), pointClass, sVar, eVar);
    return true;
}

bool Builder::ParseBinaryOutput(DNP3OutstationSessionConfig& session, const nlohmann::json& pointField)
{
    if (!pointField.contains("index") || !pointField["index"].is_number_integer() ||
        !pointField.contains("name") || !pointField["name"].is_string() ||
        !pointField.contains("class") || !pointField["class"].is_string() ||
        !pointField.contains("sVar") || !pointField["sVar"].is_string() ||
        !pointField.contains("eVar") || !pointField["eVar"].is_string())
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::INV_DNP3_POINT, pointField.dump().c_str());
        return false;
    }

    int index = pointField["index"].get<int>();
    const std::string& name = pointField["name"].get<std::string>();
    PointClass pointClass = ParsePointClass(pointField["class"].get<std::string>());
    StaticBinaryOutputStatusVariation sVar = StringToStaticBinaryOutputStatusVariation(pointField["sVar"].get<std::string>().c_str());
    EventBinaryOutputStatusVariation eVar = StringToEventBinaryOutputStatusVariation(pointField["eVar"].get<std::string>().c_str());

    session.AddBinaryOutput(index, name.c_str(), pointClass, sVar, eVar);
    return true;
}

bool Builder::ParseAnalogInput(DNP3OutstationSessionConfig& session, const nlohmann::json& pointField)
{
    if (!pointField.contains("index") || !pointField["index"].is_number_integer() ||
        !pointField.contains("name") || !pointField["name"].is_string() ||
        !pointField.contains("class") || !pointField["class"].is_string() ||
        !pointField.contains("sVar") || !pointField["sVar"].is_string() ||
        !pointField.contains("eVar") || !pointField["eVar"].is_string())
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::INV_DNP3_POINT, pointField.dump().c_str());
        return false;
    }

    int index = pointField["index"].get<int>();
    const std::string& name = pointField["name"].get<std::string>();
    PointClass pointClass = ParsePointClass(pointField["class"].get<std::string>());
    StaticAnalogVariation sVar = StringToStaticAnalogVariation(pointField["sVar"].get<std::string>().c_str());
    EventAnalogVariation eVar = StringToEventAnalogVariation(pointField["eVar"].get<std::string>().c_str());

    session.AddAnalogInput(index, name.c_str(), pointClass, 0.0f, sVar, eVar);
    return true;
}

bool Builder::ParseAnalogOutput(DNP3OutstationSessionConfig& session, const nlohmann::json& pointField)
{
    if (!pointField.contains("index") || !pointField["index"].is_number_integer() ||
        !pointField.contains("name") || !pointField["name"].is_string() ||
        !pointField.contains("class") || !pointField["class"].is_string() ||
        !pointField.contains("sVar") || !pointField["sVar"].is_string() ||
        !pointField.contains("eVar") || !pointField["eVar"].is_string())
    {
        SetError(MajorError::INV_DNP3_CONFIG, MinorError::INV_DNP3_POINT, pointField.dump().c_str());
        return false;
    }

    int index = pointField["index"].get<int>();
    const std::string& name = pointField["name"].get<std::string>();
    PointClass pointClass = ParsePointClass(pointField["class"].get<std::string>());
    StaticAnalogOutputStatusVariation sVar = StringToStaticAnalogOutputStatusVariation(pointField["sVar"].get<std::string>().c_str());
    EventAnalogOutputStatusVariation eVar = StringToEventAnalogOutputStatusVariation(pointField["eVar"].get<std::string>().c_str());

    session.AddAnalogOutput(index, name.c_str(), pointClass, 0.0f, sVar, eVar);
    return true;
}

PointClass Builder::ParsePointClass(const std::string& str)
{
    static const std::unordered_map<std::string, PointClass> map = {
        {"Class0", PointClass::Class0},
        {"Class1", PointClass::Class1},
        {"Class2", PointClass::Class2},
        {"Class3", PointClass::Class3}
    };

    auto it = map.find(str);
    return it != map.end() ? it->second : PointClass::Class1;  // Default or error handling
}

// String-to-enum conversion for StaticBinaryVariation
StaticBinaryVariation Builder::StringToStaticBinaryVariation(const std::string& str)
{
    static const std::unordered_map<std::string, StaticBinaryVariation> map = {
        {"Group1Var1", StaticBinaryVariation::Group1Var1},
        {"Group1Var2", StaticBinaryVariation::Group1Var2}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : StaticBinaryVariation::Group1Var1;  // Default or error handling
}

// String-to-enum conversion for EventBinaryVariation
EventBinaryVariation Builder::StringToEventBinaryVariation(const std::string& str)
{
    static const std::unordered_map<std::string, EventBinaryVariation> map = {
        {"Group2Var1", EventBinaryVariation::Group2Var1},
        {"Group2Var2", EventBinaryVariation::Group2Var2},
        {"Group2Var3", EventBinaryVariation::Group2Var3}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : EventBinaryVariation::Group2Var1;  // Default or error handling
}

// String-to-enum conversion for StaticBinaryOutputStatusVariation
StaticBinaryOutputStatusVariation Builder::StringToStaticBinaryOutputStatusVariation(const std::string& str)
{
    static const std::unordered_map<std::string, StaticBinaryOutputStatusVariation> map = {
        {"Group10Var2", StaticBinaryOutputStatusVariation::Group10Var2}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : StaticBinaryOutputStatusVariation::Group10Var2;  // Default or error handling
}

// String-to-enum conversion for EventBinaryOutputStatusVariation
EventBinaryOutputStatusVariation Builder::StringToEventBinaryOutputStatusVariation(const std::string& str)
{
    static const std::unordered_map<std::string, EventBinaryOutputStatusVariation> map = {
        {"Group11Var1", EventBinaryOutputStatusVariation::Group11Var1},
        {"Group11Var2", EventBinaryOutputStatusVariation::Group11Var2}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : EventBinaryOutputStatusVariation::Group11Var1;  // Default or error handling
}

// String-to-enum conversion for StaticAnalogVariation
StaticAnalogVariation Builder::StringToStaticAnalogVariation(const std::string& str)
{
    static const std::unordered_map<std::string, StaticAnalogVariation> map = {
        {"Group30Var1", StaticAnalogVariation::Group30Var1},
        {"Group30Var2", StaticAnalogVariation::Group30Var2},
        {"Group30Var3", StaticAnalogVariation::Group30Var3},
        {"Group30Var4", StaticAnalogVariation::Group30Var4},
        {"Group30Var5", StaticAnalogVariation::Group30Var5},
        {"Group30Var6", StaticAnalogVariation::Group30Var6}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : StaticAnalogVariation::Group30Var1;  // Default or error handling
}

// String-to-enum conversion for EventAnalogVariation
EventAnalogVariation Builder::StringToEventAnalogVariation(const std::string& str)
{
    static const std::unordered_map<std::string, EventAnalogVariation> map = {
        {"Group32Var1", EventAnalogVariation::Group32Var1},
        {"Group32Var2", EventAnalogVariation::Group32Var2},
        {"Group32Var3", EventAnalogVariation::Group32Var3},
        {"Group32Var4", EventAnalogVariation::Group32Var4},
        {"Group32Var5", EventAnalogVariation::Group32Var5},
        {"Group32Var6", EventAnalogVariation::Group32Var6},
        {"Group32Var7", EventAnalogVariation::Group32Var7},
        {"Group32Var8", EventAnalogVariation::Group32Var8}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : EventAnalogVariation::Group32Var1;  // Default or error handling
}

// String-to-enum conversion for StaticAnalogOutputStatusVariation
StaticAnalogOutputStatusVariation Builder::StringToStaticAnalogOutputStatusVariation(const std::string& str)
{
    static const std::unordered_map<std::string, StaticAnalogOutputStatusVariation> map = {
        {"Group40Var1", StaticAnalogOutputStatusVariation::Group40Var1},
        {"Group40Var2", StaticAnalogOutputStatusVariation::Group40Var2},
        {"Group40Var3", StaticAnalogOutputStatusVariation::Group40Var3},
        {"Group40Var4", StaticAnalogOutputStatusVariation::Group40Var4}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : StaticAnalogOutputStatusVariation::Group40Var1;  // Default or error handling
}

// String-to-enum conversion for EventAnalogOutputStatusVariation
EventAnalogOutputStatusVariation Builder::StringToEventAnalogOutputStatusVariation(const std::string& str)
{
    static const std::unordered_map<std::string, EventAnalogOutputStatusVariation> map = {
        {"Group42Var1", EventAnalogOutputStatusVariation::Group42Var1},
        {"Group42Var2", EventAnalogOutputStatusVariation::Group42Var2},
        {"Group42Var3", EventAnalogOutputStatusVariation::Group42Var3},
        {"Group42Var4", EventAnalogOutputStatusVariation::Group42Var4},
        {"Group42Var5", EventAnalogOutputStatusVariation::Group42Var5},
        {"Group42Var6", EventAnalogOutputStatusVariation::Group42Var6},
        {"Group42Var7", EventAnalogOutputStatusVariation::Group42Var7},
        {"Group42Var8", EventAnalogOutputStatusVariation::Group42Var8}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : EventAnalogOutputStatusVariation::Group42Var1;  // Default or error handling
}
#endif

// Convert MajorError to string
const char* Builder::MajorErrorToString(MajorError error)
{
    switch (error)
    {
        case MajorError::NONE: return "No error";
        case MajorError::INV_FILE: return "Invalid file path or file cannot be opened";
        case MajorError::INV_JSON_FILE: return "Invalid JSON format";
        case MajorError::INV_ENGINE_NAME: return "Missing or invalid engine name in JSON";
        case MajorError::INV_ENGINE_COMPONENTS: return "Invalid or missing engine components in JSON";
        case MajorError::INV_ENGINE_NETS: return "Invalid or missing engine nets in JSON";
        case MajorError::INV_SER: return "Invalid or missing SER configuration";
#ifdef LE_DNP3
        case MajorError::INV_DNP3_CONFIG: return "Invalid or missing DNP3 configuration in JSON";
#endif
        default: return "Unknown major error";
    }
}

// Convert MinorError to string
const char* Builder::MinorErrorToString(MinorError error)
{
    switch (error)
    {
        case MinorError::NONE: return "No error";
        case MinorError::INV_COMPONENTS_OUTPUT: return "Invalid component output in JSON";
        case MinorError::INV_ENGINE_NETS: return "Invalid engine nets in JSON";
        case MinorError::INV_SER_POINT: return "Invalid ser point";
#ifdef LE_DNP3
        case MinorError::INV_DNP3_SESSION: return "Invalid or missing DNP3 session in JSON";
        case MinorError::INV_DNP3_POINT: return "Invalid or missing DNP3 point in JSON";
#endif
        default: return "Unknown minor error";
    }
}

} // namespace LogicElements
