#pragma once

#include <cstdint>
#include <string>

namespace mspa {

constexpr uint8_t kSyncByte = 0xA5;

// Optional model features should stay disabled by default for MSpa Mist.
struct FeatureFlags {
  bool uvc_enabled = false;
  bool ozone_enabled = false;
};

enum class ParseStatus {
  kOk,
  kInvalidLength,
  kInvalidSync,
  kInvalidChecksum,
};

enum class Command : uint8_t {
  kUnknown = 0x00,
  kHeater = 0x01,
  kFilter = 0x02,
  kBubbles = 0x03,
  kTargetTemperature = 0x04,
  kCurrentTemperature = 0x06,
  kBathStatus = 0x08,
  kReset = 0x0B,
  kJet = 0x0D,
  kOzone = 0x0E,
  kUvc = 0x15,
  kHeartbeat = 0x16,
};

struct Frame {
  uint8_t sync = kSyncByte;
  uint8_t command = 0;
  uint8_t value = 0;
  uint8_t checksum = 0;
};

struct DecodedFrame {
  ParseStatus status = ParseStatus::kInvalidLength;
  Frame frame{};
  Command command_name = Command::kUnknown;
  std::string note;
};

uint8_t CalculateChecksum(uint8_t sync, uint8_t command, uint8_t value);

bool IsKnownCommand(uint8_t command, const FeatureFlags& flags);

Command ToCommand(uint8_t command, const FeatureFlags& flags);

DecodedFrame ParseFrame(const uint8_t* data, size_t len,
                       const FeatureFlags& flags = FeatureFlags{});

Frame BuildFrame(Command command, uint8_t value,
                 const FeatureFlags& flags = FeatureFlags{});

}  // namespace mspa
