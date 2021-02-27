#ifndef RTC_H
#define RTC_H

BYTE INTtoBCD (int val);
BYTE BCDtoBYTE (BYTE val);
int RTC_Command (BYTE *Command);
void ReadFromRTC ();
void WriteToRTC ();

#endif