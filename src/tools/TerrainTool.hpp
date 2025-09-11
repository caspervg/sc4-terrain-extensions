#pragma once
#include "args.hxx"
#include "cISTETerrain.h"
#include "SC4Rect.h"
#include "utils/Logger.h"
#include <cmath>

class Vector3 {
public:
	float x, z, height;

	Vector3() : x(0.0f), z(0.0f), height(0.0f) {}
	Vector3(float x_, float z_, float h_) : x(x_), z(z_), height(h_) {}

	float DistanceTo(const Vector3& other) const {
		float dx = x - other.x;
		float dz = z - other.z;
		float dh = height - other.height;
		return std::sqrt(dx * dx + dz * dz + dh * dh);
	}
};

class TerrainTool {
public:
	explicit TerrainTool(cISTETerrain* terrain) : mTerrain(terrain) {
	}
	virtual ~TerrainTool() = default;
	virtual void RegisterArguments(args::Group& commands) = 0;
	virtual bool ShouldExecute(const args::ArgumentParser& parser) const = 0;
	virtual void Execute(const args::ArgumentParser&) = 0;
	virtual const char* GetName() const = 0;
	virtual const char* GetDescription() const = 0;
	virtual const char* GetUsage() const = 0;

	void GetTileCorners(int tileX, int tileZ, Vector3 corners[4]) {
		corners[0] = Vector3(tileX, tileZ, mTerrain->GetAltitudeAtVertex(tileX, tileZ));
		corners[1] = Vector3(tileX + 1, tileZ, mTerrain->GetAltitudeAtVertex(tileX + 1, tileZ));
		corners[2] = Vector3(tileX, tileZ + 1, mTerrain->GetAltitudeAtVertex(tileX, tileZ + 1));
		corners[3] = Vector3(tileX + 1, tileZ + 1, mTerrain->GetAltitudeAtVertex(tileX + 1, tileZ + 1));
	}

	void SetAltitudeAtVertex(int tileX, int tileZ, float height) {
		if (IsValidTile(tileX, tileZ)) {
			mTerrain->SetAltitudeAtVertex(tileX, tileZ, height);
			LOG_DEBUG("Set altitude at (%d, %d) to %.2f", tileX, tileZ, height);
		} else {
			LOG_ERROR("Invalid tile (%d, %d)", tileX, tileZ);
		}
	}

	void Refresh(SC4Rect<int32_t> rect) {
		mTerrain->RedisplayTerrain(true, true, rect, 0);
		LOG_DEBUG("Terrain refreshed in rectangle");
	}

	int ClampXToTerrainBounds(int x) {
		if (x < 0) return 0;
		if (x >= static_cast<int>(mTerrain->CellCountX())) return mTerrain->CellCountX() - 1;
		return x;
	}

	int ClampZToTerrainBounds(int z) {
		if (z < 0) return 0;
		if (z >= static_cast<int>(mTerrain->CellCountZ())) return mTerrain->CellCountZ() - 1;
		return z;
	}

	void ClampToTerrainBounds(int& x, int& z) {
		x = ClampXToTerrainBounds(x);
		z = ClampZToTerrainBounds(z);
	}

	Vector3 GetTileHighestVertex(int tileX, int tileZ) {
		Vector3 corners[4];
		GetTileCorners(tileX, tileZ, corners);

		Vector3 highest = corners[0];
		for (int i = 1; i < 4; i++) {
			if (corners[i].height > highest.height) {
				highest = corners[i];
			}
		}
		return highest;
	}

	Vector3 GetTileLowestVertex(int tileX, int tileZ) {
		Vector3 corners[4];
		GetTileCorners(tileX, tileZ, corners);

		Vector3 lowest = corners[0];
		for (int i = 1; i < 4; i++) {
			if (corners[i].height < lowest.height) {
				lowest = corners[i];
			}
		}
		return lowest;
	}

	float GetTileAverageHeight(int tileX, int tileZ) {
		if (!IsValidTile(tileX, tileZ)) return 0.0f;

		Vector3 corners[4];
		GetTileCorners(tileX, tileZ, corners);

		return (corners[0].height + corners[1].height + corners[2].height + corners[3].height) / 4.0f;
	}

	bool IsValidTile(int tileX, int tileZ) {
		return tileX >= 0 && tileZ >= 0 && mTerrain->LocationIsInBounds(static_cast<float>(tileX), static_cast<float>(tileZ));
	}

	// Linear interpolation between two values
	static float Lerp(float a, float b, float t) {
		return a + t * (b - a);
	}

protected:
	cISTETerrain* mTerrain;
};