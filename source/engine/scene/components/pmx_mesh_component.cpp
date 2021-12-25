#include "pmx_mesh_component.h"
#include "../../renderer/pmx_mesh.h"
#include "../../asset_system/asset_pmx.h"
#include "../../asset_system/saba_mmd/pmx_file.h"

namespace engine
{
	class PMXMeshManager
	{
	private:
		std::unordered_map<std::string, asset_system::saba::PMXFile*> m_cache;

	public:
		static PMXMeshManager* get()
		{
			static PMXMeshManager manager;
			return &manager;
		}

		asset_system::saba::PMXFile* getPMX(const std::string& name)
		{
			if(m_cache.find(name) == m_cache.end())
			{
				asset_system::saba::PMXFile* newFile = asset_system::loadPMX(name);

				if(newFile)
				{
					m_cache[name] = newFile;
				}
				return newFile;
			}
			else
			{
				return m_cache[name];
			}
		}
	};
}


engine::PMXMeshComponent::PMXMeshComponent()
	: Component("PMXMeshComponent")
{

}

engine::PMXMeshComponent::~PMXMeshComponent()
{

}

void engine::PMXMeshComponent::setNode(std::shared_ptr<SceneNode> node)
{
	m_node = node;
}

std::string engine::PMXMeshComponent::getPath() const
{
	return m_pmxPath;
}

void engine::PMXMeshComponent::setPath(std::string newPath)
{
	if(m_pmxPath!=newPath)
	{
		m_pmxPath = newPath;

		asset_system::saba::PMXFile* pmxFile = PMXMeshManager::get()->getPMX(newPath);
		if(pmxFile)
		{
			// todo: load pmx mesh.
		}
	}
}
