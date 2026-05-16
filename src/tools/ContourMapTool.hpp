#pragma once

#include <string>
#include <vector>

class cISTETerrain;
class TerrainContourRenderer;

class ContourMapTool {
public:
    struct CommandResult {
        bool ok{true};
        std::string title{};
        std::string message{};
    };

    ContourMapTool(cISTETerrain* terrain, TerrainContourRenderer& renderer);

    CommandResult ExecuteCommand(const std::vector<std::string>& tokens);
    void SetTerrain(cISTETerrain* terrain);

private:
    [[nodiscard]] std::string BuildHelpText_() const;
    [[nodiscard]] std::string BuildStatusText_() const;
    [[nodiscard]] static std::string FormatFloat_(float value, int precision = 2);
    [[nodiscard]] static std::string ToLower_(std::string value);

private:
    cISTETerrain* terrain_{};
    TerrainContourRenderer& renderer_;
};
