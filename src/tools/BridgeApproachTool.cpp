#pragma once
#include "TerrainTool.hpp"
#include <algorithm>

class BridgeApproachTool : public TerrainTool {
private:
	std::unique_ptr<args::Command> mCommand{};
	std::unique_ptr<args::Positional<int>> mStartTileX{}, mStartTileZ{}, mEndTileX{}, mEndTileZ{};
	std::unique_ptr<args::ValueFlag<float>> mBridgeHeight{}, mApproachLength{}, mMaxGrade{}, mWidthTiles{};
	std::unique_ptr<args::Flag> mTaper{};

public:
	BridgeApproachTool(cISTETerrain* terrain) : TerrainTool(terrain) {
	}

	~BridgeApproachTool() override = default;

	void RegisterArguments(args::Group& commands) override {
		mCommand = std::make_unique<args::Command>(commands, "bridgeapproach",
			"Create bridge approaches at both ends leading to bridge points");

		mStartTileX = std::make_unique<args::Positional<int>>(*mCommand, "startx", "Bridge start tile X");
		mStartTileZ = std::make_unique<args::Positional<int>>(*mCommand, "startz", "Bridge start tile Z");
		mEndTileX = std::make_unique<args::Positional<int>>(*mCommand, "endx", "Bridge end tile X");
		mEndTileZ = std::make_unique<args::Positional<int>>(*mCommand, "endz", "Bridge end tile Z");

		mBridgeHeight = std::make_unique<args::ValueFlag<float>>(*mCommand, "height",
			"Bridge deck height",
			args::Matcher{ 'h' });
		mApproachLength = std::make_unique<args::ValueFlag<float>>(*mCommand, "length",
			"Approach length in tiles (default: auto-calculated)",
			args::Matcher{ 'l' }, -1.0f);
		mMaxGrade = std::make_unique<args::ValueFlag<float>>(*mCommand, "grade",
			"Maximum grade percentage (default: 6%)",
			args::Matcher{ 'g' }, 6.0f);
		mWidthTiles = std::make_unique<args::ValueFlag<float>>(*mCommand, "width",
			"Approach width in tiles",
			args::Matcher{ 'w' }, 2.0f);
		mTaper = std::make_unique<args::Flag>(*mCommand, "taper",
			"Enable width tapering (default: full width)",
			args::Matcher{ "taper" });
	}

	bool ShouldExecute(const args::ArgumentParser& parser) const override {
		return *mCommand && mBridgeHeight;
	}

	void Execute(const args::ArgumentParser& parser) override {
		int startTileX = args::get(*mStartTileX);
		int startTileZ = args::get(*mStartTileZ);
		int endTileX = args::get(*mEndTileX);
		int endTileZ = args::get(*mEndTileZ);
		float bridgeHeight = args::get(*mBridgeHeight);
		float approachLength = args::get(*mApproachLength);
		float maxGrade = args::get(*mMaxGrade);
		float widthTiles = args::get(*mWidthTiles);

		LOG_INFO("Creating bridge approaches from (%d,%d) to (%d,%d), height: %.2f, max grade: %.1f%%",
			startTileX, startTileZ, endTileX, endTileZ, bridgeHeight, maxGrade);

		bool useTapering = mTaper && *mTaper;  // Only taper if --taper flag is specified
		CreateBridgeApproaches(startTileX, startTileZ, endTileX, endTileZ, 
			bridgeHeight, approachLength, maxGrade, widthTiles, useTapering);
	}

	const char* GetName() const override {
		return "bridgeapproach";
	}

	const char* GetDescription() const override {
		return "Create bridge approaches at both ends leading to bridge points";
	}

	const char* GetUsage() const override {
		return "bridgeapproach <startx> <startz> <endx> <endz> --height=<meters> [--length=<tiles>] [--grade=<percent>] [--width=<tiles>] [--taper]";
	}

	void CreateBridgeApproaches(int startTileX, int startTileZ, int endTileX, int endTileZ,
		float bridgeHeight, float approachLength, float maxGrade, float widthTiles, bool useTapering = false) {
		
		// Calculate bridge direction vector
		int bridgeDx = endTileX - startTileX;
		int bridgeDz = endTileZ - startTileZ;
		float bridgeLength = std::sqrt(static_cast<float>(bridgeDx * bridgeDx + bridgeDz * bridgeDz));
		
		if (bridgeLength == 0) {
			LOG_DEBUG("Bridge start and end are the same point");
			return;
		}

		// Normalize direction vector (from start to end)
		float dirX = bridgeDx / bridgeLength;
		float dirZ = bridgeDz / bridgeLength;
		
		// Calculate approach directions (always pointing away from bridge)
		float startApproachDirX = -dirX;  // Start approach extends opposite to bridge direction
		float startApproachDirZ = -dirZ;
		float endApproachDirX = dirX;     // End approach extends along bridge direction
		float endApproachDirZ = dirZ;
		
		LOG_DEBUG("Bridge vector: ({},{}) -> ({},{}), dx={}, dz={}, length={:.2f}",
			startTileX, startTileZ, endTileX, endTileZ, bridgeDx, bridgeDz, bridgeLength);
		LOG_DEBUG("Normalized direction: ({:.3f}, {:.3f}), Start approach dir: ({:.3f}, {:.3f}), End approach dir: ({:.3f}, {:.3f})",
			dirX, dirZ, startApproachDirX, startApproachDirZ, endApproachDirX, endApproachDirZ);

		// Get terrain heights at bridge ends
		float startTerrainHeight = GetTileAverageHeight(startTileX, startTileZ);
		float endTerrainHeight = GetTileAverageHeight(endTileX, endTileZ);

		float startApproachLength = approachLength;
		float endApproachLength = approachLength;

		if (approachLength < 0) {
			// Calculate required approach lengths if not specified
			startApproachLength = CalculateOptimalApproachLength(startTileX, startTileZ, startApproachDirX, startApproachDirZ, bridgeHeight, maxGrade);
			endApproachLength = CalculateOptimalApproachLength(endTileX, endTileZ, endApproachDirX, endApproachDirZ, bridgeHeight, maxGrade);
		}

		LOG_DEBUG("Start terrain: {:.2f}, End terrain: {:.2f}, Start approach: {:.1f} tiles, End approach: {:.1f} tiles",
			startTerrainHeight, endTerrainHeight, startApproachLength, endApproachLength);

		// Create approach at start (extending away from bridge direction)
		// Calculate ideal end position
		float idealStartEndX = startTileX + (startApproachDirX * startApproachLength);
		float idealStartEndZ = startTileZ + (startApproachDirZ * startApproachLength);
		
		// Get terrain bounds
		uint32_t maxX = mTerrain->CellCountX() - 1;
		uint32_t maxZ = mTerrain->CellCountZ() - 1;
		
		// Clamp to terrain bounds and adjust approach length if needed
		int startApproachEndX = std::max(0, std::min(static_cast<int>(idealStartEndX), static_cast<int>(maxX)));
		int startApproachEndZ = std::max(0, std::min(static_cast<int>(idealStartEndZ), static_cast<int>(maxZ)));
		
		// Recalculate actual approach length based on clamped coordinates
		float actualStartApproachLength = std::sqrt(static_cast<float>(
			(startApproachEndX - startTileX) * (startApproachEndX - startTileX) + 
			(startApproachEndZ - startTileZ) * (startApproachEndZ - startTileZ)));
		
		float startApproachTerrainHeight = GetTileAverageHeight(startApproachEndX, startApproachEndZ);
		
		CreateSingleApproach(startTileX, startTileZ, startApproachEndX, startApproachEndZ,
			bridgeHeight, startApproachTerrainHeight, widthTiles, "start");

		// Create approach at end (extending away from bridge direction)
		// Calculate ideal end position
		float idealEndEndX = endTileX + (endApproachDirX * endApproachLength);
		float idealEndEndZ = endTileZ + (endApproachDirZ * endApproachLength);
		
		// Clamp to terrain bounds and adjust approach length if needed
		int endApproachEndX = std::max(0, std::min(static_cast<int>(idealEndEndX), static_cast<int>(maxX)));
		int endApproachEndZ = std::max(0, std::min(static_cast<int>(idealEndEndZ), static_cast<int>(maxZ)));
		
		// Recalculate actual approach length based on clamped coordinates
		float actualEndApproachLength = std::sqrt(static_cast<float>(
			(endApproachEndX - endTileX) * (endApproachEndX - endTileX) + 
			(endApproachEndZ - endTileZ) * (endApproachEndZ - endTileZ)));
		
		float endApproachTerrainHeight = GetTileAverageHeight(endApproachEndX, endApproachEndZ);
		
		CreateSingleApproach(endTileX, endTileZ, endApproachEndX, endApproachEndZ,
			bridgeHeight, endApproachTerrainHeight, widthTiles, "end", useTapering);
	}

private:
	float CalculateOptimalApproachLength(int bridgeX, int bridgeZ, float dirX, float dirZ,
		float bridgeHeight, float maxGrade) {

		// Start with a reasonable minimum approach length
		float currentLength = 3.0f; // Start with 5 tiles minimum
		const float maxIterations = 10;
		const float convergenceThreshold = 0.5f; // Converge within 0.5 tiles

		for (int iteration = 0; iteration < maxIterations; iteration++) {
			// Calculate where approach would start with current length
			int approachStartX = bridgeX + static_cast<int>(dirX * currentLength);
			int approachStartZ = bridgeZ + static_cast<int>(dirZ * currentLength);
			ClampToTerrainBounds(approachStartX, approachStartZ);

			// Get the ACTUAL terrain height where approach would start
			float actualStartHeight = GetTileAverageHeight(approachStartX, approachStartZ);

			// Calculate required length based on actual height difference
			float requiredLength = CalculateRequiredApproachLength(actualStartHeight, bridgeHeight, maxGrade);

			// Check for convergence
			if (std::abs(requiredLength - currentLength) < convergenceThreshold) {
				LOG_DEBUG("Converged to approach length: {:.1f} tiles (height difference: {:.2f} meters)",
					requiredLength, std::abs(bridgeHeight - actualStartHeight));
				return requiredLength;
			}

			// Update length for next iteration (with damping to prevent oscillation)
			currentLength = currentLength * 0.3f + requiredLength * 0.7f;

			// Ensure minimum approach length
			currentLength = std::max(2.0f, currentLength);
		}

		LOG_ERROR("Approach length calculation didn't converge, using: {:.1f} tiles", currentLength);
		return currentLength;
	}

	float CalculateRequiredApproachLength(float terrainHeight, float bridgeHeight, float maxGrade) {
		float heightDifference = std::abs(bridgeHeight - terrainHeight);
		float requiredLength = (heightDifference * 100.0f / maxGrade) / 16.0f; // Convert to tiles
		return std::max(2.0f, std::ceil(requiredLength * 1.2f)); // Minimum 2 tiles, 20% safety margin
	}

	void CreateSingleApproach(int startTileX, int startTileZ, int endTileX, int endTileZ,
		float startHeight, float endHeight, float widthTiles, const char* label, bool useTapering = false) {
		
		int tileDx = endTileX - startTileX;
		int tileDz = endTileZ - startTileZ;
		int pathLength = std::max(std::abs(tileDx), std::abs(tileDz));
		
		if (pathLength == 0) return;

		LOG_DEBUG("Creating %s approach: (%d,%d) to (%d,%d), %.2fm to %.2fm over %d tiles",
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
		minTileX -= widthRadius;
		maxTileX += widthRadius;
		minTileZ -= widthRadius;
		maxTileZ += widthRadius;

		// Apply grading along the approach
		for (int step = 0; step <= pathLength; step++) {
			float currentTileX = startTileX + stepX * step;
			float currentTileZ = startTileZ + stepZ * step;
			float currentHeight = startHeight + heightStep * step;

			int tileX = static_cast<int>(std::round(currentTileX));
			int tileZ = static_cast<int>(std::round(currentTileZ));

			// For the final tile, don't apply internal gradient to avoid overshooting endpoint height
			if (step == pathLength) {
				ApplyGradeToTileWidth(tileX, tileZ, currentHeight, widthTiles, slopeInX, 0.0f, useTapering);
			} else {
				ApplyGradeToTileWidth(tileX, tileZ, currentHeight, widthTiles, slopeInX, heightStep, useTapering);
			}
		}

		// Refresh the modified area
		int refreshMinX = ClampXToTerrainBounds(minTileX);
		int refreshMinZ = ClampZToTerrainBounds(minTileZ);
		int refreshMaxX = ClampXToTerrainBounds(maxTileX + 1);
		int refreshMaxZ = ClampZToTerrainBounds(maxTileZ + 1);

		Refresh(SC4Rect<int32_t>(refreshMinX, refreshMinZ, refreshMaxX, refreshMaxZ));
		LOG_TRACE("Refreshed terrain between ({}, {}) -> ({}, {})", refreshMinX, refreshMinZ, refreshMaxX, refreshMaxZ);
	}

	void ApplyGradeToTileWidth(int centerTileX, int centerTileZ, float baseHeight,
		float widthTiles, bool slopeInX, float heightStep, bool useTapering = false) {
		LOG_TRACE("Applying grade to ({}, {}) -> {} / {} / {} / {}", centerTileX, centerTileZ, baseHeight, widthTiles, slopeInX, heightStep);


		int perpDx = slopeInX ? 0 : 1;
		int perpDz = slopeInX ? 1 : 0;
		int widthRadius = static_cast<int>(std::ceil(widthTiles / 2.0f));

		for (int offset = -widthRadius; offset <= widthRadius; offset++) {
			int targetTileX = centerTileX + perpDx * offset;
			int targetTileZ = centerTileZ + perpDz * offset;

			float distanceFromCenter = static_cast<float>(std::abs(offset));
			if (distanceFromCenter <= widthTiles / 2.0f) {
				float influence = 1.0f;
				if (useTapering && widthTiles > 1.0f) {
					influence = 1.0f - (distanceFromCenter / (widthTiles / 2.0f));
					influence = std::max(0.0f, std::min(1.0f, influence));
				}

				ApplyBuildableGradeToTile(targetTileX, targetTileZ, baseHeight,
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
};