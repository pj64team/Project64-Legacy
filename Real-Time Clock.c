#include <time.h>
#include <Windows.h>
#include "Main.h"
#include "Cpu.h"
#include "Real-Time Clock.h"

// Supports the player setting a custom time.
// This will be negative if the time is lower than current, positive if higher than current.
__time64_t seconds_offset = 0;
int first_load = TRUE;

BYTE INTtoBCD(int val) {
	val %= 100;		// Conversion to 1 byte (8 bits) packed BCD supports 0 to 99 only.
	return (BYTE)(((val / 10) << 4) | (val % 10));
}

BYTE BCDtoBYTE(BYTE val) {
	val %= 0x99;	// The maximum a BCD can store in 1 byte (8 bits).
	return (BYTE)(val - ((val & 0xf0) >> 3) * 3);
}

int RTC_Command(BYTE* Command) {
	switch (Command[2]) {
	case 6:	// Real-Time Clock status
		Command[3] = 0x00;
		Command[4] = 0x10;
		Command[5] = 0x00;	// No error, 0x80 would be busy and 0x01 and 0x02 would be failure
		break;

	case 7:	// Real-Time Clock read
		switch (Command[3]) {	// Block number
		case 0x00:
			Command[4] = 0x00;	// Set blocks 1 and 2 to read-write?
			Command[5] = 0x02;	// Timer is running
			Command[12] = 0x00;	// No error
			break;
		case 0x02:
		{
			__time64_t rawtime;
			struct tm* timeinfo;

			if (first_load == TRUE) {
				ReadFromRTC();
				first_load = FALSE;
			}

			rawtime = time(NULL) - seconds_offset;
			timeinfo = _localtime64(&rawtime);

			Command[4] = INTtoBCD(timeinfo->tm_sec);
			Command[5] = INTtoBCD(timeinfo->tm_min);
			Command[6] = INTtoBCD(timeinfo->tm_hour) | 0x80;
			Command[7] = INTtoBCD(timeinfo->tm_mday);
			Command[8] = INTtoBCD(timeinfo->tm_wday);
			Command[9] = INTtoBCD(timeinfo->tm_mon) + 1;
			Command[10] = INTtoBCD(timeinfo->tm_year % 100);
			Command[11] = INTtoBCD(timeinfo->tm_year / 100);
			Command[12] = 0x00;
		}
		break;
		default:
			if (ShowPifRamErrors) { DisplayError("Unknown RTC Read Block %X", Command[3]); }
		}
		break;

	case 8:	// Real-Time Clock write
		switch (Command[3]) {
		case 0x00:	// This area should lock the chip and release it as needed when block 0x02 is written to.
					// As the chip is not real it is not necessary to emulate these commands.
					// Pif should remain locked as busy while this is happening and only be unset by block 0x02.
			if (Command[12] != 0x80 && ShowPifRamErrors) { DisplayError("RTC Write Status is not busy? %X", Command[12]); }
			break;
		case 0x02:
		{
			__time64_t rawtime = time(NULL);
			struct tm* timeinfo;

			timeinfo = localtime(&rawtime);
			timeinfo->tm_sec = BCDtoBYTE(Command[4]);
			timeinfo->tm_min = BCDtoBYTE(Command[5]);
			timeinfo->tm_hour = BCDtoBYTE(Command[6]) + 1;
			timeinfo->tm_mday = BCDtoBYTE(Command[7]);
			timeinfo->tm_wday = BCDtoBYTE(Command[8]);
			timeinfo->tm_mon = BCDtoBYTE(Command[9]) - 1;
			timeinfo->tm_year = BCDtoBYTE(Command[10]) + BCDtoBYTE(Command[11]) * 100;

			seconds_offset = time(NULL) - _mktime64(timeinfo);
			WriteToRTC();
			Command[12] = 0x00;
		}
		break;
		default:
			if (ShowPifRamErrors) { DisplayError("Unknown RTC Write Block %X", Command[3]); }
		}
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

void ReadFromRTC() {
	FILE* fp;
	char File[255], Directory[255], String[100];

	Settings_GetDirectory(AutoSaveDir, Directory, sizeof(Directory));
	sprintf(File, "%s%s.rtc", Directory, RomFullName);

	fp = fopen(File, "r");

	if (fp != NULL) {
		fscanf(fp, "%s", String);
		seconds_offset = _atoi64(String);
		fclose(fp);
	}
	else {
		seconds_offset = 0;
	}
}

void WriteToRTC() {
	FILE* fp;
	char File[255], Directory[255], String[100];

	Settings_GetDirectory(AutoSaveDir, Directory, sizeof(Directory));
	sprintf(File, "%s%s.rtc", Directory, RomFullName);

	fp = fopen(File, "w");

	// No error checking, write to it if possible otherwise don't bother.
	if (fp != NULL) {
		_i64toa(seconds_offset, String, 10);
		fprintf(fp, String);
		fclose(fp);
	}
}