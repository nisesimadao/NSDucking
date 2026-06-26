#include "CurveLibrary.h"

#include <cassert>

int main()
{
    for (const auto& info : nsducking::presetInfos)
    {
        const auto start = nsducking::evaluateCurve (info.preset, 0.0f);
        const auto middle = nsducking::evaluateCurve (info.preset, 0.5f);
        const auto end = nsducking::evaluateCurve (info.preset, 1.0f);

        assert (start >= 0.0f && start <= 1.0f);
        assert (middle >= 0.0f && middle <= 1.0f);
        assert (end >= 0.0f && end <= 1.0f);
        assert (start >= end);
    }

    assert (nsducking::presetToIndex (nsducking::CurvePreset::classic) == 0);
    assert (nsducking::presetFromIndex (-1) == nsducking::CurvePreset::classic);
    assert (nsducking::presetFromIndex (999) == nsducking::CurvePreset::classic);

    return 0;
}
