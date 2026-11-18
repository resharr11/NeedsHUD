// NeedsHUD - where the player's settings file lives (1.0.3). Kept in its own file because it needs windows.h, whose
// macros would clash with names in main.cpp. The settings go next to NeedsHUD's log, in
// Documents\My Games\Skyrim Special Edition\SKSE\NeedsHUD.ini (Documents found through Windows, so a Documents folder
// moved to OneDrive or another drive is followed), never into Data: his rule, nothing written at runtime may land in
// Data or MO2's Overwrite (1.0.3 code review).
#include "Settings.h"

#include <ShlObj.h>
#include <windows.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")  // FOLDERID_Documents

namespace NeedsHUD
{
	std::filesystem::path UserIniPath()
	{
		static const std::filesystem::path path = []() -> std::filesystem::path {
			PWSTR                 docs = nullptr;
			std::filesystem::path result;
			if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)) && docs) {
				const std::filesystem::path games = std::filesystem::path(docs) / L"My Games";
				// the folder whose NeedsHUD.log is newest is the one this game uses (the log is written before the
				// settings are read; Steam's and GOG's folders can both exist on one PC; 1.0.3 second review). No log
				// found: the first SKSE folder that exists
				std::filesystem::path           firstFound;
				std::filesystem::file_time_type newest{};
				for (const wchar_t* name : { L"Skyrim Special Edition", L"Skyrim Special Edition GOG" }) {
					std::error_code ec;
					const auto skse = games / name / L"SKSE";
					const auto when = std::filesystem::last_write_time(skse / L"NeedsHUD.log", ec);
					if (!ec && (result.empty() || when > newest)) {
						result = skse / L"NeedsHUD.ini";
						newest = when;
					}
					if (firstFound.empty() && std::filesystem::exists(skse, ec)) {
						firstFound = skse / L"NeedsHUD.ini";
					}
				}
				if (result.empty()) {
					result = firstFound;
				}
			}
			if (docs) {
				::CoTaskMemFree(docs);
			}
			// no My Games\...\SKSE folder found (SKSE writes its logs there, so it should exist): an empty path, so the
			// settings are only kept until the game closes; never a file in Data (his rule; 1.0.3 second review)
			return result;
		}();
		return path;
	}
}
