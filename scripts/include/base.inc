#if defined _base_
    #endinput
#endif
#define _base_

#define PLUGIN_VERSION "0.3"

#define DEBUG_PRINT_MORE_DETAILS
//#define DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS // Uncomment to disable the map name check.

#define MAX_COORDINATOR_INPUTS 4

enum {
    TEAMCONTROL_ATTACKER_IS_JINRAI = 0,
    TEAMCONTROL_ATTACKER_IS_NSF,
    TEAMCONTROL_ATTACKER_HAS_WON,
    TEAMCONTROL_ENUM_COUNT
};

enum {
    TIMECONTROL_INCREMENT = 0,
    TIMECONTROL_ENABLE_OVERTIME,
    TIMECONTROL_DISABLE_OVERTIME,
    TIMECONTROL_ANNOUNCE_PROGRESS,
    TIMECONTROL_ENUM_COUNT
};

enum {
    COORDINATOR_TEAMCONTROL = 0,
    COORDINATOR_TIMECONTROL,
    COORDINATOR_ENUM_COUNT
};

// Possible inputs to fire into the coordinator outputs.
// OnUserN is triggered by firing the matching "FireUserN" input;
// e.g. logic_relay --> FireUser3 --> payload_coordinator = trigger attacker victory.
// Note that the attacker must be set by using FireUser1 or 2 before sending the win signal.
new const String:g_sCoordinatorOutputs[MAX_COORDINATOR_INPUTS][] = {
        // TeamControl: Set the attacker as Jinrai.
        // TimeControl: Increment deadline.
        "OnUser1",
        // TeamControl: Set the attacker as NSF.
        // TimeControl: No operation.
        "OnUser2",
        // TeamControl: Signal that the attacker has won the round.
        // TimeControl: No operation.
        "OnUser3",
        // TeamControl: No operation.
        // TimeControl: No operation.
        "OnUser4"
};
// Target names that the logic_relays should fire into to communicate with this plugin.
// These entities should not exist in the map, as it is created at runtime by this plugin!
new const String:g_sCoordinatorTargetname[COORDINATOR_ENUM_COUNT][] = {
    "pl_coord_team_control",
    "pl_coord_time_control"
};

const int g_iNumCoordinators = 2;

new const String:g_sPluginTag[] = "[PAYLOAD]";
new const String:g_sTeamNames[][] = {
    "none", "spectator", "Jinrai", "NSF"
};

int g_iAttacker = TEAM_NONE;
int g_iCoordinatorEnt[g_iNumCoordinators] = { 0 };

ConVar g_cRoundTime = null;

Handle g_hDeadline = null;
Handle g_hDeadlineCountdown = null;

const int countdownSecs = 10;

DataPack g_dpCoordFuncs = null;

// Convert the received output string into coordinator int enum value.
// Can be cast into any coordinator enum.
int GetCoordEnum(const char[] output)
{
    for (int i = 0; i < MAX_COORDINATOR_INPUTS; i++) {
        if (StrEqual(g_sCoordinatorOutputs[i], output)) {
            return i;
        }
    }
    return -1;
}

// Create a networked entity, and hook it for receiving events.
void CreatePayloadCoordinators()
{
    g_dpCoordFuncs.Reset();
    for (int i = 0; i < g_iNumCoordinators; i++) {
        if (g_iCoordinatorEnt[i] > 0) {
            LogError("%s CreatePayloadCoordinator: Called while g_iCoordinatorEnt[%i] > 0",
                    g_sPluginTag, i);
            ClearEntHooks(g_iCoordinatorEnt[i]);
        }
        PrintDebug("Preparing to dispatch %s", g_sCoordinatorTargetname[i]);
        // This could be any networked entity with a targetname field;
        // I chose teleport destination because it has no discernible side effects.
        g_iCoordinatorEnt[i] = CreateEntityByName("info_teleport_destination");
        if (g_iCoordinatorEnt[i] == -1) {
            SetFailState("Coordinator creation failed");
        }
        // Name the entity, so that Payload map logic can target fire events to it.
        if (!DispatchKeyValue(g_iCoordinatorEnt[i], "targetname", g_sCoordinatorTargetname[i])) {
            SetFailState("Failed to dispatch targetname kv for coordinator");
        }
        if (!DispatchSpawn(g_iCoordinatorEnt[i])) {
            SetFailState("Failed to dispatch spawn for coordinator");
        }
        // Hook the coordinator output.
        EntityOutput entOutFunc = view_as<EntityOutput>(g_dpCoordFuncs.ReadFunction());
        for (int j = 0; j < MAX_COORDINATOR_INPUTS; j++) {
            HookSingleEntityOutput(g_iCoordinatorEnt[i], g_sCoordinatorOutputs[j],
                    entOutFunc, false);
            //PrintDebug("Hooked %s to %s", g_iCoordinatorEnt[i], entOutFunc);
        }
    }
}

// Format and print Payload announcements with the plugin tag included.
void PayloadMessage(const char[] message, any ...)
{
    // TODO figure out how to nicely allocate just needed amount
    int bufLen = strlen(message) + 512;
    decl String:buffer[bufLen];
    VFormat(buffer, bufLen, message, 2);
    PrintToChatAll("%s %s", g_sPluginTag, buffer);
}

void PrintDebug(const char[] msg, any ...)
{
#if defined DEBUG_PRINT_MORE_DETAILS
    // TODO figure out how to nicely allocate just needed amount
    int bufLen = strlen(msg) + 512;
    decl String:buffer[bufLen];
    if (VFormat(buffer, bufLen, msg, 2) > 0) {
        PrintToChatAll("[debug] %s", buffer);
        PrintToServer("[debug] %s", buffer);
    }
    else {
        ThrowError("VFormat failed for %s", msg);
    }
#endif
}

// Remove the entity hooks.
void ClearEntHooks(int entity)
{
    if (!entity || !IsValidEntity(entity)) {
        return;
    }
    
    for (int i = 0; i < MAX_COORDINATOR_INPUTS; i++) {
        UnhookSingleEntityOutput(entity, g_sCoordinatorOutputs[i],
            TeamControl);
    }
    entity = 0;
}

// Coordinate TeamControl output events sent to coordinator from the map.
public void TeamControl(const char[] output, int caller,
        int activator, float delay)
{
    PrintDebug("TeamControl: %s", output);

    int cmdEnum = GetCoordEnum(output);
    if (cmdEnum == -1) {
        ThrowError("GetCoordEnum failed on output: %s", output);
    }
    else if (cmdEnum == TEAMCONTROL_ATTACKER_HAS_WON) {
        DeclareVictory(GetAttackingTeam());
    }
    else {
        int team = CoordEnumToTeamEnum(cmdEnum);
        SetAttackingTeam(team);
    }
}

// Coordinate TimeControl output events sent to coordinator from the map.
public void TimeControl(const char[] output, int caller,
    int activator, float delay)
{
    PrintDebug("TimeControl: %s", output);
    
    int cmdEnum = GetCoordEnum(output);
    if (cmdEnum == -1) {
        ThrowError("GetCoordEnum failed on output: %s", output);
    }
    else if (cmdEnum == TIMECONTROL_INCREMENT) {
        IncrementDeadline();
    }
}

void IncrementDeadline()
{
    PrintDebug("Increment deadline triggered"); // TODO
}

// Cue the confetti.
void DeclareVictory(int winner)
{
    // If this is not a defender victory, the defender victory
    // timer hasn't been called. We have to clear it manually.
    // Don't call this if defenders won to avoid a double delete.
    if (winner != GetDefendingTeam()) {
        if (g_hDeadline != null) {
            delete g_hDeadline;
            g_hDeadline = null;
        }
    }

    if (winner != TEAM_JINRAI && winner != TEAM_NSF) {
        ThrowError("Declared winner with invalid team (%i)", winner);
    }

    // HACK/TODO: Just kill all losers for now to force round end.
    SetRespawning(false);
    SoftKillTeam(GetOppositeTeam(winner));

    if (winner == GetAttackingTeam()) {
        PayloadMessage("%s wins by delivering the payload!", g_sTeamNames[winner]);
    }
    else {
        PayloadMessage("%s wins by defending the base!", g_sTeamNames[winner]);
    }
    
    for (int i = 0; i < COORDINATOR_ENUM_COUNT; i++) {
        ClearEntHooks(g_iCoordinatorEnt[i]);
    }
}

// Toggle respawning with Neotokyo's warmup game state.
void SetRespawning(bool respawnEnabled)
{
    const int respawn = 1, normal = 2;
    int gamestate;

    if (respawnEnabled) {
        gamestate = respawn;
        PayloadMessage("Respawning is now enabled");
    } else {
        gamestate = normal;
        PayloadMessage("Respawning is now disabled");
    }

    GameRules_SetProp("m_iGameState", gamestate);
}

#if !defined DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS
// Unload this plugin.
void UnloadSelf()
{
    decl String:thisPluginFilename[32];
    GetPluginFilename(INVALID_HANDLE, thisPluginFilename,
            sizeof(thisPluginFilename));
    ServerCommand("sm plugins unload %s", thisPluginFilename);
}

// Return whether the current map is a Payload map.
bool IsPayloadMap()
{
    // Payload maps are identified by the naming convention nt_mapname_gamemode,
    // where the third value divided by underscores equals this value (e.g. "nt_example_pl_a43").
    new const String:payload_id[] = "pl";

    decl String:mapName[128];
    GetCurrentMap(mapName, sizeof(mapName));
    
    char buffers[3][100];
    int splits = ExplodeString(mapName, "_", buffers, sizeof(buffers), sizeof(buffers[]));
    // Map name does not follow the "nt_mapname_gamemode" pattern.
    if (splits < 2) {
        return false;
    }
    return StrEqual(buffers[2], payload_id);
}
#endif