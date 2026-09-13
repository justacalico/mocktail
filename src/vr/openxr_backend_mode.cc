#include "mocktail/vr/openxr_backend.h"
#include <cstdlib>
#include <filesystem>
namespace mocktail::vr {
std::string SelectVrRuntimeManifest(const std::string& explicit_manifest,
                                   const std::vector<std::string>& candidates) {
  if (!explicit_manifest.empty()) return explicit_manifest;
  for (const auto& path : candidates) {
    std::error_code error;
    if (std::filesystem::is_regular_file(path, error)) return path;
  }
  return {};
}

std::string VrRuntimeUnavailableHint(bool user_selected, const std::string& manifest) {
  if (user_selected) {
    return "User-selected XR_RUNTIME_JSON '" + manifest +
        "' is unavailable. Check the manifest/library and that its runtime server "
        "is running with a connected headset; this error alone does not distinguish them.";
  }
  if (!manifest.empty()) {
    return "Auto-selected WiVRn runtime is unavailable. Start WiVRn and connect "
        "the headset, then retry. If it remains unavailable, check the runtime installation.";
  }
  return "The registered OpenXR runtime is unavailable. Install/start WiVRn and "
      "connect the headset, or explicitly select an installed runtime with XR_RUNTIME_JSON.";
}

VrBackendMode ResolveVrBackendMode(bool vr_enabled) {
  if (!vr_enabled) {
    return VrBackendMode::kDisabled;
  }
  const char* value = std::getenv("MOCKTAIL_VR_BACKEND");
  const std::string mode = value ? value : "xr";
  if (mode == "native" || mode == "native-stereo") {
    return VrBackendMode::kNativeOnly;
  }
  return VrBackendMode::kXrOutput;
}

const char* VrBackendModeName(VrBackendMode mode) {
  switch (mode) {
    case VrBackendMode::kDisabled:
      return "disabled";
    case VrBackendMode::kNativeOnly:
      return "native-stereo-diagnostic";
    case VrBackendMode::kXrOutput:
      return "openxr-output";
  }
  return "disabled";
}

}
