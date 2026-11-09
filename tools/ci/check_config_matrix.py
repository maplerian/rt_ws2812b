#!/usr/bin/env python3
"""Validate WS2812B Kconfig selections and the RT-Thread SConscript source graph.

This is a non-build check. The Kconfig parser resolves dependency/choice
semantics; a constrained SCons API shim observes only the package SConscript.
It does not compile C, link a BSP, or simulate the hardware.
"""

import argparse
import json
import pathlib
import sys
import types
from dataclasses import dataclass, field
from typing import Dict

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILTINS = {
    "SOLID": "solid",
    "BLINK": "blink",
    "BREATHE": "breathe",
    "GRADIENT": "gradient",
    "GRADATION": "gradation",
    "CHASE": "chase",
    "SEQUENCE": "sequence",
}
INITIAL_KINDS = {
    "NONE": 0,
    "SOLID": 1,
    "BLINK": 2,
    "BREATHE": 3,
    "GRADIENT": 4,
    "CHASE": 5,
    "GRADATION": 6,
}


@dataclass(frozen=True)
class Case:
    name: str
    requested: Dict[str, str] = field(default_factory=dict)
    host: Dict[str, str] = field(default_factory=dict)
    expected: Dict[str, str] = field(default_factory=dict)


CASES = {
    case.name: case
    for case in [
        Case("default_static", expected={
            "WS2812B_MEMORY_STATIC": "y", "WS2812B_TIMING_DIRECT": "y",
            "WS2812B_ENCODER_FAST_ONLY": "y", "WS2812B_USING_EFFECT": "n",
            "WS2812B_KCONFIG_DEVICE0": "y", "WS2812B_USING_MSH": "n",
        }),
        Case("dynamic_heap", requested={"WS2812B_MEMORY_DYNAMIC": "y"},
             expected={"WS2812B_MEMORY_DYNAMIC": "y", "WS2812B_MEMORY_STATIC": "n"}),
        Case("timing_table_extended", requested={
            "WS2812B_TIMING_TABLE": "y", "WS2812B_CHECK_EXTENDED": "y"},
             expected={"WS2812B_TIMING_TABLE": "y", "WS2812B_CHECK_EXTENDED": "y"}),
        Case("timing_search_auto", requested={
            "WS2812B_TIMING_SEARCH": "y", "WS2812B_ENCODER_AUTO": "y",
            "WS2812B_ENCODER_SELF_CHECK": "y"},
             expected={"WS2812B_TIMING_SEARCH": "y", "WS2812B_ENCODER_AUTO": "y"}),
        Case("gamma_lut", requested={"WS2812B_USING_GAMMA": "y",
                                     "WS2812B_GAMMA_22": "y", "WS2812B_GAMMA_28": "n"},
             expected={"WS2812B_GAMMA_22": "y", "WS2812B_GAMMA_28": "n",
                       "WS2812B_GAMMA_COMPUTE": "n"}),
        Case("gamma_compute", requested={"WS2812B_USING_GAMMA": "y",
                                         "WS2812B_GAMMA_COMPUTE": "y"},
             expected={"WS2812B_GAMMA_COMPUTE": "y"}),
        Case("effect_solid_only", requested={
            "WS2812B_USING_EFFECT": "y", "WS2812B_EFFECT_BLINK": "n",
            "WS2812B_EFFECT_GRADATION": "n", "WS2812B_EFFECT_SEQUENCE": "n"},
             expected={"WS2812B_EFFECT_SOLID": "y", "WS2812B_EFFECT_BLINK": "n",
                       "WS2812B_USING_MSH": "n"}),
        Case("effect_all_msh", requested={
            "WS2812B_USING_EFFECT": "y", "WS2812B_USING_MSH": "y",
            "WS2812B_EFFECT_BREATHE": "y", "WS2812B_EFFECT_GRADIENT": "y",
            "WS2812B_EFFECT_CHASE": "y", "WS2812B_EFFECT_CUSTOM_SLOTS": "4"},
             expected={"WS2812B_USING_MSH": "y", "WS2812B_EFFECT_CHASE": "y"}),
        Case("three_devices", requested={
            "WS2812B_KCONFIG_SLOT_COUNT": "3", "WS2812B_KCONFIG_DEVICE1": "y",
            "WS2812B_KCONFIG_DEVICE2": "y", "WS2812B_MAX_DEVICES": "3"},
             expected={"WS2812B_KCONFIG_DEVICE0": "y", "WS2812B_KCONFIG_DEVICE1": "y",
                       "WS2812B_KCONFIG_DEVICE2": "y", "WS2812B_MAX_DEVICES": "3"}),
        Case("sparse_slot", requested={
            "WS2812B_KCONFIG_DEVICE0": "n", "WS2812B_KCONFIG_DEVICE2": "y",
            "WS2812B_MAX_DEVICES": "1"},
             expected={"WS2812B_KCONFIG_DEVICE0": "n", "WS2812B_KCONFIG_DEVICE2": "y",
                       "WS2812B_MAX_DEVICES": "1"}),
        Case("capacity_guard", requested={
            "WS2812B_KCONFIG_DEVICE1": "y", "WS2812B_KCONFIG_DEVICE2": "y",
            "WS2812B_MAX_DEVICES": "1", "WS2812B_CHECK_PARAMETERS": "n"},
             expected={"WS2812B_CHECK_PARAMETERS": "n", "WS2812B_KCONFIG_DEVICE2": "y"}),
        Case("no_spi_dependency", host={"RT_USING_SPI": "n"},
             expected={"PKG_USING_WS2812B": "n"}),
        Case("no_heap_dynamic", host={"RT_USING_HEAP": "n"},
             requested={"WS2812B_MEMORY_DYNAMIC": "y"},
             expected={"WS2812B_MEMORY_DYNAMIC": "n", "WS2812B_MEMORY_STATIC": "y"}),
        Case("no_finsh_msh", host={"RT_USING_FINSH": "n"},
             requested={"WS2812B_USING_EFFECT": "y", "WS2812B_USING_MSH": "y"},
             expected={"WS2812B_USING_EFFECT": "y", "WS2812B_USING_MSH": "n"}),
        Case("slot_count_guard", requested={
            "WS2812B_KCONFIG_SLOT_COUNT": "1", "WS2812B_KCONFIG_DEVICE2": "y"},
             expected={"WS2812B_KCONFIG_DEVICE2": "n", "WS2812B_KCONFIG_SLOT_COUNT": "1"}),
        Case("effect_disabled_msh", requested={"WS2812B_USING_MSH": "y"},
             expected={"WS2812B_USING_MSH": "n", "WS2812B_USING_EFFECT": "n"}),
    ]
}


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def resolved(kconf) -> Dict[str, str]:
    return {name: sym.str_value for name, sym in kconf.syms.items() if name}


def apply_kconfig(case: Case):
    # The wrapper defines only the three external RT-Thread dependencies. The
    # package's real Kconfig is parsed by Kconfiglib without text rewriting.
    import kconfiglib
    import tempfile

    with tempfile.TemporaryDirectory(prefix="ws2812b-kconfig-") as tmp:
        wrapper = pathlib.Path(tmp) / "Kconfig"
        package_kconfig = (ROOT / "Kconfig").as_posix().replace('"', '\\"')
        wrapper.write_text(
            'mainmenu "WS2812B package CI"\n'
            'config RT_USING_SPI\n    bool "RT SPI"\n    default y\n'
            'config RT_USING_HEAP\n    bool "RT heap"\n    default y\n'
            'config RT_USING_FINSH\n    bool "RT FinSH"\n    default y\n'
            f'source "{package_kconfig}"\n', encoding="utf-8")
        kconf = kconfiglib.Kconfig(str(wrapper), warn_to_stderr=False)
        for name, value in case.host.items():
            check(name in kconf.syms, f"missing host symbol: {name}")
            kconf.syms[name].set_value(value)
        kconf.syms["PKG_USING_WS2812B"].set_value("y")
        for name, value in case.requested.items():
            check(name in kconf.syms, f"missing Kconfig symbol: {name}")
            kconf.syms[name].set_value(value)
        values = resolved(kconf)
        # Save actual parser output, not a fabricated rtconfig.h. The temporary
        # .config is discarded after the effective symbols are recorded.
        kconf.write_config(str(pathlib.Path(tmp) / ".config"))
        warnings = list(kconf.warnings)
    for name, expected in case.expected.items():
        check(values.get(name) == expected,
              f"{case.name}: {name}: expected {expected}, got {values.get(name)}")
    return values, warnings


def inspect_sconscript(flags: Dict[str, bool]):
    """Execute this repository's trusted SConscript against a minimal SCons shim."""
    info = {}
    module = types.ModuleType("building")
    module.GetCurrentDir = lambda: str(ROOT)
    module.GetDepend = lambda names: all(flags.get(name, False) for name in names)

    def define_group(name, files, **kwargs):
        info["name"] = name
        info["sources"] = list(files)
        info["kwargs"] = kwargs
        return info

    module.DefineGroup = define_group
    module.Return = lambda _: None
    previous = sys.modules.get("building")
    sys.modules["building"] = module
    try:
        code = compile((ROOT / "SConscript").read_bytes(), "SConscript", "exec")
        exec(code, {"__builtins__": __builtins__}, {})
    finally:
        if previous is None:
            del sys.modules["building"]
        else:
            sys.modules["building"] = previous
    check(info.get("name") == "ws2812b", "SConscript did not register the expected group")
    return info


def expected_sources(flags: Dict[str, bool]):
    if not flags.get("PKG_USING_WS2812B", False):
        return set(), set()
    files = {"ws2812.c"}
    libs = set()
    if any(flags.get(key, False) for key in (
        "WS2812B_TIMING_TABLE", "WS2812B_TIMING_SEARCH", "WS2812B_CHECK_EXTENDED")):
        files.add("ws2812_timing.c")
    if flags.get("WS2812B_GAMMA_COMPUTE", False):
        files.add("gamma/ws2812_gamma_compute.c")
        libs.add("m")
    if flags.get("WS2812B_USING_EFFECT", False):
        files.add("effect/ws2812_effect.c")
        for key, source in BUILTINS.items():
            if flags.get("WS2812B_EFFECT_" + key, False):
                files.add("effect/ws2812_effect_" + source + ".c")
        if flags.get("WS2812B_USING_MSH", False):
            files.add("ws2812b_msh.c")
    return files, libs


def validate_resolved(case: Case, values: Dict[str, str]):
    enabled = lambda name: values.get(name) == "y"
    flags = {name: value == "y" for name, value in values.items()}
    info = inspect_sconscript(flags)
    actual = set(info["sources"]) if enabled("PKG_USING_WS2812B") else set()
    actual_libs = set(info["kwargs"].get("LIBS", [])) if actual else set()
    expected, expected_libs = expected_sources(flags)
    check(actual == expected,
          f"{case.name}: SConscript selected {sorted(actual)}, expected {sorted(expected)}")
    check(actual_libs == expected_libs,
          f"{case.name}: libraries {sorted(actual_libs)}, expected {sorted(expected_libs)}")
    for source in actual:
        check((ROOT / source).is_file(), f"missing selected source: {source}")
    check(info["kwargs"].get("depend") == ["PKG_USING_WS2812B"],
          "SConscript must preserve the package dependency")

    if enabled("PKG_USING_WS2812B"):
        enabled_slots = [index for index in range(3) if enabled(f"WS2812B_KCONFIG_DEVICE{index}")]
        slots = int(values["WS2812B_KCONFIG_SLOT_COUNT"])
        capacity = int(values["WS2812B_MAX_DEVICES"])
        check(all(index < slots for index in enabled_slots),
              f"{case.name}: device enabled outside visible slot count")
        check(capacity >= len(enabled_slots),
              f"{case.name}: capacity {capacity} below {len(enabled_slots)} enabled slots")
        check(enabled("WS2812B_MEMORY_STATIC") != enabled("WS2812B_MEMORY_DYNAMIC"),
              f"{case.name}: memory backend is not exclusive")
        check(sum(enabled(key) for key in (
            "WS2812B_TIMING_DIRECT", "WS2812B_TIMING_TABLE", "WS2812B_TIMING_SEARCH")) == 1,
            f"{case.name}: timing choice is not exclusive")
        check(sum(enabled(key) for key in (
            "WS2812B_ENCODER_FAST_ONLY", "WS2812B_ENCODER_AUTO")) == 1,
            f"{case.name}: encoder choice is not exclusive")
        if enabled("WS2812B_USING_EFFECT"):
            for index in enabled_slots:
                prefix = f"WS2812B_DEV{index}_INITIAL_"
                selected = [method for method in INITIAL_KINDS
                            if method != "NONE" and enabled(prefix + method)]
                if enabled(prefix + "NONE"):
                    selected.append("NONE")
                check(len(selected) == 1,
                      f"{case.name}: expected one initial choice for device {index}, got {selected}")
                kind = int(values[prefix + "KIND"])
                check(kind == INITIAL_KINDS[selected[0]],
                      f"{case.name}: device {index} kind {kind} differs from {selected[0]}")
        else:
            check(not enabled("WS2812B_USING_MSH"), "MSH enabled without effects")
    return {"case": case.name, "status": "PASS", "scope": "non-build Kconfig/SConscript",
            "effective": {key: values[key] for key in sorted(values)
                          if key.startswith(("WS2812B_", "PKG_USING_WS2812B", "RT_USING_"))},
            "sources": sorted(actual), "libs": sorted(actual_libs)}


def selftest():
    profiles = [
        {"PKG_USING_WS2812B": True, "WS2812B_TIMING_DIRECT": True,
         "WS2812B_ENCODER_FAST_ONLY": True},
        {"PKG_USING_WS2812B": True, "WS2812B_TIMING_SEARCH": True,
         "WS2812B_GAMMA_COMPUTE": True, "WS2812B_USING_EFFECT": True,
         "WS2812B_EFFECT_SOLID": True, "WS2812B_EFFECT_SEQUENCE": True,
         "WS2812B_USING_MSH": True},
    ]
    for flags in profiles:
        selected = inspect_sconscript(flags)
        expected, libs = expected_sources(flags)
        check(set(selected["sources"]) == expected, "selftest: source graph mismatch")
        check(set(selected["kwargs"].get("LIBS", [])) == libs, "selftest: linker libs mismatch")
    print("PASS: SConscript source graph selftest (no Kconfig parser, no compilation)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=sorted(CASES))
    parser.add_argument("--list-cases", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--report", type=pathlib.Path)
    args = parser.parse_args()
    if args.list_cases:
        print("\n".join(sorted(CASES)))
        return 0
    if args.selftest:
        selftest()
        return 0
    if not args.case:
        parser.error("--case, --list-cases or --selftest is required")
    case = CASES[args.case]
    try:
        values, warnings = apply_kconfig(case)
        result = validate_resolved(case, values)
        result["warnings"] = warnings
    except (AssertionError, ImportError, ValueError, OSError) as exc:
        result = {"case": case.name, "status": "FAIL", "error": str(exc)}
    serialized = json.dumps(result, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(serialized, encoding="utf-8")
    print(serialized)
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
