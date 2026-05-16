#pragma once

#include <cstdint>

class cISC4View3DWin;
class TerrainExtensionsDllDirector;

namespace TerrainCatalogHook
{
    namespace ItemId
    {
        inline constexpr uint32_t Flatten = 0x2099E801;
        inline constexpr uint32_t ConstantGrade = 0x2099E802;
        inline constexpr uint32_t BridgeApproach = 0x2099E803;
        inline constexpr uint32_t BlueprintCapture = 0x2099E804;
        inline constexpr uint32_t BlueprintExport = 0x2099E805;
        inline constexpr uint32_t BlueprintStamp = 0x2099E806;
    }

    bool Install(TerrainExtensionsDllDirector& director);
    void Remove();

    bool IsCustomTerrainItemId(uint32_t itemId);
}
