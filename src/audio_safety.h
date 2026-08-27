#pragma once

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <string>

inline float sanitizeAudioSample(double sample) {
  if (!std::isfinite(sample)) return 0.0f;
  return static_cast<float>(std::clamp(sample, -1.0, 1.0));
}

inline std::wstring normalizedEndpointName(std::wstring name) {
  std::transform(name.begin(), name.end(), name.begin(),
                 [](wchar_t value) { return static_cast<wchar_t>(std::towlower(value)); });
  return name;
}

inline std::wstring virtualEndpointStem(const std::wstring& name,
                                        const std::wstring& direction) {
  const std::wstring normalized = normalizedEndpointName(name);
  const std::size_t directionPosition = normalized.find(direction);
  if (directionPosition == std::wstring::npos) return {};
  std::wstring stem = normalized.substr(0, directionPosition);
  while (!stem.empty() && std::iswspace(stem.back())) stem.pop_back();
  return stem;
}

inline bool isVirtualAudioEndpoint(const std::wstring& name) {
  const std::wstring normalized = normalizedEndpointName(name);
  return normalized.find(L"vb-audio") != std::wstring::npos ||
         normalized.find(L"voicemeeter") != std::wstring::npos ||
         normalized.find(L"virtual cable") != std::wstring::npos;
}

inline bool isVirtualCableMonitorLoop(const std::wstring& captureName,
                                      const std::wstring& renderName) {
  const std::wstring capture = normalizedEndpointName(captureName);
  const std::wstring render = normalizedEndpointName(renderName);
  if (!isVirtualAudioEndpoint(capture)) return false;
  const std::wstring captureStem = virtualEndpointStem(capture, L" output");
  const std::wstring renderStem = virtualEndpointStem(render, L" input");
  return !captureStem.empty() && captureStem == renderStem;
}
