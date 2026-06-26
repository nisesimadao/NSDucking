#pragma once

#include <array>
#include <cmath>
#include <string_view>

namespace nsducking
{
constexpr size_t editablePointCount = 32;

enum class CurvePreset
{
    classic = 0,
    tight,
    pump,
    soft,
    hard,
    longRelease,
    side,
    gate,
    count
};

struct PresetInfo
{
    CurvePreset preset;
    std::string_view name;
};

constexpr std::array<PresetInfo, static_cast<size_t> (CurvePreset::count)> presetInfos {{
    { CurvePreset::classic, "Classic" },
    { CurvePreset::tight, "Tight" },
    { CurvePreset::pump, "Pump" },
    { CurvePreset::soft, "Soft" },
    { CurvePreset::hard, "Hard" },
    { CurvePreset::longRelease, "Long" },
    { CurvePreset::side, "Side" },
    { CurvePreset::gate, "Gate" },
}};

float evaluateCurve (CurvePreset preset, float position);
float evaluateVolumeCurve (CurvePreset preset, float position);
std::array<float, editablePointCount> makeVolumePoints (CurvePreset preset);
std::string_view getPresetName (CurvePreset preset);
CurvePreset presetFromIndex (int index);
int presetToIndex (CurvePreset preset);
} // namespace nsducking
