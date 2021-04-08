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
	int len;
	size_t find;
	vector<string>::iterator found;
	Entry* entry;

	len = setting == NULL ? 0 : strlen(setting);

	// Find the start of the section we are interested in and the end
	entry = FindEntry("[" + (string)id + "]");

	if (entry != NULL && !entry->data.empty()) {
		if (fetch) {			
			// Search for setting=
			found = find_if(entry->data.begin(), entry->data.end(), [setting, len](string p) { return p.compare(0, len + 1, (string)setting + "=") == 0; });
			if (found != entry->data.end()) {

				// Make sure that comments are ignored
				find = (*found).find("//");
				if (find != string::npos)
					value = (*found).substr(len + 1, find - len + 1);
				else
					value = (*found).substr(len + 1, find);

				// Trim whitespace off the end
				for (int i = value.length() - 1; i >= 0; i--) {
					if (isspace(value[i]))
						value[i] = '\0';
					else
						break;
				}
			}
		}

		// No = but this may be an entry alone (For example ExpansionPak // Use expansion)
		else {
			found = find_if(entry->data.begin(), entry->data.end(), [setting, len](string p) { return p.compare(0, len, setting) == 0; });
			if (found != entry->data.end()) {
				if (setting == NULL)
					value = STR_EMPTY;
				else
					value = string(setting);
			}
		}
	}

	if (value.empty())
		value = (def == NULL) ? STR_EMPTY : string(def);

	return value;
}

string FileStuff::GetLine(char* id, size_t line_number) {
	Entry* entry;

	entry = FindEntry("[" + (string)id + "]");

	if (entry != NULL && !entry->data.empty() && (size_t)line_number - 1 < entry->data.size())
		return entry->data[line_number - 1];

	return STR_EMPTY;
}

string FileStuff::GetKeys(char* id) {
	string result;
	size_t loc = string::npos;
	Entry* entry;
	const char remove_chars[] = { '\r', '\n' };
	const string find_str[] = { "=", "//" };

	entry = FindEntry("[" + string(id) + "]");

	if (entry == NULL || entry->data.empty())
		return result;

	for (string line : entry->data) {

		// A key will not have a new line or carriage return, remove these from the line
		for (const char re : remove_chars)
			line.erase(std::remove(line.begin(), line.end(), re), line.end());

		// Nothing to examine, either this is an empty line or a comment noted by // at the start
		if (line.empty()|| line.compare(0, 2, "//") == 0)
			continue;

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
	Entry* entry;
	string tmp;

	entry = FindEntry("[" + string(id) + "]");

	// Add a new entry if none was found
	if (entry == NULL) {
		Entry temp;
		temp.ClearData();
		filebuffer.push_back(temp);
		entry = &filebuffer.back();

		entry->StoreData("[" + string(id) + "]" + LINEFEED);
	}
	// Existing entry, delete the setting of the same name if it exists
	else {
		char* delstr = (char*)malloc(sizeof(char) * (strlen(setting) + 2));

		if (delstr != NULL) {
			strcpy(delstr, setting);

			if (value != NULL && strlen(value) != 0)
				sprintf(delstr, "%s=", setting);
			else
				sprintf(delstr, "%s", setting);

			RemoveSetting(id, delstr);

			free(delstr);
		}
	}

	// Insert the new setting/value pair into the file buffer
	if (value != NULL && strlen(value) != 0)
		entry->StoreData((string)setting + "=" + value + LINEFEED);
	else
		entry->StoreData((string)setting + LINEFEED);
}

void FileStuff::WriteLine(char* id, char* line) {
	Entry* entry;

	entry = FindEntry("[" + string(id) + "]");

	// No entry found, need to add a blank Entry to the back and set the header
	if (entry == NULL) {
		Entry tmp;
		tmp.ClearData();
		filebuffer.push_back(tmp);
		entry = &filebuffer.back();

		entry->StoreData("[" + (string)id + "]" + LINEFEED);
	}

	entry->StoreData(line);
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
}

void FileStuff::RemoveEntry(char* id) {
	Entry* entry;

	entry = FindEntry("[" + string(id) + "]");
	if (entry != NULL)
		filebuffer.erase(filebuffer.begin() + current_entry);
}

void FileStuff::WriteToFile() {
	ofstream myFile;

	// Write the modified file buffer to memory
	myFile.open(fullpath, ios::binary);

	// Write to file the entire filebuffer
	for (size_t i = 0; i < filebuffer.size(); i++) {
		myFile << filebuffer[i].header;

		for (size_t o = 0; o < filebuffer[i].data.size(); o++) {
			myFile << filebuffer[i].data[o];
		}

		if (!filebuffer[i].data.empty())
			myFile << LINEFEED;
	}

	// Clean-up
	myFile.close();

	// The buffer and file are the same right now, update the file time
	_stat(fullpath.c_str(), &file_time);
}

void FileStuff::DeleteLine(char* id, char* line) {
	RemoveSetting(id, line);
}

void FileStuff::SortEntry(char* id) {
	Entry* entry;

	entry = FindEntry("[" + string(id) + "]");
	if (entry != NULL && !entry->data.empty())
		entry->SortData();
}


/////////////////////////////////
// CLASS IMPLEMENTATION (PRIVATE)
/////////////////////////////////
void FileStuff::LoadFile() {
	ifstream myFile;
	string junk;
	Entry entry;
	Entry::read_states state;

	// Check if the file is loaded
	if (!filebuffer.empty() && !HasChanged())
		return;

	// Clear the cache
	filebuffer.clear();

	// Open the file for reading in binary mode (The file will be copied byte for byte)
	myFile.open(fullpath, ios_base::binary);
	myFile.seekg(0, ios::beg);

	// Make sure entry is empty, it will be filled and added to the filebuffer
	entry.ClearData();

	junk.reserve(100);
	// Line by line parsing, if this is too slow move to reading chunks at a time into a buffer
	while (getline(myFile, junk)) {
		state = entry.StoreData(junk + "\n");

		switch (state) {
		case Entry::read_states::FULL:
			entry.TrimEmptyLines();
			filebuffer.push_back(entry);
			entry.ClearData();
			entry.StoreData(junk + "\n");
			break;
		case Entry::read_states::UNFORMATTED:
			entry.TrimEmptyLines();
			filebuffer.push_back(entry);
			entry.ClearData();
			break;
		default:
			break;
		}
	}

	if (!entry.header.empty()) {
		entry.TrimEmptyLines();
		filebuffer.push_back(entry);
	}

	// Before closing the file get the file times
	_stat(fullpath.c_str(), &file_time);
	last_checked = time(NULL);

	// Clean up by closing the file handle
	if (myFile.is_open())
		myFile.close();
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

void WriteHandler(char* filename, char* id, char* setting, char* value, supported_writes write_type) {
	DWORD wait_result;
	FileStuff* file;

	// Simple mutex to prevent accessing memory across multiple threads
	wait_result = WaitForSingleObject(gLVMutex, INFINITE);

	// Some kind of error!?
	if (wait_result != WAIT_OBJECT_0)
		return;

	file = GetTheFile(filename);

	// To do
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

	// If the entry was not deleted then sort the section
	if (write_type != supported_writes::DELETE_ENTRY)
		file->SortEntry(id);

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

////////////////////////////////////////////
// Implementation of the struct starts here
////////////////////////////////////////////

Entry::Entry() {
	ClearData();
}

void Entry::SortData() {

	vector<string>::iterator found, it;
	const static string s[] = { "Good Name=", "Internal Name=", "Name=" };

	// Sorting can only happen if the data does not have comments (// Example) or empty lines (Mainly newlines \r\n or \r or \n)
	if (!can_be_sorted || data.empty())
		return;

	it = data.begin();

	// Only prioritize Good Name, Internal Name, and Name if this is a game entry
	// The format is [xxxxxxxx-xxxxxxxx-C:xx] where x are hex characters
	if (IsGameHeader(header)) {
		for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); i++) {
			found = find_if(data.begin(), data.end(), [i](string p) { return p.compare(0, s[i].length(), s[i]) == 0; });
			if (found != data.end()) {
				std::swap(*it, *found);
				++it;
			}
		}
	}

	std::sort(it, data.end(),
		[](const string& a, const string& b) {

		// CheatX(_Y)= is a special case, otherwise use the normal method
		// Where X is a number and _Y is an optional parameter, where Y is either N, O, or similar but = is always there, as is _ after the number
		if (a.find("Cheat", 0) == 0 && b.find("Cheat", 0) == 0) {
			int loc_a, loc_b;
			string num_a, num_b;

			loc_a = a.find_first_of("=_");
			loc_b = b.find_first_of("=_");

			if (loc_a != string::npos && loc_b != string::npos) {
				num_a = a.substr(5, loc_a - 5);
				num_b = b.substr(5, loc_b - 5);

				return (atoi(num_a.c_str()) < atoi(num_b.c_str()));
			}
		}

		// The usual case
		return (a.compare(b) > 0) ? false : true;
	});
}


bool Entry::IsHeader(string str) {
	// This regular expression was causing a massive slowdown on the debug build
	// return regex_match(str, regex("^\\[([A-Za-z0-9]|\\s)+\\]\\s*(?://?.*?\\s*)?$"));

	// Unfortunately this does not have a fixed format like IsGameHeader but it should at least be 3 characters long [X]
	// Where X is anything except a comment // so in theory even a / could be accepted here
	if (str.length() > 3 && str[0] == '[') {
		size_t loc1, loc2, loc3;

		loc1 = str.find(']', 1);
		loc2 = str.find("//", 1);
		loc3 = str.find("=");

		// Did not find a matching closing square bracket
		if (loc1 == string::npos)
			return false;

		// There is a comment before the closing square bracket
		if (loc2 < loc1)
			return false;

		// There is an equal sign before the comment, this is a value and not a header
		if (loc3 < loc2)
			return false;

		// Otherwise it passes the rules
		return true;
	}
	return false;
}


bool Entry::IsGameHeader(string str) {
	// This regular expression was causing a massive slowdown on the debug build
	// return regex_match(str, regex("^\\[([A-Fa-f0-9]){8}-([A-Fa-f0-9]){8}-C:([A-Fa-f0-9]){2}\\]\\s*(?://?.*?\\s*)?$"));

	// The format to check against, X is a hex value so much be checked for 0 through 9 and A through F, including a through f
	char format[] = "[XXXXXXXX-XXXXXXXX-C:XX]";

	if (str.size() < strlen(format))
		return false;

	for (size_t i = 0; i < strlen(format); i++) {
		if (str[i] == format[i])
			continue;
		else {
			if (format[i] == 'X') {
				if ((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'A' && str[i] <= 'F') || (str[i] >= 'a' && str[i] <= 'f'))
					continue;
			}

			return false;
		}
	}
	return true;
}

Entry::read_states Entry::StoreData(string str) {

	// Store the header if not empty, otherwise return false (Basically, the entry is full so failed to store)
	if (IsGameHeader(str) || IsHeader(str)) {
		if (!header.empty())
			return read_states::FULL;
		else {
			ClearData();
			header = str;
			return read_states::GOOD;
		}
	}

	// Not a header, simply text
	if (header.empty()) {
		ClearData();
		header = str;
		can_be_sorted = false;
		return read_states::UNFORMATTED;
	}
	// Data to be added
	else {
		if (str.empty() || str[0] == '\r' || str[0] == '\n')
			empty_lines++;

		// Do not sort if the line passed (str) is a comment ex: // This is a comment
		if (str.length() >= 2 && str[0] == '/' && str[1] == '/')
			can_be_sorted = false;

		data.push_back(str);
		return read_states::GOOD;
	}
}

void Entry::ClearData() {
	header.clear();
	data.clear();
	can_be_sorted = true;
	empty_lines = 0;
}

void Entry::TrimEmptyLines() {

	if (data.empty())
		return;

	// Remove empty lines from the end
	while (data.back().empty() || data.back()[0] == '\r' || data.back()[0] == '\n') {
		data.pop_back();
		empty_lines--;

		if (data.empty())
			break;
	}

	// Failed to remove all the empty lines
	if (empty_lines != 0)
		can_be_sorted = false;
}