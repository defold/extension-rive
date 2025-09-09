// Generated. Do not edit! See ./utils/write_rive_version_header.py

//Improve Layout display handling incorporating it with isCollapsed (#10473) 0389f47d3d
//We revisited how we handle layout display changes and how they related to the collapsed dirt on a LayoutComponent:
//
//We now override isCollapsed in LayoutComponent to take into account both the collapsed dirt that may have been propagated down from a parent AND the LayoutComponent's own display value. If the layout has collapsed dirt, it MUST have been propagated down so it takes precendence, otherwise use its own display value
//We removed the need to walk up the parent tree to figure out if any layout in the hierarchy is hidden
//We removed a workaround to prevent a 1 frame visual glitch. This workaround was resolved by this fix and is no longer needed.
//
//Co-authored-by: Philip Chung <philterdesign@gmail.com>
const char* RIVE_RUNTIME_AUTHOR="philter";
const char* RIVE_RUNTIME_DATE="2025-08-30T01:05:03Z";
const char* RIVE_RUNTIME_SHA1="4d6675c0ff635c8b0038846397945da56cff2fe6";
