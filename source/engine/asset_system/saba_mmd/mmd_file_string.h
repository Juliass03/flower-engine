#pragma once
#include <string>
#include "unicode.h"
#include "file.h"
#include "sjis2unicode.h"

namespace engine{ namespace asset_system{ namespace saba{

template <size_t Size>
struct MMDFileString
{
	MMDFileString()
	{
		Clear();
	}

	void Clear()
	{
		for (auto& ch : m_buffer)
		{
			ch = '\0';
		}
	}

	void Set(const char* str)
	{
		size_t i = 0;
		while (i < Size && str[i] != '\0')
		{
			m_buffer[i] = str[i];
			i++;
		}

		for (; i < Size + 1; i++)
		{
			m_buffer[i] = '\0';
		}
	}

	const char* ToCString() const { return m_buffer; }
	std::string ToString() const { return std::string(m_buffer); }
	//std::wstring ToWString() const { return ConvertSjisToWString(m_buffer); }
	std::string ToUtf8String() const;

	char	m_buffer[Size + 1];
};

template <size_t Size>
bool Read(MMDFileString<Size>* str, File& file)
{
	return file.Read(str->m_buffer, Size);
}

template<size_t Size>
inline std::string MMDFileString<Size>::ToUtf8String() const
{
	std::u16string u16Str = saba::ConvertSjisToU16String(m_buffer);
	std::string u8Str;
	saba::ConvU16ToU8(u16Str, u8Str);
	return u8Str;
}

}}}