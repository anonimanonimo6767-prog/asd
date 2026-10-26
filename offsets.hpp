#pragma once
#include <cstdint>

namespace Offsets {
    // version-9ac8595712b447a7 — 2026-10-20
    constexpr uintptr_t DataModel           = 0x05628F28; // FakeDataModel->RealDataModel path
    constexpr uintptr_t FakeDataModelPtr    = 0x08ECA150; // VisualEngine::FakeDataModel pointer
    constexpr uintptr_t FakeDataModelReal   = 504;        // FakeDataModel->RealDataModel

    // DataModel
    constexpr uintptr_t DataModelWorkspace  = 328;
    constexpr uintptr_t DataModelChildren   = 0x70;       // same structure

    // Instance
    constexpr uintptr_t InstanceThis        = 8;
    constexpr uintptr_t InstanceName        = 104;        // NameContainer offset
    constexpr uintptr_t InstanceNameStr     = 0;          // Name inside NameContainer
    constexpr uintptr_t InstanceChildren    = 112;        // ChildrenStart
    constexpr uintptr_t InstanceParent      = 96;

    // Player
    constexpr uintptr_t LocalPlayer         = 280;
    constexpr uintptr_t PlayerCharacter     = 656;        // ModelInstance

    // Humanoid
    constexpr uintptr_t HumanoidHealth      = 388;
    constexpr uintptr_t HumanoidMaxHealth   = 412;
    constexpr uintptr_t HumanoidRootPart    = 1120;

    // BasePart / Primitive
    constexpr uintptr_t PartPrimitive       = 376;
    constexpr uintptr_t PartPosition        = 212;        // Primitive->Position
    constexpr uintptr_t PartSize            = 452;        // Primitive->Size

    // Camera
    constexpr uintptr_t CurrentCamera       = 1184;       // Workspace->CurrentCamera
    constexpr uintptr_t CameraViewMatrix    = 192;        // Camera->Rotation (ViewMatrix)
    constexpr uintptr_t CameraPosition      = 228;        // Camera->Position
}