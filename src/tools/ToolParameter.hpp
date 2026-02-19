#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

struct ModifierCombo {
    bool alt = false;
    bool shift = false;
    bool ctrl = false;

    static constexpr auto kAlt = 0x4u;
    static constexpr auto kShift = 0x1u;
    static constexpr auto kCtrl = 0x2u;

    bool Matches(uint32_t modifiers) const noexcept {
        const auto a = (modifiers & kAlt) != 0;
        const auto s = (modifiers & kShift) != 0;
        const auto c = (modifiers & kCtrl) != 0;
        return a == alt && s == shift && c == ctrl;
    }

    std::string ToString() const {
        std::string result;
        if (alt) result += "Alt+";
        if (shift) result += "Shift+";
        if (ctrl) result += "Ctrl+";
        return result.empty() ? "(none)" : result.append("Scroll");
    }
};

struct ParameterDescriptor {
    const char* name = nullptr;
    const char* unit = nullptr;
    ModifierCombo scrollBinding;

    std::function<float()> GetAsFloat;
    std::function<void(int32_t)> AdjustByDelta;

    std::string HintText(bool isActive = false) const {
        std::string text = scrollBinding.ToString()
            + ": " + name
            + " (" + FormatValue_() + unit + ")";
        if (isActive) {
            return "[" + text + "]";
        }
        return text;
    }

private:
    std::string FormatValue_() const {
        const float v = GetAsFloat();
        // Show as integer if the value has no fractional part
        if (v == static_cast<float>(static_cast<int>(v))) {
            return std::to_string(static_cast<int>(v));
        }
        // One decimal place for fractional values
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", v);
        return buf;
    }
};

template <typename T>
struct TypedParameter {
    static_assert(std::is_arithmetic_v<T>, "TypedParameter only supports arithmetic types (float, int, etc.)");

    T value;
    T minValue;
    T maxValue;
    T step;

    constexpr void Validate() noexcept {
        value = std::clamp(value, minValue, maxValue);
    }

    T Adjust(const int32_t delta) noexcept {
        const T deltaStep = (delta > 0) ? step : -step;
        value = std::clamp(value + deltaStep, minValue, maxValue);
        return value;
    }

    void ResetToMid() const noexcept {
        value = minValue + (maxValue - minValue) / T{2};
    }

    ParameterDescriptor Describe(const char* name, const char* unit, const ModifierCombo binding) {
        return ParameterDescriptor{
            .name = name,
            .unit = unit,
            .scrollBinding = binding,
            .GetAsFloat = [this]() { return value; },
            .AdjustByDelta = [this](const int32_t delta) { return Adjust(delta); }
        };
    }
};

class ToolParameterSet {
public:
    void Add(ParameterDescriptor desc) {
        descriptors_.push_back(std::move(desc));
    }

    std::optional<ParameterDescriptor> FindByModifiers(const uint32_t modifiers) {
        for (auto& desc : descriptors_) {
            if (desc.scrollBinding.Matches(modifiers)) {
                return desc;
            }
        }
        return std::nullopt;
    }

    std::optional<const ParameterDescriptor> FindByModifiers(const uint32_t modifiers) const {
        for (const auto& desc : descriptors_) {
            if (desc.scrollBinding.Matches(modifiers)) {
                return desc;
            }
        }
        return std::nullopt;
    }

    std::string BuildHintText(const uint32_t modifiers) const {
        std::string result;
        for (const auto& desc : descriptors_) {
            if (!result.empty()) {
                result += " | ";
            }
            result += desc.HintText(desc.scrollBinding.Matches(modifiers));
        }
        return result;
    }

    const std::vector<ParameterDescriptor>& Descriptors() const noexcept { return descriptors_; }

    bool IsEmpty() const noexcept { return descriptors_.empty(); }

    void Clear() noexcept { descriptors_.clear(); }

    size_t Count() const noexcept { return descriptors_.size(); }

private:
    std::vector<ParameterDescriptor> descriptors_;
};
