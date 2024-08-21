#include "le_Builder/le_Builder.hpp"

// Static member initialization
le_Builder::MajorError le_Builder::majorError = le_Builder::MajorError::NONE;
le_Builder::MinorError le_Builder::minorError = le_Builder::MinorError::NONE;
char le_Builder::sErrorMessage[MAX_ERROR_LENGTH] = "";
char le_Builder::sErroneousJson[MAX_ERROR_LENGTH] = "";

// Load configuration from file
bool le_Builder::LoadFromFile(const std::string& filePath, le_Engine*& engine,
#ifdef LE_DNP3
    le_DNP3Outstation_Config*& dnp3Config
#else
    void*& dnp3Config
#endif
)
{
    ClearErrors();
    std::string fileContent = readFile(filePath);
    return fileContent.empty() ? false : LoadConfig(fileContent.c_str(), engine, dnp3Config);
}

// Load configuration from JSON string
bool le_Builder::LoadConfig(const char* jsonString, le_Engine*& engine,
#ifdef LE_DNP3
    le_DNP3Outstation_Config*& dnp3Config
#else
    void*& dnp3Config
#endif
)
{
    ClearErrors();
    json_t* pool = new json_t[MAX_POOL_SIZE];
    json_t const* root = json_create((char*)jsonString, pool, MAX_POOL_SIZE);
    if (!root)
    {
        delete[] pool;
        return SetError(MajorError::INV_JSON_FILE, MinorError::NONE, jsonString), false;
    }

    const char* engineName = json_getValue(json_getProperty(root, "name"));
    if (!engineName)
    {
        delete[] pool;
        return SetError(MajorError::INV_ENGINE_NAME, MinorError::NONE, jsonString), false;
    }
    engine = new le_Engine(engineName);

    if (!ParseElements(engine, json_getProperty(root, "elements")) ||
        !ParseNets(engine, json_getProperty(root, "nets")))
    {
        delete engine;
        delete[] pool;
        return false;
    }

#ifdef LE_DNP3
    json_t const* dnp3Field = json_getProperty(root, "dnp3");
    if (dnp3Field)
    {
        dnp3Config = new le_DNP3Outstation_Config();
        if (!ParseOutstationConfig(dnp3Config, json_getProperty(dnp3Field, "oustation")))
        {
            delete dnp3Config;
            dnp3Config = nullptr;
            delete[] pool;
            return false;
        }
    }
    else
    {
        dnp3Config = nullptr;
    }
#endif

    delete[] pool;
    return true;
}


// Get error information
void le_Builder::GetErrorString(char* buffer, uint16_t length)
{
    if (buffer == nullptr || length == 0) {
        return;
    }

    // Retrieve individual error information
    MajorError major = GetMajorError();
    MinorError minor = GetMinorError();
    const char* errorMessage = GetErrorMessage();
    const char* erroneousJson = GetErroneousJson();

    // Format the combined error string
    snprintf(buffer, length,
        "Major Error: %d, Minor Error: %d\nError Message: %s\nErroneous JSON: %.200s",
        static_cast<int>(major),
        static_cast<int>(minor),
        errorMessage ? errorMessage : "None",
        erroneousJson ? erroneousJson : "None");
}

// Retrieve the last major error
le_Builder::MajorError le_Builder::GetMajorError()
{
    return majorError;
}

// Retrieve the last minor error
le_Builder::MinorError le_Builder::GetMinorError()
{
    return minorError;
}

// Retrieve the last error message
const char* le_Builder::GetErrorMessage()
{
    return sErrorMessage;
}

// Retrieve the erroneous JSON string
const char* le_Builder::GetErroneousJson()
{
    return sErroneousJson;
}

// Clear error state
void le_Builder::ClearErrors()
{
    majorError = MajorError::NONE;
    minorError = MinorError::NONE;
    sErrorMessage[0] = '\0';
    sErroneousJson[0] = '\0';
}

// Set error state and return nullptr
le_Engine* le_Builder::SetError(MajorError major, MinorError minor, const char* erroneousJson)
{
    majorError = major;
    minorError = minor;
    snprintf(sErrorMessage, MAX_ERROR_LENGTH, "Major Error: %d, Minor Error: %d", static_cast<int>(major), static_cast<int>(minor));
    if (erroneousJson)
        snprintf(sErroneousJson, MAX_ERROR_LENGTH, "Erroneous JSON: %.500s", erroneousJson);
    return nullptr;
}

// Read file into string
std::string le_Builder::readFile(const std::string& filePath)
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
bool le_Builder::ParseElements(le_Engine* engine, json_t const* elementsField)
{
    if (!elementsField || JSON_ARRAY != json_getType(elementsField))
        return SetError(MajorError::INV_ENGINE_COMPONENTS, MinorError::NONE, json_getValue(elementsField)), false;

    for (json_t const* element = json_getChild(elementsField); element; element = json_getSibling(element))
    {
        const char* name = json_getValue(json_getProperty(element, "name"));
        const char* type = json_getValue(json_getProperty(element, "type"));

        if (!name || !type) return SetError(MajorError::INV_ENGINE_COMPONENTS, MinorError::INV_COMPONENTS_OUTPUT, json_getValue(element)), false;

        le_Element_Type compType = le_Engine::ParseElementType(std::string(type));
        if (compType == le_Element_Type::LE_INVALID) return SetError(MajorError::INV_ENGINE_COMPONENTS, MinorError::INV_COMPONENTS_OUTPUT, type), false;

        le_Engine::le_Element_TypeDef comp(name, compType);
        ParseElementArguments(&comp, json_getProperty(element, "args"));
        engine->AddElement(&comp);
    }
    return true;
}

// Parse element arguments
void le_Builder::ParseElementArguments(le_Engine::le_Element_TypeDef* comp, json_t const* args)
{
    uint8_t i = 0;
    for (json_t const* arg = json_getChild(args); arg && i < 5; arg = json_getSibling(arg), ++i)
    {
        switch (json_getType(arg))
        {
        case JSON_INTEGER: comp->args[i].uArg = (uint16_t)json_getInteger(arg); break;
        case JSON_REAL: comp->args[i].fArg = (float)json_getReal(arg); break;
        case JSON_BOOLEAN: comp->args[i].bArg = json_getBoolean(arg); break;
        case JSON_TEXT: strncpy(comp->args[i].sArg, json_getValue(arg), LE_ELEMENT_ARGUMENT_LENGTH); break;
        default: --i; break;
        }
    }
}

// Parse nets section
bool le_Builder::ParseNets(le_Engine* engine, json_t const* netsField)
{
    if (!netsField || JSON_ARRAY != json_getType(netsField))
        return SetError(MajorError::INV_ENGINE_NETS, MinorError::NONE, json_getValue(netsField)), false;

    for (json_t const* net = json_getChild(netsField); net; net = json_getSibling(net))
    {
        le_Engine::le_Element_Net_TypeDef netDef("", 0);
        if (!ParseNetConnection(&netDef.output, json_getProperty(net, "output"))) return false;

        json_t const* inputs = json_getProperty(net, "inputs");
        if (!inputs || JSON_ARRAY != json_getType(inputs))
            return SetError(MajorError::INV_ENGINE_NETS, MinorError::NONE, json_getValue(inputs)), false;

        for (json_t const* input = json_getChild(inputs); input; input = json_getSibling(input))
        {
            le_Engine::le_Element_Net_Connection_TypeDef connection;
            if (ParseNetConnection(&connection, input))
                netDef.inputs.push_back(connection);
        }

        engine->AddNet(&netDef);
    }
    return true;
}

// Parse a single net connection
bool le_Builder::ParseNetConnection(le_Engine::le_Element_Net_Connection_TypeDef* connection, json_t const* j)
{
    const char* name = json_getValue(json_getProperty(j, "name"));
    int64_t slot = json_getInteger(json_getProperty(j, "slot"));

    if (!name || slot < 0)
        return SetError(MajorError::INV_ENGINE_NETS, MinorError::INV_ENGINE_NETS, json_getValue(j)), false;

    strncpy(connection->name, name, LE_ELEMENT_NAME_LENGTH);
    connection->slot = static_cast<uint16_t>(slot);

    return true;
}

#ifdef LE_DNP3
// Parse outstation configuration
bool le_Builder::ParseOutstationConfig(le_DNP3Outstation_Config* config, json_t const* outstationField)
{
    if (!outstationField) return SetError(MajorError::INV_DNP3_CONFIG, MinorError::NONE, json_getValue(outstationField)), false;

    config->name = json_getValue(json_getProperty(outstationField, "name"));
    if (!ParseDNP3Address(config->outstation, json_getProperty(outstationField, "address"))) return false;

    json_t const* sessionsField = json_getProperty(outstationField, "sessions");
    if (!sessionsField || JSON_ARRAY != json_getType(sessionsField)) return SetError(MajorError::INV_DNP3_CONFIG, MinorError::INV_DNP3_SESSION, json_getValue(sessionsField)), false;

    for (json_t const* sessionField = json_getChild(sessionsField); sessionField; sessionField = json_getSibling(sessionField))
    {
        le_DNP3Outstation_Session_Config session;
        session.name = json_getValue(json_getProperty(sessionField, "name"));
        if (!ParseDNP3Address(session.client, json_getProperty(sessionField, "address"))) return false;
        if (!ParsePoints(session, json_getProperty(sessionField, "points"))) return false;
        config->AddSession(session);
    }

    return true;
}

// Parse DNP3 address configuration
bool le_Builder::ParseDNP3Address(le_DNP3_Address& address, json_t const* addressField)
{
    if (!addressField) return SetError(MajorError::INV_DNP3_CONFIG, MinorError::NONE, json_getValue(addressField)), false;

    address.ip = json_getValue(json_getProperty(addressField, "ip"));
    address.dnp = json_getInteger(json_getProperty(addressField, "dnp"));
    address.port = json_getInteger(json_getProperty(addressField, "port"));

    return true;
}

// Parse points for a DNP3 session
bool le_Builder::ParsePoints(le_DNP3Outstation_Session_Config& session, json_t const* pointsField)
{
    if (!pointsField) return SetError(MajorError::INV_DNP3_CONFIG, MinorError::INV_DNP3_POINT, json_getValue(pointsField)), false;

    // Parse Binary Inputs
    json_t const* binaryInputsField = json_getProperty(pointsField, "binary_inputs");
    if (binaryInputsField && JSON_ARRAY == json_getType(binaryInputsField))
    {
        for (json_t const* pointField = json_getChild(binaryInputsField); pointField; pointField = json_getSibling(pointField))
        {
            int index = json_getInteger(json_getProperty(pointField, "index"));
            const char* name = json_getValue(json_getProperty(pointField, "name"));
            StaticBinaryVariation sVar = StringToStaticBinaryVariation(json_getValue(json_getProperty(pointField, "sVar")));
            EventBinaryVariation eVar = StringToEventBinaryVariation(json_getValue(json_getProperty(pointField, "eVar")));

            session.AddBinaryInput(index, name, PointClass::Class1, sVar, eVar);
        }
    }

    // Parse Binary Outputs
    json_t const* binaryOutputsField = json_getProperty(pointsField, "binary_outputs");
    if (binaryOutputsField && JSON_ARRAY == json_getType(binaryOutputsField))
    {
        for (json_t const* pointField = json_getChild(binaryOutputsField); pointField; pointField = json_getSibling(pointField))
        {
            int index = json_getInteger(json_getProperty(pointField, "index"));
            const char* name = json_getValue(json_getProperty(pointField, "name"));
            StaticBinaryOutputStatusVariation sVar = StringToStaticBinaryOutputStatusVariation(json_getValue(json_getProperty(pointField, "sVar")));
            EventBinaryOutputStatusVariation eVar = StringToEventBinaryOutputStatusVariation(json_getValue(json_getProperty(pointField, "eVar")));

            session.AddBinaryOutput(index, name, PointClass::Class1, sVar, eVar);
        }
    }

    // Parse Analog Inputs
    json_t const* analogInputsField = json_getProperty(pointsField, "analog_inputs");
    if (analogInputsField && JSON_ARRAY == json_getType(analogInputsField))
    {
        for (json_t const* pointField = json_getChild(analogInputsField); pointField; pointField = json_getSibling(pointField))
        {
            int index = json_getInteger(json_getProperty(pointField, "index"));
            const char* name = json_getValue(json_getProperty(pointField, "name"));
            StaticAnalogVariation sVar = StringToStaticAnalogVariation(json_getValue(json_getProperty(pointField, "sVar")));
            EventAnalogVariation eVar = StringToEventAnalogVariation(json_getValue(json_getProperty(pointField, "eVar")));

            session.AddAnalogInput(index, name, PointClass::Class1, sVar, eVar);
        }
    }

    // Parse Analog Outputs
    json_t const* analogOutputsField = json_getProperty(pointsField, "analog_outputs");
    if (analogOutputsField && JSON_ARRAY == json_getType(analogOutputsField))
    {
        for (json_t const* pointField = json_getChild(analogOutputsField); pointField; pointField = json_getSibling(pointField))
        {
            int index = json_getInteger(json_getProperty(pointField, "index"));
            const char* name = json_getValue(json_getProperty(pointField, "name"));
            StaticAnalogOutputStatusVariation sVar = StringToStaticAnalogOutputStatusVariation(json_getValue(json_getProperty(pointField, "sVar")));
            EventAnalogOutputStatusVariation eVar = StringToEventAnalogOutputStatusVariation(json_getValue(json_getProperty(pointField, "eVar")));

            session.AddAnalogOutput(index, name, PointClass::Class1, sVar, eVar);
        }
    }

    return true;
}

// String-to-enum conversion for StaticBinaryVariation
StaticBinaryVariation le_Builder::StringToStaticBinaryVariation(const char* str)
{
    static const std::unordered_map<std::string, StaticBinaryVariation> map = {
        {"Group1Var1", StaticBinaryVariation::Group1Var1},
        {"Group1Var2", StaticBinaryVariation::Group1Var2}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : StaticBinaryVariation::Group1Var1;  // Default or error handling
}

// String-to-enum conversion for EventBinaryVariation
EventBinaryVariation le_Builder::StringToEventBinaryVariation(const char* str)
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
StaticBinaryOutputStatusVariation le_Builder::StringToStaticBinaryOutputStatusVariation(const char* str)
{
    static const std::unordered_map<std::string, StaticBinaryOutputStatusVariation> map = {
        {"Group10Var2", StaticBinaryOutputStatusVariation::Group10Var2}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : StaticBinaryOutputStatusVariation::Group10Var2;  // Default or error handling
}

// String-to-enum conversion for EventBinaryOutputStatusVariation
EventBinaryOutputStatusVariation le_Builder::StringToEventBinaryOutputStatusVariation(const char* str)
{
    static const std::unordered_map<std::string, EventBinaryOutputStatusVariation> map = {
        {"Group11Var1", EventBinaryOutputStatusVariation::Group11Var1},
        {"Group11Var2", EventBinaryOutputStatusVariation::Group11Var2}
    };
    auto it = map.find(str);
    return it != map.end() ? it->second : EventBinaryOutputStatusVariation::Group11Var1;  // Default or error handling
}

// String-to-enum conversion for StaticAnalogVariation
StaticAnalogVariation le_Builder::StringToStaticAnalogVariation(const char* str)
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
EventAnalogVariation le_Builder::StringToEventAnalogVariation(const char* str)
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
StaticAnalogOutputStatusVariation le_Builder::StringToStaticAnalogOutputStatusVariation(const char* str)
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
EventAnalogOutputStatusVariation le_Builder::StringToEventAnalogOutputStatusVariation(const char* str)
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