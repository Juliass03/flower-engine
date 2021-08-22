#pragma once
#include "core.h"

namespace engine{


struct Crc
{
	static uint32 CRCTablesSB8[8][256];

	static uint32 memCrc32( const void* Data, int32 Length, uint32 CRC=0 );

	template <typename T> static uint32 typeCrc32( const T& Data, uint32 CRC=0 )
	{
		return memCrc32(&Data, sizeof(T), CRC);
	}

	template <typename CharType>
	static typename TEnableIf<sizeof(CharType) != 1, uint32>::Type strCrc32(const CharType* Data, uint32 CRC = 0)
	{
		static_assert(sizeof(CharType) <= 4, "StrCrc32 only works with CharType up to 32 bits.");

		CRC = ~CRC;
		while (CharType Ch = *Data++)
		{
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
			Ch >>= 8;
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
			Ch >>= 8;
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
			Ch >>= 8;
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
		}
		return ~CRC;
	}

	template <typename CharType>
	static typename TEnableIf<sizeof(CharType) == 1, uint32>::Type strCrc32(const CharType* Data, uint32 CRC = 0)
	{
		CRC = ~CRC;
		while (CharType Ch = *Data++)
		{
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC ^ Ch) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC     ) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC     ) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[0][(CRC     ) & 0xFF];
		}
		return ~CRC;
	}
};

}