#include "version.h"

#include "utils.h"
#include <iostream>
#include <iomanip>
#include <string>

#define STATE_MAJOR 0
#define STATE_MINOR 1
#define STATE_HASH  2
#define STATE_DEV   3

size_t findSearchString(const char* searchString, size_t searchStringLen, char* payloadData, size_t payloadSize, size_t payloadOffset);
std::string getVersion(char* payloadData, size_t payloadSize);

std::string getCommit(std::string commitString) {
	std::string commit = "";
	for (int i = 0; i < 7; i++)
	{
		commit += commitString[i];
	}
	return commit;
}

const std::string LumaVersion::toString(bool printBranch) const {
	std::string currentVersionStr = release;
	if (!commit.empty())
		currentVersionStr += "-" + getCommit(commit);
	
	return currentVersionStr;
}

template< typename T > std::string to_hex( T i )
{
  std::stringstream stream;
  stream << std::hex << i;
  return stream.str();
}

bool isNumeric(char c) {
	return (c >= 0x30) && (c <= 0x39);
}

bool isAlpha(char c) {
	return ((c >= 0x41) && (c <= 0x5A)) || ((c >= 0x61) && (c <= 0x7A));
}

bool isAlphaNumeric(char c) {
	return isNumeric(c) || isAlpha(c);
}

/* Luma3DS 0x2e svc version struct */
struct PACKED SvcLumaVersion {
	char magic[4];
	uint8_t major;
	uint8_t minor;
	uint8_t build;
	uint8_t flags;
	uint32_t commit;
	uint32_t unused;
};


int NAKED svcGetLumaVersion(SvcLumaVersion UNUSED *info) {
	asm volatile(
		"svc 0x2E\n"
		"bx lr"
	);
}


LumaVersion versionSvc() {
	SvcLumaVersion info;
	if (svcGetLumaVersion(&info) != 0) {
		return LumaVersion{};
	}

	LumaVersion version;

	std::stringstream releaseBuilder;
	releaseBuilder << (int)info.major << "." << (int)info.minor;
	if (info.build > 0) {
		releaseBuilder << "." << (int)info.build;
	}
	version.release = releaseBuilder.str();

	if (info.commit > 0) {
		std::stringstream commitBuilder;
		commitBuilder << std::hex << info.commit;
		version.commit = commitBuilder.str();
	}

	version.isDev = (info.flags & 0x1) == 0x1;

	logPrintf("%s\n", version.toString().c_str());
	return version;
}

LumaVersion versionSvcGetSystemInfo() {
	LumaVersion ver;
	s64 version;
	// Get SystemVersion Maj.Min.Build
	svcGetSystemInfo(&version, 0x10000, 0);
	if(!version)
		return LumaVersion{};
	int major = version >> 24;
	int minor = version >> 16;
	std::string smajor = std::to_string(major);
	std::string sminor = std::to_string(minor);
	ver.release = "Luma3DS v" + smajor + "." + sminor;
	// Get Commit Hash
	svcGetSystemInfo(&version, 0x10000, 1);
	ver.commit = to_hex(version);
	// Get Release Flag
	svcGetSystemInfo(&version, 0x10000, 0x200);
	ver.isDev = version == 0x01;
	logPrintf("%s\n", ver.toString().c_str());
	return ver;
}

LumaVersion versionMemsearch(const std::string& path) {
	std::ifstream payloadFile(path, std::ios::binary | std::ios::ate);
	if (!payloadFile) {
		logPrintf("Could not open existing payload, does it exists?\n");
		return LumaVersion{};
	}

	/* Load entire file into local buffer */
	size_t payloadSize = payloadFile.tellg();
	payloadFile.seekg(0, std::ios::beg);
	char* payloadData = (char*)std::malloc(payloadSize);
	payloadFile.read(payloadData, payloadSize);
	payloadFile.close();

	logPrintf("Loaded existing payload in memory, searching for version number...\n");

	std::string versionString = getVersion(payloadData, payloadSize);

	std::free(payloadData);

	const size_t separator = versionString.find("-");
	if (separator == std::string::npos) {
		return LumaVersion{ versionString, "", false };
	}

	LumaVersion version;
	version.release = versionString.substr(0, separator);

	const size_t end = versionString.find(" ", separator);
	if (end == std::string::npos) {
		version.commit = versionString.substr(separator + 1);
		version.isDev = false;
	} else {
		version.commit = versionString.substr(separator + 1, end - separator - 1);
		version.isDev = versionString.find("(dev)", separator) != std::string::npos;
	}

	return version;
}

std::string getVersion(char* payloadData, size_t payloadSize) {
	const static char searchString[] = "Luma3DS v";
	const static size_t searchStringLen = sizeof(searchString)/sizeof(char) - 1;
	size_t payloadOffset = 0;
	size_t versionLen = 0;
	bool found = false;
	std::string version = "";
	
	while (payloadOffset != std::string::npos && !found) {
		u8 state = STATE_MAJOR;
		bool regexFound = false;
		bool regexValid = true;
		size_t verOffset = findSearchString(searchString, searchStringLen, payloadData, payloadSize, payloadOffset);
		payloadOffset = verOffset;
		size_t offset = verOffset;
		size_t lastOffset = verOffset;
		
		// Version regex should be [0-9]+\.[0-9].
		while (!regexFound && regexValid && (offset != std::string::npos) && (offset < payloadSize)) {
			if (isNumeric(payloadData[offset]) && ((state == STATE_MAJOR) || (state == STATE_MINOR))) {
				offset++;
			}
			else if (isAlphaNumeric(payloadData[offset]) && ((state == STATE_MAJOR) || (state == STATE_MINOR))) {
				offset++;
			}
			else if ((payloadData[offset] == 0x2E) && (state == STATE_MAJOR) && (offset != lastOffset)) {
				state = STATE_MINOR;
				lastOffset = offset;
				offset++;
			}
			else if ((payloadData[offset] == 0x2D) && (state == STATE_MINOR) && (offset != lastOffset)) {
				state = STATE_HASH;
				lastOffset = offset;
				offset++;
			}
			else if ((payloadData[offset] == 0x20) && 
				 ((offset+3 < payloadSize) && (payloadData[offset+1] == 0x64) && (payloadData[offset+2] == 0x65) && (payloadData[offset+3] == 0x76)) && 
				 (state == STATE_HASH) && (offset != lastOffset)) {
				state = STATE_DEV;
				offset += 4; // skip next three bytes 'dev' and increment by one
			}
			else if ((payloadData[offset] == 0x20) && 
				 ((offset+1 < payloadSize) && (payloadData[offset+1] == 0x63)) && 
				 (state >= STATE_MINOR) && (offset != lastOffset)) {
				offset--;
				regexFound = true;
			}
			else {
				regexValid = false;
			}
		}
		if (regexFound) {
			versionLen = offset - verOffset;
			found = true;
		}
	}
	if (found) {
		version = std::string(payloadData + payloadOffset, versionLen);
	}
	return version;
}

size_t findSearchString(const char* searchString, size_t searchStringLen, char* payloadData, size_t payloadSize, size_t payloadOffset) {
	size_t curProposedOffset = 0;
	unsigned short curStringIndex = 0;
	bool found = false;

	// Byte-by-byte search. (memcmp might be faster?)
	// Since " " (1st char) is only used once in the whole string we can search in O(n)
	for (size_t offset = payloadOffset; offset < payloadSize - searchStringLen; ++offset) {
		if (payloadData[offset] == searchString[curStringIndex]) {
			if (curStringIndex == searchStringLen - 1) {
				found = true;
				break;
			}
			if (curStringIndex == 0) {
				curProposedOffset = offset;
			}
			curStringIndex++;
		}
		else {
			if (curStringIndex > 0) {
				curStringIndex = 0;
			}
		}
	}
	if (found) {
		curProposedOffset += searchStringLen;
		return curProposedOffset;
	}
	return std::string::npos;
}
