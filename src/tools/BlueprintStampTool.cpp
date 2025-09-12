#pragma once
#include "TerrainTool.hpp"
#include "BlueprintData.hpp"
#include "cISC4City.h"
#include "cISC4ZoneManager.h"
#include "cISC4LotManager.h"
#include "cISC4NetworkManager.h"
#include "cISC4NetworkTool.h"
#include "cISC4OccupantManager.h"
#include "cISC4Occupant.h"
#include "cISC4NetworkOccupant.h"
#include "cRZAutoRefCount.h"
#include "cISC4ZoneDeveloper.h"
#include "cISC4Demolition.h"
#include "SC4CellRegion.h"
#include "utils/Logger.h"
#include "filters/NetworkOccupantFilter.h"
#include <memory>
#include <unordered_map>

class BlueprintStampTool : public TerrainTool {
  private:
    cISC4City* mCity; // non-owning
    std::unique_ptr<args::Command> mCommand;
    std::unique_ptr<args::Positional<int>> mTX, mTZ; // destination top-left
    std::unique_ptr<args::Flag> mNoZones;
    std::unique_ptr<args::Flag> mNoNetworks;
    std::unique_ptr<args::Flag> mNoLots;
    std::unique_ptr<args::Flag> mClear;
    std::unique_ptr<args::ValueFlag<int>> mNetRotate; // 0/90/180/270 only

    constexpr static uintptr_t kFn_cSC4NetworkOccupant_SetRotationAndFlip = 0x00604df0; // thiscall
    using cSC4NetworkOccupant_SetRotationAndFlip = void(__thiscall*)(void* /*this*/, uint8_t /*rotFlip*/);
  public:
    BlueprintStampTool(cISTETerrain* terrain, cISC4City* city)
        : TerrainTool(terrain), mCity(city) {}

    const char* GetName() const override { return "bps"; }
    const char* GetDescription() const override { return "Stamp last captured blueprint at a target location (zones, lots, networks)"; }
    const char* GetUsage() const override { return "blueprint-stamp <tx> <tz> [--no-zones] [--no-networks] [--no-lots] [--clear] [--net-rotate=<0|90|180|270>]"; }

    void RegisterArguments(args::Group& commands) override {
        mCommand = std::make_unique<args::Command>(commands, GetName(), GetDescription());
        mTX = std::make_unique<args::Positional<int>>(*mCommand, "tx", "Target top-left X");
        mTZ = std::make_unique<args::Positional<int>>(*mCommand, "tz", "Target top-left Z");
        mNoZones = std::make_unique<args::Flag>(*mCommand, "no-zones", "Do not place zones", args::Matcher{"no-zones"});
        mNoNetworks = std::make_unique<args::Flag>(*mCommand, "no-networks", "Do not place networks", args::Matcher{"no-networks"});
        mNoLots = std::make_unique<args::Flag>(*mCommand, "no-lots", "Do not create lots (only zones)", args::Matcher{"no-lots"});
        mClear = std::make_unique<args::Flag>(*mCommand, "clear", "Demolish target area before stamping", args::Matcher{"clear"});
        mNetRotate = std::make_unique<args::ValueFlag<int>>(*mCommand, "net-rotate", "Rotate only networks 0/90/180/270 (zones/lots unrotated)", args::Matcher{"net-rotate"}, 0);
    }

    bool ShouldExecute(const args::ArgumentParser& parser) const override {
        return mCommand && *mCommand; // invoked
    }

    void Execute(const args::ArgumentParser& parser) override {
        if (!mCity) { LOG_ERROR("BlueprintStamp: City pointer null"); return; }
        if (!g_LastCapturedBlueprint.IsValid()) { LOG_ERROR("BlueprintStamp: No valid captured blueprint"); return; }

        const auto& bp = g_LastCapturedBlueprint;
        int tx = args::get(*mTX);
        int tz = args::get(*mTZ);
        int width = bp.width;
        int height = bp.height;

        // Bounds validation for overall rectangle (zones/lots region)
        int32_t cityMaxX = static_cast<int32_t>(mCity->CellCountX()) - 1;
        int32_t cityMaxZ = static_cast<int32_t>(mCity->CellCountZ()) - 1;
        if (tx < 0 || tz < 0 || tx + width - 1 > cityMaxX || tz + height - 1 > cityMaxZ) {
            LOG_ERROR("BlueprintStamp: Target rectangle out of city bounds dest=({}, {}) size={}x{} cityMax=({}, {})", tx, tz, width, height, cityMaxX, cityMaxZ);
            return;
        }

        bool doZones = !*mNoZones;
        bool doNetworks = !*mNoNetworks;
        bool doLots = !*mNoLots && doZones; // lots depend on zones
        bool doClear = *mClear;

        int netRotate = *mNetRotate ? args::get(*mNetRotate) : 0;
        if (!(netRotate == 0 || netRotate == 90 || netRotate == 180 || netRotate == 270)) {
            LOG_ERROR("BlueprintStamp: --net-rotate must be 0,90,180,270");
            return;
        }

        // Count actionable items first (after filtering rules) to early abort if nothing to do
        int candidateParcels = 0;
        if (doZones) {
            for (const auto& p : bp.zoneLotParcels) {
                if (p.zoneType <= 9 && p.zoneType > 0) ++candidateParcels; // skip None(0) and >9 (Military..Plopped)
            }
        }
        int candidateNetworks = (doNetworks ? static_cast<int>(bp.networkPieces.size()) : 0);
        if (candidateParcels == 0 && candidateNetworks == 0) {
            LOG_ERROR("BlueprintStamp: Nothing to stamp (no eligible parcels or networks)");
            return;
        }

        // Optional clearing
        if (doClear) {
            cISC4Demolition* demo = mCity->GetDemolitionUtility();
            if (demo) {
                SC4CellRegion<int32_t> region(tx, tz, tx + width - 1, tz + height - 1, true);
                if (demo->DemolishRegion(true, region, 0, 0, true, nullptr, nullptr, 0, nullptr, 0, 0)) {
                    LOG_INFO("BlueprintStamp: Cleared destination area {}x{} at ({},{})", width, height, tx, tz);
                } else {
                    LOG_WARN("BlueprintStamp: DemolishRegion returned false (continuing)");
                }
            } else {
                LOG_WARN("BlueprintStamp: Demolition utility unavailable; --clear ignored");
            }
        }

        // Acquire managers
        cISC4ZoneManager* zoneMgr = mCity->GetZoneManager();
        cISC4LotManager* lotMgr = mCity->GetLotManager();
        cISC4NetworkManager* netMgr = mCity->GetNetworkManager();
        cISC4OccupantManager* occMgr = mCity->GetOccupantManager();

        if (doZones && !zoneMgr) { LOG_ERROR("BlueprintStamp: ZoneManager unavailable"); doZones = false; doLots = false; }
        if (doLots && !lotMgr) { LOG_WARN("BlueprintStamp: LotManager unavailable; skipping lot creation"); doLots = false; }
        if (doNetworks && !netMgr) { LOG_ERROR("BlueprintStamp: NetworkManager unavailable"); doNetworks = false; }
        if (doNetworks && !occMgr) { LOG_WARN("BlueprintStamp: OccupantManager unavailable; skipping networks"); doNetworks = false; }

        int parcelsProcessed = 0;
        int zonesPlaced = 0;
        int lotCreates = 0;
        int zoneFailures = 0;
        int lotFailures = 0;

        // Stamp zones & lots via parcels
        if (doZones) {
            for (const auto& parcel : bp.zoneLotParcels) {
                if (parcel.zoneType <= 0 || parcel.zoneType > 9) continue; // filter out None or >9 (incl Plopped)
                ++parcelsProcessed;
                int worldX1 = tx + parcel.relX;
                int worldZ1 = tz + parcel.relZ;
                int worldX2 = worldX1 + parcel.width - 1;
                int worldZ2 = worldZ1 + parcel.height - 1;
                if (worldX1 < 0 || worldZ1 < 0 || worldX2 > cityMaxX || worldZ2 > cityMaxZ) {
                    LOG_WARN("BlueprintStamp: Parcel out-of-bounds after translation; skipping");
                    continue;
                }
                SC4CellRegion<int32_t> region(worldX1, worldZ1, worldX2, worldZ2, true);
                auto zt = static_cast<cISC4ZoneManager::ZoneType>(parcel.zoneType);
                auto outZonedCellCount = 0LL;
                auto outErrorCode = 0;
                auto outLotsDemolishedSet = reinterpret_cast<intptr_t>(nullptr);
                LOG_DEBUG("Zone placement: type={} at ({},{})->({},{})", parcel.zoneType, worldX1, worldZ1, worldX2, worldZ2);
                bool ok = zoneMgr->PlaceZone(region, zt, true, true, false, true, false, &outZonedCellCount, &outErrorCode, outLotsDemolishedSet);
                LOG_DEBUG("Zone placed: type={} at ({},{})->({},{}) result={}", parcel.zoneType, worldX1, worldZ1, worldX2, worldZ2, ok);
                if (ok) {
                    zonesPlaced += parcel.width * parcel.height;
                    if (doLots) {
                        cISC4Lot* newLot = nullptr;
                        LOG_DEBUG("Lot creation: type={} at ({},{})->({},{}) facing={} historical={} building={}", parcel.zoneType, worldX1, worldZ1, worldX2, worldZ2, parcel.facing, parcel.isHistorical, parcel.hasBuilding);
                        if (lotMgr->CreateLot(worldX1, worldZ1, parcel.width, parcel.height, parcel.facing, newLot)) {
                            ++lotCreates;
                            LOG_DEBUG("Lot created: type={} at ({},{})->({},{}) facing={} historical={} building={}", parcel.zoneType, worldX1, worldZ1, worldX2, worldZ2, parcel.facing, parcel.isHistorical, parcel.hasBuilding);
                        } else {
                            ++lotFailures;
                            LOG_WARN("BlueprintStamp: CreateLot failed at ({},{})->({},{})", worldX1, worldZ1, worldX2, worldZ2);
                        }
                    }
                } else {
                    ++zoneFailures;
                    LOG_WARN("BlueprintStamp: PlaceZone failed at ({},{})->({},{})", worldX1, worldZ1, worldX2, worldZ2);
                }
            }
        }

        LOG_DEBUG("Starting network placement...");

        // Networks
        int networkPiecesPlaced = 0;
        int networkFailures = 0;
        if (doNetworks && netMgr) {
            std::unordered_map<uint32_t, cISC4NetworkTool*> toolCache;
            toolCache.reserve(8);

            auto rotateCoord = [&](int relX, int relZ, int& outX, int& outZ) {
                switch (netRotate) {
                    case 0: outX = relX; outZ = relZ; break;
                    case 90: outX = height - 1 - relZ; outZ = relX; break;
                    case 180: outX = width - 1 - relX; outZ = height - 1 - relZ; break;
                    case 270: outX = relZ; outZ = width - 1 - relX; break;
                    default: outX = relX; outZ = relZ; break;
                }
            };

            auto *netFilter = new NetworkOccupantFilter(NetworkTypeFlags::AllTransportationNetworks);
            for (const auto& piece : bp.networkPieces) {
                int rX=0, rZ=0; rotateCoord(piece.relX, piece.relZ, rX, rZ);
                int worldX = tx + rX;
                int worldZ = tz + rZ;
                if (worldX < 0 || worldZ < 0 || worldX > cityMaxX || worldZ > cityMaxZ) {
                    ++networkFailures;
                    LOG_WARN("BlueprintStamp: Network piece out-of-bounds after rotation at rel({},{})->world({},{}), skipped", piece.relX, piece.relZ, worldX, worldZ);
                    continue;
                }
                cISC4NetworkTool* tool = nullptr;
                auto it = toolCache.find(piece.networkType);
                if (it == toolCache.end()) {
                    tool = netMgr->GetNetworkTool(static_cast<int32_t>(piece.networkType), true);
                    tool->Init();
                    toolCache[piece.networkType] = tool;
                } else {
                    tool = it->second;
                }
                if (!tool) {
                    ++networkFailures;
                    LOG_WARN("BlueprintStamp: Null network tool for type {}", piece.networkType);
                    continue;
                }
                uint32_t beforeFail = tool->GetFailureState();
                tool->PlaceNetworkOccupantById(worldX, worldZ, piece.pieceId, true, static_cast<cISC4NetworkOccupant::eNetworkType>(piece.networkType));
                if (tool->GetFailureState() != 0 && tool->GetFailureState() != beforeFail) {
                    ++networkFailures;
                    LOG_WARN("BlueprintStamp: Failed to place network piece id={} type={} at ({},{})", piece.pieceId, piece.networkType, worldX, worldZ);
                } else {
                    auto cSC4NetworkOccupant_SetRotationAndFlipFn = reinterpret_cast<cSC4NetworkOccupant_SetRotationAndFlip>(kFn_cSC4NetworkOccupant_SetRotationAndFlip);
                    cSC4NetworkOccupant_SetRotationAndFlipFn(tool, piece.rotationAndFlip);
                    cISC4Occupant* occ = nullptr;
                    if (occMgr->GetFirstOccupantByStandardCityCell(occ, worldX, worldZ, netFilter) && occ) {
                        cRZAutoRefCount<cISC4NetworkOccupant> netOcc;
                        if (occ->QueryInterface(GZIID_cISC4NetworkOccupant, netOcc.AsPPVoid())) {
                            netOcc->SetVariation(piece.variation);
                            LOG_DEBUG("Attempting to set rotationAndFlip={} on network piece at ({},{})", (int)piece.rotationAndFlip, worldX, worldZ);
                            cSC4NetworkOccupant_SetRotationAndFlipFn(netOcc, piece.rotationAndFlip);
                            LOG_DEBUG("Set rotationAndFlip on network piece at ({},{}) to {}", worldX, worldZ, (int)piece.rotationAndFlip);
                            LOG_DEBUG("Properties: {} {} {} {} {}", (int)netOcc->GetRotation(), (int)netOcc->GetFlip(), (int)netOcc->GetRotationAndFlip(), (int)netOcc->GetVariation(), (int)netOcc->PieceId());
                            LOG_DEBUG("More properties: {} {} {}", netOcc->IsPlaced(), netOcc->IsUsable(), netOcc->IsImmovable());
                            netOcc->SetUsable(true);

                        }
                    }
                    ++networkPiecesPlaced;
                }
            }
        }

        LOG_INFO("BlueprintStamp: parcelsProcessed={} zonesPlacedCells={} lotsCreated={} zoneFailures={} lotFailures={} networksPlaced={} networkFailures={} netRotate={}", parcelsProcessed, zonesPlaced, lotCreates, zoneFailures, lotFailures, networkPiecesPlaced, networkFailures, netRotate);
    }
};
