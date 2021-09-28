#include "asset_common.h"
#include "../core/file_system.h"
#include <filesystem>
#include <fstream>
#include "../core/core.h"

namespace engine{ namespace asset_system{

std::string rawPathToAssetPath(const std::string& pathIn,EAssetFormat format)
{
	switch(format)
	{
	case engine::asset_system::EAssetFormat::T_R8G8B8A8:
		return FileSystem::getFileRawName(pathIn) + ".texture";
	case engine::asset_system::EAssetFormat::M_StaticMesh_Obj:
		return FileSystem::getFileRawName(pathIn) + ".mesh";
	case engine::asset_system::EAssetFormat::Unknown:
	default:
		LOG_FATAL("Unkonw error!");
		return "";
	}
}

EAssetFormat toFormat(const char* f)
{
    if(strcmp(f,"T_R8G8B8A8")==0)
    {
        return EAssetFormat::T_R8G8B8A8;
    }

    return EAssetFormat::Unknown;
}

bool saveBinFile(const char* path,const AssetFile& file)
{
	std::ofstream outfile;
	outfile.open(path,std::ios::binary | std::ios::out);

	if(!outfile.is_open())
	{
		LOG_IO_FATAL("Fail to write file {0}.",path);
		return false;
	}

	// 1. type
	outfile.write(file.type,4);

	// 2. version
	uint32_t version = file.version;
	outfile.write((const char*)&version,sizeof(uint32_t));

	// 3. json lenght
	uint32_t lenght = static_cast<uint32_t>(file.json.size());
	outfile.write((const char*)&lenght,sizeof(uint32_t));

	// 4. blob lenght
	uint32_t bloblenght = static_cast<uint32_t>(file.binBlob.size());
	outfile.write((const char*)&bloblenght,sizeof(uint32_t));

	// 5. json stream
	outfile.write(file.json.data(), lenght);

	// 6. blob stream
	outfile.write(file.binBlob.data(), file.binBlob.size());

	outfile.close();
	return true;
}

bool loadBinFile(const char* path,AssetFile& out_file)
{
	std::ifstream infile;
	infile.open(path,std::ios::binary);

	if(!infile.is_open())
	{
		LOG_IO_FATAL("Fail to open file {0}.",path);
		return false;
	}

	infile.seekg(0);

	// 1. type
	infile.read(out_file.type,4);

	// 2. version
	infile.read((char*)&out_file.version,sizeof(uint32_t));

	// 3. json lenght
	uint32_t jsonlen = 0;
	infile.read((char*)&jsonlen,sizeof(uint32_t));

	// 4. blob lenght
	uint32_t bloblen = 0;
	infile.read((char*)&bloblen,sizeof(uint32_t));

	// 5. json stream
	out_file.json.resize(jsonlen);
	infile.read(out_file.json.data(),jsonlen);

	// 6. blob stream
	out_file.binBlob.resize(bloblen);
	infile.read(out_file.binBlob.data(),bloblen);

	infile.close();
	return true;
}

ECompressMode toCompressMode(const char* type)
{
	if(strcmp(type,"LZ4") == 0)
	{
		return ECompressMode::LZ4;
	}
	else
	{
		return ECompressMode::None;
	}
	return ECompressMode::None;
}

}}