#include <Windows.h>
#include <stdio.h>
#include "RomTools_Common.h"
#include "Settings Api.h"
#include "Pif.h"
#include "xxhash64.h"
#include "FileHandler.h"
#include <string>

void CountryCodeToString(char string[], BYTE Country, int length) {
	switch (Country) {
	case '7': strncpy(string, "Beta", length); break;
	case 'A': strncpy(string, "NTSC", length); break;
	case 'B': strncpy(string, "Brazil", length); break;
	case 'C': strncpy(string, "China", length); break;
	case 'D': strncpy(string, "Germany", length); break;
	case 'E': strncpy(string, "USA", length); break;
	case 'F': strncpy(string, "France", length); break;
	case 'G': strncpy(string, "Gateway (NTSC)", length); break;
	case 'I': strncpy(string, "Italy", length); break;
	case 'J': strncpy(string, "Japan", length); break;
	case 'L': strncpy(string, "Lodgenet (PAL)", length); break;
	case 'P': strncpy(string, "Europe", length); break;
	case 'S': strncpy(string, "Spain", length); break;
	case 'U': strncpy(string, "Australia", length); break;
	case 'X': strncpy(string, "PAL", length); break;
	case 'Y': strncpy(string, "PAL", length); break;
	case ' ': strncpy(string, "None (PD by NAN)", length); break;
	case 0: strncpy(string, "None (PD)", length); break;
	default:
		if (length > 20)
			sprintf(string, "Unknown %c (%02X)", Country, Country);
		break;
	}
}

void CountryCodeToShortString(char string[], BYTE Country, int length) {
	switch (Country) {
	case '7': strncpy(string, "(Beta)", length); break;
	case 'A': strncpy(string, "(NTSC)", length); break;
	case 'B': strncpy(string, "(B)", length); break;
	case 'C': strncpy(string, "(C)", length); break;
	case 'D': strncpy(string, "(G)", length); break;
	case 'E': strncpy(string, "(U)", length); break;
	case 'F': strncpy(string, "(F)", length); break;
	case 'I': strncpy(string, "(I)", length); break;
	case 'J': strncpy(string, "(J)", length); break;
	case 'P': strncpy(string, "(E)", length); break;
	case 'S': strncpy(string, "(S)", length); break;
	case 'U': strncpy(string, "(A)", length); break;
	case 'X': strncpy(string, "(PAL)", length); break;
	case 'Y': strncpy(string, "(PAL)", length); break;
	case ' ': strncpy(string, "(PD by NAN)", length); break;
	case 0: strncpy(string, "(PD)", length); break;
	default:
		if (length > 20)
			sprintf(string, "(%02X)", Country);
		break;
	}
}

int GetRomRegion(BYTE* RomData) {
	BYTE Country;
	GetRomCountry(&Country, RomData);
	return GetRomRegionByCode(Country);
}

int GetRomRegionByCode(BYTE CountryCode) {
	switch (CountryCode) {
	case 'D': // Germany
	case 'F': // French
	case 'I': // Italian
	case 'L': // Lodgenet (PAL)
	case 'S': // Spanish
	case 'U': // Australia
	case 'X': // PAL
	case 'Y': // PAL
		return PAL_Region;

	case '7':	// Beta
	case 'A':	// NTSC (Only 1080 JU?)
	case 'B':	// Brazil
	case 'C':	// China
	case 'E':	// USA
	case 'G':   // Gateway (NTSC)
	case 'J':	// Japan
	case ' ':	// PD
	case 0:		// PD
		return NTSC_Region;

	default:
		return Unknown_Region;
	}
}

void GetRomName(char* Name, BYTE* RomData) {
	int count;

	memcpy(Name, (void*)(RomData + 0x20), 20);
	Name[20] = '\0';

	// Byteswap the Internal Name
	for (count = 0; count < 20; count += 4) {
		Name[count] ^= Name[count + 3];
		Name[count + 3] ^= Name[count];
		Name[count] ^= Name[count + 3];
		Name[count + 1] ^= Name[count + 2];
		Name[count + 2] ^= Name[count + 1];
		Name[count + 1] ^= Name[count + 2];
	}

	// Remove excess spaces at the end
	for (count = 19; count >= 0; count--) {
		if (Name[count] != '\0' && Name[count] != ' ')
			break;
		else
			Name[count] = '\0';
	}
}

void GetRomFullName(char* FullName, BYTE* RomData, char* FullPath) {
	char Identifier[100], InternalName[25], ShortCountryCode[25];
	BYTE Country;
	std::string junk;

	RomID(Identifier, RomData);

	junk = ReadStr(RDS_NAME, Identifier, "Name", STR_EMPTY);

	// We have an existing entry in the RDS, use that as the full name
	if (!junk.empty()) {
		strncpy(FullName, junk.c_str(), junk.length());
		FullName[junk.length()] = '\0';
		return;
	}

	// Generate a Name using the Internal Name and the Country Code
	GetRomName(InternalName, RomData);
	GetRomCountry(&Country, RomData);
	CountryCodeToShortString(ShortCountryCode, Country, sizeof(ShortCountryCode));

	if (strlen(InternalName) != 0 && strlen(ShortCountryCode) != 0) {
		sprintf(FullName, "%s %s", InternalName, ShortCountryCode);
		return;
	}

	// Attempts to read or generate a name have failed, use the file name itself
	_splitpath(FullPath, NULL, NULL, FullName, NULL);
}

void GetRomCartID(char* ID, BYTE* RomData) {
	ID[0] = *(RomData + 0x3F);
	ID[1] = *(RomData + 0x3E);
	ID[2] = '\0';
}

void GetRomReleaseVersion(int* ReleaseVersion, BYTE* RomData) {
	*ReleaseVersion = (int)RomData[0x3C];
}

void GetRomSdkVersion(BYTE* SdkVersion, BYTE* RomData) {
	SdkVersion[0] = RomData[0x0C];
	SdkVersion[1] = RomData[0x0D];
}

void GetRomManufacturer(BYTE* Manufacturer, BYTE* RomData) {
	*Manufacturer = *(BYTE*)(RomData + 0x38);
}

void GetRomCountry(BYTE* Country, BYTE* RomData) {
	*Country = *(RomData + 0x3D);
}

void GetRomCRC1(DWORD* Crc1, BYTE* RomData) {
	*Crc1 = *(DWORD*)(RomData + 0x10);
}

void GetRomCRC2(DWORD* Crc2, BYTE* RomData) {
	*Crc2 = *(DWORD*)(RomData + 0x14);
}

enum CIC_CHIP GetRomCicChipID(BYTE* RomData) {
	return GetCicChipID(RomData);
}

void GetRomCicChipString(BYTE* RomData, char String[], int length) {
	BuildRomCicChipString(GetRomCicChipID(RomData), String, length, GetRomRegion(RomData));
}

void BuildRomCicChipString(enum CIC_CHIP ID, char String[], int length, int region) {
	switch (ID) {
		case CIC_NUS_6101:
			region == PAL_Region ? strcpy(&String[0], "CIC-NUS-7102") : strcpy(&String[0], "CIC-NUS-6101");
			break;
		case CIC_NUS_6102:
			region == PAL_Region ? strcpy(&String[0], "CIC-NUS-7101") : strcpy(&String[0], "CIC-NUS-6102");
			break;
		case CIC_NUS_6103:
			region == PAL_Region ? strcpy(&String[0], "CIC-NUS-7103") : strcpy(&String[0], "CIC-NUS-6103");
			break;
		case CIC_NUS_6105:
			region == PAL_Region ? strcpy(&String[0], "CIC-NUS-7105") : strcpy(&String[0], "CIC-NUS-6105");
			break;
		case CIC_NUS_6106:
			region == PAL_Region ? strcpy(&String[0], "CIC-NUS-7106") : strcpy(&String[0], "CIC-NUS-6106");
			break;
		case CIC_NUS_5167:
			strcpy(&String[0], "CIC-NUS-5167");
			break;
		case CIC_NUS_8303:
			strcpy(&String[0], "CIC-NUS-8303");
			break;
		case CIC_NUS_DDUS:
			strcpy(&String[0], "CIC-NUS-DDUS");
			break;
		case CIC_NUS_8401:
			strcpy(&String[0], "CIC-NUS-8401");
			break;
		default:
			snprintf(&String[0], length, "Unknown");
			break;
	}
}

void RomID(char* ID, BYTE* RomData) {
	DWORD CRC1, CRC2;
	BYTE Country;

	GetRomCRC1(&CRC1, RomData);
	GetRomCRC2(&CRC2, RomData);
	GetRomCountry(&Country, RomData);

	RomIDPreScanned(ID, &CRC1, &CRC2, &Country);
}

void RomIDPreScanned(char* ID, DWORD* CRC1, DWORD* CRC2, BYTE* Country) {
	sprintf(ID, "%08X-%08X-C:%02X", *CRC1, *CRC2, *Country);
}

void RomHASH(char* ID, BYTE* FullRom, size_t rom_size) {
	uint64_t hash = XXHash64::hash(FullRom, rom_size, 64);
	sprintf(ID, "%llu", hash);
}

void RomCRC32(char* ID, BYTE* FullRom, size_t rom_size) {

}