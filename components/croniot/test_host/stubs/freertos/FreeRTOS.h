#pragma once

// Minimal host stub for <freertos/FreeRTOS.h>. Provides just enough types
// for Tasks/TaskBase.h to parse on host g++ (no ESP-IDF toolchain). Nothing
// that uses these types is ever invoked from host tests - TaskBase itself
// is never constructed on host, only used as an (always-null) pointer type
// in CommChannel/MessageBus function signatures.
using UBaseType_t = unsigned int;
using TickType_t = unsigned int;
using TaskHandle_t = void*;
