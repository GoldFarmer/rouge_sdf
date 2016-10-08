#pragma once

#include <string>
#include <stdint.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>

std::vector<std::wstring> EnumerateDirectory(const std::wstring &directory, const std::wstring &filter = L"*");

std::wstring ExtractFilePath(const std::wstring &file_name);
std::wstring ExtractFileName(const std::wstring &file_name);


std::wstring Number(uint64_t i);

void WriteData(const std::wstring &name, const unsigned char *data, uint64_t dataSize);
void WriteDataApp(const std::wstring &name, const unsigned char *data, uint64_t dataSize);

bool IsFileExist(const std::wstring & fileName);

void CreateLinkByPath(const std::wstring &newName, const std::wstring &existingName);

int CreateDirectoryRecursively(const std::wstring &path);

std::string UnicodeToAnsi(const std::wstring &string);
std::wstring AnsiToUnicode(const std::string &string);

unsigned long long FileSize(const std::wstring &fileName);