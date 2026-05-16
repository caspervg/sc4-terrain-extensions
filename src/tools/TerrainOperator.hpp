#pragma once
#include <cmath>

#include "cISTETerrain.h"


class Vector3 {
public:
    float x, z, height;

    Vector3() : x(0.0f), z(0.0f), height(0.0f) {}
    Vector3(const float x_, const float z_, const float h_) : x(x_), z(z_), height(h_) {}
    Vector3(int x_, int z_, float h_) : x(static_cast<float>(x_)), z(static_cast<float>(z_)), height(h_) {}

    [[nodiscard]] float DistanceTo(const Vector3& other) const {
        float dx = x - other.x;
        float dz = z - other.z;
        float dh = height - other.height;
        return std::sqrt(dx * dx + dz * dz + dh * dh);
    }
};

class TerrainOperator {
public:
    explicit TerrainOperator(cISTETerrain* terrain)
        : terrain_(terrain) {};

    virtual ~TerrainOperator() = default;

    TerrainOperator(const TerrainOperator&) = delete;
    TerrainOperator& operator=(const TerrainOperator&) = delete;

protected:
    [[nodiscard]] float GetTileAverageHeight(int tileX, int tileZ) const;
    void GetTileCorners(int tileX, int tileZ, Vector3 corners[4]) const;
    [[nodiscard]] bool IsValidTile(int tileX, int tileZ) const;

    void SetAltitudeAtVertex(int x, int z, float height);

    void Refresh(const SC4Rect<int32_t>& rect);

    void ClampToTerrainBounds(int& x, int& z) const;
    [[nodiscard]] int ClampXToTerrainBounds(int x) const;
    [[nodiscard]] int ClampZToTerrainBounds(int z) const;

    static float Lerp(float a, float b, float t) noexcept {
        return a + (b - a) * t;
    }

protected:
    cISTETerrain* terrain_;
};
