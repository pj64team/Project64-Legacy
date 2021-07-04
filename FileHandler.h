#ifdef __cplusplus
extern "C" {
#endif
	char* ReadStr(char* filename, char* id, char* setting, char* defaultvalue);
	int IsSet(char* filename, char* id, char* setting);
	int FetchIntValue(char* filename, char* id, char* setting, int def);
	int EntryExists(char* filename, char* id);

	void Write(char* filename, char* id, char* setting, char* value);
	void Delete(char* filename, char* id, char* setting);
	void DeleteAll(char* filename, char* id);

	// Added for support of the new cheat system
	char* ReadLine(char* filename, char* id, size_t line_number);
	void WriteLine(char* filename, char* id, char* line);
	void DeleteLine(char* filename, char* id, char* line);

	// Added for supporting renumbering of cheats but this may be useful in other places
	void ChangeKey(char* filename, char* id, char* oldkey, char* newkey);

#ifdef __cplusplus  
} // extern "C"  
#endif

#ifdef __cplusplus
#include <string>
#include <vector>
#include <list>

using namespace std;

#ifndef FHANDLER_H
#define FHANDLER_H

// This struct will hold a header and data
// In the case the header is a simple string (Not encapsulated by [ ] square brackets) then data shall be empty
struct Entry {
	string header;
	vector<string> data;
	size_t unsortable_lines;

	// GOOD = The header has been accepted and/or the data has been aded
	// FULL = A header has been passed to an already contructed entry, save the current entry and make a new one that contains the header
	// UNFORMATTED = This was simple text that was saved, unlike FULL it denotes that the entry was saved so the new entry will not have this text copied to it
	enum class read_states { GOOD, FULL, UNFORMATTED };

	Entry();

	// A normal header is encased in brackets [ ] at the start and may end with a comment
	bool IsHeader(const string& str);

	// A game header is very specific [XXXXXXXX:XXXXXXXX-C:XX] (Where X represents a Hex number) and may end with a comment
	bool IsGameHeader(const string& str);

	// The rules are as follows
	// The header must always contain something, if it is a Game Header or Header it may contain data (Returns true)
	// If the header is neither then it will only contain a string (regular text inside the file) and it cannot store data (Returns false) or be sorted
	// Only one header may be contained at a time, therefore attempting to store another header will return false
	read_states StoreData(const string& str);

	void ClearData();

	static bool CompareKeys(const string& key1, const string& key2);
	static bool IsCheatKey(const string& key, int& num);
};

// This class will handle the majority of the work when doing file related things
class FileStuff {
public:
	FileStuff();
	FileStuff(char* _filename);
	~FileStuff();

	void SetFileName(char* _filename);
	string GetFileName();

	// Reading from the buffer
	string GetValue(char* id, char* setting, char* def, bool fetch);
	string GetKeys(char* id);
	string GetLine(char* id, size_t line_number);

	bool DoesEntryExist(char* id);

	// Changes to the buffer
	void AddSettingValue(char* id, char* setting, char* value);
	void WriteLine(char* id, char* line);

	// A special case where the key must be changed
	void ChangeCurrentKey(char* id, char* oldkey, char* newkey);

	// Removals / Deletions from the buffer
	void RemoveSetting(char* id, char* setting);
	void RemoveEntry(char* id);
	void DeleteLine(char* id, char* line);

	// Basic file write
	void WriteToFile();

private:
	// A copy of the file in memory, some processing will be done when it is loaded
	vector<Entry> filebuffer;

	// The index of the current entry being examined, mostly only useful when reading multiple times from the same entry
	// So whenever the cheats or settings are being loaded
	size_t current_entry;

	// The name of the file, set by constructor or SetFileName
	string filename;
	string fullpath;

	// The time in seconds to wait before checking the file's date and time
	static const size_t check_time = 2;

	// Used for checking when the file shouild be re-read and loaded into memory
	struct _stat file_time;
	time_t last_checked;

	void LoadFile();
	bool HasChanged();
	Entry* FindEntry(string search);
};


#endif // End of #ifdef FHANDLER_H

#endif // End of __cplusplus