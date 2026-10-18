#!/usr/bin/env python3
"""Read EAGLE pinrefs; report source discrepancies without modifying legacy files."""
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parent.parent
sheet = ET.parse(root / "hardware/OpenTempescope/opentempescope.sch")
nets = []
for net in sheet.findall(".//nets/net"):
    refs = [(p.attrib["part"], p.attrib["pin"]) for p in net.findall(".//pinref")]
    nets.append((net.attrib["name"], refs))
for pin, expected in [("4-PD2(PCINT18/INT0)", ("R3", "2")),
                      ("11-PD5(PCINT21/OC0B/T1)", ("IC2", "5")),
                      ("12-PD6(PCINT22/OC0A/AIN0)", ("IC2", "7")),
                      ("15-OC1A/PCINT1)", ("IC2", "8")),
                      ("17-PB3(MOSI/OC2A/PCINT3)", ("IC2", "6")),
                      ("18-PB5(MISO/PCINT4)", ("IC2", "4"))]:
    found = [(name, refs) for name, refs in nets if ("U$3", pin) in refs]
    assert len(found) == 1 and expected in found[0][1], (pin, found)
    print(f"{pin}: {found[0]}")
assert not any(part == "U$3" and pin.startswith("19-")
               for _, refs in nets for part, pin in refs)
for pin in ("12", "13", "14"):
    print(f"TD output {pin}:", [entry for entry in nets if ("IC2", pin) in entry[1]])
print("PASS: source pinrefs; mist is physical 18, physical 19 is unwired.")
print("Physical 18 is PB4/D12 despite the source symbol saying PB5.")
print("Firmware RGB G/B intent differs from the schematic current paths.")
