#ifdef __cplusplus
extern "C" {
#endif
	char *ReadStr(char *filename, char *id, char *setting, char *defaultvalue);
	int IsSet(char *filename, char *id, char *setting);
	int FetchIntValue(char *filename, char *id, char *setting, int def);

	void Write(char *filename, char *id, char *setting, char *value);
	void Delete(char *filename, char *id, char *setting);
	void DeleteAll(char *filename, char *id);
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

class FileStuff {
public:
	FileStuff();
	FileStuff(char *_filename);
	~FileStuff();

	void SetFileName(char *_filename);
	string GetFileName();

	// An entry will be sorted as such
	// [Identifying tag]
	// Good Name=The Rom Good Name
	// Internal Name=Internal Name
	// Anything else that follows is to be sorted alphabetically
	void SortEntry(string search);

	// Reading from the buffer
	string GetValue(char *id, char *setting, char *def, bool fetch);
	string GetKeys(char *id);

	// Changes to the buffer
	void AddSettingValue(char *id, char *setting, char *value);
	void RemoveSetting(char *id, char *setting);
	void RemoveEntry(char *id);

	// Basic file write
	void WriteToFile();

private:
	// The name of the file, set by constructor or SetFileName
	string filename;
	string fullpath;

	// The file is loaded into memory and kept here
	vector<string> buffer;

	struct _stat file_time;
	time_t last_checked;
	
	// Used/Set by FindEntry
	// last_offset is an optimization, remains even if vector iterators are invalidated
	size_t last_offset;
	vector<string>::iterator entry_start;
	vector<string>::iterator entry_end;

	// The time in seconds to wait before checking the file's date and time
	static const size_t check_time = 2;

	void LoadFile();
	bool HasChanged();
	void FindEntry(string search);
};
#endif
#endif}