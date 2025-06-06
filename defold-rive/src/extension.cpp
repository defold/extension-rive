
#include <dmsdk/sdk.h>
#include <dmsdk/resource/resource.h>
#include "script_rive.h"

static dmExtension::Result AppInitializeRive(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeRive(dmExtension::Params* params)
{
#if !defined(DM_RIVE_UNSUPPORTED)
    dmResource::HFactory factory = dmExtension::GetContextAsType<dmResource::HFactory>(params, "factory");
    dmRive::ScriptRegister(params->m_L, factory);
    dmLogInfo("Registered Rive extension\n");
#endif
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeRive(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeRive(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}


// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)
DM_DECLARE_EXTENSION(RiveExt, "RiveExt", AppInitializeRive, AppFinalizeRive, InitializeRive, 0, 0, FinalizeRive);
