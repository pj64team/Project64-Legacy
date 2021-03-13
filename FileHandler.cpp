#include "FileHandler.h"
#include "Settings Common Defines.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <Windows.h>
#include <algorithm>
#include <sys/stat.h>
#include <regex>

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
enum class supported_writes {WRITE_ENTRY, DELETE_SETTING, DELETE_ENTRY};

// The function prototypes (Used to communicate with the class)
FileStuff *GetTheFile(char *filename);
string ReadHandler(char *filename, char *id, char *setting, char *def, BOOLEAN getting_value);
void WriteHandler(char *filename, char *id, char *setting, char *value, supported_writes write_type);


////////////////////////////////
// CLASS IMPLEMENTATION (PUBLIC)
////////////////////////////////
FileStuff::FileStuff() {
	this->buffer.clear();
	this->entry_start = this->buffer.end();
	this->entry_end = this->buffer.end();
	this->filename = "";
	this->fullpath = "";
	this->last_offset = 0;
}

FileStuff::FileStuff(char *_filename) {
	this->buffer.clear();
	this->entry_start = this->buffer.end();
	this->entry_end = this->buffer.end();
	this->SetFileName(_filename);
	this->last_offset = 0;
}

FileStuff::~FileStuff() {
	this->buffer.clear();
	this->buffer.shrink_to_fit();
}

void FileStuff::SetFileName(char *_filename) {
	char path[MAX_PATH];
	string::size_type pos;

	this->filename = _filename;
	
	// Create the full path to be used to open the file
	GetModuleFileName(NULL, path, MAX_PATH);
    pos = string(path).find_last_of("\\/");
	this->fullpath = string(path).substr(0, pos) + "\\Config\\" + _filename;
}

string FileStuff::GetFileName() {
	return this->filename;
}

void FileStuff::SortEntry(string search) {
	vector<string>::iterator found;
	const static string s[] = {"Good Name=", "Internal Name=", "Name="};

	// regex to exactly match a game ID [00000000-00000000-C:00]
	// Comments are optional at the end but they can be read as well
	regex expr ("^\\[([A-Fa-f0-9]){8}-([A-Fa-f0-9]){8}-C:([A-Fa-f0-9]){2}\\]\\s*(?://?.*?\\s*)?$");

	this->FindEntry(search);

	// The entry is empty
	if (this->entry_start == this->buffer.end() || this->entry_end == this->buffer.begin())
		return;
	
	// Only prioritize Good Name, Internal Name, and Name if this is a game entry
	// The format is [xxxxxxxx-xxxxxxxx-C:xx] where x are hex characters
	if (regex_match(*this->entry_start, expr)) {
		for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); i++) {
			found = find_if(this->entry_start, this->entry_end, [i](string p) { return p.compare(0, s[i].length(), s[i]) == 0;});
			if (found != this->entry_end) {
				++this->entry_start;
				std::swap(*this->entry_start, *found);
			}
		}
	}

	// Ignore white space and comments at the bottom, it will end up at the top of the list after a sort
	while(isprint((*(this->entry_end - 1))[0]) == 0 || ((*(this->entry_end - 1))[0] == '/' && (*(this->entry_end - 1))[1] == '/'))
		--this->entry_end;

	// This sort does not currently handle spaces or comments, if they are in new lines they will be moved to the top
	std::sort(this->entry_start + 1, this->entry_end, 
		[](const string& a, const string& b) {
		for (unsigned int count = 0; count < a.length() && count < b.length(); count++) {

			// The same character, keep on checking
			if (a[count] == b[count])
				continue;

			// Check the next character (if available)
			// This is to prevent Cheat10= coming before Cheat2=
			if (count + 1 < a.length() && count + 1 < b.length())
				if ((a[count + 1] == '=' || a[count + 1] == '_') && b[count + 1] >= '0' && b[count + 1] <= '9')
					return true;

			return a[count] < b[count];
		}
		// These two are the same so it doesn't matter, just say a is less than
		return true;
	});
}

string FileStuff::GetValue(char *id, char *setting, char *def, bool fetch) {
	string search, value;
	int len;
	size_t find;
	vector<string>::iterator found;

	// The search string
	search = "[" + (string)id + "]";

	len = setting == NULL ? 0 : strlen(setting);

	// Find the start of the section we are interested in and the end
	this->FindEntry(search);

	if (fetch) {
		// Search for setting=
		found = find_if(this->entry_start, this->entry_end, [setting, len](string p) { return p.compare(0, len + 1, (string)setting + "=") == 0;});
		if (found != this->entry_end) {

			// Make sure that comments are ignored
			find = (*found).find("//");
			if (find != string::npos)
				value = (*found).substr(len + 1, find - len + 1);
			else
				value = (*found).substr(len + 1, find);
		
			// Trim whitespace off the end
			for (int i = value.length() - 1; i >= 0; i--) {
				if(isspace(value[i]))
					value[i] = '\0';
				else
					break;
			}
		}
	}

	// No = but this may be an entry alone (For example ExpansionPak // Use expansion)
	else {
		found = find_if(this->entry_start, this->entry_end, [setting, len](string p) { return p.compare(0, len, setting) == 0; });
		if (found != this->entry_end)
			if (setting == NULL)
				value = "";
			else
				value = string(setting);
	}

	if (value.empty())
		value = (def == NULL) ? "" : string(def);

	return value;
}

string FileStuff::GetKeys(char *id) {
	string result, hold;
	size_t loc;

	this->FindEntry("[" + string(id) +"]");

	for (vector<string>::iterator it = this->entry_start; it < this->entry_end; ++it) {

		hold = (*it);

		hold.erase(remove(hold.begin(), hold.end(), '\r'), hold.end());
		hold.erase(remove(hold.begin(), hold.end(), '\n'), hold.end());

		// Skip empty lines, the first entry that contains the header, and comments
		if (hold.length() == 0 || hold[0] == '[' || hold.compare(0, 2, "//") == 0)
			continue;

		loc = hold.find("=");
		if (loc != string::npos) {
			result += hold.substr(0, loc) + ",";
			continue;
		}

		loc = hold.find("//");
		if (loc != string::npos) {
			result += hold.substr(0, loc) + ",";
			continue;
		}

		result += hold + ",";
	}

	return result;
}

void FileStuff::AddSettingValue(char *id, char *setting, char *value) {

	this->FindEntry("[" + string(id) + "]");

	// New entry, adjust sect_start to point to the end
	if (this->entry_start == this->entry_end) {

		// If this is a new file do not insert a newline at the top
		if (this->buffer.begin() != this->buffer.end())
			this->buffer.push_back(LINEFEED);

		this->buffer.push_back("[" + string(id) + "]" + LINEFEED);

		this->entry_start = this->buffer.end() - 1;
		this->entry_end = this->buffer.end();
	}
	else {
		char* delstr = (char*)malloc(sizeof(char) * strlen(setting) + 2);

		if (delstr != NULL) {
			strcpy(delstr, setting);

			if (value != NULL && strlen(value) != 0)
				strcat(delstr, "=");

			this->RemoveSetting(id, delstr);

			free(delstr);
		}
	}
	
	// Insert the new setting/value pair into the file buffer
	if (value != NULL && strlen(value) != 0)
		this->entry_start = this->buffer.insert(this->entry_start + 1, (string)setting + "=" + value + LINEFEED) - 1;
	else
		this->entry_start = this->buffer.insert(this->entry_start + 1, (string)setting + LINEFEED) - 1;
}

void FileStuff::RemoveSetting(char *id, char *setting) {
	vector<string>::iterator del_start;
	string str_set = (string)setting;

	this->FindEntry("[" + string(id) + "]");

	// Nothing to remove, skip this section
	if (this->entry_start == this->buffer.end())
		return;

	del_start = remove_if(this->entry_start, this->entry_end, [str_set](string str) { return str.compare(0, str_set.length(), str_set) == 0; });
	if (del_start != this->entry_end) {
		buffer.erase(del_start, this->entry_end);
		this->FindEntry("[" + string(id) + "]");
	}
}

void FileStuff::RemoveEntry(char *id) {
	
	this->FindEntry("[" + string(id) + "]");

	if (this->entry_start != this->buffer.end()) {
		// Also include any line feeds following the entry's end
		// Is this actually needed??? I cannot recall why this bit of code was written
		if (this->entry_end != this->buffer.end() && *(this->entry_end + 1) == LINEFEED)
			++this->entry_end;
		this->buffer.erase(this->entry_start, this->entry_end);
		this->entry_start = this->buffer.end();
		this->entry_end = this->buffer.end();
	}	
}

void FileStuff::WriteToFile() {	
	ofstream myFile;

	// Write the modified file buffer to memory
	myFile.open(this->fullpath, ios::binary);

	// Write each line to file
	for (vector<string>::iterator it = this->buffer.begin(); it != this->buffer.end(); ++it)
		myFile << *it;

	// Clean-up
	myFile.close();

	// The buffer and file are the same right now, update the file time
	_stat(this->fullpath.c_str(), &this->file_time);

	this->entry_start = this->buffer.end();
	this->entry_end = this->buffer.end();
}


/////////////////////////////////
// CLASS IMPLEMENTATION (PRIVATE)
/////////////////////////////////
void FileStuff::LoadFile() {
	ifstream myFile;
	string junk;

	// Check if the file is loaded
	if (!this->buffer.empty() && !this->HasChanged())
		return;

	// Clear the cache
	this->buffer.clear();

	// Open the file for reading in binary mode (The file will be copied byte for byte)
	myFile.open(fullpath, ios_base::binary);
	myFile.seekg(0, ios::beg);

	// Line by line parsing, if this is too slow move to reading chunks at a time into a buffer
	while (getline(myFile, junk))
		this->buffer.push_back(junk + "\n");
	
	// Before closing the file get the file times
	_stat(fullpath.c_str(), &this->file_time);
	this->last_checked = time(NULL);

	// Clean up by closing the file handle
	if (myFile.is_open())
		myFile.close();
}

bool FileStuff::HasChanged() {
	struct _stat current_filetime;
	time_t current_time;

	// check_time will decide how often to check if the file has changed
	current_time = time(NULL);
	if (current_time - this->last_checked > this->check_time)
		return false;

	// Compare the file time and the last stored time for the file
	_stat(this->fullpath.c_str(), &current_filetime);
	if (current_filetime.st_mtime != this->file_time.st_mtime)
		return true;

	return false;
}

void FileStuff::FindEntry(string search) {
	// regex to exactly match a game ID [00000000-00000000-C:00]
	// Comments are optional at the end but they can be read as well
	regex expr ("^\\[([A-Fa-f0-9]){8}-([A-Fa-f0-9]){8}-C:([A-Fa-f0-9]){2}\\]\\s*(?://?.*?\\s*)?$");
	
	// regex to match, at the start [, one or more letters and/or digits and/or spaces, a ], 
	// optional spaces, a comment, followed by anything and ending in whitespace characters (any amount)
	regex expr2 ("^\\[([A-Za-z0-9]|\\s)+\\]\\s*(?://?.*?\\s*)?$");

	this->LoadFile();

	if (this->last_offset < this->buffer.size() && this->buffer[last_offset].compare(0, search.length(), search) != 0) {
		
		// Reset the start and end to bad values
		this->entry_start = this->buffer.end();
		this->entry_end = this->buffer.end();
		
		// Use std algorithm to find the first entry
		this->entry_start = find_if(this->buffer.begin(), this->buffer.end(), [search](string p) { return p.compare(0, search.length(), search) == 0;});
	}
	else
		this->entry_start = this->buffer.begin() + this->last_offset;

	// Find the next entry or the end of the file
	if (this->entry_start != this->buffer.end()) {
		this->last_offset = distance(this->buffer.begin(), this->entry_start);
		this->entry_end = find_if(this->entry_start + 1, this->buffer.end(), [search, expr, expr2](string p) { return (regex_match(p, expr) || regex_match(p, expr2));});
	}
	else
		this->entry_end = this->buffer.end();
}


//////////////////////////
// INTERFACE WITH CLASS
//////////////////////////
FileStuff *GetTheFile(char *filename) {
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

string ReadHandler(char *filename, char *id, char *setting, char *def, BOOLEAN getting_value) {
	string search, value;
	DWORD wait_result;
	FileStuff *file;

	// The search string
	search = "[" + (string)id + "]";

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

void WriteHandler(char *filename, char *id, char *setting, char *value, supported_writes write_type) {
	string built_id;
	DWORD wait_result;
	FileStuff *file;
	
	built_id = "[" + (string)id + "]";

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
	}

	// If the entry was not deleted then sort the section
	if (write_type != supported_writes::DELETE_ENTRY)
		file->SortEntry(built_id);

	file->WriteToFile();

	if (!ReleaseMutex(gLVMutex))
		MessageBox(NULL, "Failed to release a mutex???", "Error", MB_OK);
}


//////////////////////////
// THESE INTERFACE WITH C
//////////////////////////
char *ReadStr(char *filename, char *id, char *setting, char *defaultvalue) {
	string result;
	char *ret;
	
	result = ReadHandler(filename, id, setting, defaultvalue, TRUE);

	// Does this need error-checking? A non-null default should not be passed.
	ret = (char *)malloc(sizeof(char) * (result.length() + 1));
	if (ret != NULL)
		strcpy(ret, result.c_str());

	return ret;
}

int IsSet(char *filename, char *id, char *setting) {
	string result = ReadHandler(filename, id, setting, STR_FALSE, FALSE);
	return (result.compare(STR_FALSE) == 0) ? FALSE : TRUE;
}

int FetchIntValue(char *filename, char *id, char *setting, int def) {
	string result = ReadHandler(filename, id, setting, STR_FALSE, TRUE);
	return (result.compare(STR_FALSE) == 0) ? def : atoi(result.c_str());
}

void Write(char *filename, char *id, char *setting, char *value) {
	WriteHandler(filename, id, setting, value, supported_writes::WRITE_ENTRY);
}

void Delete(char *filename, char *id, char *setting) {
	WriteHandler(filename, id, setting, NULL, supported_writes::DELETE_SETTING);
}

void DeleteAll(char *filename, char *id) {
	WriteHandler(filename, id, NULL, NULL, supported_writes::DELETE_ENTRY);
}