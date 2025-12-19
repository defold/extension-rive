#!/usr/bin/env python3
"""
Generate `defold-rive/api/rive.script_api` from the Doxygen documentation inside
`defold-rive/src/script_rive.cpp` and `defold-rive/src/script_rive_cmd.cpp`.
"""

import re
from pathlib import Path
from typing import Dict, List, Optional, Set


INPUT_RIVE = Path("defold-rive/src/script_rive.cpp")
INPUT_CMD = Path("defold-rive/src/script_rive_cmd.cpp")
OUTPUT_API = Path("defold-rive/api/rive.script_api")


TYPE_ALIAS_BY_NAME = {
    "file_handle": "FileHandle",
    "artboard_handle": "ArtboardHandle",
    "state_machine_handle": "StateMachineHandle",
    "view_model_handle": "ViewModelInstanceHandle",
    "view_model_instance_handle": "ViewModelInstanceHandle",
    "nested_handle": "ViewModelInstanceHandle",
    "image_handle": "RenderImageHandle",
    "render_image_handle": "RenderImageHandle",
    "audio_handle": "AudioSourceHandle",
    "font_handle": "FontHandle",
}

TYPE_ALIAS_BY_BULLET = {
    "file": "FileHandle",
    "artboard": "ArtboardHandle",
    "stateMachine": "StateMachineHandle",
    "viewModel": "ViewModelInstanceHandle",
    "renderImage": "RenderImageHandle",
    "audioSource": "AudioSourceHandle",
    "font": "FontHandle",
    "viewModelInstance": "ViewModelInstanceHandle",
}


def parse_documentation(text: str) -> Dict[str, dict]:
    result = {}
    pattern = re.compile(r"/\*\*((?:.|\n)*?)\*/\s+static int (Script_\w+)\(", re.MULTILINE)

    for match in pattern.finditer(text):
        doc_text = match.group(1)
        script_name = match.group(2)
        result[script_name] = parse_doc_block(doc_text)
    return result


def parse_doc_block(doc_text: str) -> dict:
    lines = []
    for raw in doc_text.splitlines():
        raw = raw.strip()
        if raw.startswith("*"):
            raw = raw[1:].lstrip()
            lines.append(raw)
    description_lines = []
    params = []
    returns = []
    names: List[str] = []
    desc_done = False
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("@name"):
            names_line = line[len("@name"):].strip()
            parts = [part.strip() for part in names_line.split("/") if part.strip()]
            names.extend(parts)
            desc_done = True
            i += 1
            continue
        if line.startswith("@param"):
            desc_done = True
            param = parse_typed_line(line[len("@param") :].strip())
            i += 1
            extra = []
            while i < len(lines) and not lines[i].startswith("@"):
                extra.append(lines[i])
                i += 1
            param["extras"] = extra
            params.append(param)
            continue
        if line.startswith("@return"):
            desc_done = True
            returns.append(parse_typed_line(line[len("@return") :].strip()))
            i += 1
            continue
        if not desc_done and line:
            description_lines.append(line)
        i += 1
    description = " ".join(description_lines).strip()
    for param in params:
        apply_type_alias(param)
        if param["name"] == "callback":
            nested = parse_callback_sections(param.get("extras", []))
            if nested:
                param["parameters"] = nested
        elif param.get("extras"):
            # drop extras for non-callback parameters
            pass
    for ret in returns:
        apply_type_alias(ret)
    return {
        "description": description,
        "parameters": params,
        "returns": returns,
        "names": names,
    }


def parse_typed_line(text: str) -> dict:
    match = re.match(r"(\w+)\s+\[type:\s*([^\]]+)\]\s*(.*)", text)
    if not match:
        raise SystemExit(f"Could not parse typed line: {text!r}")
    name = match.group(1)
    type_text = match.group(2).strip()
    desc = match.group(3).strip()
    return {"name": name, "type": type_text, "desc": desc}


def apply_type_alias(entry: dict):
    name = entry["name"]
    type_text = entry["type"]
    alias = TYPE_ALIAS_BY_NAME.get(name)
    if alias:
        entry["type"] = alias
        return
    if type_text in TYPE_ALIAS_BY_NAME:
        entry["type"] = TYPE_ALIAS_BY_NAME[type_text]
        return
    entry["type"] = TYPE_ALIAS_BY_NAME.get(type_text, type_text)


def parse_callback_sections(lines: List[str]) -> List[dict]:
    sections = []
    current: Optional[dict] = None
    bullets: List[str] = []
    for raw in lines:
        text = raw.strip()
        if not text:
            continue
        if text.startswith("`") and "`" in text[1:]:
            if current:
                current["bullets"] = bullets
                bullets = []
            key = text.strip("`").strip()
            current = {"name": key, "type": "", "desc": "", "bullets": []}
            sections.append(current)
            continue
        if text.startswith(":"):
            match = re.match(r":\s*\[type:\s*([^\]]+)\]\s*(.*)", text)
            if match and current:
                current["type"] = match.group(1).strip()
                current["desc"] = match.group(2).strip()
            continue
        if text.startswith("-"):
            bullets.append(text[1:].strip())
            continue
    if current:
        current["bullets"] = bullets
    result = []
    for section in sections:
        entry = {
            "name": section["name"],
            "type": section["type"] or "string",
            "desc": section["desc"],
        }
        if entry["name"] == "event" and section["bullets"]:
            extras = ", ".join(b.strip("` ") for b in section["bullets"])
            entry["desc"] = f"{entry['desc']} {extras}".strip()
        if entry["name"] == "data" and section["bullets"]:
            subparams = []
            for bullet in section["bullets"]:
                match = re.match(r"`?([^`]+)`?:\s*\[type:\s*([^\]]+)\]\s*(.*)", bullet)
                if not match:
                    continue
                child_name = match.group(1).strip()
                child_type = match.group(2).strip()
                child_desc = match.group(3).strip()
                child_type = TYPE_ALIAS_BY_BULLET.get(child_name, child_type)
                subparams.append(
                    {"name": child_name, "type": child_type, "desc": child_desc}
                )
            if subparams:
                entry["parameters"] = subparams
        result.append(entry)
    return result


def parse_registration(text: str, symbol: str) -> List[tuple]:
    pattern = re.compile(
        rf"static const luaL_reg {symbol}\[\]\s*=\s*\{{(.*?)\}};", re.S
    )
    match = pattern.search(text)
    if not match:
        raise SystemExit(f"Registration table {symbol} not found.")
    content = match.group(1)
    funcs = []
    for item in re.findall(r'\{"([^"]+)",\s*Script_(\w+)\}', content):
        funcs.append(item)
    return funcs


def write_members(members: List[dict]) -> List[str]:
    lines = []
    first = True
    for member in members:
        if not first:
            lines.append("")
            lines.append("#*****************************************************************************************************")
        first = False
        lines.append("")
        lines.append(f"  - name: {member['name']}")
        lines.append("    type: function")
        lines.append(f"    desc: {member['desc']}")
        if member.get("parameters"):
            lines.append("    parameters:")
            for param in member["parameters"]:
                lines.extend(format_param(param, 6))
        if member.get("returns"):
            lines.append("    return:")
            for ret in member["returns"]:
                lines.extend(
                    [
                        "      - name: {}".format(ret["name"]),
                        "        type: {}".format(ret["type"]),
                        "        desc: {}".format(ret["desc"]),
                    ]
                )
    return lines


def format_param(param: dict, indent: int) -> List[str]:
    lines = []
    lines.append(" " * indent + f"- name: {param['name']}")
    lines.append(" " * (indent + 2) + f"type: {param['type']}")
    lines.append(" " * (indent + 2) + f"desc: {param['desc']}")
    if param.get("parameters"):
        lines.append(" " * (indent + 2) + "parameters:")
        for child in param["parameters"]:
            lines.extend(format_param(child, indent + 4))
    return lines


def build_api():
    rive_text = INPUT_RIVE.read_text()
    cmd_text = INPUT_CMD.read_text()
    rive_docs = parse_documentation(rive_text)
    cmd_docs = parse_documentation(cmd_text)
    rive_order = parse_registration(rive_text, "RIVE_FUNCTIONS")
    cmd_order = parse_registration(cmd_text, "RIVE_COMMAND_FUNCTIONS")

    rive_members = []
    for lua_name, script_name in rive_order:
        doc = rive_docs.get(f"Script_{script_name}")
        if not doc:
            continue
        rive_members.append(
            {
                "name": lua_name,
                "desc": doc["description"] or lua_name,
                "parameters": doc["parameters"],
                "returns": doc["returns"],
            }
        )

    cmd_members = []
    for lua_name, script_name in cmd_order:
        doc = cmd_docs.get(f"Script_{script_name}")
        if not doc:
            continue
        cmd_members.append(
            {
                "name": lua_name,
                "desc": doc["description"] or lua_name,
                "parameters": doc["parameters"],
                "returns": doc["returns"],
            }
        )

    lines = []
    lines.append("- name: rive")
    lines.append("  type: table")
    lines.append('  desc: Rive animation helpers exposed to Lua scripts')
    lines.append("")
    lines.append("  members:")
    lines.extend(write_members(rive_members))
    lines.append("")
    lines.append("#*****************************************************************************************************")
    lines.append("")
    lines.append("- name: rive.cmd")
    lines.append("  type: table")
    lines.append("  desc: Command queue helpers for interacting with the Rive runtime")
    lines.append("")
    lines.append("  members:")
    lines.extend(write_members(cmd_members))
    lines.append("")

    OUTPUT_API.write_text("\n".join(lines).strip() + "\n")
    update_lua_annotations(rive_members, cmd_members)


def gather_handle_aliases(members: List[dict]) -> Set[str]:
    aliases: Set[str] = set()
    for member in members:
        for entry in member.get("parameters", []) + member.get("returns", []):
            type_name = entry["type"]
            if type_name in TYPE_ALIAS_BY_NAME.values():
                aliases.add(type_name)
    return aliases


def update_lua_annotations(rive_members: List[dict], cmd_members: List[dict]):
    lua_lines = [
        "-- Auto generated from utils/update_script_api.py",
        "-- WARNING: Do not edit manually.",
        "",
        "--[[",
        "Rive API documentation",
        "Functions and constants for interacting with Rive models",
        "--]]",
        "",
        "---@meta",
        "---@diagnostic disable: lowercase-global",
        "---@diagnostic disable: missing-return",
        "---@diagnostic disable: duplicate-doc-param",
        "---@diagnostic disable: duplicate-set-field",
        "---@diagnostic disable: args-after-dots",
        "",
    ]
    handles = sorted(gather_handle_aliases(rive_members + cmd_members))
    for alias in handles:
        lua_lines.append(f"---@alias {alias} integer")
    if handles:
        lua_lines.append("")

    lua_lines.append("--- @class rive")
    lua_lines.append("rive = {}")
    lua_lines.append("")
    for member in rive_members:
        lua_lines.extend(format_lua_function("rive", member))
    lua_lines.append("--- @class rive.cmd")
    lua_lines.append("rive.cmd = {}")
    lua_lines.append("")
    for member in cmd_members:
        lua_lines.extend(format_lua_function("rive.cmd", member))

    OUTPUT_API.parent.mkdir(parents=True, exist_ok=True)
    lua_file = OUTPUT_API.with_suffix(".lua")
    lua_file.write_text("\n".join(lua_lines).strip() + "\n")


def format_lua_function(namespace: str, member: dict) -> List[str]:
    lines = []
    lines.append(f"--- {member['desc']}")
    for param in member.get("parameters", []):
        type_text = param["type"]
        optional = False
        if "|nil" in type_text:
            type_text = type_text.replace("|nil", "")
            optional = True
        type_text = type_text.strip()
        if param.get("parameters"):
            inner = ", ".join(entry["name"] for entry in param["parameters"])
            lua_type = f"fun({inner})"
        elif "function" in type_text:
            lua_type = "function"
        else:
            lua_type = type_text
        param_name = f"{param['name']}?" if optional else param['name']
        lines.append(f"---@param {param_name} {lua_type} {param['desc']}")
        if param.get("parameters"):
            lines.extend(format_callback_subparams(param["name"], param["parameters"]))
    for ret in member.get("returns", []):
        lines.append(f"---@return {ret['type']} {ret['name']} {ret['desc']}")
    params = ", ".join(param["name"] for param in member.get("parameters", []))
    func_ns = namespace
    lines.append(f"function {func_ns}.{member['name']}({params}) end")
    lines.append("")
    return lines


def format_callback_subparams(base: str, entries: List[dict]) -> List[str]:
    lines = []
    for entry in entries:
        child_name = f"{base}_{entry['name']}"
        lines.append(f"---@param {child_name} {entry['type']} {entry['desc']}")
        if entry.get("parameters"):
            lines.extend(format_callback_subparams(child_name, entry["parameters"]))
    return lines


if __name__ == "__main__":
    build_api()
