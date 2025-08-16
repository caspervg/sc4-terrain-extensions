#pragma once
#include "TerrainTool.hpp"
#include <algorithm>

class TunnelApproachTool : public TerrainTool {
private:
	std::unique_ptr<args::Command> mCommand;
	std::unique_ptr<args::Positional<int>> mStartTileX, mStartTileZ, mEndTileX, mEndTileZ;
	std::unique_ptr<args::ValueFlag<float>> mTunnelHeight, mApproachLength, mMaxGrade, mWidthTiles, mCutDepth;

public:
	TunnelApproachTool(cISTETerrain* terrain) : TerrainTool(terrain) {
	}

	~TunnelApproachTool() override = default;

	void RegisterArguments(args::Group& commands) override {
		mCommand = std::make_unique<args::Command>(commands, "tunnelapproach",
			"Create tunnel approaches with proper drainage and portal cuts");

		mStartTileX = std::make_unique<args::Positional<int>>(*mCommand, "startx", "Tunnel start portal tile X");
		mStartTileZ = std::make_unique<args::Positional<int>>(*mCommand, "startz", "Tunnel start portal tile Z");
		mEndTileX = std::make_unique<args::Positional<int>>(*mCommand, "endx", "Tunnel end portal tile X");
		mEndTileZ = std::make_unique<args::Positional<int>>(*mCommand, "endz", "Tunnel end portal tile Z");

		mTunnelHeight = std::make_unique<args::ValueFlag<float>>(*mCommand, "height",
			"Tunnel floor height",
			args::Matcher{ 'h' });
		mApproachLength = std::make_unique<args::ValueFlag<float>>(*mCommand, "length",
			"Approach length in tiles (default: auto-calculated)",
			args::Matcher{ 'l' }, -1.0f);
		mMaxGrade = std::make_unique<args::ValueFlag<float>>(*mCommand, "grade",
			"Maximum grade percentage (default: 8%)",
			args::Matcher{ 'g' }, 8.0f);
		mWidthTiles = std::make_unique<args::ValueFlag<float>>(*mCommand, "width",
			"Approach width in tiles",
			args::Matcher{ 'w' }, 2.0f);
		mCutDepth = std::make_unique<args::ValueFlag<float>>(*mCommand, "cut",
			"Cut depth at portal (default: 3m)",
			args::Matcher{ 'c' }, 3.0f);
	}

	bool ShouldExecute(const args::ArgumentParser& parser) const override {
		return *mCommand && mTunnelHeight;
	}

	void Execute(const args::ArgumentParser& parser) override {
		int startTileX = args::get(*mStartTileX);
		int startTileZ = args::get(*mStartTileZ);
		int endTileX = args::get(*mEndTileX);
		int endTileZ = args::get(*mEndTileZ);
		float tunnelHeight = args::get(*mTunnelHeight);
		float approachLength = args::get(*mApproachLength);
		float maxGrade = args::get(*mMaxGrade);
		float widthTiles = args::get(*mWidthTiles);
		float cutDepth = args::get(*mCutDepth);

		mLogger->WriteLineFormatted(LogLevel::Info,
			"Creating tunnel approaches from (%d,%d) to (%d,%d), height: %.2f, max grade: %.1f%%, cut: %.1fm",
			startTileX, startTileZ, endTileX, endTileZ, tunnelHeight, maxGrade, cutDepth);

		CreateTunnelApproaches(startTileX, startTileZ, endTileX, endTileZ, 
			tunnelHeight, approachLength, maxGrade, widthTiles, cutDepth);
	}

	const char* GetName() const override {
		return "tunnelapproach";
	}

	const char* GetDescription() const override {
		return "Create tunnel approaches with proper drainage and portal cuts";
	}

	const char* GetUsage() const override {
		return "tunnelapproach <startx> <startz> <endx> <endz> --height=<meters> [--length=<tiles>] [--grade=<percent>] [--width=<tiles>] [--cut=<meters>]";
	}

private:
	void CreateTunnelApproaches(int startTileX, int startTileZ, int endTileX, int endTileZ,
		float tunnelHeight, float approachLength, float maxGrade, float widthTiles, float cutDepth) {
		
		// Calculate tunnel direction vector
		int tunnelDx = endTileX - startTileX;
		int tunnelDz = endTileZ - startTileZ;
		float tunnelLength = std::sqrt(static_cast<float>(tunnelDx * tunnelDx + tunnelDz * tunnelDz));
		
		if (tunnelLength == 0) {
			mLogger->WriteLineFormatted(LogLevel::Error, "Tunnel start and end are the same point");
			return;
		}

		// Normalize direction vector
		float dirX = tunnelDx / tunnelLength;
		float dirZ = tunnelDz / tunnelLength;

		// Get terrain heights at tunnel portals
		float startTerrainHeight = GetTileAverageHeight(startTileX, startTileZ);
		float endTerrainHeight = GetTileAverageHeight(endTileX, endTileZ);

		// Calculate portal heights (tunnel height plus clearance for cut)
		float startPortalHeight = tunnelHeight + cutDepth;
		float endPortalHeight = tunnelHeight + cutDepth;

		// Calculate required approach lengths if not specified
		float startApproachLength = approachLength;
		float endApproachLength = approachLength;

		if (approachLength < 0) {
			startApproachLength = CalculateRequiredApproachLength(startTerrainHeight, startPortalHeight, maxGrade);
			endApproachLength = CalculateRequiredApproachLength(endTerrainHeight, endPortalHeight, maxGrade);
		}

		mLogger->WriteLineFormatted(LogLevel::Info,
			"Start terrain: %.2f, End terrain: %.2f, Portal height: %.2f, Start approach: %.1f tiles, End approach: %.1f tiles",
			startTerrainHeight, endTerrainHeight, startPortalHeight, startApproachLength, endApproachLength);

		// Create approach at start (extending backwards from tunnel start)
		int startApproachEndX = startTileX - static_cast<int>(dirX * startApproachLength);
		int startApproachEndZ = startTileZ - static_cast<int>(dirZ * startApproachLength);
		ClampToTerrainBounds(startApproachEndX, startApproachEndZ);
		float startApproachTerrainHeight = GetTileAverageHeight(startApproachEndX, startApproachEndZ);
		CreateSingleApproach(startApproachEndX, startApproachEndZ, startTileX, startTileZ,
			startApproachTerrainHeight, startPortalHeight, widthTiles, "start", true);

		// Create approach at end (extending forwards from tunnel end)
		int endApproachEndX = endTileX + static_cast<int>(dirX * endApproachLength);
		int endApproachEndZ = endTileZ + static_cast<int>(dirZ * endApproachLength);
		ClampToTerrainBounds(endApproachEndX, endApproachEndZ);
		float endApproachTerrainHeight = GetTileAverageHeight(endApproachEndX, endApproachEndZ);
		CreateSingleApproach(endApproachEndX, endApproachEndZ, endTileX, endTileZ,
			endApproachTerrainHeight, endPortalHeight, widthTiles, "end", true);

		// Create portal cuts
		CreatePortalCut(startTileX, startTileZ, tunnelHeight, cutDepth, widthTiles, dirX, dirZ, "start");
		CreatePortalCut(endTileX, endTileZ, tunnelHeight, cutDepth, widthTiles, -dirX, -dirZ, "end");
	}

	float CalculateRequiredApproachLength(float terrainHeight, float portalHeight, float maxGrade) {
		float heightDifference = std::abs(terrainHeight - portalHeight);
		float requiredLength = (heightDifference * 100.0f / maxGrade) / 16.0f; // Convert to tiles
		return std::max(3.0f, std::ceil(requiredLength * 1.2f)); // Minimum 3 tiles, 20% safety margin
	}

	void CreateSingleApproach(int startTileX, int startTileZ, int endTileX, int endTileZ,
		float startHeight, float endHeight, float widthTiles, const char* label, bool addDrainage) {
		
		int tileDx = endTileX - startTileX;
		int tileDz = endTileZ - startTileZ;
		int pathLength = std::max(std::abs(tileDx), std::abs(tileDz));
		
		if (pathLength == 0) return;

		mLogger->WriteLineFormatted(LogLevel::Info,
			"Creating %s approach: (%d,%d) to (%d,%d), %.2fm to %.2fm over %d tiles",
			label, startTileX, startTileZ, endTileX, endTileZ, startHeight, endHeight, pathLength);

		bool slopeInX = std::abs(tileDx) >= std::abs(tileDz);
		float stepX = static_cast<float>(tileDx) / pathLength;
		float stepZ = static_cast<float>(tileDz) / pathLength;
		float heightStep = (endHeight - startHeight) / pathLength;

		// Calculate bounds for refresh
		int minTileX = std::min(startTileX, endTileX);
		int maxTileX = std::max(startTileX, endTileX);
		int minTileZ = std::min(startTileZ, endTileZ);
		int maxTileZ = std::max(startTileZ, endTileZ);

		int widthRadius = static_cast<int>(std::ceil(widthTiles / 2.0f));
		minTileX -= widthRadius + 2; // Extra for drainage
		maxTileX += widthRadius + 2;
		minTileZ -= widthRadius + 2;
		maxTileZ += widthRadius + 2;

		// Apply grading along the approach
		for (int step = 0; step <= pathLength; step++) {
			float currentTileX = startTileX + stepX * step;
			float currentTileZ = startTileZ + stepZ * step;
			float currentHeight = startHeight + heightStep * step;

			int tileX = static_cast<int>(std::round(currentTileX));
			int tileZ = static_cast<int>(std::round(currentTileZ));

			ApplyGradeToTileWidth(tileX, tileZ, currentHeight, widthTiles, slopeInX, heightStep, addDrainage);
		}

		// Refresh the modified area
		int refreshMinX = ClampXToTerrainBounds(minTileX);
		int refreshMinZ = ClampZToTerrainBounds(minTileZ);
		int refreshMaxX = ClampXToTerrainBounds(maxTileX + 1);
		int refreshMaxZ = ClampZToTerrainBounds(maxTileZ + 1);

		Refresh(SC4Rect<int32_t>(refreshMinX, refreshMinZ, refreshMaxX, refreshMaxZ));
	}

	void CreatePortalCut(int portalTileX, int portalTileZ, float tunnelHeight, float cutDepth,
		float widthTiles, float dirX, float dirZ, const char* label) {
		
		mLogger->WriteLineFormatted(LogLevel::Info,
			"Creating %s portal cut at (%d,%d), tunnel height: %.2f, cut depth: %.2f",
			label, portalTileX, portalTileZ, tunnelHeight, cutDepth);

		// Create a wedge-shaped cut extending into the hillside
		int cutLength = 3; // Cut extends 3 tiles into the hillside
		int widthRadius = static_cast<int>(std::ceil(widthTiles / 2.0f));

		for (int step = 0; step < cutLength; step++) {
			float cutTileX = portalTileX + dirX * step;
			float cutTileZ = portalTileZ + dirZ * step;
			
			int tileX = static_cast<int>(std::round(cutTileX));
			int tileZ = static_cast<int>(std::round(cutTileZ));

			// Calculate cut height - deeper at portal, shallower as we go back
			float cutProgress = static_cast<float>(step) / cutLength;
			float currentCutHeight = tunnelHeight + cutDepth * (1.0f - cutProgress);

			// Apply cut across the width
			for (int offset = -widthRadius - 1; offset <= widthRadius + 1; offset++) {
				int targetTileX = tileX + (dirZ == 0 ? offset : 0);
				int targetTileZ = tileZ + (dirX == 0 ? offset : 0);

				if (IsValidTile(targetTileX, targetTileZ)) {
					float currentHeight = GetTileAverageHeight(targetTileX, targetTileZ);
					
					// Only cut if terrain is above tunnel level
					if (currentHeight > currentCutHeight) {
						float targetHeight = std::max(currentCutHeight, currentHeight - cutDepth * (1.0f - cutProgress * 0.5f));
						SetAllCornersToHeight(targetTileX, targetTileZ, targetHeight);
					}
				}
			}
		}
	}

	void ApplyGradeToTileWidth(int centerTileX, int centerTileZ, float baseHeight,
		float widthTiles, bool slopeInX, float heightStep, bool addDrainage) {

		int perpDx = slopeInX ? 0 : 1;
		int perpDz = slopeInX ? 1 : 0;
		int widthRadius = static_cast<int>(std::ceil(widthTiles / 2.0f));

		// Extended radius for drainage ditches
		int drainageRadius = addDrainage ? widthRadius + 2 : widthRadius;

		for (int offset = -drainageRadius; offset <= drainageRadius; offset++) {
			int targetTileX = centerTileX + perpDx * offset;
			int targetTileZ = centerTileZ + perpDz * offset;

			float distanceFromCenter = static_cast<float>(std::abs(offset));
			
			if (distanceFromCenter <= widthTiles / 2.0f) {
				// Main roadway area
				float influence = 1.0f;
				if (widthTiles > 1.0f) {
					influence = 1.0f - (distanceFromCenter / (widthTiles / 2.0f));
					influence = std::max(0.0f, std::min(1.0f, influence));
				}

				ApplyBuildableGradeToTile(targetTileX, targetTileZ, baseHeight,
					influence, slopeInX, heightStep);
			}
			else if (addDrainage && distanceFromCenter <= drainageRadius) {
				// Drainage ditch area - slope away from road
				float ditchDepth = 1.0f; // 1 meter deep ditch
				float ditchHeight = baseHeight - ditchDepth;
				float influence = 0.5f; // Gentle influence for natural look

				ApplyBuildableGradeToTile(targetTileX, targetTileZ, ditchHeight,
					influence, slopeInX, heightStep);
			}
		}
	}

	void ApplyBuildableGradeToTile(int tileX, int tileZ, float baseHeight,
		float influence, bool slopeInX, float heightStep) {

		if (!IsValidTile(tileX, tileZ)) return;

		Vector3 corners[4];
		GetTileCorners(tileX, tileZ, corners);

		Vector3 targetCorners[4];

		if (slopeInX) {
			float leftHeight = baseHeight;
			float rightHeight = baseHeight + heightStep;

			targetCorners[0] = Vector3(tileX, tileZ, leftHeight);
			targetCorners[1] = Vector3(tileX + 1, tileZ, rightHeight);
			targetCorners[2] = Vector3(tileX, tileZ + 1, leftHeight);
			targetCorners[3] = Vector3(tileX + 1, tileZ + 1, rightHeight);
		}
		else {
			float bottomHeight = baseHeight;
			float topHeight = baseHeight + heightStep;

			targetCorners[0] = Vector3(tileX, tileZ, bottomHeight);
			targetCorners[1] = Vector3(tileX + 1, tileZ, bottomHeight);
			targetCorners[2] = Vector3(tileX, tileZ + 1, topHeight);
			targetCorners[3] = Vector3(tileX + 1, tileZ + 1, topHeight);
		}

		for (int i = 0; i < 4; i++) {
			float newHeight = Lerp(corners[i].height, targetCorners[i].height, influence);
			SetAltitudeAtVertex(static_cast<int>(targetCorners[i].x),
				static_cast<int>(targetCorners[i].z), newHeight);
		}
	}

	void SetAllCornersToHeight(int tileX, int tileZ, float height) {
		if (!IsValidTile(tileX, tileZ)) return;

		SetAltitudeAtVertex(tileX, tileZ, height);
		SetAltitudeAtVertex(tileX + 1, tileZ, height);
		SetAltitudeAtVertex(tileX, tileZ + 1, height);
		SetAltitudeAtVertex(tileX + 1, tileZ + 1, height);
	}
};