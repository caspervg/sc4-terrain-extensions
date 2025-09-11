#pragma once
#include <vector>
#include <memory>
#include "utils/Logger.h"
#include "TerrainTool.hpp"

class TerrainToolRegistry {
private:
	std::vector<std::unique_ptr<TerrainTool>> tools;
public:
	void RegisterTool(std::unique_ptr<TerrainTool> tool) {
		tools.push_back(std::move(tool));
	}

	void RegisterAllArguments(args::Group& commands) {
		for (auto& tool : tools) {
			tool->RegisterArguments(commands);
		}
	}

	bool ExecuteAnyTool(const args::ArgumentParser& parser) {
		for (auto& tool : tools) {
			if (tool->ShouldExecute(parser)) {
				tool->Execute(parser);
				return true;
			}
		}
		return false;
	}

	void ListTools() const {
		for (const auto& tool : tools) {
			LOG_DEBUG("Tool {} -> {}", tool->GetName(), tool->GetDescription());
		}
	}
};