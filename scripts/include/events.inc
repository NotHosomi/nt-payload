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
    
    g_cDeadlineExtendTime = CreateConVar("sm_neopayload_deadline_extend_time", "30",
        "How much to extend payload deadline upon attackers reaching \
a checkpoint, in seconds.", FCVAR_NOTIFY, true, 0.0, true, maxRoundTime * 60);

    g_cDebugLvl = CreateConVar("sm_neopayload_debug", "1",
        "Payload debug verbosity level. 0 = no debug, 1 = basic debug; store \
debug to server log and display in client console. 2 = same as 1, but also \
print debug to client chat. 3 = same as 2, but also print debug to server \
output.", FCVAR_NOTIFY, true, 0.0, true, 3.0);

    decl String:mapCheckDesc[232];
    Format(mapCheckDesc, sizeof(mapCheckDesc) + sizeof(payload_id) + 1,
        "If 0, only maps following the pattern mapname_%s(...) are considered \
payload maps, and this plugin will auto-unload when changing to a non-payload map. \
If 1, ignore the check and consider all maps as payload maps. Debug command.",
        payload_id);
    g_cDebugAllMapsArePlMaps = CreateConVar("sm_neopayload_all_maps_are_payload_maps",
        "0", mapCheckDesc, FCVAR_NOTIFY, true, 0.0, true, 1.0);
    
    g_cNodePassAnnounceFreq = CreateConVar("sm_neopayload_node_pass_announce_frequency",
        "15", "How much payload push progress, in percentages of 100, must be accumulated \
before announcing another path node pass event. Setting this to 100 will never \
announce node passing. Setting this to 0 will announce every node pass.",
        FCVAR_NOTIFY, true, 0.0, true, 100.0);
    
    AutoExecConfig(true);
    
    HookConVarChange(g_cRoundTime, CvarChanged_PayloadRoundTime);
    
    HookEvent("game_round_start", Event_RoundStart);

    if (g_dpCoordFuncs == null) {
        g_dpCoordFuncs = new DataPack();
        g_dpCoordFuncs.Reset();
        // NOTE: These *have* to be in the COORDINATOR_... enum order,
        // and must include a EntityOutput pointer for each coordinator function!
        g_dpCoordFuncs.WriteFunction(view_as<EntityOutput>(TeamControl));
        g_dpCoordFuncs.WriteFunction(view_as<EntityOutput>(TimeControl));
    }
}

public void OnConfigsExecuted()
{
    // We are not in a payload map. Bail out!
    if (!g_cDebugAllMapsArePlMaps.BoolValue && !IsPayloadMap()) {
        UnloadSelf();
    }
    ConVar neoRoundTime = FindConVar("neo_round_timelimit");
    neoRoundTime.SetFloat(g_cRoundTime.FloatValue, true, false);
    delete neoRoundTime;
}

// Clear the coordinators' output hooks before it gets destroyed.
// We don't need to do this for OnPluginEnd because it is guaranteed
// to release all hooks automatically.
public void OnMapEnd()
{
    EndPayload();
}

public void OnClientDisconnect_Post(int client)
{
    int players = 0;
    for (int c = 1; c <= MaxClients; c++) {
        if (IsClientInGame(c) && !IsClientSourceTV(c)) {
#if !defined DEBUG_BOT_PLAYERS_COUNT_AS_HUMANS
            if (IsFakeClient(c)) {
                continue;
            }
#endif
            players++;
        }
    }
    // The last player just disconnected. Stop the payload mode.
    if (players == 0) {
        EndPayload();
        PrintDebug(false, "Last player just dc'd, stopping payload");
    }
}

public Action Event_RoundStart(Handle event, const char[] name, bool dontBroadcast)
{
    EndPayload();    
    CreateTimer(2.0, Timer_DelayedSetRespawn, _, TIMER_FLAG_NO_MAPCHANGE);
    StartPayload();
}

// Propagate payload roundtime cvar change to native game rules cvar.
public void CvarChanged_PayloadRoundTime(ConVar convar, const char[] oldValue, const char[] newValue)
{
    ConVar neoRoundTime = FindConVar("neo_round_timelimit");
    neoRoundTime.SetString(newValue, true, false);
    delete neoRoundTime;
}