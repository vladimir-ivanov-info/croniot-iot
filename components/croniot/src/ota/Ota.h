#pragma once

#include <functional>

// Firmware updates over the air, A/B mode (documentation/decisions/0010,
// documentation/plans/ota.md). The SDK does the whole flow by itself once
// CommonSetup::setup() runs; a project only adds what the SDK cannot know
// about its hardware, through the three hooks below. Call them before
// CommonSetup::setup(). All three are optional.
namespace croniot::ota {

// A check the new firmware must pass before it is confirmed - e.g. "the
// ADC answers". Runs once, on the croniot_ota task, after the firmware has
// been up for CONFIG_CRONIOT_OTA_STABLE_SECONDS. Any check returning false
// rolls the device back to the previous firmware.
void registerSelfTest(const char* name, std::function<bool()> check);

// Return false while an update must not start or restart the device -
// e.g. while the pump is running. The SDK asks again a minute later.
void setUpdateGate(std::function<bool()> canUpdateNow);

// Runs right before every restart the update flow triggers: put the
// hardware in a safe state (pump off, valves closed). Keep it short and
// non-blocking.
void setBeforeRestart(std::function<void()> prepare);

}  // namespace croniot::ota
