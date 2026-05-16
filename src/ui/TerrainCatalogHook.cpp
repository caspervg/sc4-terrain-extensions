#include "ui/TerrainCatalogHook.hpp"

#include "TerrainExtensionsDllDirector.hpp"

#include "Patcher.h"
#include "cISC4View3DWin.h"
#include "utils/Logger.h"

namespace
{
    using InvokeCatalogItemCommand_t =
        void(__thiscall*)(cISC4View3DWin* thisPtr, uint32_t catalogClassId, uint32_t itemId, char activateTool);

    // Windows SimCity 4.exe: cSC4View3DWin::InvokeCatalogItemCommand
    constexpr uintptr_t kInvokeCatalogItemCommandAddress = 0x007E9FA0;
    constexpr size_t kInvokeCatalogItemCommandStolenBytes = 7;

    InvokeCatalogItemCommand_t sOriginalInvokeCatalogItemCommand = nullptr;
    TerrainExtensionsDllDirector* sDirector = nullptr;

    void __fastcall Hook_InvokeCatalogItemCommand(
        cISC4View3DWin* thisPtr,
        void*,
        uint32_t catalogClassId,
        uint32_t itemId,
        char activateTool)
    {
        LOG_INFO(
            "InvokeCatalogItemCommand(catalogClassId=0x{:08X}, itemId=0x{:08X}, activateTool={})",
            catalogClassId,
            itemId,
            activateTool != 0);

        if (sDirector && TerrainCatalogHook::IsCustomTerrainItemId(itemId)) {
            LOG_INFO(
                "Dispatching custom terrain catalog item 0x{:08X} from catalog class 0x{:08X}",
                itemId,
                catalogClassId);
            if (sDirector->HandleCustomTerrainCatalogItem(itemId, thisPtr, activateTool != 0)) {
                return;
            }
        }

        if (sOriginalInvokeCatalogItemCommand) {
            sOriginalInvokeCatalogItemCommand(thisPtr, catalogClassId, itemId, activateTool);
        }
    }
}

bool TerrainCatalogHook::Install(TerrainExtensionsDllDirector& director)
{
    if (sOriginalInvokeCatalogItemCommand) {
        sDirector = &director;
        return true;
    }

    sDirector = &director;
    sOriginalInvokeCatalogItemCommand = reinterpret_cast<InvokeCatalogItemCommand_t>(
        Patcher::InstallJumpHook(
            kInvokeCatalogItemCommandAddress,
            reinterpret_cast<uintptr_t>(&Hook_InvokeCatalogItemCommand),
            kInvokeCatalogItemCommandStolenBytes));

    const bool installed = sOriginalInvokeCatalogItemCommand != nullptr;
    if (installed) {
        LOG_INFO("Installed terrain catalog hook at 0x{:08X}", static_cast<uint32_t>(kInvokeCatalogItemCommandAddress));
    } else {
        LOG_ERROR("Failed to install terrain catalog hook at 0x{:08X}", static_cast<uint32_t>(kInvokeCatalogItemCommandAddress));
    }

    return installed;
}

void TerrainCatalogHook::Remove()
{
    sDirector = nullptr;
}

bool TerrainCatalogHook::IsCustomTerrainItemId(uint32_t itemId)
{
    switch (itemId) {
    case ItemId::Flatten:
    case ItemId::ConstantGrade:
    case ItemId::BridgeApproach:
    case ItemId::BlueprintCapture:
    case ItemId::BlueprintExport:
    case ItemId::BlueprintStamp:
        return true;
    default:
        return false;
    }
}
