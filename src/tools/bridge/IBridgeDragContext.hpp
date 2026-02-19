#pragma once
#include <cstdint>
#include <optional>
#include "BridgePlacement.hpp"

class IBridgeDragContext {
public:
    virtual ~IBridgeDragContext() = default;

    virtual int32_t GetDragStartX() const noexcept = 0;
    virtual int32_t GetDragStartZ() const noexcept = 0;
    virtual int32_t GetDragCurrentX() const noexcept = 0;
    virtual int32_t GetDragCurrentZ() const noexcept = 0;

    virtual void SetDragStart(int32_t x, int32_t z) noexcept = 0;
    virtual void SetDragCurrent(int32_t x, int32_t z) noexcept = 0;

    virtual std::
    optional<BridgePlacement> ComputePlacement() const = 0;
};
