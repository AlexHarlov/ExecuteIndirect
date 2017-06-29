#pragma once
#include "pch.h"
#include "ShaderStructures.h"
#include "RenderItem.h"

namespace ExecuteIndirect
{
	class Scene
	{
	public:
		Scene();
		~Scene();
		void SetOccluders(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& renderItems);
		void BuildInstanceData(std::unordered_map<std::string, std::unique_ptr<RenderItem>>& renderItems);
	private:
		
		const UINT bisonCount = 900;
		const UINT farmHouseCount = 150;
		const UINT houseCount = 140;
		const UINT tigerCount = 900;
		const UINT firCount = 2400;
		const UINT complexTreeCount = 10;
		const UINT stoneCount = 2400;
		const UINT grassCount = 2400;
		const UINT deerCount = 900;
		const UINT wolfCount = 900;
		std::unordered_map<std::string, InstanceData> m_InstanceData;
		std::vector<std::string> m_OccluderModelNames;
	};

}