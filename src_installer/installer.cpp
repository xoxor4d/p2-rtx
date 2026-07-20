#define MINIZ_HEADER_FILE_ONLY
#include "miniz.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <winhttp.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

std::string open_file_dialog()
{
	char filename[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = "Portal2 Executable\0portal2.exe\0All Files\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Select your portal2.exe";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.lpstrInitialDir = nullptr;

	if (GetOpenFileNameA(&ofn)) {
		return filename;
	}
		

	return "";
}

bool file_exists(const std::string& path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// the p2-rtx patched portal2.exe contains the imported dll name as a plain string
// returns 1 = has the import, 0 = vanilla, -1 = could not read the file
int exe_p2rtx_import_state(const std::string& exe_path)
{
	std::error_code ec;
	const auto size = std::filesystem::file_size(exe_path, ec);
	if (ec || !size) {
		return -1;
	}

	std::ifstream file(exe_path, std::ios::binary);
	if (!file.is_open()) {
		return -1;
	}

	std::string data(static_cast<size_t>(size), '\0');
	file.read(data.data(), static_cast<std::streamsize>(size));
	if (file.gcount() != static_cast<std::streamsize>(size)) {
		return -1;
	}

	return data.find("p2-rtx.dll") != std::string::npos ? 1 : 0;
}

std::filesystem::path get_installer_dir()
{
	wchar_t buf[MAX_PATH] = { 0 };
	GetModuleFileNameW(nullptr, buf, MAX_PATH);
	return std::filesystem::path(buf).parent_path();
}

bool init_zip_from_path(const std::filesystem::path& zip_path, mz_zip_archive& zip, std::vector<char>& buffer)
{
	std::ifstream file(zip_path, std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	file.seekg(0, std::ios::end);
	const std::streamoff size = file.tellg();
	if (size <= 0) {
		return false;
	}
	buffer.resize(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), size);
	if (!file) {
		return false;
	}

	return mz_zip_reader_init_mem(&zip, buffer.data(), buffer.size(), 0) == MZ_TRUE;
}

std::string read_file_from_zip(const std::filesystem::path& zip_path, const std::string& file_path_in_zip)
{
	std::vector<char> zip_buffer;
	mz_zip_archive zip = {};
	if (!init_zip_from_path(zip_path, zip, zip_buffer)) {
		return "";
	}

	size_t file_size = 0;
	void* file_data = mz_zip_reader_extract_file_to_heap(&zip, file_path_in_zip.c_str(), &file_size, 0);
	
	std::string result;
	if (file_data && file_size > 0) {
		result.assign(static_cast<const char*>(file_data), file_size);
		mz_free(file_data);
	}

	mz_zip_reader_end(&zip);
	return result;
}

std::string read_file_from_disk(const std::string& file_path)
{
	std::ifstream file(file_path);
	if (!file.is_open()) {
		return "";
	}
	
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return content;
}

bool extract_single_file_from_zip(const std::filesystem::path& zip_path, const std::string& file_path_in_zip, const std::filesystem::path& target_path)
{
	std::vector<char> zip_buffer;
	mz_zip_archive zip = {};
	if (!init_zip_from_path(zip_path, zip, zip_buffer)) {
		return false;
	}

	// Find the file in the zip
	int file_index = mz_zip_reader_locate_file(&zip, file_path_in_zip.c_str(), nullptr, 0);
	if (file_index < 0) {
		mz_zip_reader_end(&zip);
		return false;
	}

	// Extract to memory then write using wide-capable filesystem path to handle non-ASCII
	size_t file_size = 0;
	void* file_data = mz_zip_reader_extract_to_heap(&zip, file_index, &file_size, 0);
	bool result = false;
	if (file_data && file_size > 0) {
		std::ofstream out(target_path, std::ios::binary);
		if (out.is_open()) {
			out.write(static_cast<const char*>(file_data), file_size);
			result = static_cast<bool>(out);
		}
		mz_free(file_data);
	}

	mz_zip_reader_end(&zip);
	return result;
}

bool download_file_to_path(const std::wstring& url, const std::filesystem::path& target_path)
{
	// Ensure parent directory exists
	try {
		std::filesystem::create_directories(target_path.parent_path());
	} catch (...) {
		return false;
	}

	// Download to a temp file first, then atomically replace the target.
	std::filesystem::path tmp_path = target_path;
	tmp_path += L".download";

	URL_COMPONENTS uc{};
	uc.dwStructSize = sizeof(uc);
	uc.dwSchemeLength = static_cast<DWORD>(-1);
	uc.dwHostNameLength = static_cast<DWORD>(-1);
	uc.dwUrlPathLength = static_cast<DWORD>(-1);
	uc.dwExtraInfoLength = static_cast<DWORD>(-1);

	if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
		return false;
	}

	const bool is_https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
	const std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
	std::wstring path;
	if (uc.lpszUrlPath && uc.dwUrlPathLength > 0) {
		path.append(uc.lpszUrlPath, uc.dwUrlPathLength);
	}
	if (uc.lpszExtraInfo && uc.dwExtraInfoLength > 0) {
		path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
	}
	if (path.empty()) {
		path = L"/";
	}

	HINTERNET hSession = WinHttpOpen(L"Portal2-Remix-CompMod-Installer/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0);
	if (!hSession) {
		return false;
	}

	// Always follow redirects (GitHub often redirects to codeload).
	DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

	HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
	if (!hConnect) {
		WinHttpCloseHandle(hSession);
		return false;
	}

	DWORD reqFlags = is_https ? WINHTTP_FLAG_SECURE : 0;
	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
		nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, reqFlags);
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	size_t lastPrintedLen = 0;
	const auto print_progress_line = [&](const std::string& line) {
		std::cout << "\r" << line;
		if (line.size() < lastPrintedLen) {
			std::cout << std::string(lastPrintedLen - line.size(), ' ');
		}
		std::cout << std::flush;
		lastPrintedLen = line.size();
	};

	const ULONGLONG startTick = GetTickCount64();
	print_progress_line("Downloading... 0%");

	bool ok = false;
	std::ofstream out(tmp_path, std::ios::binary);
	if (!out.is_open()) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return false;
	}

	if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
		WinHttpReceiveResponse(hRequest, nullptr))
	{
		// Validate status code
		DWORD statusCode = 0;
		DWORD statusSize = sizeof(statusCode);
		if (!WinHttpQueryHeaders(hRequest,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX))
		{
			statusCode = 0;
		}

		if (statusCode >= 200 && statusCode < 300)
		{
			// Try to read content length (optional)
			ULONGLONG contentLength = 0;
			{
				std::wstring lenStr;
				DWORD lenSize = 0;
				if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &lenSize, WINHTTP_NO_HEADER_INDEX)
					&& GetLastError() == ERROR_INSUFFICIENT_BUFFER && lenSize > 0)
				{
					lenStr.resize(lenSize / sizeof(wchar_t));
					if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, lenStr.data(), &lenSize, WINHTTP_NO_HEADER_INDEX)) {
						// Ensure null-termination then parse
						lenStr.resize((lenSize / sizeof(wchar_t)) ? (lenSize / sizeof(wchar_t) - 1) : 0);
						try {
							contentLength = std::stoull(lenStr);
						} catch (...) {
							contentLength = 0;
						}
					}
				}
			}

			std::vector<char> buffer(64 * 1024);
			ULONGLONG downloaded = 0;
			DWORD lastPercent = 101;
			ULONGLONG lastPrintTick = startTick;

			for (;;)
			{
				DWORD avail = 0;
				if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
					break;
				}
				if (avail == 0) {
					ok = true;
					break;
				}

				while (avail > 0)
				{
					DWORD toRead = (avail > buffer.size()) ? static_cast<DWORD>(buffer.size()) : avail;
					DWORD read = 0;
					if (!WinHttpReadData(hRequest, buffer.data(), toRead, &read) || read == 0) {
						avail = 0;
						break;
					}

					out.write(buffer.data(), read);
					if (!out) {
						avail = 0;
						break;
					}

					downloaded += read;
					avail -= read;

					const ULONGLONG now = GetTickCount64();
					const double seconds = (now > startTick) ? (static_cast<double>(now - startTick) / 1000.0) : 0.0;
					const double mbps = (seconds > 0.0) ? (static_cast<double>(downloaded) / seconds / (1024.0 * 1024.0)) : 0.0;

					if (contentLength > 0)
					{
						const DWORD percent = static_cast<DWORD>((downloaded * 100ull) / contentLength);
						if (percent != lastPercent && (now - lastPrintTick) >= 100) {
							lastPercent = percent;
							lastPrintTick = now;
							std::ostringstream oss;
							oss.setf(std::ios::fixed);
							oss << "Downloading... " << percent << "%";
							if (mbps > 0.01) {
								oss << " (" << std::setprecision(2) << mbps << " MB/s)";
							}
							print_progress_line(oss.str());
						}
					}
					else
					{
						// No content-length; print periodic progress
						if ((now - lastPrintTick) >= 250) {
							lastPrintTick = now;
							const double downloadedMB = static_cast<double>(downloaded) / (1024.0 * 1024.0);
							std::ostringstream oss;
							oss.setf(std::ios::fixed);
							oss << "Downloading... " << std::setprecision(2) << downloadedMB << " MB";
							if (mbps > 0.01) {
								oss << " (" << std::setprecision(2) << mbps << " MB/s)";
							}
							print_progress_line(oss.str());
						}
					}
				}

				if (!out) {
					break;
				}
			}
		}
	}

	out.close();

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	if (!ok) {
		std::error_code ec;
		std::filesystem::remove(tmp_path, ec);
		print_progress_line("Downloading... failed.");
		std::cout << "\n" << std::flush;
		return false;
	}

	// Replace target with tmp
	{
		std::error_code ec;
		std::filesystem::remove(target_path, ec); // ignore if doesn't exist
		ec.clear();
		std::filesystem::rename(tmp_path, target_path, ec);
		if (ec) {
			// Best-effort cleanup
			std::filesystem::remove(tmp_path, ec);
			print_progress_line("Downloading... failed.");
			std::cout << "\n" << std::flush;
			return false;
		}
	}

	print_progress_line("Downloading... done.");
	std::cout << "\n" << std::flush;
	return true;
}

bool extract_zip(const std::filesystem::path& zip_path, const std::string& target_dir, const std::string& inner_folder = "", size_t* out_extracted_files = nullptr)
{
	std::vector<char> zip_buffer;
	mz_zip_archive zip = {};
	if (!init_zip_from_path(zip_path, zip, zip_buffer)) {
		return false;
	}

	bool result = true;
	size_t extracted_files = 0;
	mz_uint file_count = mz_zip_reader_get_num_files(&zip);
	
	// Validate output root once (canonical requires the directory to exist)
	std::filesystem::path canonical_target;
	try {
		canonical_target = std::filesystem::canonical(std::filesystem::path(target_dir));
	} catch (...) {
		mz_zip_reader_end(&zip);
		return false;
	}

	for (mz_uint i = 0u; i < file_count; i++)
	{
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
			continue;
		}

		auto entry_path = std::filesystem::path(stat.m_filename);
		if (!inner_folder.empty())
		{
			std::filesystem::path inner(inner_folder);
			if (!entry_path.native().starts_with(inner.native())) {
				continue;
			}
			entry_path = entry_path.lexically_relative(inner);
		}

		std::filesystem::path out_path = std::filesystem::path(target_dir) / entry_path;
		
		// Validate output path to prevent directory traversal
		std::filesystem::path canonical_output = std::filesystem::absolute(out_path);
		if (!canonical_output.native().starts_with(canonical_target.native())) {
			continue; // Skip paths outside target directory
		}

		// Some zips (notably Windows-created zips) store directory entries with a trailing '\' instead of '/',
		// and miniz doesn't always flag them as directories. Treat trailing slash/backslash as directory.
		bool is_dir = stat.m_is_directory != 0;
		if (!is_dir && stat.m_filename) {
			const std::string name(stat.m_filename);
			if (!name.empty() && (name.back() == '/' || name.back() == '\\')) {
				is_dir = true;
			}
		}
		// Ask miniz too (handles some edge-cases)
		if (!is_dir && mz_zip_reader_is_file_a_directory(&zip, i)) {
			is_dir = true;
		}

		if (is_dir) {
			try {
				create_directories(out_path);
			} catch (const std::exception&) {
				MessageBoxA(nullptr, ("Failed to create directory: " + out_path.string()).c_str(), "Error", MB_ICONERROR);
				result = false;
			}
			continue;
		}

		try {
			create_directories(out_path.parent_path());
		} catch (const std::exception&) {
			MessageBoxA(nullptr, ("Failed to create directory: " + out_path.parent_path().string()).c_str(), "Error", MB_ICONERROR);
			result = false;
			continue;
		}

		if (i > 0 && (i % 30 == 0)) {
			Sleep(10);
		}

		if (!mz_zip_reader_extract_to_file(&zip, i, out_path.string().c_str(), 0))
		{
			MessageBoxA(nullptr, ("Failed to extract: " + std::string(stat.m_filename)).c_str(), "Error", MB_ICONERROR);
			result = false;
		}
		else
		{
			extracted_files++;
		}
	}

	mz_zip_reader_end(&zip);
	if (out_extracted_files) {
		*out_extracted_files = extracted_files;
	}
	return result;
}

int main()
{
	Sleep(200);
	
	std::cout << "Select the Portal2 directory by selecting your portal2.exe ...\n";
	Sleep(500);

	// select portal2.exe
    std::string portal2_exe_path = open_file_dialog();
	if (portal2_exe_path.empty()) 
	{
		std::cout << "Path invalid. Exiting ...\n";
		return 0;
	}

    const std::string game_dir = std::filesystem::path(portal2_exe_path).parent_path().string();
	
	// Validate game directory exists
	if (!std::filesystem::exists(game_dir) || !std::filesystem::is_directory(game_dir)) {
		MessageBoxA(nullptr, "Invalid game directory selected.", "Error", MB_ICONERROR);
		return 1;
	}
	
	std::cout << "Using Path: '" << game_dir << "'\n\n";
	
	// Find zip file first (needed for version comparison)
	static const wchar_t* zip_prefix = L"Portal2-Remix-CompatibilityMod";
	std::filesystem::path found_zip;

	for (const auto& entry : std::filesystem::directory_iterator(get_installer_dir()))
	{
		if (!entry.is_regular_file()) {
			continue;
		}

		const auto& p = entry.path();

		if (p.extension() == L".zip" && p.stem().wstring().starts_with(zip_prefix))
		{
			found_zip = p;
			break;  // take the first match
		}
	}

	if (found_zip.empty()) 
	{
		std::cout << "[ERR] Could not find any zip starting with 'Portal2-Remix-CompatibilityMod'.\n";
		MessageBoxA(nullptr, "Could not find 'Portal2-Remix-CompatibilityMod.zip' in the installer directory.", "Error", MB_ICONERROR);
		return 1;
	}
	
	// Validate zip file exists and is readable
	if (!std::filesystem::exists(found_zip) || !std::filesystem::is_regular_file(found_zip)) 
	{
		std::cout << "[ERR] Found zip file but it is not accessible: " << found_zip.string() << "\n";
		MessageBoxA(nullptr, "The zip file found is not accessible.", "Error", MB_ICONERROR);
		return 1;
	}

	Sleep(500);

	// check and disable asiloader if found
	if (file_exists(game_dir + "\\bin\\winmm.dll"))
	{
		if (MoveFileExA(
			(game_dir + "\\bin\\winmm.dll").c_str(),
			(game_dir + "\\bin\\winmm.dll.bak").c_str(),
			MOVEFILE_REPLACE_EXISTING))
		{
			std::cout << "Renamed 'bin/winmm.dll' to 'bin/winmm.dll.bak'\n";
		}
		Sleep(25);
	}

	// keep a copy of the untouched exe so the mod can be cleanly disabled later
	// (the extracted files replace portal2.exe with the p2-rtx import-patched one)
	// an unreadable exe must not count as vanilla - that would clobber the backup
	if (file_exists(portal2_exe_path) && exe_p2rtx_import_state(portal2_exe_path) == 0)
	{
		if (CopyFileA(portal2_exe_path.c_str(), (game_dir + "\\portal2.exe.vanilla").c_str(), FALSE))
		{
			std::cout << "Backed up the vanilla portal2.exe to 'portal2.exe.vanilla'\n";
		}
		Sleep(25);
	}

	// check if comp mod and remix are installed -> update
	const bool has_remix_comp_mod = file_exists(game_dir + "\\bin\\d3d9.dll") &&
									file_exists(game_dir + "\\p2-rtx.dll");

	if (has_remix_comp_mod) {
		std::cout << "Detected another version of the RTX Remix Compatibility Mod. Updating ... \n";
	}

	// extract comp files

	std::cout << "Extracting zip ...\n";
	Sleep(100); // Small delay before extraction

	if (!extract_zip(found_zip, game_dir, "Portal2-Remix-CompatibilityMod"))
	{
		std::cout << "[ERR] Failed to extract 'Portal2-Remix-CompatibilityMod' files from 'Portal2-Remix-CompatibilityMod.zip'\n";
		std::cout << "> Aborting installation. Please extract files manually.\n";
		return 0;
	}

	Sleep(100); // Small delay between extractions

	// Optional: download and install base remix-mod (mods folder into rtx-remix)
	{
		static const char* base_mod_zip_url = "https://github.com/xoxor4d/p2-rtx-base-mod/archive/refs/heads/master.zip";
		static const char* base_mod_repo_url = "https://github.com/xoxor4d/p2-rtx-base-mod";
		static const char* base_mod_zip_inner_mods_github = "p2-rtx-base-mod-master/mods";
		static const char* base_mod_zip_inner_mods_flat = "mods";

		// Print full info (including links) to console so the user can copy them.
		std::cout
			<< "\n\nRequired: Download and extract the base remix-mod?\n"
			<< "This contains actual remix replacements such as PBR textures, light overrides etc.\n\n"
			<< "Direct zip link: " << base_mod_zip_url << "\n"
			<< "Repo: " << base_mod_repo_url << "\n\n"
			<< "This will place the downloaded zip next to the installer, then extract the 'mods' folder into:\n"
			<< (game_dir + "\\rtx-remix\\") << "\n";

		const std::string prompt = std::string("Required: Download and extract the base remix-mod?");
		const std::filesystem::path base_zip_path = get_installer_dir() / "master.zip";
		const std::filesystem::path mods_dir = std::filesystem::path(game_dir) / "rtx-remix" / "mods";

		const int userChoice = MessageBoxA(nullptr, prompt.c_str(), "Base Remix-Mod", MB_YESNO | MB_ICONQUESTION);

		// Handle "No" but file already exists (or was downloaded while the prompt was open).
		if (std::filesystem::exists(base_zip_path))
		{
			std::cout << "Found existing base remix-mod zip: " << base_zip_path.string() << "\n";
		}
		else if (userChoice != IDYES)
		{
			MessageBoxA(nullptr,
				("Base remix-mod is required to continue.\n\n"
				 "File was not found:\n"
				 + base_zip_path.string() +
				 "\n\nPlease download 'master.zip' and place it next to the installer.\n"
				 "Links are printed in the console window.").c_str(),
				"Base Remix-Mod Required",
				MB_OK | MB_ICONERROR);
			return 0;
		}

		// If the user already downloaded it manually, reuse it.
		if (!std::filesystem::exists(base_zip_path))
		{
			std::cout << "Downloading base remix-mod zip to: " << base_zip_path.string() << "\n";
			if (!download_file_to_path(L"https://github.com/xoxor4d/p2-rtx-base-mod/archive/refs/heads/master.zip", base_zip_path))
			{
				MessageBoxA(nullptr,
					("Failed to download base remix-mod.\n\n"
					 "Please download 'master.zip' and place it next to the installer here:\n"
					 + base_zip_path.string()
					 + "\n\nLinks are printed in the console window.\n"
					 "Then run the installer again to continue.").c_str(),
					"Error",
					MB_ICONERROR);
				return 0;
			}
		}

		try {
			std::filesystem::create_directories(mods_dir);
		} catch (...) {
			MessageBoxA(nullptr, ("Failed to create directory: " + mods_dir.string()).c_str(), "Error", MB_ICONERROR);
			return 0;
		}

		std::cout << "Extracting base remix-mod into rtx-remix/mods ...\n";
		size_t extracted = 0;
		bool ok_extract = extract_zip(base_zip_path, mods_dir.string(), base_mod_zip_inner_mods_github, &extracted);
		if (!ok_extract || extracted == 0)
		{
			// Fallback for archives that have 'mods/...' at the root
			extracted = 0;
			ok_extract = extract_zip(base_zip_path, mods_dir.string(), base_mod_zip_inner_mods_flat, &extracted);
		}

		if (!ok_extract || extracted == 0)
		{
			MessageBoxA(nullptr,
				("Failed to extract base remix-mod.\n\nYou can extract it manually from:\n"
					+ std::string(base_mod_zip_url)
					+ "\n\nRepo:\n"
					+ std::string(base_mod_repo_url)
					+ "\n\nMake sure the extracted folder ends up here:\n"
					+ (std::filesystem::path(game_dir) / "rtx-remix" / "mods").string()).c_str(),
				"Error",
				MB_ICONERROR);
			return 0;
		}
	}

	// Only prompt about DirectX if this is a fresh install (p2-rtx.dll doesn't exist)
	if (!has_remix_comp_mod)
	{
		std::cout
			<< "\n\nIt's recommended to install Microsoft DirectX June 2010 Redistributable.\n"
			<< "https://www.microsoft.com/en-us/download/details.aspx?id=8109\n";

		// DX9 June 2010 runtime
		if (MessageBoxA(nullptr, "It's recommended to install Microsoft DirectX June 2010 Redistributable.\nDo you want to open a link to the installer?", "DirectX Runtime", MB_YESNO | MB_ICONQUESTION) == IDYES) {
			ShellExecuteA(nullptr, "open", "https://www.microsoft.com/en-us/download/details.aspx?id=8109", nullptr, nullptr, SW_SHOWNORMAL);
		}
	}

	std::cout << "\n\nIf you run into issues, please create an issue on the GitHub repository.\n> Please include 'portal2-rtx/logs/logfile.txt'\n> The log files from 'rtx-remix/logs'\n> A short description and anything else that might help to identify the issue.\n";

	MessageBoxA(nullptr, "Installation complete!\nYou can now launch Portal 2\nby running run-p2-rtx.bat\n\nNote: Steam updates restore the vanilla portal2.exe and disable the mod.\nrun-p2-rtx.bat detects this and repairs the install automatically.", "Success", MB_ICONINFORMATION);
    return 0;
}