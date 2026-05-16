#include "ContourMapTool.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

#include "args.hxx"
#include "utils/Logger.h"
#include "viz/TerrainContourRenderer.hpp"

namespace {
std::string MajorLineModeName(const ContourMajorLineMode mode) {
    switch (mode) {
    case ContourMajorLineMode::EveryNth:
        return "every-nth";
    case ContourMajorLineMode::HeightStep:
        return "height-step";
    }

    return "every-nth";
}
}

ContourMapTool::ContourMapTool(cISTETerrain* terrain, TerrainContourRenderer& renderer)
    : terrain_(terrain)
    , renderer_(renderer) {
}

ContourMapTool::CommandResult ContourMapTool::ExecuteCommand(const std::vector<std::string>& tokens) {
    if (!terrain_) {
        return {
            false,
            "Contour error",
            "Terrain is not available."
        };
    }

    if (tokens.empty()) {
        return {
            false,
            "Contour error",
            "No contour command was provided."
        };
    }

    if (tokens.size() == 1) {
        const bool enableNow = !renderer_.IsEnabled();
        renderer_.SetEnabled(enableNow, terrain_);
        LOG_INFO("Contour map {}", enableNow ? "enabled" : "disabled");
        return {};
    }

    if (ToLower_(tokens[1]) == "help") {
        return {
            true,
            "Contour help",
            BuildHelpText_()
        };
    }

    args::ArgumentParser parser("Contour overlay commands");
    args::HelpFlag help(parser, "help", "Show contour help", {"help"});
    args::Command setCommand(parser, "set", "Update contour settings");
    args::Command statusCommand(parser, "status", "Show current contour settings");
    args::Command refreshCommand(parser, "refresh", "Rebuild the contour overlay");
    args::Command resetCommand(parser, "reset", "Restore default contour settings");

    args::ValueFlag<float> intervalMeters(setCommand, "meters",
        "Contour interval in meters", args::Matcher{"interval"});
    args::ValueFlag<float> minorThickness(setCommand, "value",
        "Minor contour thickness in world units", args::Matcher{"minor-thickness"});
    args::ValueFlag<float> majorThickness(setCommand, "value",
        "Major contour thickness in world units", args::Matcher{"major-thickness"});
    args::ValueFlag<float> minorOpacity(setCommand, "alpha",
        "Minor contour opacity in the range 0..1", args::Matcher{"minor-opacity"});
    args::ValueFlag<float> majorOpacity(setCommand, "alpha",
        "Major contour opacity in the range 0..1", args::Matcher{"major-opacity"});
    args::ValueFlag<std::string> majorLinesBy(setCommand, "mode",
        "How major contours are chosen (every-nth|height-step)", args::Matcher{"major-lines-by"});
    args::ValueFlag<int> majorEvery(setCommand, "count",
        "Mark every Nth contour line as major", args::Matcher{"major-every"});
    args::ValueFlag<float> majorHeightStep(setCommand, "meters",
        "Mark contours at absolute height steps as major", args::Matcher{"major-height-step"});

    std::vector<char*> argv;
    argv.reserve(tokens.size());
    for (const auto& token : tokens) {
        argv.push_back(const_cast<char*>(token.c_str()));
    }

    try {
        parser.ParseCLI(static_cast<int>(argv.size()), argv.data());
    } catch (args::Help&) {
        return {
            true,
            "Contour help",
            BuildHelpText_()
        };
    } catch (args::ParseError& e) {
        return {
            false,
            "Contour parse error",
            std::string("Parse error: ") + e.what() + "\n\n" + BuildHelpText_()
        };
    } catch (args::ValidationError& e) {
        return {
            false,
            "Contour validation error",
            std::string("Validation error: ") + e.what() + "\n\n" + BuildHelpText_()
        };
    }

    if (statusCommand) {
        return {
            true,
            "Contour status",
            BuildStatusText_()
        };
    }

    if (refreshCommand) {
        if (renderer_.IsEnabled()) {
            renderer_.Rebuild(terrain_);
            LOG_INFO("Contour map refreshed");
        } else {
            LOG_INFO("Contour refresh skipped because the overlay is OFF");
        }
        return {};
    }

    if (resetCommand) {
        renderer_.ResetConfig(terrain_, true);
        LOG_INFO("Contour settings reset to defaults");
        return {
            true,
            "Contour status",
            BuildStatusText_()
        };
    }

    if (setCommand) {
        const bool hasAnySetting = intervalMeters || minorThickness || majorThickness
            || minorOpacity || majorOpacity || majorLinesBy || majorEvery || majorHeightStep;
        if (!hasAnySetting) {
            return {
                false,
                "Contour validation error",
                "No settings were provided.\n\n" + BuildHelpText_()
            };
        }

        ContourRenderConfig config = renderer_.GetConfig();

        if (intervalMeters) {
            config.intervalMeters = args::get(intervalMeters);
        }
        if (minorThickness) {
            config.minorThickness = args::get(minorThickness);
        }
        if (majorThickness) {
            config.majorThickness = args::get(majorThickness);
        }
        if (minorOpacity) {
            config.minorOpacity = args::get(minorOpacity);
        }
        if (majorOpacity) {
            config.majorOpacity = args::get(majorOpacity);
        }
        if (majorLinesBy) {
            const std::string mode = ToLower_(args::get(majorLinesBy));
            if (mode == "every-nth") {
                config.majorLineMode = ContourMajorLineMode::EveryNth;
            } else if (mode == "height-step") {
                config.majorLineMode = ContourMajorLineMode::HeightStep;
            } else {
                return {
                    false,
                    "Contour validation error",
                    "Invalid --major-lines-by value. Expected every-nth or height-step."
                };
            }
        }

        if (majorEvery) {
            config.majorEvery = args::get(majorEvery);
        }
        if (majorHeightStep) {
            config.majorHeightStepMeters = args::get(majorHeightStep);
        }

        if (majorEvery && config.majorLineMode != ContourMajorLineMode::EveryNth) {
            return {
                false,
                "Contour validation error",
                "--major-every can only be used with --major-lines-by=every-nth."
            };
        }

        if (majorHeightStep && config.majorLineMode != ContourMajorLineMode::HeightStep) {
            return {
                false,
                "Contour validation error",
                "--major-height-step can only be used with --major-lines-by=height-step."
            };
        }

        std::string validationError;
        if (!renderer_.SetConfig(config, terrain_, true, &validationError)) {
            return {
                false,
                "Contour validation error",
                validationError
            };
        }

        LOG_INFO("Contour settings updated: interval={}m minor-thickness={} major-thickness={} minor-opacity={} major-opacity={} major-lines-by={} major-every={} major-height-step={}m",
            config.intervalMeters,
            config.minorThickness,
            config.majorThickness,
            config.minorOpacity,
            config.majorOpacity,
            MajorLineModeName(config.majorLineMode),
            config.majorEvery,
            config.majorHeightStepMeters);

        return {
            true,
            "Contour status",
            BuildStatusText_()
        };
    }

    return {
        false,
        "Contour error",
        BuildHelpText_()
    };
}

void ContourMapTool::SetTerrain(cISTETerrain* terrain) {
    terrain_ = terrain;
}

std::string ContourMapTool::BuildHelpText_() const {
    return
        "contour\n"
        "  Toggle the contour overlay on or off.\n"
        "  Example: contour\n\n"
        "contour set [options]\n"
        "  Update contour settings without toggling.\n"
        "  If the overlay is ON, settings are applied immediately.\n"
        "  Example: contour set --interval=5 --minor-thickness=0.9 --major-thickness=1.7\n\n"
        "contour status\n"
        "  Show the current contour settings.\n"
        "  Example: contour status\n\n"
        "contour refresh\n"
        "  Rebuild the contour overlay using the current settings.\n"
        "  Example: contour refresh\n\n"
        "contour reset\n"
        "  Restore default contour settings.\n"
        "  If the overlay is ON, the defaults are applied immediately.\n"
        "  Example: contour reset\n\n"
        "Options for contour set:\n"
        "  --interval=<meters>\n"
        "      Contour interval in meters. Must be > 0.\n"
        "  --minor-thickness=<value>\n"
        "      Minor contour thickness in world units. Must be > 0.\n"
        "  --major-thickness=<value>\n"
        "      Major contour thickness in world units. Must be > 0.\n"
        "  --minor-opacity=<0..1>\n"
        "      Minor contour opacity. 0 is invisible, 1 is fully opaque.\n"
        "  --major-opacity=<0..1>\n"
        "      Major contour opacity. 0 is invisible, 1 is fully opaque.\n"
        "  --major-lines-by=<every-nth|height-step>\n"
        "      Choose how major contour lines are determined.\n"
        "      every-nth: every Nth contour line is major.\n"
        "      height-step: contours at absolute height steps are major.\n"
        "  --major-every=<n>\n"
        "      Used only with --major-lines-by=every-nth. Must be >= 1.\n"
        "  --major-height-step=<meters>\n"
        "      Used only with --major-lines-by=height-step. Must be > 0.\n\n"
        "Examples:\n"
        "  contour set --minor-opacity=0.72 --major-opacity=0.96\n"
        "  contour set --major-lines-by=every-nth --major-every=5\n"
        "  contour set --major-lines-by=height-step --major-height-step=25\n";
}

std::string ContourMapTool::BuildStatusText_() const {
    const ContourRenderConfig& config = renderer_.GetConfig();

    std::ostringstream oss;
    oss << "Contour map: " << (renderer_.IsEnabled() ? "ON" : "OFF") << '\n'
        << "interval=" << FormatFloat_(config.intervalMeters) << "m\n"
        << "minor-thickness=" << FormatFloat_(config.minorThickness) << '\n'
        << "major-thickness=" << FormatFloat_(config.majorThickness) << '\n'
        << "minor-opacity=" << FormatFloat_(config.minorOpacity) << '\n'
        << "major-opacity=" << FormatFloat_(config.majorOpacity) << '\n'
        << "major-lines-by=" << MajorLineModeName(config.majorLineMode) << '\n'
        << "major-every=" << config.majorEvery << '\n'
        << "major-height-step=" << FormatFloat_(config.majorHeightStepMeters) << "m";
    return oss.str();
}

std::string ContourMapTool::FormatFloat_(const float value, const int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    std::string text = oss.str();

    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }

    return text;
}

std::string ContourMapTool::ToLower_(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
