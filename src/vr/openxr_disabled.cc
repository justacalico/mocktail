#include "mocktail/vr/openxr_probe.h"
#include "mocktail/vr/openxr_backend.h"
#include "mocktail/vr/openxr_preview.h"

namespace mocktail::vr {
OpenXrBackend::~OpenXrBackend() = default;
Status OpenXrBackend::Arm(VrGraphicsApi) {
  return Status::Error(StatusCode::kUnavailable, "OpenXR support is disabled in this build");
}
void OpenXrBackend::Disarm() {}
OpenXrBackend* ActiveVrBackend() { return nullptr; }
void OpenXrBackend::NotePoseApplied(void*, std::uint64_t) {}
ScriptedPoseSample OpenXrBackend::PublishedHeadPose() const { return {}; }


PreviewResult RunOpenXrPreview(const PreviewOptions&) {
  PreviewResult result;
  result.message =
      "This build does not include OpenXR support. Use a VR-enabled build of "
      "Mocktail.";
  return result;
}

bool IsOpenXrSupportCompiled() { return false; }

ProbeReport ProbeOpenXr() {
  ProbeReport report;
  report.status = ProbeStatus::kBuildDisabled;
  report.message =
      "This build does not include OpenXR support. Use a "
      "VR-enabled build of Mocktail.";
  return report;
}

}  // namespace mocktail::vr
