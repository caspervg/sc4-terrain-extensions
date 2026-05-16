#pragma once
#include "args.hxx"
#include "cISTETerrain.h"

#include "TerrainOperator.hpp"

class TerrainTool : public TerrainOperator {
public:
	explicit TerrainTool(cISTETerrain* terrain) : TerrainOperator(terrain) {}
	~TerrainTool() override = default;
	virtual void RegisterArguments(args::Group& commands) = 0;
	virtual bool ShouldExecute(const args::ArgumentParser& parser) const = 0;
	virtual void Execute(const args::ArgumentParser&) = 0;
	virtual const char* GetName() const = 0;
	virtual const char* GetDescription() const = 0;
	virtual const char* GetUsage() const = 0;
};
