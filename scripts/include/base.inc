#if defined _base_
    #endinput
#endif
#define _base_


#pragma semicolon 1

#include <sourcemod>
#include <sdktools>
#include <neotokyo>


#define PLUGIN_VERSION "0.2"
#define DEBUG_PRINT_MORE_DETAILS


enum {
    COORD_ATTACKER_IS_JINRAI = 0,
    COORD_ATTACKER_IS_NSF,
    COORD_ATTACKER_HAS_WON,
    COORD_ADD_TIME,
    COORD_ENUM_COUNT
};

// Target name that the logic_relays should fire into to communicate with this plugin.
// This entity should not exist in the map, as it is created at runtime by this plugin!
new const String:g_sCoordinatorName[] = "payload_coordinator";

// Payload maps are identified by the naming convention nt_mapname_gamemode,
// where the third value divided by underscores equals this value (e.g. "nt_example_pl_a43").
new const String:g_sPayloadMapIdentifier[] = "pl";
#define DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS // Uncomment to disable the map name check.

// Possible inputs to fire into the coordinator outputs.
// OnUserN is triggered by firing the matching "FireUserN" input;
// e.g. logic_relay --> FireUser3 --> payload_coordinator = trigger attacker victory.
// Note that the attacker must be set by using FireUser1 or 2 before sending the win signal.
new const String:g_sCoordinatorOutputs[COORD_ENUM_COUNT][] = {
        "OnUser1",  // Set the attacker as Jinrai.
        "OnUser2",  // Set the attacker as NSF.
        "OnUser3",  // Signal that the attacker has won the round.
        "OnUser4"   // Add time to timer -H
};

new const String:g_sPluginTag[] = "[PAYLOAD]";
new const String:g_sTeamNames[][] = {
    "none", "spectator", "Jinrai", "NSF"
};

int g_iAttacker = TEAM_NONE;
int g_iCoordinatorEnt = 0;

ConVar g_cRoundTime = null;
Handle g_hDeadline = null;


// Convert the received output string into coordinator int enum value.
int GetCoordEnum(const char[] output)
{
    for (int i = 0; i < COORD_ENUM_COUNT; i++) {
        if (StrEqual(g_sCoordinatorOutputs[i], output)) {
            return i;
        }
    }
    return -1;
}

// Create a networked entity, and hook it for receiving events.
void CreatePayloadCoordinator()
{
    if (g_iCoordinatorEnt > 0) {
        LogError("%s CreatePayloadCoordinator: Called while g_iCoordinatorEnt > 0",
                g_sPluginTag);
        ClearEntHooks();
    }
    // This could be any networked entity with a targetname field;
    // I chose teleport destination because it has no discernible side effects.
    g_iCoordinatorEnt = CreateEntityByName("info_teleport_destination");
    if (g_iCoordinatorEnt == -1) {
        SetFailState("Coordinator creation failed");
    }
    // Name the entity, so that Payload map logic can target fire events to it.
    if (!DispatchKeyValue(g_iCoordinatorEnt, "targetname", g_sCoordinatorName)) {
        SetFailState("Failed to dispatch targetname for coordinator");
    }
    // Hook the coordinator outputs.
    for (int i = 0; i < COORD_ENUM_COUNT; i++) {
        HookSingleEntityOutput(g_iCoordinatorEnt, g_sCoordinatorOutputs[i],
                CoordinatePayload, false);
    }
}

// Format and print Payload announcements with the plugin tag included.
void PayloadMessage(const char[] message, any ...)
{   
    decl String:prettyMessage[sizeof(g_sPluginTag) + 512];
    Format(prettyMessage, sizeof(prettyMessage), "%s %s", g_sPluginTag, message);
    
    decl String:buffer[sizeof(prettyMessage)];
    VFormat(buffer, sizeof(buffer), prettyMessage, 2);

    PrintToChatAll(buffer);

#if defined DEBUG_PRINT_MORE_DETAILS
    PrintToServer(buffer);
#endif
}

// Remove the entity hooks.
void ClearEntHooks()
{
    if (!g_iCoordinatorEnt || !IsValidEntity(g_iCoordinatorEnt)) {
        return;
    }
    
    for (int i = 0; i < COORD_ENUM_COUNT; i++) {
        UnhookSingleEntityOutput(g_iCoordinatorEnt, g_sCoordinatorOutputs[i],
                CoordinatePayload);
    }
    g_iCoordinatorEnt = 0;
}

// Coordinate output events sent to coordinator from the map.
public void CoordinatePayload(const char[] output, int caller,
        int activator, float delay)
{
#if defined DEBUG_PRINT_MORE_DETAILS
    PayloadMessage("(debug) CoordinatePayload: %s", output);
#endif

    int coordEnum = GetCoordEnum(output);
    if (coordEnum == -1) {
        ThrowError("GetCoordEnum failed on output: %s", output);
    }
    else if (coordEnum == COORD_ATTACKER_HAS_WON) {
        DeclareVictory(GetAttackingTeam());
    }
    else {
        int team = CoordEnumToTeamEnum(coordEnum);
        SetAttackingTeam(team);
    }
}

// Cue the confetti.
void DeclareVictory(int team)
{
    // Kill the defenders' deadline timer when victory is declared.
    if (g_hDeadline != null) {
        delete g_hDeadline;
        g_hDeadline = null; // TODO: Does delete null it already?
    }

    if (team != TEAM_JINRAI && team != TEAM_NSF) {
        ThrowError("Declared winner with invalid team (%i)", team);
    }

    // HACK/TODO: Just kill all losers for now to force round end.
    SetRespawning(false);
    SoftKillTeam(GetOppositeTeam(team));

    if (team == GetAttackingTeam()) {
        PayloadMessage("%s wins by delivering the payload!", g_sTeamNames[team]);
    }
    else {
        PayloadMessage("%s wins by defending the base!", g_sTeamNames[team]);
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

// Unload this plugin.
void UnloadSelf()
{
    decl String:thisPluginFilename[32];
    GetPluginFilename(INVALID_HANDLE, thisPluginFilename,
            sizeof(thisPluginFilename));
    ServerCommand("sm plugins unload %s", thisPluginFilename);
}

