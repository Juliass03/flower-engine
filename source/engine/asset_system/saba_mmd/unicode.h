#pragma once
#include <string>
#include <array>

namespace engine{ namespace asset_system{ namespace saba{

	extern std::wstring ToWString(const std::string& utf8Str);
	extern std::string ToUtf8String(const std::wstring& wStr);

	extern bool TryToWString(const std::string& utf8Str, std::wstring& wStr);
	extern bool TryToUtf8String(const std::wstring& wStr, std::string& utf8Str);

	extern bool ConvChU8ToU16(const std::array<char, 4>& u8Ch, std::array<char16_t, 2>& u16Ch);
	extern bool ConvChU8ToU32(const std::array<char, 4>& u8Ch, char32_t& u32Ch);

	extern bool ConvChU16ToU8(const std::array<char16_t, 2>& u16Ch, std::array<char, 4>& u8Ch);
	extern bool ConvChU16ToU32(const std::array<char16_t, 2>& u16Ch, char32_t& u32Ch);

	extern bool ConvChU32ToU8(const char32_t u32Ch, std::array<char, 4>& u8Ch);
	extern bool ConvChU32ToU16(const char32_t u32Ch, std::array<char16_t, 2>& u16Ch);

	extern bool ConvU8ToU16(const std::string& u8Str, std::u16string& u16Str);
	extern bool ConvU8ToU32(const std::string& u8Str, std::u32string& u32Str);

	extern bool ConvU16ToU8(const std::u16string& u16Str, std::string& u8Str);
	extern bool ConvU16ToU32(const std::u16string& u16Str, std::u32string& u32Str);

	extern bool ConvU32ToU8(const std::u32string& u32Str, std::string& u8Str);
	extern bool ConvU32ToU16(const std::u32string& u32Str, std::u16string& u16Str);

}}}