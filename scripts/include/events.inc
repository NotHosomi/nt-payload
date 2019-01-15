#if defined _events_
    #endinput
#endif
#define _events_

public void OnPluginStart()
{
// Sanity check for outputs; cannot have more than we define.
#if sizeof(g_sCoordinatorOutputs) > MAX_COORDINATOR_INPUTS
#error "Too many coordinator outputs."
#endif

    g_dpCoordFuncs = new DataPack();
    g_dpCoordFuncs.Reset();
    // NOTE: These *have* to be in the COORDINATOR_... enum order,
    // and must include a EntityOutput pointer for each coordinator function!
    g_dpCoordFuncs.WriteFunction(view_as<EntityOutput>(TeamControl));
    g_dpCoordFuncs.WriteFunction(view_as<EntityOutput>(TimeControl));
    PrintToServer("Wrote funcs");

    float minRoundTime, maxRoundTime;
    ConVar neoRoundTime = FindConVar("neo_round_timelimit");
    if (!neoRoundTime) {
        SetFailState("Failed to retrieve Neotokyo native round time cvar");
    }
    neoRoundTime.GetBounds(ConVarBound_Lower, minRoundTime);
    neoRoundTime.GetBounds(ConVarBound_Upper, maxRoundTime);
    delete neoRoundTime;

    g_cRoundTime = CreateConVar("sm_neopayload_roundtime", "10",
            "Payload round time, in minutes.", FCVAR_NOTIFY,
            true, minRoundTime, true, maxRoundTime);
    
    CreateConVar("sm_neopayload_version", PLUGIN_VERSION,
            "Neotokyo Payload plugin version.",
            FCVAR_NOTIFY | FCVAR_DONTRECORD | FCVAR_SPONLY);

    AutoExecConfig(true);
    
    HookConVarChange(g_cRoundTime, CvarChanged_PayloadRoundTime);
    
    HookEvent("game_round_start", Event_RoundStart);
}

public void OnConfigsExecuted()
{
    ConVar neoRoundTime = FindConVar("neo_round_timelimit");
    neoRoundTime.SetFloat(g_cRoundTime.FloatValue, true, false);
    delete neoRoundTime;
}

#if !defined DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS
public void OnMapStart()
{
    if (!IsPayloadMap()) {
        UnloadSelf();
    }
}
#endif

// Clear the coordinators' output hooks before it gets destroyed.
public void OnMapEnd()
{
    ClearEntHooks();
}

public Action Event_RoundStart(Handle event, const char[] name, bool dontBroadcast)
{
    ClearEntHooks();
    CreatePayloadCoordinators(); // is the old coordinator deleted on round end? -H
    CreateTimer(2.0, Timer_DelayedSetRespawn, _, TIMER_FLAG_NO_MAPCHANGE);
    CreateDeadlineTimer();
}

// Propagate payload roundtime cvar change to native game rules cvar.
public void CvarChanged_PayloadRoundTime(ConVar convar, const char[] oldValue, const char[] newValue)
{
    ConVar neoRoundTime = FindConVar("neo_round_timelimit");
    neoRoundTime.SetString(newValue, true, false);
    delete neoRoundTime;
}