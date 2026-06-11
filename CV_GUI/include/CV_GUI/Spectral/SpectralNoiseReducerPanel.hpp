#pragma once

#include <algorithm>
#include <cstddef>

#if defined(CV_GUI_ENABLE_IMGUI) && CV_GUI_ENABLE_IMGUI
#include "imgui.h"
#endif

namespace CV::GUI::Spectral {

struct SpectralNoiseReducerPanelStyle
{
    const char* title {"Spectral Noise Reducer"};
    const char* inputLabel {"Input"};
    const char* noiseLabel {"Noise"};
    const char* outputLabel {"Output"};
    const char* reductionLabel {"Reduction"};
    float spectrumHeight {180.0f};
    bool showInput {true};
    bool showNoiseProfile {true};
    bool showOutput {true};
    bool showReduction {true};
};

#if defined(CV_GUI_ENABLE_IMGUI) && CV_GUI_ENABLE_IMGUI
namespace Detail {

template<typename Spectrum>
void drawNormalizedCurve(
    const Spectrum& values,
    ImU32 color,
    const ImVec2& origin,
    const ImVec2& size)
{
    const std::size_t count = values.size();
    if (count < 2 || size.x <= 1.0f || size.y <= 1.0f)
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    auto pointForBin = [&](std::size_t index) noexcept {
        const float x = origin.x + (static_cast<float>(index) / static_cast<float>(count - 1)) * size.x;
        const float value = std::clamp(static_cast<float>(values[index]), 0.0f, 1.0f);
        const float y = origin.y + (1.0f - value) * size.y;
        return ImVec2(x, y);
    };

    ImVec2 previous = pointForBin(0);
    for (std::size_t index = 1; index < count; ++index)
    {
        const ImVec2 current = pointForBin(index);
        drawList->AddLine(previous, current, color, 1.5f);
        previous = current;
    }
}

inline void drawLegendItem(ImU32 color, const char* label)
{
    ImGui::SameLine();
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(cursor.x, cursor.y + 4.0f),
        ImVec2(cursor.x + 12.0f, cursor.y + 16.0f),
        color,
        2.0f);
    ImGui::Dummy(ImVec2(16.0f, 18.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
}

} // namespace Detail
#endif

template<typename Snapshot>
void drawSpectralNoiseReducerPanel(
    const Snapshot& snapshot,
    const SpectralNoiseReducerPanelStyle& style = {})
{
#if defined(CV_GUI_ENABLE_IMGUI) && CV_GUI_ENABLE_IMGUI
    ImGui::TextUnformatted(style.title);
    ImGui::Separator();

    ImGui::Text("State: %s%s%s",
                snapshot.learning ? "Learning " : "",
                snapshot.subtracting ? "Subtracting " : "",
                snapshot.profileReady ? "Profile Ready" : "No Profile");
    ImGui::Text("Learned frames: %zu / %zu", snapshot.learnedFrameCount, snapshot.minimumLearnFrames);
    ImGui::ProgressBar(std::clamp(static_cast<float>(snapshot.learnProgress), 0.0f, 1.0f),
                       ImVec2(-1.0f, 0.0f),
                       "Learn progress");
    ImGui::Text("Average reduction: %.2f dB | Sample rate: %.0f Hz",
                static_cast<double>(snapshot.averageReductionDb),
                static_cast<double>(snapshot.sampleRate));

    const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, style.spectrumHeight);
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 background = IM_COL32(16, 18, 24, 255);
    const ImU32 border = IM_COL32(82, 88, 104, 255);
    const ImU32 grid = IM_COL32(50, 55, 68, 130);
    drawList->AddRectFilled(canvasOrigin,
                            ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y),
                            background,
                            6.0f);
    drawList->AddRect(canvasOrigin,
                      ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y),
                      border,
                      6.0f);

    for (int line = 1; line < 4; ++line)
    {
        const float y = canvasOrigin.y + (canvasSize.y * static_cast<float>(line) / 4.0f);
        drawList->AddLine(ImVec2(canvasOrigin.x, y), ImVec2(canvasOrigin.x + canvasSize.x, y), grid, 1.0f);
    }

    if (style.showInput)
        Detail::drawNormalizedCurve(snapshot.inputNormalized, IM_COL32(120, 190, 255, 255), canvasOrigin, canvasSize);
    if (style.showNoiseProfile)
        Detail::drawNormalizedCurve(snapshot.noiseProfileNormalized, IM_COL32(255, 196, 80, 255), canvasOrigin, canvasSize);
    if (style.showOutput)
        Detail::drawNormalizedCurve(snapshot.outputNormalized, IM_COL32(120, 235, 145, 255), canvasOrigin, canvasSize);
    if (style.showReduction)
        Detail::drawNormalizedCurve(snapshot.reductionNormalized, IM_COL32(255, 100, 130, 255), canvasOrigin, canvasSize);

    ImGui::Dummy(canvasSize);

    ImGui::TextUnformatted("Legend:");
    if (style.showInput)
        Detail::drawLegendItem(IM_COL32(120, 190, 255, 255), style.inputLabel);
    if (style.showNoiseProfile)
        Detail::drawLegendItem(IM_COL32(255, 196, 80, 255), style.noiseLabel);
    if (style.showOutput)
        Detail::drawLegendItem(IM_COL32(120, 235, 145, 255), style.outputLabel);
    if (style.showReduction)
        Detail::drawLegendItem(IM_COL32(255, 100, 130, 255), style.reductionLabel);
    ImGui::NewLine();
#else
    (void)snapshot;
    (void)style;
#endif
}

} // namespace CV::GUI::Spectral
