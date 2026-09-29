#!/usr/bin/env python3
"""
Validation script for sample OpenConfig Optical Amplifier XML files.
Verifies XML syntax, namespace conformance, leaf types, fraction-digits constraints,
and config-state mirroring against openconfig-optical-amplifier.yang.
"""

import os
import re
import xml.etree.ElementTree as ET

NAMESPACE_NETCONF = "urn:ietf:params:xml:ns:netconf:base:1.0"
NAMESPACE_OPT_AMP = "http://openconfig.net/yang/optical-amplfier"

EXPECTED_CONFIG_LEAVES = [
    "name", "type", "target-gain", "min-gain", "max-gain",
    "target-gain-tilt", "gain-range", "amp-mode",
    "target-output-power", "max-output-power", "enabled"
]

TELEMETRY_CONTAINERS = [
    "actual-gain", "actual-gain-tilt", "input-power-total",
    "input-power-c-band", "output-power-total", "output-power-c-band",
    "laser-bias-current", "optical-return-loss"
]

DECIMAL64_PATTERN = re.compile(r"^-?\d+\.\d{2}$")

def validate_xml(file_path):
    print(f"[*] Validating {os.path.basename(file_path)}...")
    tree = ET.parse(file_path)
    root = tree.getroot()

    # Check root tag and namespace
    if root.tag != f"{{{NAMESPACE_NETCONF}}}config":
        raise ValueError(f"Root tag must be {{{NAMESPACE_NETCONF}}}config, found: {root.tag}")

    amp_top = root.find(f"{{{NAMESPACE_OPT_AMP}}}optical-amplifier")
    if amp_top is None:
        raise ValueError("Missing <optical-amplifier> with correct namespace")

    amplifiers = amp_top.find(f"{{{NAMESPACE_OPT_AMP}}}amplifiers")
    if amplifiers is None:
        raise ValueError("Missing <amplifiers> container")

    amp = amplifiers.find(f"{{{NAMESPACE_OPT_AMP}}}amplifier")
    if amp is None:
        raise ValueError("Missing <amplifier> entry")

    key_name = amp.find(f"{{{NAMESPACE_OPT_AMP}}}name")
    if key_name is None or not key_name.text:
        raise ValueError("Missing list key <name>")

    config = amp.find(f"{{{NAMESPACE_OPT_AMP}}}config")
    if config is None:
        raise ValueError("Missing <config> container")

    state = amp.find(f"{{{NAMESPACE_OPT_AMP}}}state")
    if state is None:
        raise ValueError("Missing <state> container")

    # Validate config leaves
    cfg_name = config.find(f"{{{NAMESPACE_OPT_AMP}}}name")
    if cfg_name is None or cfg_name.text != key_name.text:
        raise ValueError("config/name does not match amplifier key name")

    for leaf in EXPECTED_CONFIG_LEAVES:
        elem = config.find(f"{{{NAMESPACE_OPT_AMP}}}{leaf}")
        if elem is None:
            raise ValueError(f"Missing required leaf <config/{leaf}>")
        # Check mirrored in state
        st_elem = state.find(f"{{{NAMESPACE_OPT_AMP}}}{leaf}")
        if st_elem is None:
            raise ValueError(f"Leaf <{leaf}> in config is NOT mirrored in <state>")
        if st_elem.text != elem.text:
            raise ValueError(f"Value mismatch between config/{leaf} ({elem.text}) and state/{leaf} ({st_elem.text})")

    # Check decimal64 2 fraction digits
    decimal_leaves = ["target-gain", "min-gain", "max-gain", "target-gain-tilt", "target-output-power", "max-output-power"]
    for leaf in decimal_leaves:
        val = config.find(f"{{{NAMESPACE_OPT_AMP}}}{leaf}").text
        if not DECIMAL64_PATTERN.match(val):
            raise ValueError(f"Leaf <{leaf}> value '{val}' does not satisfy fraction-digits 2 (e.g. 18.00)")

    # Validate telemetry containers in state
    for tc in TELEMETRY_CONTAINERS:
        cont = state.find(f"{{{NAMESPACE_OPT_AMP}}}{tc}")
        if cont is None:
            raise ValueError(f"Missing telemetry container <state/{tc}>")
        for stat in ["instant", "avg", "min", "max"]:
            st = cont.find(f"{{{NAMESPACE_OPT_AMP}}}{stat}")
            if st is None or not st.text:
                raise ValueError(f"Missing stat <{stat}> in container <state/{tc}>")
            if not DECIMAL64_PATTERN.match(st.text):
                raise ValueError(f"Stat <{tc}/{stat}> value '{st.text}' does not satisfy fraction-digits 2")

    # Validate ports in state
    for port_leaf in ["ingress-port", "egress-port"]:
        elem = state.find(f"{{{NAMESPACE_OPT_AMP}}}{port_leaf}")
        if elem is None or not elem.text:
            raise ValueError(f"Missing <state/{port_leaf}>")

    print(f"    [+] {os.path.basename(file_path)}: PASSED ALL CHECKS.")

def main():
    dir_path = os.path.dirname(os.path.abspath(__file__))
    files = [f for f in os.listdir(dir_path) if f.endswith(".xml")]
    if not files:
        print("No XML files found to validate.")
        return 1
    for f in sorted(files):
        validate_xml(os.path.join(dir_path, f))
    print(f"\nAll {len(files)} XML files successfully validated against schema requirements!")
    return 0

if __name__ == "__main__":
    exit(main())
