#include <gtest/gtest.h>
#include <openxr/openxr.h>
#include "mocktail/vr/openxr_backend.h"
#include "mocktail/vr/roblox_vr_device_bridge.h"

namespace mocktail::vr {
namespace internal {
struct VrBridgeTestAccess {
  static void Enable(RobloxVrDeviceBridge& bridge) { bridge.active_ = true; }
};
}
struct OpenXrBackendTestAccess {
  template<class T> static T Handle(std::uintptr_t n) { return reinterpret_cast<T>(n); }
  static void Configure(OpenXrBackend& backend) {
    backend.vk_instance_ = Handle<VkInstance>(1);
    backend.vk_device_ = Handle<VkDevice>(2);
    backend.recording_ = true;
    backend.published_pose_.frame = 7;
    backend.published_pose_.valid = true;
  }
  static VkInstance Instance(const OpenXrBackend& b) { return b.vk_instance_; }
  static void Attachment(OpenXrBackend& b, int id, void* owner, VkDevice device) {
    auto image = Handle<VkImage>(id);
    auto& r = b.images_[image];
    r.image = image; r.device = device; r.owner = owner;
    r.width = r.height = 500; r.format = VK_FORMAT_R8G8B8A8_UNORM;
    r.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    b.image_views_[Handle<VkImageView>(id)] = image;
    b.framebuffer_views_[Handle<VkFramebuffer>(id)] = {Handle<VkImageView>(id)};
  }
  static void Pass(OpenXrBackend& b, RobloxVrDeviceBridge& bridge, int eye, int id, void* owner,
                   VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    bridge.OnEyeGetterCall(owner, eye, Handle<void*>(id));
    VkRenderPassBeginInfo info{};
    info.framebuffer = Handle<VkFramebuffer>(id);
    info.renderPass = Handle<VkRenderPass>(99);
    b.render_pass_final_layouts_[info.renderPass] = {layout};
    b.NoteRenderPassBegin(VK_NULL_HANDLE, &info);
  }
  static VkImage Image(const OpenXrBackend& b, int eye) { return b.eyes_[eye].image; }
  static VkImageLayout Layout(const OpenXrBackend& b, int eye) { return b.eyes_[eye].final_layout; }
  static void Invalidate(OpenXrBackend& b) {
    b.InvalidateInFlightPose("test");
  }
  static void SetFrameOpen(OpenXrBackend& b, bool open) { b.frame_open_ = open; }
  static void SetVisibilityLost(OpenXrBackend& b, bool lost) { b.visibility_lost_ = lost; }
  static bool VisibilityLost(const OpenXrBackend& b) { return b.visibility_lost_; }
  static void NotePose(OpenXrBackend& b, void* owner) {
    b.NotePoseApplied(owner, b.published_pose_.frame);
  }
  static std::size_t AppliedPoses(const OpenXrBackend& b) {
    return b.applied_poses_.size();
  }
  static void State(OpenXrBackend& b, XrSessionState state) { b.HandleSessionState(state); }
  static void ReferenceChange(OpenXrBackend& b, std::int64_t time) { b.HandleReferenceSpaceChange(time); }
  static void FrameAt(OpenXrBackend& b, std::uint64_t time) {
    b.frame_open_ = true; b.frame_views_valid_ = true; b.frame_display_time_ = time;
    b.published_pose_.valid = true;
  }
  static bool Recovering(OpenXrBackend& b) { return b.session_recovery_pending_; }
  static VkDevice Device(OpenXrBackend& b) { return b.vk_device_; }
  static std::size_t Resources(OpenXrBackend& b) { return b.images_.size(); }
  static void SeedResource(OpenXrBackend& b) { b.images_[Handle<VkImage>(99)] = {}; }
  static void LatchCanted(OpenXrBackend& b) { b.canted_rejection_logged_ = true; }
  static bool CantedLogged(const OpenXrBackend& b) { return b.canted_rejection_logged_; }
};
using Access = OpenXrBackendTestAccess;
TEST(OpenXrBackendLifecycle, DeviceDestructionPreservesLiveInstance) {
  OpenXrBackend b;
  Access::Configure(b);
  b.NoteDeviceDestroyed(Access::Handle<VkDevice>(3));
  EXPECT_TRUE(b.WantsResourceRecords());
  b.NoteDeviceDestroyed(Access::Handle<VkDevice>(2));
  EXPECT_EQ(Access::Instance(b), Access::Handle<VkInstance>(1));
  EXPECT_FALSE(b.WantsResourceRecords());
  EXPECT_FALSE(b.PublishedHeadPose().valid);
  b.NoteInstanceDestroyed(Access::Handle<VkInstance>(1));
  EXPECT_EQ(Access::Instance(b), VK_NULL_HANDLE);
}
TEST(OpenXrBackendBinding, SwitchesOwnerWithoutWaitingForOldResourcesToDie) {
  OpenXrBackend b;
  RobloxVrDeviceBridge bridge;
  internal::VrBridgeTestAccess::Enable(bridge);
  Access::Configure(b);
  auto* first = Access::Handle<void*>(100);
  auto* second = Access::Handle<void*>(200);
  for (int id = 10; id < 14; ++id)
    Access::Attachment(b, id, id < 12 ? first : second, Access::Handle<VkDevice>(2));
  Access::Pass(b, bridge, 0, 10, first);
  Access::Pass(b, bridge, 1, 11, first);
  ASSERT_TRUE(b.EyeBindingComplete());
  Access::Pass(b, bridge, 0, 12, second);
  EXPECT_FALSE(b.EyeBindingComplete());
  Access::Pass(b, bridge, 1, 13, second);
  EXPECT_TRUE(b.EyeBindingComplete());
  EXPECT_EQ(Access::Image(b, 0), Access::Handle<VkImage>(12));
  EXPECT_EQ(Access::Image(b, 1), Access::Handle<VkImage>(13));
  Access::Pass(b, bridge, 0, 12, second, VK_IMAGE_LAYOUT_GENERAL);
  EXPECT_EQ(Access::Layout(b, 0), VK_IMAGE_LAYOUT_GENERAL);
}
TEST(OpenXrBackendBinding, RejectsAnotherDeviceImages) {
  OpenXrBackend b;
  RobloxVrDeviceBridge bridge;
  internal::VrBridgeTestAccess::Enable(bridge);
  Access::Configure(b);
  auto* owner = Access::Handle<void*>(100);
  Access::Attachment(b, 10, owner, Access::Handle<VkDevice>(3));
  Access::Pass(b, bridge, 0, 10, owner);
  EXPECT_EQ(Access::Image(b, 0), VK_NULL_HANDLE);
  EXPECT_FALSE(b.EyeBindingComplete());
}

TEST(OpenXrBackendLifecycle, FutureRecenterPreservesEarlierFrameAndLateChangeDropsIt) {
  OpenXrBackend b;
  Access::Configure(b);
  Access::FrameAt(b, 100);
  Access::ReferenceChange(b, 200);
  EXPECT_TRUE(b.PublishedHeadPose().valid);
  Access::ReferenceChange(b, 100);
  EXPECT_FALSE(b.PublishedHeadPose().valid);
  Access::FrameAt(b, 300);
  Access::ReferenceChange(b, 200);
  EXPECT_FALSE(b.PublishedHeadPose().valid);
}

TEST(OpenXrBackendLifecycle, SynchronizedVisibilityTransitionUsesActualStateHandler) {
  OpenXrBackend b;
  Access::Configure(b);
  Access::State(b, XR_SESSION_STATE_VISIBLE);
  Access::State(b, XR_SESSION_STATE_SYNCHRONIZED);
  EXPECT_TRUE(Access::VisibilityLost(b));
  EXPECT_FALSE(b.PublishedHeadPose().valid);
  Access::FrameAt(b, 100);
  Access::State(b, XR_SESSION_STATE_VISIBLE);
  EXPECT_FALSE(Access::VisibilityLost(b));
  EXPECT_FALSE(b.PublishedHeadPose().valid);
}

TEST(OpenXrBackendLifecycle, SessionLossKeepsLiveDeviceProvenanceButDeviceDestructionCancelsRecovery) {
  OpenXrBackend b;
  Access::Configure(b);
  Access::SeedResource(b);
  Access::State(b, XR_SESSION_STATE_LOSS_PENDING);
  EXPECT_TRUE(Access::Recovering(b));
  EXPECT_EQ(Access::Device(b), Access::Handle<VkDevice>(2));
  EXPECT_EQ(Access::Resources(b), 1u);
  EXPECT_TRUE(b.WantsResourceRecords());
  EXPECT_FALSE(b.PublishedHeadPose().valid);
  b.NoteDestroyImage(Access::Handle<VkImage>(99));
  EXPECT_EQ(Access::Resources(b), 0u);
  b.NoteDeviceDestroyed(Access::Handle<VkDevice>(2));
  EXPECT_FALSE(Access::Recovering(b));
  EXPECT_EQ(Access::Device(b), VK_NULL_HANDLE);
}

TEST(OpenXrBackendLifecycle, InvalidationDropsPoseAndAppliedRecord) {
  OpenXrBackend b;
  Access::Configure(b);
  Access::SetFrameOpen(b, true);
  auto* owner = Access::Handle<void*>(100);
  Access::NotePose(b, owner);
  ASSERT_EQ(Access::AppliedPoses(b), 1u);
  ASSERT_TRUE(b.PublishedHeadPose().valid);
  // A reference-space change / visibility transition must drop the pose and
  // the per-owner application record so the frame cycle cannot retag the
  // in-flight images with a pose from the previous origin.
  Access::Invalidate(b);
  EXPECT_FALSE(b.PublishedHeadPose().valid);
  EXPECT_EQ(Access::AppliedPoses(b), 0u);
}

TEST(OpenXrBackendLifecycle, VisibilityLostStateSurvivesUntilSessionReset) {
  OpenXrBackend b;
  Access::Configure(b);
  Access::SetVisibilityLost(b, true);
  EXPECT_TRUE(Access::VisibilityLost(b));
  // Teardown (device destroyed) resets the flag so a recreated session starts
  // visible; it must not inherit IDLE state from the previous session.
  b.NoteDeviceDestroyed(Access::Handle<VkDevice>(2));
  EXPECT_FALSE(Access::VisibilityLost(b));
}

TEST(OpenXrBackendLifecycle, TeardownClearsCantedRejectionLatch) {
  OpenXrBackend b;
  Access::Configure(b);
  Access::LatchCanted(b);
  ASSERT_TRUE(Access::CantedLogged(b));
  b.NoteDeviceDestroyed(Access::Handle<VkDevice>(2));
  EXPECT_FALSE(Access::CantedLogged(b));
}
}  // namespace mocktail::vr
