#include "CurveLibrary.h"

namespace nsducking
{
namespace
{
float clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

float smoothstep (float value)
{
    const auto x = clamp01 (value);
    return x * x * (3.0f - 2.0f * x);
}
} // namespace

float evaluateCurve (CurvePreset preset, float position)
{
    const auto x = clamp01 (position);

    switch (preset)
    {
        case CurvePreset::classic:
            return clamp01 (std::pow (1.0f - x, 1.9f));

        case CurvePreset::tight:
            return x < 0.32f ? std::pow (1.0f - (x / 0.32f), 0.65f) : 0.0f;

        case CurvePreset::pump:
            return clamp01 (std::pow (1.0f - x, 1.15f) * (0.88f + 0.12f * std::sin (x * 3.14159265f)));

        case CurvePreset::soft:
            return 0.72f * std::pow (1.0f - smoothstep (x), 1.35f);

        case CurvePreset::hard:
            if (x < 0.16f)
                return 1.0f;
            if (x < 0.46f)
                return 1.0f - ((x - 0.16f) / 0.30f);
            return 0.0f;

        case CurvePreset::longRelease:
            return clamp01 (std::pow (1.0f - x, 0.72f));

        case CurvePreset::side:
            return clamp01 (0.92f * std::pow (1.0f - x, 1.45f));

        case CurvePreset::gate:
            if (x < 0.52f)
                return 1.0f;
            if (x < 0.60f)
                return 1.0f - ((x - 0.52f) / 0.08f);
            return 0.0f;

        case CurvePreset::count:
            break;
    }

    return 0.0f;
}

float evaluateVolumeCurve (CurvePreset preset, float position)
{
    return clamp01 (1.0f - evaluateCurve (preset, position));
}

std::array<float, editablePointCount> makeVolumePoints (CurvePreset preset)
{
    std::array<float, editablePointCount> points {};

    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto position = static_cast<float> (i) / static_cast<float> (points.size() - 1);
        points[i] = evaluateVolumeCurve (preset, position);
    }

    return points;
}

std::string_view getPresetName (CurvePreset preset)
{
    const auto index = presetToIndex (preset);
    return presetInfos[static_cast<size_t> (index)].name;
}

CurvePreset presetFromIndex (int index)
{
    if (index < 0 || index >= static_cast<int> (CurvePreset::count))
        return CurvePreset::classic;

    return static_cast<CurvePreset> (index);
}

int presetToIndex (CurvePreset preset)
{
    const auto index = static_cast<int> (preset);
    if (index < 0 || index >= static_cast<int> (CurvePreset::count))
        return 0;

    return index;
}
} // namespace nsducking
