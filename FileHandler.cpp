#include "FileHandler.h"
#include "Settings Common Defines.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <Windows.h>
#include <algorithm>
#include <sys/stat.h>

using namespace std;

////////////////////////
// GLOBAL VARIABLES!!!
////////////////////////
vector<FileStuff> myFiles;

// Used for atomic access to myFiles
HANDLE gLVMutex = CreateMutex(NULL, FALSE, NULL);

// WRITE_ENTRY is a normal write to file, this will remove any duplicate settings
// DELETE_SETTING is a removal of the setting and (optional) value pair
// DELETE_ENTRY will remove the entire entry from it's [id] and including all setting/value pairs underneath it
// WRITE_LINE will add the line to the entry, deletion of duplicates also done
// CHANGE_KEY will change an old key to a new key (This is a replacement, so if it's changing Key= then pass Key= not just Key)
// DELETE_LINE an alias to DELETE_SETTING, currently uses that code
enum class supported_writes { WRITE_ENTRY, DELETE_SETTING, DELETE_ENTRY, WRITE_LINE, CHANGE_KEY, DELETE_LINE };

// The function prototypes (Used to communicate with the class)
FileStuff* GetTheFile(char* filename);
string ReadHandler(char* filename, char* id, char* setting, char* def, BOOLEAN getting_value);
string LineReadHandler(char* filename, char* id, size_t line);
void WriteHandler(char* filename, char* id, char* setting, char* value, supported_writes write_type);



////////////////////////////////
// CLASS IMPLEMENTATION (PUBLIC)
////////////////////////////////
FileStuff::FileStuff() {
	filebuffer.clear();
	filename = "";
	fullpath = "";
	current_entry = 0;
}

FileStuff::FileStuff(char* _filename) {
	filebuffer.clear();
	current_entry = 0;
	SetFileName(_filename);
}

FileStuff::~FileStuff() {
	filebuffer.clear();
	filebuffer.shrink_to_fit();
}

void FileStuff::SetFileName(char* _filename) {
	char path[MAX_PATH];
	string::size_type pos;

	filename = _filename;

	// Create the full path to be used to open the file
	GetModuleFileName(NULL, path, MAX_PATH);
	pos = string(path).find_last_of("\\/");
	fullpath = string(path).substr(0, pos) + "\\Config\\" + _filename;
}

string FileStuff::GetFileName() {
	return filename;
}

string FileStuff::GetValue(char* id, char* setting, char* def, bool fetch) {
	string value;
	Entry* entry;

	// No setting to search for, return the (def)ault string if one was provided otherwise return the defined empty string STR_EMPTY
	if (setting == NULL)
		return (def == NULL) ? STR_EMPTY : string(def);

	// Find the start of the section we are interested in and the end
	entry = FindEntry("[" + (string)id + "]");

	if (entry != NULL && !entry->data.empty()) {
		vector<string>::iterator found;
		size_t find, str_end, str_len;

		str_len = strlen(setting);

		// Search for setting=
		if (fetch) {
			str_len++;	// Searching for setting= so increment the length by 1
			found = find_if(entry->data.begin(), entry->data.end(), [setting, str_len](string p) { return p.compare(0, str_len, (string)setting + "=") == 0; });

			if (found != entry->data.end()) {
				// Make sure that comments are ignored
				find = (*found).find("//");
				str_end = (find == string::npos) ? (*found).length() - str_len : find - str_len;

				value = (*found).substr(str_len, str_end);
			}
		}

		// No = but this may be an entry alone (For example ExpansionPak // Use expansion)
		else {
			found = find_if(entry->data.begin(), entry->data.end(), [setting, str_len](string p) { return p.compare(0, str_len, setting) == 0; });

			if (found != entry->data.end())
				value = string(setting);
		}
	}

	if (value.empty())
		value = (def == NULL) ? STR_EMPTY : string(def);
	else {
		// Trim whitespace
		for (int i = value.length() - 1; i > 0; i--) {
			if (isspace(value[i]))
				value[i] = '\0';
			else
				break;
		}
	}

	return value;
}

string FileStuff::GetLine(char* id, size_t line_number) {
	Entry* entry;

	if (line_number < 1)
		return STR_EMPTY;
	else
		line_number--;

	entry = FindEntry("[" + (string)id + "]");

	if (entry != NULL && !entry->data.empty() && line_number < entry->data.size())
		return entry->data[line_number];

	return STR_EMPTY;
}

bool FileStuff::DoesEntryExist(char* id) {
	return (FindEntry("[" + (string)id + "]") == NULL ? FALSE : TRUE);
}

string FileStuff::GetKeys(char* id) {
	string result;
	size_t loc = string::npos;
	Entry* entry;
	const string find_str[] = { "=", "//" };

	entry = FindEntry("[" + string(id) + "]");

	if (entry == NULL || entry->data.empty())
		return result;

	for (string line : entry->data) {

		// Ignore comments and empty lines
		if (isspace(line[0]) || line.compare(0, 2, "//") == 0)
			continue;

		// Strip empty space at the end
		for (int i = line.length() - 1; i > 0; i--) {
			if (isspace(line[i]))
				line[i] = '\0';
			else
				break;
		}

		for (string s : find_str) {
			loc = line.find(s);
			if (loc != string::npos) {
				result += line.substr(0, loc) + ",";
				break;
			}
		}

		// Previous loop did not find a = or // so use the entire line
		if (loc == string::npos)
			result += line + ",";
	}

	return result;
}

void FileStuff::AddSettingValue(char* id, char* setting, char* value) {
	Entry* entry = FindEntry("[" + string(id) + "]");

	// Add a new entry if none was found
	if (entry == NULL) {
		filebuffer.push_back(Entry());
		entry = &filebuffer.back();
		entry->StoreData("[" + string(id) + "]" + LINEFEED);
	}

	// Insert the setting/value pair into the file buffer
	if (value != NULL && strlen(value) != 0)
		entry->StoreData((string)setting + "=" + value + LINEFEED);
	else
		entry->StoreData((string)setting + LINEFEED);

	if (entry->unsortable_lines == 0 && entry->data.size() != 0) {
		std::sort(entry->data.begin(), entry->data.end(), entry->CompareKeys);
	}
}

void FileStuff::WriteLine(char* id, char* line) {
	Entry* entry = FindEntry("[" + string(id) + "]");

	// No entry found, need to add a blank Entry to the back and set the header
	if (entry == NULL) {
		filebuffer.push_back(Entry());
		entry = &filebuffer.back();
		entry->StoreData("[" + (string)id + "]" + LINEFEED);
	}

	entry->StoreData(line);

	if (entry->unsortable_lines == 0 && entry->data.size() != 0) {
		std::sort(entry->data.begin(), entry->data.end(), entry->CompareKeys);
	}
}

void FileStuff::ChangeCurrentKey(char* id, char* oldkey, char* newkey) {
	Entry* entry;
	string tmp;

	entry = FindEntry("[" + string(id) + "]");

	// Nothing to change
	if (entry == NULL || entry->data.empty())
		return;

	for (size_t i = 0; i < entry->data.size(); i++) {
		if (entry->data[i].compare(0, strlen(oldkey), oldkey) == 0) {
			tmp = newkey + entry->data[i].substr(strlen(oldkey));
			entry->data[i] = tmp;
		}
	}
}

void FileStuff::RemoveSetting(char* id, char* setting) {
	vector<string>::iterator del_start;
	string str_set = (string)setting;
	Entry* entry;

	entry = FindEntry("[" + string(id) + "]");

	// Nothing to remove, skip this section
	if (entry == NULL || entry->data.empty())
		return;

	del_start = remove_if(entry->data.begin(), entry->data.end(), [str_set](string str) { return str.compare(0, str_set.length(), str_set) == 0; });
	if (del_start != entry->data.end()) {
		entry->data.erase(del_start, entry->data.end());
	}

	// Remove entry if it is empty
	if (entry->data.empty()) {
		RemoveEntry(id);
	}
}

void FileStuff::RemoveEntry(char* id) {
	Entry* entry = FindEntry("[" + string(id) + "]");

	if (entry != NULL)
		filebuffer.erase(filebuffer.begin() + current_entry);
}

void FileStuff::WriteToFile() {
	ofstream myFile;

	// Write the modified file buffer to memory
	myFile.open(fullpath, ios::binary);

	// Write to the file using the filebuffer
	for (Entry f : filebuffer) {
		myFile << f.header;

		// Write out the data (If any)
		for (string d : f.data)
			myFile << d;
	}

	// Clean-up
	myFile.close();

	// The buffer and file are the same right now, update the file time
	_stat(fullpath.c_str(), &file_time);
}

void FileStuff::DeleteLine(char* id, char* line) {
	RemoveSetting(id, line);
}



/////////////////////////////////
// CLASS IMPLEMENTATION (PRIVATE)
/////////////////////////////////
void FileStuff::LoadFile() {
	ifstream myFile;
	string junk;
	Entry entry = Entry();
	Entry::read_states state;

	// Check if the file is loaded
	if (!filebuffer.empty() && !HasChanged())
		return;

	// Clear the cache
	filebuffer.clear();

	// Open the file for reading in binary mode (The file will be copied byte for byte)
	myFile.open(fullpath, ios_base::binary);
	myFile.seekg(0, ios::beg);

	junk.reserve(200);
	// Line by line parsing, if this is too slow move to reading chunks at a time into a buffer
	while (getline(myFile, junk)) {
		state = entry.StoreData(junk + "\n");

		switch (state) {
		case Entry::read_states::FULL: {
			size_t start;
			string tail;

			tail.reserve(100);

			// Starting from the last entry find where data does not contain comments or new lines
			for (start = entry.data.size() - 1; start > 0; start--) {
				if (entry.data[start].compare(0, 2, "//") == 0 || entry.data[start].compare(0, 2, "\r\n") == 0 || entry.data[start][0] == '\n') {
					continue;
				}
				break;
			}

			if (start != entry.data.size() - 1) {
				// A copy of the data that will be removed (Comments and new lines)
				for (size_t i = start + 1; i < entry.data.size(); i++) {
					tail += entry.data[i];
					entry.unsortable_lines--;
				}

				// Erase the lines that have just been saved
				entry.data.erase(entry.data.begin() + start + 1, entry.data.end());
			}

			// Save the entry, at this point comments and new lines should have been removed
			filebuffer.push_back(entry);

			// Now save the comments and new lines that were at the end of the entry
			if (!tail.empty()) {
				entry.ClearData();
				entry.StoreData(tail);
				filebuffer.push_back(entry);
			}

			// Finished saving the previous entry, start the new one
			entry.ClearData();
			entry.StoreData(junk + "\n");
		}
			break;
		case Entry::read_states::UNFORMATTED:
			filebuffer.push_back(entry);
			entry.ClearData();
			break;
		default:
			break;
		}
	}

	if (!entry.header.empty()) {
		filebuffer.push_back(entry);
	}

	// Before closing the file get the file times
	_stat(fullpath.c_str(), &file_time);
	last_checked = time(NULL);

	// Clean up by closing the file handle
	if (myFile.is_open())
		myFile.close();
	
	// Sort the file before updating it
	for (Entry &e : filebuffer) {
		if (e.unsortable_lines == 0 && e.data.size() != 0) {
			std::sort(e.data.begin(), e.data.end(), e.CompareKeys);
		}
	}

	WriteToFile();
}

bool FileStuff::HasChanged() {
	struct _stat current_filetime;
	time_t current_time;

	// check_time will decide how often to check if the file has changed
	current_time = time(NULL);
	if (current_time - last_checked < check_time)
		return false;

	// Compare the file time and the last stored time for the file
	_stat(fullpath.c_str(), &current_filetime);
	if (current_filetime.st_mtime != file_time.st_mtime)
		return true;

	return false;
}

Entry* FileStuff::FindEntry(string search) {
	LoadFile();

	// Fetch the last entry that was loaded (if available)
	if (current_entry < filebuffer.size()) {
		// The stored entry is the one being searched for, go no further
		if (filebuffer[current_entry].header.compare(0, search.length(), search) == 0)
			return &filebuffer[current_entry];
	}

	// Scan the buffer for the entry
	for (size_t i = 0; i < filebuffer.size(); i++) {
		if (filebuffer[i].header.compare(0, search.length(), search) == 0) {
			current_entry = i;
			return &filebuffer[i];
		}
	}

	// Failure to find anything
	return NULL;
}



//////////////////////////
// INTERFACE WITH CLASS
//////////////////////////
FileStuff* GetTheFile(char* filename) {
	vector<FileStuff>::iterator it;
	FileStuff file;

	// Check if the file has previously been loaded and exists in the vector
	for (it = myFiles.begin(); it != myFiles.end(); ++it) {
		if ((*it).GetFileName() == (string)filename)
			return &(*it);
	}

	// File has not been loaded
	file = *new FileStuff(filename);
	myFiles.push_back(file);
	return &myFiles.back();
}

string ReadHandler(char* filename, char* id, char* setting, char* def, BOOLEAN getting_value) {
	string value;
	DWORD wait_result;
	FileStuff* file;

	// Simple mutex to prevent accessing memory across multiple threads
	wait_result = WaitForSingleObject(gLVMutex, INFINITE);

	// Some kind of error!?
	if (wait_result != WAIT_OBJECT_0)
		return def;

	// Fetch the handle to the file
	file = GetTheFile(filename);

	// Null setting denotes we're fetching key names
	// Used in Rom Status to load the colors ahead of time (Before the name is known)
	if (setting != NULL)
		value = file->GetValue(id, setting, def, getting_value == TRUE ? true : false);
	else
		value = file->GetKeys(id);

	if (!ReleaseMutex(gLVMutex))
		MessageBox(NULL, "Failed to release a mutex???", "Error", MB_OK);

	return value;
}

string LineReadHandler(char* filename, char* id, size_t line) {
	string ret;
	DWORD wait_result;
	FileStuff* file;

	// Simple mutex to prevent accessing memory across multiple threads
	wait_result = WaitForSingleObject(gLVMutex, INFINITE);

	// Some kind of error!?
	if (wait_result != WAIT_OBJECT_0)
		return STR_EMPTY;

	// Fetch the handle to the file
	file = GetTheFile(filename);

	ret = file->GetLine(id, line);

	if (!ReleaseMutex(gLVMutex))
		MessageBox(NULL, "Failed to release a mutex???", "Error", MB_OK);

	return ret;
}

BOOL CheckEntryExists(char* filename, char* id) {
	DWORD wait_result;
	FileStuff* file;
	bool result;

	// Simple mutex to prevent accessing memory across multiple threads
	wait_result = WaitForSingleObject(gLVMutex, INFINITE);

	// Some kind of error!?
	if (wait_result != WAIT_OBJECT_0)
		return FALSE;

	// Fetch the handle to the file
	file = GetTheFile(filename);

	result = file->DoesEntryExist(id);

	if (!ReleaseMutex(gLVMutex))
		MessageBox(NULL, "Failed to release a mutex???", "Error", MB_OK);

	return result ? TRUE : FALSE;
}

void WriteHandler(char* filename, char* id, char* setting, char* value, supported_writes write_type) {
	DWORD wait_result;
	FileStuff* file;

	// Simple mutex to prevent accessing memory across multiple threads
	wait_result = WaitForSingleObject(gLVMutex, INFINITE);

	// Some kind of error!?
	if (wait_result != WAIT_OBJECT_0)
		return;

	file = GetTheFile(filename);

	// TO DO!
	// Write a section here that verifies there is at least 1 setting left
	// If there would be 0 settings left that would be equivalent to an entry deletion

	// Make any modifications to the file buffer before writing to file
	switch (write_type) {
	case supported_writes::WRITE_ENTRY:
		file->AddSettingValue(id, setting, value);
		break;
	case supported_writes::DELETE_ENTRY:
		file->RemoveEntry(id);
		break;
	case supported_writes::DELETE_SETTING:
		file->RemoveSetting(id, setting);
		break;
	case supported_writes::WRITE_LINE:
		file->WriteLine(id, setting);
		break;
	case supported_writes::CHANGE_KEY:
		file->ChangeCurrentKey(id, setting, value);
		break;
	case supported_writes::DELETE_LINE:
		file->DeleteLine(id, setting);
		break;
	}

	// This is very hacky, until a delayed write is added in this will have to do
	if (write_type != supported_writes::CHANGE_KEY)
		file->WriteToFile();

	if (!ReleaseMutex(gLVMutex))
		MessageBox(NULL, "Failed to release a mutex???", "Error", MB_OK);
}


//////////////////////////
// THESE INTERFACE WITH C
//////////////////////////
char* ReadStr(char* filename, char* id, char* setting, char* defaultvalue) {
	string result;
	char* ret;

	result = ReadHandler(filename, id, setting, defaultvalue, TRUE);

	// Does this need error-checking? A non-null default should not be passed.
	ret = (char*)malloc(sizeof(char) * (result.length() + 1));
	if (ret != NULL)
		strcpy(ret, result.c_str());

	return ret;
}

int IsSet(char* filename, char* id, char* setting) {
	string result = ReadHandler(filename, id, setting, STR_FALSE, FALSE);
	return (result.compare(STR_FALSE) == 0) ? FALSE : TRUE;
}

int FetchIntValue(char* filename, char* id, char* setting, int def) {
	string result = ReadHandler(filename, id, setting, STR_FALSE, TRUE);
	return (result.compare(STR_FALSE) == 0) ? def : atoi(result.c_str());
}

void Write(char* filename, char* id, char* setting, char* value) {
	WriteHandler(filename, id, setting, value, supported_writes::WRITE_ENTRY);
}

void Delete(char* filename, char* id, char* setting) {
	WriteHandler(filename, id, setting, NULL, supported_writes::DELETE_SETTING);
}

void DeleteAll(char* filename, char* id) {
	WriteHandler(filename, id, NULL, NULL, supported_writes::DELETE_ENTRY);
}

char* ReadLine(char* filename, char* id, size_t line_number) {
	string result;
	char* ret;

	result = LineReadHandler(filename, id, line_number);

	ret = (char*)malloc(sizeof(char) * (result.length() + 1));
	if (ret != NULL)
		strcpy(ret, result.c_str());

	return ret;
}

void WriteLine(char* filename, char* id, char* line) {
	WriteHandler(filename, id, line, NULL, supported_writes::WRITE_LINE);
}

void DeleteLine(char* filename, char* id, char* line) {
	WriteHandler(filename, id, line, NULL, supported_writes::DELETE_LINE);
}

void ChangeKey(char* filename, char* id, char* oldkey, char* newkey) {
	WriteHandler(filename, id, oldkey, newkey, supported_writes::CHANGE_KEY);
}

int EntryExists(char* filename, char *id) {
	return CheckEntryExists(filename, id);
}



////////////////////////////////////////////
// Implementation of the struct starts here
////////////////////////////////////////////

Entry::Entry() {
	ClearData();
}


bool Entry::IsHeader(const string& str) {
	// This regular expression was causing a massive slowdown on the debug build
	// return regex_match(str, regex("^\\[([A-Za-z0-9]|\\s)+\\]\\s*(?://?.*?\\s*)?$"));

	// Unfortunately this does not have a fixed format like IsGameHeader but it should at least be 3 characters long [X]
	// Where X is anything except a comment // so in theory even a / could be accepted here
	if (str.length() > 3 && str[0] == '[') {
		size_t loc1, loc2, loc3;

		// Did not find a matching closing square bracket
		loc1 = str.find(']', 1);
		if (loc1 == string::npos)
			return false;

		// There is a comment before the closing square bracket
		loc2 = str.find("//", 1);
		if (loc2 < loc1)
			return false;

		// There is an equal sign before the comment, this is a value and not a header
		loc3 = str.find("=");
		if (loc3 < loc2)
			return false;

		// Otherwise it passes the rules
		return true;
	}
	return false;
}


bool Entry::IsGameHeader(const string& str) {
	// This regular expression was causing a massive slowdown on the debug build
	// return regex_match(str, regex("^\\[([A-Fa-f0-9]){8}-([A-Fa-f0-9]){8}-C:([A-Fa-f0-9]){2}\\]\\s*(?://?.*?\\s*)?$"));

	// The format to check against, X is a hex value so much be checked for 0 through 9 and A through F, including a through f
	const std::string format = "[XXXXXXXX-XXXXXXXX-C:XX]";

	if (str.length() < format.length())
		return false;

	for (size_t i = 0; i < format.length(); i++) {
		if (str[i] == format[i])
			continue;

		if (format[i] == 'X' && isxdigit(str[i]))
			continue;

		return false;
	}
	return true;
}

Entry::read_states Entry::StoreData(const string& str) {
	vector<string>::iterator found;
	size_t find;
	string key;

	// Store the header if not empty, otherwise return false (Basically, the entry is full so failed to store)
	if (IsGameHeader(str) || IsHeader(str)) {
		if (!header.empty())
			return read_states::FULL;
		else {
			header = str;
			return read_states::GOOD;
		}
	}

	// Not a header, simply text
	if (header.empty()) {
		header = str;
		return read_states::UNFORMATTED;
	}

	// Do not sort if the line passed (str) is a comment ex: // This is a comment
	if (str.compare(0, 2, "//") == 0 || isspace((unsigned char)str[0])) {
		unsortable_lines++;
		data.push_back(str);
		return read_states::GOOD;
	}

	// The key for matching will include the = or be the entire line
	find = str.find("=");
	key = (find == string::npos) ? str : str.substr(0, find + 1);

	// Check to see if the entry exists (It must start with key)
	found = find_if(data.begin(), data.end(), [&key](const string& p) { 		
		return p.compare(0, key.length(), key) == 0;
	});

	// A match was found, update the entry
	if (found != data.end()) {
		*found = str;
		return read_states::GOOD;
	}

	// Add to the back of the vector
	data.push_back(str);	
	return read_states::GOOD;
}

void Entry::ClearData() {
	header.clear();
	data.clear();
	unsortable_lines = 0;
}

bool Entry::CompareKeys(const string& str1, const string& str2) {
	int num1, num2;
	size_t key1_length, key2_length;
	const string name_key = "Name=";
	
	if (IsCheatKey(str1, num1) && IsCheatKey(str2, num2)) {
		if (num1 != num2)
			return num1 < num2;
	}

	// Give priority to Name=
	if (str1.compare(0, name_key.length(), name_key) == 0)
		return true;

	if (str2.compare(0, name_key.length(), name_key) == 0)
		return false;

	key1_length = str1.find("=");
	key1_length = (key1_length == string::npos) ? str1.length() : key1_length++;

	key2_length = str2.find("=");
	key2_length = (key2_length == string::npos) ? str2.length() : key2_length++;

	// Custom scanning to avoid making substrings
	for (size_t scan = 0; scan < min(key1_length, key2_length); scan++) {
		if (str1[scan] < str2[scan])
			return true;

		if (str1[scan] > str2[scan])
			return false;
	}

	// Strings compared the same up to the length checked, now see if a string is longer (include equality here)
	if (key1_length <= key2_length)
		return true;
	else
		return false;
}

// A cheat is defined as an entry that has the following format CheatX(_Y)=
// Where X is a number (as of June 2021, no more than 500) and _Y is an optional parameter, either _O or _N
// In order to properly sort these entries the number must be extracted and compared as an integer and not as text
bool Entry::IsCheatKey(const string& key, int& num) {
	const string cheat = "Cheat";
	const char* start;
	size_t num_start;

	num = 0;

	// Must start with Cheat (using a const for this)
	if (key.compare(0, cheat.length(), cheat) != 0)
		return false;

	// The section between Cheat and _ or = must contain only numbers
	for (num_start = cheat.length(); num_start < key.length(); num_start++) {

		// Additional stop conditions, the cheat may contain _O= or _N= or may simply end with a =
		if (key[num_start] == '_' || key[num_start] == '=')
			break;

		if (!isdigit(key[num_start]))
			return false;
	}

	// The start of the string to convert
	start = key.c_str() + cheat.length();

	num = atoi(start);
	return true;
}
