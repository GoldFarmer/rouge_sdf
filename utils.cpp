
#include "utils.h"
#include <windows.h>
#include <Shlobj.h>
#include <unordered_map>
#include <memory>


std::vector<std::wstring> EnumerateDirectory(const std::wstring &directory, const std::wstring &filter)
{
    std::vector<std::wstring> res;
    WIN32_FIND_DATAW fd;
    HANDLE fh = FindFirstFileW((directory + filter).c_str(), &fd);
    if (fh != INVALID_HANDLE_VALUE)
    {
        do
        {
            res.push_back(directory + fd.cFileName);
        } while (FindNextFileW(fh, &fd));

        FindClose(fh);
    }
    return res;
}


std::wstring ExtractFilePath(const std::wstring &file_name)
{
    const size_t last_slash_idx = file_name.rfind('\\');
    if (std::wstring::npos != last_slash_idx)
    {
        return file_name.substr(0, last_slash_idx + 1);
    }
    else
    {
        return L".\\";
    }
}
std::wstring ExtractFileName(const std::wstring &file_name)
{
    const size_t last_slash_idx = file_name.rfind('\\');
    if (std::wstring::npos != last_slash_idx)
    {
        return file_name.substr(last_slash_idx + 1, -1);
    }
    else
    {
        return file_name;
    }
}

std::wstring AbsolutePath(const std::wstring &path)
{
    std::wstring abs_path(path);
    if (abs_path.size() < 2 || abs_path[1] != ':')
    {
        wchar_t cur_path[MAX_PATH] = { 0 };
        GetCurrentDirectoryW(sizeof(cur_path), cur_path);
        abs_path = std::wstring(cur_path) + L"\\" + abs_path;
    }
    return abs_path;
}
int CreateDirectoryRecursively(const std::wstring &path)
{
    return SHCreateDirectoryExW(NULL, AbsolutePath(path).c_str(), NULL);
}

void CreateLinkByPath(const std::wstring &newName, const std::wstring &existingName)
{
    CreateDirectoryRecursively(ExtractFilePath(newName));
    DeleteFileW(AbsolutePath(newName).c_str());

    if (!CreateHardLinkW(AbsolutePath(newName).c_str(), AbsolutePath(existingName).c_str(), nullptr))
    {
        CopyFileW(existingName.c_str(), newName.c_str(), false);
    }

}


std::wstring Number(uint64_t i)
{
    std::wstringstream ss;
    ss << i;
    return ss.str();
}


void WriteData(const std::wstring &name, const unsigned char *data, uint64_t data_size)
{
    CreateDirectoryRecursively(ExtractFilePath(name));
    std::ofstream s(name, std::ios::binary);
    s.write((const char*) data, data_size);
    if (!s.good())
    {
        throw std::exception("Failed to write file");
    }
    std::wcout << L">" << name << std::endl;
}

void WriteDataApp(const std::wstring &name, const unsigned char *data, uint64_t data_size)
{
    std::ofstream s(name, std::ios::binary | std::ios::app);
    s.write((const char*) data, data_size);
    if (!s.good())
    {
        throw std::exception("Failed to write file");
    }
}

bool IsFileExist(const std::wstring & fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

std::string UnicodeToAnsi(const std::wstring &string)
{
    int len;
    DWORD slength = DWORD(string.length()) + 1;
    len = WideCharToMultiByte(CP_ACP, 0, string.c_str(), slength, 0, 0, 0, 0);
    std::string result(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, string.c_str(), slength, &result[0], len, 0, 0);
    result.resize(len - 1);
    return result;
}
std::wstring AnsiToUnicode(const std::string &string)
{
    int len;
    DWORD slength = DWORD(string.length()) + 1;
    len = MultiByteToWideChar(CP_ACP, 0, string.c_str(), slength, 0, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, string.c_str(), slength, &result[0], len);
    result.resize(len - 1);
    return result;
}

unsigned long long FileSize(const std::wstring &fileName)
{
    std::ifstream f(fileName, std::ios::binary);
    f.seekg(0, std::ios_base::end);
    if (!f.good())
        throw std::exception("Cannot get file size");
    return f.tellg();
}