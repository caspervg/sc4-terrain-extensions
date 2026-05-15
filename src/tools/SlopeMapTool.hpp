#pragma once

#include <array>
#include <string>
#include <vector>

class cISTETerrain;
class TerrainSlopeRenderer;

class SlopeMapTool {
public:
    struct CommandResult {
        bool ok{true};
        std::string title{};
        std::string message{};
    };

    SlopeMapTool(cISTETerrain* terrain, TerrainSlopeRenderer& renderer);

    CommandResult ExecuteCommand(const std::vector<std::string>& tokens);
    void SetTerrain(cISTETerrain* terrain);

private:
    [[nodiscard]] std::string BuildHelpText_() const;
    [[nodiscard]] std::string BuildStatusText_() const;
    [[nodiscard]] static std::string FormatFloat_(float value, int precision = 2);
    [[nodiscard]] static std::string ToLower_(std::string value);
    [[nodiscard]] static bool TryParseThresholds_(const std::string& text, std::array<float, 4>& outValues);

private:
    cISTETerrain* terrain_{};
    TerrainSlopeRenderer& renderer_;
};
