#include "le_Version.hpp"

#if defined(_DEBUG)
#define FIRMWARE_TITLE "Debug"
#else
#define FIRMWARE_TITLE "Release"
#endif
#define FIRMWARE_ID_MAJOR "0"
#define FIRMWARE_ID_MINOR "012"

const char* le_Version::GetVersion()
{
	static const char buffer[] = FIRMWARE_TITLE " v" FIRMWARE_ID_MAJOR "." FIRMWARE_ID_MINOR;
	return buffer;
}

