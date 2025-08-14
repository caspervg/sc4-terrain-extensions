#include "Logger.h"

#include "cISTETerrain.h"
#include "algorithm"
#include "SC4Rect.h"

// template class SC4Rect<uint32_t>;

class SlopeMaker {
private:

	static float SmoothStep(float t) {
		return t * t * (3.0f - 2.0f * t);
	}



	static float CalculateProjectionFactor(const Vector3& lineStart, const Vector3& lineEnd,
		const Vector3& point) {
		float dx = lineEnd.x - lineStart.x;
		float dz = lineEnd.z - lineStart.z;

		if (dx == 0 && dz == 0) return 0.0f;

		float lineLengthSq = dx * dx + dz * dz;
		float dot = (point.x - lineStart.x) * dx + (point.z - lineStart.z) * dz;

		return dot / lineLengthSq;
	}

	static float CalculateDistanceToLine(const Vector3& lineStart, const Vector3& lineEnd,
		const Vector3& point) {
		float dx = lineEnd.x - lineStart.x;
		float dz = lineEnd.z - lineStart.z;

		if (dx == 0 && dz == 0) {
			return point.DistanceTo(lineStart);
		}

		float lineLengthSq = dx * dx + dz * dz;
		float t = (std::max)(0.0f, std::min(1.0f,
			((point.x - lineStart.x) * dx + (point.z - lineStart.z) * dz) / lineLengthSq));

		Vector3 projection(lineStart.x + t * dx, lineStart.z + t * dz, 0);
		return point.DistanceTo(projection);
	}

public:
	static void CreateSmoothSlope(cISTETerrain* terrain, int t1X, int t1Z, int t2X, int t2Z,
		float influence = 3.0f) {
		Logger& logger = Logger::GetInstance();

		// Log input parameters
		logger.WriteLine(LogLevel::Info, "CreateSmoothSlope called with parameters:");
		logger.WriteLineFormatted(LogLevel::Info, "  Start tile: (%d, %d)", t1X, t1Z);
		logger.WriteLineFormatted(LogLevel::Info, "  End tile: (%d, %d)", t2X, t2Z);
		logger.WriteLineFormatted(LogLevel::Info, "  Influence: %.2f", influence);

		Vector3 startVertex = GetTileHighestVertex(terrain, t1X, t1Z);
		Vector3 endVertex = GetTileLowestVertex(terrain, t2X, t2Z);

		// Log calculated vertices
		logger.WriteLineFormatted(LogLevel::Info, "  Start vertex: (%.2f, %.2f, %.2f)", startVertex.x, startVertex.z, startVertex.height);
		logger.WriteLineFormatted(LogLevel::Info, "  End vertex: (%.2f, %.2f, %.2f)", endVertex.x, endVertex.z, endVertex.height);

		int minX = static_cast<int>(std::min(startVertex.x, endVertex.x) - influence);
		int maxX = static_cast<int>(std::max(startVertex.x, endVertex.x) + influence) + 1;
		int minZ = static_cast<int>(std::min(startVertex.z, endVertex.z) - influence);
		int maxZ = static_cast<int>(std::max(startVertex.z, endVertex.z) + influence) + 1;

		minX = std::max(0, minX);
		maxX = std::min(terrain->CellCountX(), (uint32_t)maxX);
		minZ = std::max(0, minZ);
		maxZ = std::min(terrain->CellCountZ(), (uint32_t)maxZ);

		float totalDistance = startVertex.DistanceTo(endVertex);
		if (totalDistance < 0.01f) return;

		for (int z = minZ; z < maxZ; z++) {
			for (int x = minX; x < maxX; x++) {
				Vector3 currentPos(static_cast<float>(x), static_cast<float>(z), 0);

				float t = CalculateProjectionFactor(startVertex, endVertex, currentPos);
				t = std::max(0.0f, std::min(1.0f, t));

				float lineDistance = CalculateDistanceToLine(startVertex, endVertex, currentPos);
				float influenceFactor = std::max(0.0f, 1.0f - (lineDistance / influence));

				if (influenceFactor > 0.0f) {
					float smoothT = SmoothStep(t);
					float targetHeight = Lerp(startVertex.height, endVertex.height, smoothT);

					float currentHeight = terrain->GetAltitudeAtVertex(x, z);
					float newHeight = Lerp(currentHeight, targetHeight, influenceFactor);

					terrain->SetAltitudeAtVertex(x, z, newHeight);
				}
			}
		}

		logger.WriteLineFormatted(LogLevel::Info, "SmoothSlope done");


		SC4Rect<int32_t> refreshRect(minX, minZ, maxX, maxZ);
		terrain->RedisplayTerrain(true, true, refreshRect, 0);
		logger.WriteLineFormatted(LogLevel::Info, "Terrain refreshed in rectangle");
	}
};