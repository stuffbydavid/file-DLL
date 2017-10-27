#pragma comment (lib, "zlib.lib")

#define ZLIB_WINAPI

#include <Windows.h>
#include <boost/filesystem.hpp>
#include <codecvt>
#include "zlib.h"
#include "unzip.h"

#define GMEXPORT extern "C" __declspec (dllexport)

using namespace std;
using namespace boost;

wstring towstr(const string str)
{
	wstring buffer;
	buffer.resize(MultiByteToWideChar(CP_UTF8, 0, &str[0], -1, 0, 0));
	MultiByteToWideChar(CP_UTF8, 0, &str[0], -1, &buffer[0], buffer.size());
	return &buffer[0];
}

string tostr(const wstring wstr)
{
	std::string buffer;
	buffer.resize(WideCharToMultiByte(CP_UTF8, 0, &wstr[0], -1, 0, 0, NULL, NULL));
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], -1, &buffer[0], buffer.size(), NULL, NULL);
	return &buffer[0];
}

string toHex(unsigned int num)
{
	if (num == 0)
		return "0";

	string h = "0123456789ABCDEF";
	string hex = "";
	while (num)
	{
		unsigned char byte = num & 255;
		unsigned char hi = h.at(byte / 16);
		unsigned char lo = h.at(byte % 16);
		hex = string(1, hi) + string(1, lo) + hex;
		num = num >> 8;
	}

	return hex;
}

void addperms(wstring file)
{
	filesystem::permissions(file, filesystem::add_perms | filesystem::owner_all | filesystem::group_all | filesystem::others_all);
}

BOOL WINAPI DllMain(HANDLE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
	return TRUE;
}

GMEXPORT double open_url(const char* url)
{
	wstring wurl = towstr(url);
	ShellExecuteW(NULL, L"open", &wurl[0], NULL, NULL, SW_SHOWDEFAULT);
	return 0;
}

GMEXPORT double execute(const char* file, const char* param, double wait)
{
	wstring wparam = L"\"" + towstr(file) + L"\" " + towstr(param);

	STARTUPINFOW siStartupInfo;
	PROCESS_INFORMATION piProcessInfo;
	memset(&siStartupInfo, 0, sizeof(siStartupInfo));
	memset(&piProcessInfo, 0, sizeof(piProcessInfo));

	int ret = CreateProcessW(NULL, &wparam[0], 0, 0, false, CREATE_DEFAULT_ERROR_MODE, 0, 0, &siStartupInfo, &piProcessInfo);
	if (ret && wait)
		WaitForSingleObject(piProcessInfo.hProcess, INFINITE);

	return ret;
}

GMEXPORT double unzip(const char* file, const char* dir)
{
	HZIP zip = OpenZip(towstr(file).c_str(), "");
	SetUnzipBaseDir(zip, towstr(dir).c_str());

	ZIPENTRY currentFile;
	int i = 0;
	while (!GetZipItem(zip, i++, &currentFile))
		UnzipItem(zip, currentFile.index, currentFile.name);

	CloseZip(zip);
	return i;
}

GMEXPORT double gzunzip(const char* file, const char* dest)
{
	char buf[1024];
	gzFile in = gzopen_w(towstr(file).c_str(), "rb8");
	FILE* out = _wfopen(towstr(dest).c_str(), L"wb");
	while (int len = gzread(in, buf, sizeof(buf)))
		fwrite(buf, 1, len, out);
	gzclose(in);
	fclose(out);
	return 0;
}

GMEXPORT double gzzip(const char* file, const char* dest)
{
	char buf[1024];
	FILE* in = _wfopen(towstr(file).c_str(), L"rb");
	gzFile out = gzopen_w(towstr(dest).c_str(), "wb");
	while (int len = fread(buf, 1, sizeof(buf), in))
		gzwrite(out, buf, len);
	fclose(in);
	gzclose(out);
	return 0;
}

GMEXPORT double directory_create(const char* dir)
{
	wstring wdir = towstr(dir);
	if (filesystem::exists(wdir))
		return 1;
	system::error_code err;
	filesystem::create_directories(wdir, err);
	return err.value();
}

GMEXPORT double directory_delete(const char* dir)
{
	wstring wdir = towstr(dir);
	if (!filesystem::exists(wdir))
		return 1;
	system::error_code err;
	filesystem::remove_all(wdir, err);
	return err.value();
}

GMEXPORT double directory_exists(const char* dir)
{
	wstring wdir = towstr(dir);
	return filesystem::exists(wdir);
}

GMEXPORT double file_rename(const char* file, const char* name)
{
	wstring wfile = towstr(file);
	wstring wname = towstr(name);
	if (!filesystem::exists(wfile))
		return 1;
	system::error_code err;
	filesystem::rename(wfile, wname, err);
	return err.value();
}

GMEXPORT double file_copy(const char* source, const char* dest)
{
	wstring wsource = towstr(source);
	wstring wdest = towstr(dest);
	if (!filesystem::exists(wsource))
		return 1;
	if (filesystem::exists(wdest)) {
		addperms(wdest);
		filesystem::remove(wdest);
	}
	system::error_code err;
	filesystem::copy(wsource, wdest, err);
	addperms(wdest);
	return err.value();
}

GMEXPORT double file_delete(const char* file)
{
	wstring wfile = towstr(file);
	if (!filesystem::exists(wfile))
		return 1;
	system::error_code err;
	addperms(wfile);
	filesystem::remove(wfile, err);
	return err.value();
}

GMEXPORT double file_exists(const char* file) 
{
	return filesystem::exists(towstr(file));
}

GMEXPORT double file_json_convert_unicode(const char* file, const char* dest)
{
	wifstream in(towstr(file));
	ofstream out(towstr(dest));

	in.imbue(locale(locale::empty(), new codecvt_utf8<wchar_t>));

	for (istreambuf_iterator<wchar_t> it(in), end; it != end; ++it)
	{
		wchar_t wch = *it;
		wstring wstr(1, wch);
		string str = tostr(wstr);
		if (str.length() > 1)
			out << "\\u" << toHex((unsigned int)wch);
		else
			out << str;
	}

	in.close();
	out.close();

	return 0;
}