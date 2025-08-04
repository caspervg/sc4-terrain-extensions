#pragma once
#include "TerrainTool.hpp"
#include <algorithm>

class ConstantGradeTool : public TerrainTool {
private:
	std::unique_ptr<args::Command> mCommand;
	std::unique_ptr<args::Positional<int>> mStartTileX, mStartTileZ, mEndTileX, mEndTileZ;
	std::unique_ptr<args::ValueFlag<float>> mWidthTiles, mGradePercent, mStartHeight, mEndHeight;
	std::unique_ptr<args::ValueFlag<bool>> mAutoHeight;
public:
	ConstantGradeTool(cISTETerrain* terrain) : TerrainTool(terrain) {
	}

	~ConstantGradeTool() override = default;

	void RegisterArguments(args::Group& commands) override {
		mCommand = std::make_unique<args::Command>(commands, "constantgrade",
			"Create path with constant grade");

		mStartTileX = std::make_unique<args::Positional<int>>(*mCommand, "startx", "Start tile X");
		mStartTileZ = std::make_unique<args::Positional<int>>(*mCommand, "startz", "Start tile Z");
		mEndTileX = std::make_unique<args::Positional<int>>(*mCommand, "endx", "End tile X");
		mEndTileZ = std::make_unique<args::Positional<int>>(*mCommand, "endz", "End tile Z");

		mWidthTiles = std::make_unique<args::ValueFlag<float>>(*mCommand, "width",
			"Path width in tiles",
			args::Matcher{ 'w' }, 1.0f);
		mGradePercent = std::make_unique<args::ValueFlag<float>>(*mCommand, "grade",
			"Target grade percentage",
			args::Matcher{ 'g' }, -1000.0f);
		mStartHeight = std::make_unique<args::ValueFlag<float>>(*mCommand, "start",
			"Start height",
			args::Matcher{ 's' }, -1000.0f);
		mEndHeight = std::make_unique<args::ValueFlag<float>>(*mCommand, "end",
			"End height",
			args::Matcher{ 'e' }, -1000.0f);
		mAutoHeight = std::make_unique<args::ValueFlag<bool>>(*mCommand, "auto",
			"Auto-detect heights",
			args::Matcher{ 'a' }, false);
	}

	bool ShouldExecute(const args::ArgumentParser& parser) const override {
		// Check if the command is active and all required arguments are provided
		return *mCommand;
	}

	void Execute(const args::ArgumentParser& parser) override {
		int startTileX = args::get(*mStartTileX);
		int startTileZ = args::get(*mStartTileZ);
		int endTileX = args::get(*mEndTileX);
		int endTileZ = args::get(*mEndTileZ);
		float widthTiles = args::get(*mWidthTiles);
		float gradePercent = args::get(*mGradePercent);
		float startHeight = args::get(*mStartHeight);
		float endHeight = args::get(*mEndHeight);
		bool autoHeight = args::get(*mAutoHeight);

		mLogger->WriteLineFormatted(LogLevel::Info,
			"Creating constant grade path from tile (%d,%d) to (%d,%d), width: %.1f tiles",
			startTileX, startTileZ, endTileX, endTileZ, widthTiles);

		CreateConstantGradePath(startTileX, startTileZ, endTileX, endTileZ,
			widthTiles, gradePercent, startHeight, endHeight, autoHeight);
	}

	const char* GetName() const override {
		return "constantgrade";
	}

	const char* GetDescription() const override {
		return "Create path with constant grade";
	}

	const char* GetUsage() const override {
		return "constantgrade <startx> <startz> <endx> <endz> [--width=<tiles>] [--grade=<percent>] "
			"[--start=<height>] [--end=<height>] [--auto]";
	}
private:
	void CreateConstantGradePath(int startTileX, int startTileZ, int endTileX, int endTileZ, float widthTiles, float gradePercent, float startH, float endH, bool autoHeight) {
		// Determine actual heights
		float actualStartHeight = DetermineStartHeight(startTileX, startTileZ, startH, autoHeight);
		float actualEndHeight = DetermineEndHeight(endTileX, endTileZ, endH, autoHeight,
			actualStartHeight, gradePercent, startTileX, startTileZ);

		// Calculate path parameters
		int tileDx = endTileX - startTileX;
		int tileDz = endTileZ - startTileZ;
		int pathLengthTiles = std::max(abs(tileDx), abs(tileDz)); // Chebyshev distance

		if (pathLengthTiles == 0) {
			mLogger->WriteLineFormatted(LogLevel::Info, "Start and end tiles are the same");
			return;
		}

		// Determine primary slope direction (X or Z)
		bool slopeInX = abs(tileDx) >= abs(tileDz);

		// Calculate step parameters
		float stepX = static_cast<float>(tileDx) / pathLengthTiles;
		float stepZ = static_cast<float>(tileDz) / pathLengthTiles;
		float heightStep = (actualEndHeight - actualStartHeight) / pathLengthTiles;
		float actualGrade = (heightStep / 1.0f) * 100.0f; // Grade per tile

		mLogger->WriteLineFormatted(LogLevel::Info,
			"Path: %d tiles, %.2f height change, %.2f%% grade per tile, %s-direction slope",
			pathLengthTiles, actualEndHeight - actualStartHeight, actualGrade,
			slopeInX ? "X" : "Z");

		// Calculate bounding rectangle for all influenced tiles
		int minTileX = std::min(startTileX, endTileX);
		int maxTileX = std::max(startTileX, endTileX);
		int minTileZ = std::min(startTileZ, endTileZ);
		int maxTileZ = std::max(startTileZ, endTileZ);

		// Expand bounds by width radius
		int widthRadius = static_cast<int>(std::ceil(widthTiles / 2.0f));
		minTileX -= widthRadius;
		maxTileX += widthRadius;
		minTileZ -= widthRadius;
		maxTileZ += widthRadius;

		// Apply grading to each tile along the path
		for (int step = 0; step <= pathLengthTiles; step++) {
			float currentTileX = startTileX + stepX * step;
			float currentTileZ = startTileZ + stepZ * step;
			float currentHeight = actualStartHeight + heightStep * step;

			int tileX = static_cast<int>(std::round(currentTileX));
			int tileZ = static_cast<int>(std::round(currentTileZ));

			if ((heightStep < 0.0f && currentHeight < actualEndHeight) || (heightStep > 0.0f && currentHeight > actualEndHeight)) {
				mLogger->WriteLineFormatted(LogLevel::Info, "currentHeight %f outside of actualEndHeight %f for tile (%d,%d)", currentHeight, actualEndHeight, currentTileX, currentTileZ);
				continue;
			}
			ApplyGradeToTileWidth(tileX, tileZ, currentHeight, widthTiles, slopeInX, heightStep);
		}

		// Create refresh rectangle (convert tile coordinates to vertex coordinates)
		// Each tile spans from (tileX, tileZ) to (tileX+1, tileZ+1) in vertex space
		int refreshMinX = minTileX;
		int refreshMaxX = maxTileX + 1;  // +1 because tile (N,M) includes vertex (N+1,M+1)
		int refreshMinZ = minTileZ;
		int refreshMaxZ = maxTileZ + 1;

		// Clamp to valid terrain bounds
		refreshMinX = ClampXToTerrainBounds(refreshMinX);
		refreshMinZ = ClampZToTerrainBounds(refreshMinZ);
		refreshMaxX = ClampXToTerrainBounds(refreshMaxX);
		refreshMaxZ = ClampZToTerrainBounds(refreshMaxZ);

		mLogger->WriteLineFormatted(LogLevel::Info, "Constant grade path created successfully");

		Refresh(SC4Rect<int32_t>(refreshMinX, refreshMinZ, refreshMaxX, refreshMaxZ));
	}

	float DetermineStartHeight(int tileX, int tileZ, float specifiedHeight, bool autoHeight) {
		if (autoHeight || specifiedHeight < -999.0f) {
			mLogger->WriteLineFormatted(LogLevel::Info, "Determine start height in if: (%d, %d) -> (%f) and autoheight %b", tileX, tileZ, specifiedHeight, autoHeight);
			return GetTileAverageHeight(tileX, tileZ);
		}
		mLogger->WriteLineFormatted(LogLevel::Info, "Determine start height outside if: (%d, %d) -> (%f) and autoheight %b", tileX, tileZ, specifiedHeight, autoHeight);
		return specifiedHeight;
	}

	float DetermineEndHeight(int tileX, int tileZ, float specifiedHeight, bool autoHeight,
		float startHeight, float gradePercent, int startTileX, int startTileZ) {
		if (gradePercent > -999.0f) {
			// Calculate end height based on grade percentage and REAL distance
			int dx = tileX - startTileX;
			int dz = tileZ - startTileZ;
			float horizontalDistanceMeters = std::sqrt(static_cast<float>(dx * dx + dz * dz)) * 16.0f;
			float heightChangeMeters = (gradePercent / 100.0f) * horizontalDistanceMeters;
			return startHeight + heightChangeMeters;
		}

		if (autoHeight || specifiedHeight < -999.0f) {
			return GetTileAverageHeight(tileX, tileZ);
		}
		return specifiedHeight;
	}

	void ApplyGradeToTileWidth(int centerTileX, int centerTileZ, float baseHeight,
		float widthTiles, bool slopeInX, float heightStep) {

		// Calculate perpendicular direction for width
		int perpDx = slopeInX ? 0 : 1;  // If sloping in X, width goes in Z
		int perpDz = slopeInX ? 1 : 0;  // If sloping in Z, width goes in X

		int widthRadius = static_cast<int>(std::ceil(widthTiles / 2.0f));

		for (int offset = -widthRadius; offset <= widthRadius; offset++) {
			int targetTileX = centerTileX + perpDx * offset;
			int targetTileZ = centerTileZ + perpDz * offset;

			float distanceFromCenter = static_cast<float>(abs(offset));
			if (distanceFromCenter <= widthTiles / 2.0f) {

				// Calculate influence (full in center, fade at edges)
				float influence = 1.0f;
				if (widthTiles > 1.0f) {
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

		// Get current corner heights
		Vector3 corners[4];
		GetTileCorners(tileX, tileZ, corners);

		// Calculate target heights for buildable slope
		Vector3 targetCorners[4];

		if (slopeInX) {
			// Slope in X direction: left edge same height, right edge same height
			float leftHeight = baseHeight;
			float rightHeight = baseHeight + heightStep;

			targetCorners[0] = Vector3(tileX, tileZ, leftHeight);         // bottom-left
			targetCorners[1] = Vector3(tileX + 1, tileZ, rightHeight);    // bottom-right
			targetCorners[2] = Vector3(tileX, tileZ + 1, leftHeight);     // top-left
			targetCorners[3] = Vector3(tileX + 1, tileZ + 1, rightHeight);// top-right
		}
		else {
			// Slope in Z direction: bottom edge same height, top edge same height
			float bottomHeight = baseHeight;
			float topHeight = baseHeight + heightStep;

			targetCorners[0] = Vector3(tileX, tileZ, bottomHeight);       // bottom-left
			targetCorners[1] = Vector3(tileX + 1, tileZ, bottomHeight);   // bottom-right
			targetCorners[2] = Vector3(tileX, tileZ + 1, topHeight);      // top-left
			targetCorners[3] = Vector3(tileX + 1, tileZ + 1, topHeight);  // top-right
		}

		// Apply heights with influence blending
		for (int i = 0; i < 4; i++) {
			float newHeight = Lerp(corners[i].height, targetCorners[i].height, influence);
			SetAltitudeAtVertex(static_cast<int>(targetCorners[i].x),
				static_cast<int>(targetCorners[i].z), newHeight);
		}
	}

	float CalculateHorizontalDistance(int x1, int z1, int x2, int z2) {
		float dx = static_cast<float>(x2 - x1);
		float dz = static_cast<float>(z2 - z1);
		return std::sqrt(dx * dx + dz * dz);
	}
};