// Generated. Do not edit! See ./build_version_header.sh

//fix(wgpu): Bit rot (#11245) 47f4ec46d4
//* Clean up some assumptions in WebGPU that support for PLS meant it was
//  actually being used. Now that we support MSAA, it might not be in use.
//
//* Clean up a refactoring typo that caused an input attachment to be
//  deleted before it was used.
//
//Co-authored-by: Chris Dalton <99840794+csmartdalton@users.noreply.github.com>

const char* RIVE_RUNTIME_AUTHOR="csmartdalton";
const char* RIVE_RUNTIME_DATE="2025-12-09T23:23:00Z";
const char* RIVE_RUNTIME_SHA1="3b5d9785f579e8055ec869382caceee19958c3ed";
