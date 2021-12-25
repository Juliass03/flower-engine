#pragma once
#include <string>

namespace engine{ namespace asset_system{ namespace saba{

	extern char16_t ConvertSjisToU16Char(int ch);
	extern std::u16string ConvertSjisToU16String(const char* sjisCode);
	extern std::u32string ConvertSjisToU32String(const char* sjisCode);

}}}