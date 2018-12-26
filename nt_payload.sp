/*
    This is an accompanying plugin for a custom Neotokyo Payload game mode.
    For map makers: see the comments on usage starting around line 25.
    
    An example .vmf with logic_relays implemented:
    https://gist.githubusercontent.com/Rainyan/b1c48f048e15f62999aeb2f66f22e56b/raw/f29762a21ab1b04aecc97005c0ab17a7c500a5ed/nt_payload_plugin-relays.vmf
*/

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
    COORD_ENUM_COUNT
};

// Target name that the logic_relays should fire into to communicate with this plugin.
// This entity should not exist in the map, as it is created at runtime by this plugin!
new const String:g_sCoordinatorName[] = "payload_coordinator";

// Payload maps are identified by the naming convention nt_mapname_gamemode,
// where the third value divided by underscores equals this value (e.g. "nt_example_pl_a43").
new const String:g_sPayloadMapIdentifier[] = "pl";
//#define DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS // Uncomment to disable the map name check.

// Possible inputs to fire into the coordinator outputs.
// OnUserN is triggered by firing the matching "FireUserN" input;
// e.g. logic_relay --> FireUser3 --> payload_coordinator = trigger attacker victory.
// Note that the attacker must be set by using FireUser1 or 2 before sending the win signal.
new const String:g_sCoordinatorOutputs[COORD_ENUM_COUNT][] = {
        "OnUser1",  // Set the attacker as Jinrai.
        "OnUser2",  // Set the attacker as NSF.
        "OnUser3"   // Signal that the attacker has won the round.
};

new const String:g_sPluginTag[] = "[PAYLOAD]";
new const String:g_sTeamNames[][] = {
    "none", "spectator", "Jinrai", "NSF"
};

int g_iAttacker = TEAM_NONE;
int g_iCoordinatorEnt = 0;

ConVar g_cRoundTime = null;
Handle g_hDeadline = null;

public Plugin myinfo = {
    name = "NEOTOKYO° Payload",
    description = "A custom Payload game mode for Neotokyo.",
    version = PLUGIN_VERSION,
    author = "Rain",
    url = "https://gist.github.com/Rainyan/b1c48f048e15f62999aeb2f66f22e56b"
};

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
    // DEBUG/TODO: Likely unneccessary, as this cvar is already asserted once in OnPluginStart
    if (!neoRoundTime) {
        SetFailState("Failed to retrieve Neotokyo native round time cvar");
    }
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
    CreatePayloadCoordinator();
    CreateTimer(2.0, Timer_DelayedSetRespawn, _, TIMER_FLAG_NO_MAPCHANGE);

    if (g_hDeadline != null) {
        LogError("%s g_hDeadline != null on Event_RoundStart - this should never happen.",
                g_sPluginTag);
        delete g_hDeadline;
        g_hDeadline = null;
    }
    // Timers aren't exactly precise, so give a grace time of ~2 seconds
    // before actual round end for triggering the defender victory.
    g_hDeadline = CreateTimer(g_cRoundTime.FloatValue - 2.0, Timer_Deadline);
    if (g_hDeadline == null) {
        LogError("%s Failed to CreateTimer on Event_RoundStart", g_hDeadline);
    }
}

// Delay respawn set to dodge native game rules overriding it on round start.
public Action Timer_DelayedSetRespawn(Handle timer)
{
    SetRespawning(true);
}

// Deadline for delivering the Payload has passed; defenders win.
public Action Timer_Deadline(Handle timer)
{
    /*  TODO: Because the defender win has to happen few seconds before
        time runs out (because there aren't suitable events for it),
        there should be a visual countdown for the defender victory,
        so players get a sense of exactly how many seconds remain. */
    DeclareVictory(GetDefendingTeam());
}

// Propagate payload roundtime cvar change to native game rules cvar.
public void CvarChanged_PayloadRoundTime(ConVar convar, const char[] oldValue, const char[] newValue)
{
    ConVar neoRoundTime = FindConVar("neo_round_timelimit");
    // DEBUG/TODO: Likely unneccessary, as this cvar is already asserted once in OnPluginStart
    if (!neoRoundTime) {
        SetFailState("Failed to retrieve Neotokyo native round time cvar");
    }
    neoRoundTime.SetString(newValue, true, false);
    delete neoRoundTime;
}

// Return whether the current map is a Payload map.
bool IsPayloadMap()
{
#if defined DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS
    return true;
#endif
    decl String:mapName[128];
    GetCurrentMap(mapName, sizeof(mapName));
    
    char buffers[3][100];
    int splits = ExplodeString(mapName, "_", buffers, sizeof(buffers), sizeof(buffers[]));
    // Map name does not follow the "nt_mapname_gamemode" pattern.
    if (splits < 2) {
        return false;
    }
    return StrEqual(buffers[2], g_sPayloadMapIdentifier);
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

// Convert the coordinator's attacker enum into a NT team enum.
int CoordEnumToTeamEnum(int coordEnum)
{
    if (coordEnum == COORD_ATTACKER_IS_JINRAI) {
        return TEAM_JINRAI;
    }
    else if (coordEnum == COORD_ATTACKER_IS_NSF) {
        return TEAM_NSF;
    }
    else {
        LogError("%s CoordEnumToTeamEnum: invalid team coord enum (%i)",
                g_sPluginTag, coordEnum);
        return TEAM_NONE;
    }
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

// Kill all in team, and then cancel their XP loss and death increment.
void SoftKillTeam(int team)
{
    for (int i = 1; i <= MaxClients; i++) {
        if (!IsClientConnected(i) || !IsPlayerAlive(i)) {
            continue;
        }
        if (GetClientTeam(i) == team) {
            ForcePlayerSuicide(i);
            SetPlayerDeaths(i, GetPlayerDeaths(i) - 1);
            SetPlayerXP(i, GetPlayerXP(i) + 1);
        }
    }
}

// Set the attackers, according to the OnUserN enum
void SetAttackingTeam(int team)
{
    if (team != TEAM_JINRAI && team != TEAM_NSF) {
        ThrowError("Tried to set invalid attacker (%i)", team);
    }

    g_iAttacker = team;

    PayloadMessage("Attacking team is now %s", g_sTeamNames[team]);
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

int GetAttackingTeam()
{
    return g_iAttacker;
}

int GetDefendingTeam()
{
    return g_iAttacker == TEAM_JINRAI ? TEAM_NSF : TEAM_JINRAI;
}

int GetOppositeTeam(int team)
{
    return team == TEAM_JINRAI ? TEAM_NSF : TEAM_JINRAI;
}
