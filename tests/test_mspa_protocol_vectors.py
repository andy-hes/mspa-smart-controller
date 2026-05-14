from pathlib import Path
import re


def calc(sync: int, cmd: int, val: int) -> int:
    return (sync + cmd + val) & 0xFF


def test_checksum_vectors_match_protocol_doc():
    assert calc(0xA5, 0x02, 0x01) == 0xA8  # filter on
    assert calc(0xA5, 0x02, 0x00) == 0xA7  # filter off
    assert calc(0xA5, 0x04, 0x28) == 0xD1  # setpoint 40C (x1 variant)


def test_header_contains_optional_feature_flags_disabled_by_default():
    header = Path("firmware/common/mspa_protocol.h").read_text(encoding="utf-8")
    assert "bool uvc_enabled = false;" in header
    assert "bool ozone_enabled = false;" in header


def test_cpp_handles_unknown_command():
    cpp = Path("firmware/common/mspa_protocol.cpp").read_text(encoding="utf-8")
    assert "unknown_command" in cpp
    assert re.search(r"Command::kUnknown", cpp)
