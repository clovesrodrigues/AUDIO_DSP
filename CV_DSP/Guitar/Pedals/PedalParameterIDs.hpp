#ifndef CVDSP_GUITAR_PEDALS_PEDALPARAMETERIDS_HPP
#define CVDSP_GUITAR_PEDALS_PEDALPARAMETERIDS_HPP

/**
 * @file PedalParameterIDs.hpp
 * @brief Stable neutral parameter IDs shared by CV_DSP guitar pedals.
 *
 * Not every pedal exposes every ID. The list intentionally reserves a broad
 * common surface so adapters can map related controls consistently.
 */

#include "../../Manager/ParameterDescriptor.hpp"

namespace cvdsp::guitar::pedals
{

using PedalParameterID = manager::ParameterID;

namespace PedalParameterIDs
{
inline constexpr PedalParameterID Bypass = 1000u;
inline constexpr PedalParameterID InputGain = 1001u;
inline constexpr PedalParameterID Drive = 1002u;
inline constexpr PedalParameterID OutputLevel = 1003u;
inline constexpr PedalParameterID DryWetMix = 1004u;
inline constexpr PedalParameterID PhaseInvert = 1005u;
inline constexpr PedalParameterID Oversampling = 1006u;
inline constexpr PedalParameterID QualityMode = 1007u;
inline constexpr PedalParameterID VoiceMode = 1008u;

inline constexpr PedalParameterID GateEnable = 1020u;
inline constexpr PedalParameterID GateThreshold = 1021u;
inline constexpr PedalParameterID GateRelease = 1022u;

inline constexpr PedalParameterID PreHighPassFrequency = 1040u;
inline constexpr PedalParameterID PreHighPassSlope = 1041u;
inline constexpr PedalParameterID PreLowPassFrequency = 1042u;
inline constexpr PedalParameterID PreBassGain = 1043u;
inline constexpr PedalParameterID PreLowMidFrequency = 1044u;
inline constexpr PedalParameterID PreLowMidGain = 1045u;
inline constexpr PedalParameterID PreLowMidQ = 1046u;
inline constexpr PedalParameterID PreMidFrequency = 1047u;
inline constexpr PedalParameterID PreMidGain = 1048u;
inline constexpr PedalParameterID PreMidQ = 1049u;
inline constexpr PedalParameterID PreTrebleGain = 1050u;
inline constexpr PedalParameterID PrePresenceFrequency = 1051u;
inline constexpr PedalParameterID PrePresenceGain = 1052u;
inline constexpr PedalParameterID PreBoostGain = 1053u;
inline constexpr PedalParameterID PreBoostFrequency = 1054u;
inline constexpr PedalParameterID PreBoostQ = 1055u;

inline constexpr PedalParameterID ClipMode = 1080u;
inline constexpr PedalParameterID ClipBlend = 1081u;
inline constexpr PedalParameterID PositiveThreshold = 1082u;
inline constexpr PedalParameterID NegativeThreshold = 1083u;
inline constexpr PedalParameterID ThresholdLink = 1084u;
inline constexpr PedalParameterID Knee = 1085u;
inline constexpr PedalParameterID Bias = 1086u;
inline constexpr PedalParameterID Asymmetry = 1087u;
inline constexpr PedalParameterID EvenHarmonics = 1088u;
inline constexpr PedalParameterID OddHarmonics = 1089u;
inline constexpr PedalParameterID FoldbackAmount = 1090u;
inline constexpr PedalParameterID RectifyMode = 1091u;
inline constexpr PedalParameterID StageCount = 1092u;
inline constexpr PedalParameterID InterstageGain = 1093u;
inline constexpr PedalParameterID InterstageFrequency = 1094u;
inline constexpr PedalParameterID InterstageQ = 1095u;
inline constexpr PedalParameterID Stage1Drive = 1096u;
inline constexpr PedalParameterID Stage1Softness = 1097u;
inline constexpr PedalParameterID Stage2Drive = 1098u;
inline constexpr PedalParameterID HardThreshold = 1099u;

inline constexpr PedalParameterID Tone = 1120u;
inline constexpr PedalParameterID PostHighPassFrequency = 1121u;
inline constexpr PedalParameterID PostLowPassFrequency = 1122u;
inline constexpr PedalParameterID PostLowPassSlope = 1123u;
inline constexpr PedalParameterID Bass = 1124u;
inline constexpr PedalParameterID Middle = 1125u;
inline constexpr PedalParameterID MidFrequency = 1126u;
inline constexpr PedalParameterID MidQ = 1127u;
inline constexpr PedalParameterID Treble = 1128u;
inline constexpr PedalParameterID Presence = 1129u;
inline constexpr PedalParameterID PresenceFrequency = 1130u;
inline constexpr PedalParameterID FizzCutFrequency = 1131u;
inline constexpr PedalParameterID NotchFrequency = 1132u;
inline constexpr PedalParameterID NotchDepth = 1133u;
inline constexpr PedalParameterID NotchQ = 1134u;
inline constexpr PedalParameterID CabinetEnable = 1135u;
inline constexpr PedalParameterID CabinetMix = 1136u;
inline constexpr PedalParameterID LowMidGain = 1137u;
inline constexpr PedalParameterID LowMidFrequency = 1138u;
inline constexpr PedalParameterID LowMidQ = 1139u;
inline constexpr PedalParameterID HighMidGain = 1140u;
inline constexpr PedalParameterID HighMidFrequency = 1141u;
inline constexpr PedalParameterID HighMidQ = 1142u;
inline constexpr PedalParameterID TightLowCut = 1143u;
} // namespace PedalParameterIDs

} // namespace cvdsp::guitar::pedals

#endif // CVDSP_GUITAR_PEDALS_PEDALPARAMETERIDS_HPP
