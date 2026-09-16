# -*- coding: utf-8 -*-
"""
Batch-rename the Modbus signal varNames inside a Huayan robot config file (*.json).

How it works:
  1. Locate the "modbusClient" key (prefers the subtree under `robot_name`, otherwise
     falls back to the first match anywhere in the file).
  2. Take the "modbus" array and pick the device whose "name" == `modbus_device_name`.
  3. Walk every "group" -> "signal" and find the anchor signal matching BOTH:
        addr    == start_address_num
        varName == start_address_name      (leave "" to match on addr only)
     (both are needed because addr repeats across groups; varName picks the right group)
  4. Starting at that anchor, rename `write_amount` consecutive signals using
     `write_format`, numbering upwards from `start_write_num`.

Name format (`write_format`): `*` marks the number slot.
     "HR*"    -> HR0,   HR1,   ... HR15
     "HR***"  -> HR000, HR001, ... HR015     (number of `*` = zero-padded width)
     "M_3_*"  -> M_3_0, M_3_1, ...
     With no `*`, the number is appended to the end of the string.

Usage:
     python modbus_auto_name.py                 # preview only (dry run), nothing written
     python modbus_auto_name.py --write         # overwrite the file (creates a .bak first)
     python modbus_auto_name.py --device Modbus_2 --addr 0 --name M_2_1 --format "HR***" --amount 16 --write
"""

import argparse
import json
import os
import re
import shutil
import sys

# ---------------------------------------------------------------------------
# CONFIGURATION (every value can be overridden from the command line)
# ---------------------------------------------------------------------------

json_file = "test_crimp_feeding.json"   # config file path (relative to this .py file)

robot_name = "Robot0"
modbus_device_name = "Modbus_3"

start_address_num = 1                   # "addr" of the first signal to rename
start_address_name = "M3_C_1"           # "varName" of the first signal ("" = ignore)

write_format = "HR***"                  # new name format, `*` marks the number slot
start_write_num = 0                     # first number to use
write_amount = 16                       # how many signals to rename

dry_run = True                          # True = preview only, do not write the file
make_backup = True                      # create a .bak copy before overwriting

# ---------------------------------------------------------------------------

FUNC_CODE_LABELS = {
    "0": "0x01 Coil (read/write bit)",
    "1": "0x02 Discrete Input (read bit)",
    "2": "0x03 Holding Register (read/write word)",
    "3": "0x04 Input Register (read word)",
}


def fail(msg):
    print("[ERROR] " + msg)
    sys.exit(1)


def find_modbus_client(root, preferred_robot=None):
    """Find the dict behind the "modbusClient" key, preferring the `preferred_robot` branch."""
    found = []

    def walk(node, path):
        if isinstance(node, dict):
            for key, value in node.items():
                if key == "modbusClient" and isinstance(value, dict):
                    found.append((path + [key], value))
                walk(value, path + [key])
        elif isinstance(node, list):
            for i, item in enumerate(node):
                walk(item, path + ["[%d]" % i])

    walk(root, [])
    if not found:
        return None, None

    if preferred_robot:
        for path, node in found:
            if preferred_robot in path:
                return path, node
    return found[0][0], found[0][1]


def get_devices(modbus_client):
    devices = modbus_client.get("modbus")
    if not isinstance(devices, list):
        fail('No "modbus" array found inside "modbusClient".')
    return devices


def find_device(devices, name):
    for dev in devices:
        if isinstance(dev, dict) and dev.get("name") == name:
            return dev
    fail('Device "%s" not found. Available devices: %s'
         % (name, ", ".join(str(d.get("name")) for d in devices)))


def as_int(value, default=None):
    try:
        return int(str(value).strip())
    except (TypeError, ValueError):
        return default


def find_anchor(device, addr_num, addr_name):
    """Return (group, index of the anchor signal), matched on addr + varName."""
    groups = device.get("group")
    if not isinstance(groups, list):
        fail('Device "%s" has no "group" array.' % device.get("name"))

    matches = []
    for group in groups:
        signals = group.get("signal") or []
        for idx, sig in enumerate(signals):
            if as_int(sig.get("addr")) != addr_num:
                continue
            if addr_name and sig.get("varName") != addr_name:
                continue
            matches.append((group, idx))

    if not matches:
        print('[ERROR] No anchor signal with addr=%s, varName="%s" in device "%s".'
              % (addr_num, addr_name, device.get("name")))
        print("        Groups available in this device:")
        for group in groups:
            signals = group.get("signal") or []
            if not signals:
                continue
            print('          groupId=%s funCode=%s (%s) | addr %s..%s | varName %s .. %s (%d signals)'
                  % (group.get("groupId"), group.get("funCode"),
                     FUNC_CODE_LABELS.get(str(group.get("funCode")), "?"),
                     signals[0].get("addr"), signals[-1].get("addr"),
                     signals[0].get("varName"), signals[-1].get("varName"), len(signals)))
        sys.exit(1)

    if len(matches) > 1:
        print('[WARNING] %d signals match the anchor; using the first one (groupId=%s). '
              'Use a more specific start_address_name.'
              % (len(matches), matches[0][0].get("groupId")))
    return matches[0]


def build_names(fmt, start_num, amount):
    """Build the list of new names from a format string containing `*`."""
    if amount <= 0:
        fail("write_amount must be greater than 0.")

    match = re.search(r"\*+", fmt)
    if match:
        width = len(match.group(0))
        prefix, suffix = fmt[:match.start()], fmt[match.end():]
    else:
        width = 0
        prefix, suffix = fmt, ""

    names = []
    for i in range(amount):
        num = str(start_num + i)
        if width:
            num = num.zfill(width)
        names.append(prefix + num + suffix)

    if len(set(names)) != len(names):
        fail("Format '%s' produces duplicate names." % fmt)
    return names


def collect_varnames(devices):
    """{varName: [location description, ...]} across every device."""
    table = {}
    for dev in devices:
        for group in dev.get("group") or []:
            for sig in group.get("signal") or []:
                name = sig.get("varName")
                if name:
                    table.setdefault(name, []).append(
                        "%s/group %s/addr %s" % (dev.get("name"), group.get("groupId"), sig.get("addr")))
    return table


def find_references(root, names):
    """Find old names still referenced by program scripts / expressions."""
    hits = {}
    pattern = {name: re.compile(r"['\"]%s['\"]" % re.escape(name)) for name in names}

    def walk(node):
        if isinstance(node, dict):
            for key, value in node.items():
                if isinstance(value, str) and key in ("expression", "Content", "conditional"):
                    for name, rx in pattern.items():
                        if rx.search(value):
                            hits.setdefault(name, 0)
                            hits[name] += 1
                else:
                    walk(value)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    walk(root)
    return hits


def load_json(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        text = f.read()
    try:
        return json.loads(text), ("\r\n" if "\r\n" in text else "\n")
    except ValueError as exc:
        fail("Invalid JSON file: %s" % exc)


def save_json(path, data, newline, backup):
    if backup:
        shutil.copy2(path, path + ".bak")
        print("[i] Backup created: %s.bak" % os.path.basename(path))
    text = json.dumps(data, indent=4, ensure_ascii=False)
    if newline != "\n":
        text = text.replace("\n", newline)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def parse_args():
    p = argparse.ArgumentParser(description="Rename Modbus signal varNames in order.")
    p.add_argument("--file", default=None, help="path to the json config file")
    p.add_argument("--robot", default=None, help='robot name, e.g. "Robot0"')
    p.add_argument("--device", default=None, help='modbus device name, e.g. "Modbus_3"')
    p.add_argument("--addr", type=int, default=None, help="addr of the first signal")
    p.add_argument("--name", default=None, help="varName of the first signal")
    p.add_argument("--format", dest="fmt", default=None, help='new name format, e.g. "HR***"')
    p.add_argument("--start", type=int, default=None, help="first number to use")
    p.add_argument("--amount", type=int, default=None, help="how many signals to rename")
    p.add_argument("--write", action="store_true", help="write the file (default is preview only)")
    p.add_argument("--no-backup", action="store_true", help="do not create the .bak copy")
    return p.parse_args()


def main():
    args = parse_args()

    path = args.file or json_file
    if not os.path.isabs(path):
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), path)
    if not os.path.isfile(path):
        fail("File not found: %s" % path)

    robot = args.robot if args.robot is not None else robot_name
    device_name = args.device if args.device is not None else modbus_device_name
    addr_num = args.addr if args.addr is not None else start_address_num
    addr_name = args.name if args.name is not None else start_address_name
    fmt = args.fmt if args.fmt is not None else write_format
    start_num = args.start if args.start is not None else start_write_num
    amount = args.amount if args.amount is not None else write_amount
    apply_changes = args.write or not dry_run
    backup = make_backup and not args.no_backup

    data, newline = load_json(path)

    mb_path, modbus_client = find_modbus_client(data, robot)
    if modbus_client is None:
        fail('No "modbusClient" key found in the file.')
    print('[i] File        : %s' % path)
    print('[i] modbusClient: %s' % " -> ".join(mb_path))

    devices = get_devices(modbus_client)
    device = find_device(devices, device_name)
    group, anchor_idx = find_anchor(device, addr_num, addr_name)
    signals = group["signal"]

    print('[i] Device      : %s (ip %s:%s)' % (device.get("name"), device.get("ip"), device.get("port")))
    print('[i] Group       : groupId=%s funCode=%s -> %s, %d signals'
          % (group.get("groupId"), group.get("funCode"),
             FUNC_CODE_LABELS.get(str(group.get("funCode")), "?"), len(signals)))
    print('[i] Anchor      : index %d, addr=%s, varName="%s"'
          % (anchor_idx, signals[anchor_idx].get("addr"), signals[anchor_idx].get("varName")))

    available = len(signals) - anchor_idx
    if amount > available:
        fail("Only %d signals left from the anchor, not enough for write_amount=%d."
             % (available, amount))

    new_names = build_names(fmt, start_num, amount)
    targets = signals[anchor_idx:anchor_idx + amount]
    old_names = [sig.get("varName") for sig in targets]

    # Check the new names against signals outside the rename range
    existing = collect_varnames(devices)
    renaming = set(old_names)
    conflicts = []
    for new in new_names:
        for place in existing.get(new, []):
            if new not in renaming:
                conflicts.append((new, place))
    if conflicts:
        print("[WARNING] New names collide with existing signals:")
        for new, place in conflicts:
            print('    "%s" already exists at %s' % (new, place))

    # Before / after report
    print("")
    print("%-6s %-8s %-24s %-24s" % ("#", "addr", "old varName", "new varName"))
    print("-" * 66)
    changed = 0
    for i, sig in enumerate(targets):
        old = sig.get("varName")
        new = new_names[i]
        mark = "" if old == new else "  *"
        if old != new:
            changed += 1
        print("%-6d %-8s %-24s %-24s%s" % (anchor_idx + i, sig.get("addr"), old, new, mark))
    print("-" * 66)
    print("Total: %d signals, %d changed." % (amount, changed))

    # Warn about old names still referenced by program scripts
    refs = find_references(data, [n for n in old_names if n and n not in set(new_names)])
    if refs:
        print("")
        print("[WARNING] These old names are still used in scripts/expressions "
              "and must be updated by hand:")
        for name, count in sorted(refs.items()):
            print('    "%s" (%d place(s))' % (name, count))

    if changed == 0:
        print("\n[i] Nothing to change.")
        return

    if not apply_changes:
        print("\n[i] Preview mode (dry run). Add --write to apply the changes.")
        return

    for i, sig in enumerate(targets):
        sig["varName"] = new_names[i]

    save_json(path, data, newline, backup)
    print("\n[OK] Wrote %d change(s) to %s" % (changed, os.path.basename(path)))


if __name__ == "__main__":
    main()
