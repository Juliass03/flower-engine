#include <stdexcept>
#include <string>
#include <array>
#include "saba_mmd/pmx_file.h"
#include "../core/file_system.h"

namespace engine{ namespace asset_system{


saba::PMXFile* loadPMX(const std::string& path)
{
	CHECK(FileSystem::endWith(path,".pmx"));

	if(std::filesystem::exists(path))
	{
		saba::PMXFile* newFile = new saba::PMXFile();

		if(ReadPMXFile(newFile,path.c_str()))
		{
			return newFile;
		}
		else
		{
			delete newFile;
		}
	}

	LOG_IO_WARN("Load pmx file {0} fail.", path);
	return nullptr;
}

}}