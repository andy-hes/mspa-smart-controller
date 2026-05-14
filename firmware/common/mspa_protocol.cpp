#include "mspa_protocol.h"

namespace mspa {

uint8_t CalculateChecksum(uint8_t sync, uint8_t command, uint8_t value) {
  return static_cast<uint8_t>((sync + command + value) & 0xFF);
}

bool IsKnownCommand(uint8_t command, const FeatureFlags& flags) {
  switch (command) {
    case static_cast<uint8_t>(Command::kHeater):
    case static_cast<uint8_t>(Command::kFilter):
    case static_cast<uint8_t>(Command::kBubbles):
    case static_cast<uint8_t>(Command::kTargetTemperature):
    case static_cast<uint8_t>(Command::kCurrentTemperature):
    case static_cast<uint8_t>(Command::kBathStatus):
    case static_cast<uint8_t>(Command::kReset):
    case static_cast<uint8_t>(Command::kJet):
    case static_cast<uint8_t>(Command::kHeartbeat):
      return true;
    case static_cast<uint8_t>(Command::kUvc):
      return flags.uvc_enabled;
    case static_cast<uint8_t>(Command::kOzone):
      return flags.ozone_enabled;
    default:
      return false;
  }
}

Command ToCommand(uint8_t command, const FeatureFlags& flags) {
  if (!IsKnownCommand(command, flags)) {
    return Command::kUnknown;
  }
  return static_cast<Command>(command);
}

DecodedFrame ParseFrame(const uint8_t* data, size_t len, const FeatureFlags& flags) {
  DecodedFrame out{};
  if (len != 4 || data == nullptr) {
    out.status = ParseStatus::kInvalidLength;
    out.note = "frame_length_must_be_4";
    return out;
  }

  out.frame = Frame{data[0], data[1], data[2], data[3]};

  if (out.frame.sync != kSyncByte) {
    out.status = ParseStatus::kInvalidSync;
    out.note = "unexpected_sync";
    return out;
  }

  const uint8_t expected = CalculateChecksum(out.frame.sync, out.frame.command, out.frame.value);
  if (out.frame.checksum != expected) {
    out.status = ParseStatus::kInvalidChecksum;
    out.note = "invalid_checksum";
    return out;
  }

  out.status = ParseStatus::kOk;
  out.command_name = ToCommand(out.frame.command, flags);
  out.note = (out.command_name == Command::kUnknown) ? "unknown_command" : "ok";
  return out;
}

Frame BuildFrame(Command command, uint8_t value, const FeatureFlags& flags) {
  const uint8_t command_u8 = static_cast<uint8_t>(command);
  Frame frame{};
  frame.sync = kSyncByte;

  if (!IsKnownCommand(command_u8, flags)) {
    frame.command = static_cast<uint8_t>(Command::kUnknown);
    frame.value = value;
    frame.checksum = CalculateChecksum(frame.sync, frame.command, frame.value);
    return frame;
  }

  frame.command = command_u8;
  frame.value = value;
  frame.checksum = CalculateChecksum(frame.sync, frame.command, frame.value);
  return frame;
}

}  // namespace mspa
