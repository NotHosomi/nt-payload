#if defined _events_
    #endinput
#endif
#define _events_

public void OnPluginStart()
{
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

public void OnMapStart()
{
    if (!IsPayloadMap()) {
        UnloadSelf();
    }
}

// Clear the coordinator output hooks before it gets destroyed.
public void OnMapEnd()
{
    ClearEntHooks();
}

public Action Event_RoundStart(Handle event, const char[] name, bool dontBroadcast)
{
    ClearEntHooks();
    CreatePayloadCoordinator(); // is the old coordinator deleted on round end? -H
    CreateTimer(2.0, Timer_DelayedSetRespawn, _, TIMER_FLAG_NO_MAPCHANGE);

    if (g_hDeadline != null) {
        LogError("%s g_hDeadline != null on Event_RoundStart - this should never happen.",
                g_sPluginTag);
        delete g_hDeadline;
        g_hDeadline = null;
    }
    // Timers aren't exactly precise, so give a grace time of ~2 seconds
    // before actual round end for triggering the defender victory.
    int timerInaccuracy = 2;
    g_hDeadline = CreateTimer(g_cRoundTime.FloatValue * 60 - timerInaccuracy, Timer_Deadline);
    if (g_hDeadline == null) {
        LogError("%s Failed to CreateTimer on Event_RoundStart", g_sPluginTag);
    }
}


// Return whether the current map is a Payload map.
bool IsPayloadMap()
{
#if defined DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS
    return true;
#else
    decl String:mapName[128];
    GetCurrentMap(mapName, sizeof(mapName));
    
    char buffers[3][100];
    int splits = ExplodeString(mapName, "_", buffers, sizeof(buffers), sizeof(buffers[]));
    // Map name does not follow the "nt_mapname_gamemode" pattern.
    if (splits < 2) {
        return false;
    }
    return StrEqual(buffers[2], g_sPayloadMapIdentifier);
#endif
}