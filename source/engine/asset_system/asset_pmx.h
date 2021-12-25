#pragma once
#include "../core/core.h"

namespace engine{ namespace asset_system{ 

namespace saba
{
	class PMXFile;
}

extern saba::PMXFile* loadPMX(const std::string&);

}}