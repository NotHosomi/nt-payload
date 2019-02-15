#if defined _base_
    #endinput
#endif
#define _base_

#define PLUGIN_VERSION "0.4"

new const String:g_sPluginTag[] = "[PAYLOAD]";
new const String:g_sTeamNames[][] = {
    "none", "spectator", "Jinrai", "NSF"
};

enum {
    DEBUGLVL_NONE = 0,
    DEBUGLVL_PUBLIC_TEST,
    DEBUGLVL_PUBLIC_TEST_VERBOSE,
    DEBUGLVL_DEV
};

// Payload maps are identified by the naming convention nt_mapname_gamemode,
// where the third value divided by underscores equals this value
// (e.g. "nt_example_pl_a43").
new const String:payload_id[] = "pl";

ConVar g_cRoundTime = null, g_cDeadlineExtendTime = null,
    g_cDebugLvl = null, g_cDebugAllMapsArePlMaps = null,
    g_cNodePassAnnounceFreq = null;

Handle g_hDeadline = null, g_hDeadlineCountdown = null,
    g_hOvertimePoll = null;

// Once the deadline ends, count down from this value to defender victory.
const int countdownSecs = 10;

static int iLastNodeAnnounce = 0;

static bool bIsQualifiedForOvertime = false;

void SetIsQualifiedForOvertime(bool qualifiedForOt)
{
    PrintDebug(_, "SetIsQualifiedForOvertime %i", qualifiedForOt);
    bIsQualifiedForOvertime = qualifiedForOt;
}

bool GetIsQualifiedForOvertime()
{
    return bIsQualifiedForOvertime;
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

void PrintDebug(bool onlyOnce = true, const char[] msg, any ...)
{
    if (g_cDebugLvl.IntValue == DEBUGLVL_NONE) {
        return;
    }
    // TODO figure out how to nicely allocate just needed amount
    int bufLen = strlen(msg) + 512;
    decl String:buffer[bufLen];
    if (VFormat(buffer, bufLen, msg, 3) < 1) {
        ThrowError("VFormat failed for %s", msg);
    }
    // Only continue if this message is different from the last one.
    if (onlyOnce) {
        static char lastMessage[1024];
        bool wasEqual = StrEqual(buffer, lastMessage);
        strcopy(lastMessage, sizeof(lastMessage), buffer);
        if (wasEqual) {
            return;
        }
    }
    if (g_cDebugLvl.IntValue >= DEBUGLVL_PUBLIC_TEST) {
        PrintToConsoleAll("[debug] %s", buffer);
        LogToFile("addons/sourcemod/logs/payload.log", buffer);
    }
    if (g_cDebugLvl.IntValue >= DEBUGLVL_PUBLIC_TEST_VERBOSE) {
        PrintToChatAll("[debug] %s", buffer);
    }
    if (g_cDebugLvl.IntValue >= DEBUGLVL_DEV) {
        PrintToServer("[debug] %s", buffer);
    }
}

// Takes currently passed node position (starting from 1),
// and the total amount of payload nodes in the map.
// Announces the percentage of the payload track completed.
// TODO: calculate node distances to get an accurate reading;
// currently this naïvely assumes nodes are equal distance
// away from each other.
void AnnounceNodePass(int nodePos, int numNodesTotal)
{
    if (numNodesTotal < 1) {
        ThrowError("Got total nodes < 1");
    }
    else if (nodePos < 1) {
        ThrowError("Got node pos < 1");
    }
    else if (nodePos > numNodesTotal) {
        ThrowError("Got node pos higher than nodes total");
    }
    int percentComplete = RoundToNearest(100.0 * nodePos / numNodesTotal);
    // How many percent since the last announce?
    // If >= announce frequency, we're allowed to announce again.
    if (percentComplete - iLastNodeAnnounce >= g_cNodePassAnnounceFreq.IntValue)
    {
        PayloadMessage("Attacker progress: %i\%", percentComplete);
        iLastNodeAnnounce = percentComplete;
    }
}

void IncrementDeadline()
{
    // Get round times
    float roundTimeLeft = GameRules_GetPropFloat("m_fRoundTimeLeft");
    float newTime = roundTimeLeft + g_cDeadlineExtendTime.IntValue;
    // Change game internal round timer
    GameRules_SetPropFloat("m_fRoundTimeLeft", newTime);
    // Change plugin timer
    CreateDeadlineTimer(true, newTime);
    PrintDebug(_, "Called CreateDeadlineTimer with %f time", newTime);
}

// Cue the confetti.
void DeclareVictory(int winner)
{
    EndPayload();

    if (winner != TEAM_JINRAI && winner != TEAM_NSF) {
        PayloadMessage("Error with win condition; payload disabled for this \
round!");
        // DEBUG/TODO: this can throw if mapper fires the wrong input
        ThrowError("Declared winner with invalid team (%i)", winner);
    }

    // HACK/TODO: Just kill all losers for now to force round end.
    SetRespawning(false);
    SoftKillTeam(GetOppositeTeam(winner));

    if (winner == GetAttackingTeam()) {
        PayloadMessage("%s wins by delivering the payload!",
            g_sTeamNames[winner]);
    }
    else {
        PayloadMessage("%s wins by defending the base!", g_sTeamNames[winner]);
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

// Return whether the current map is a Payload map.
bool IsPayloadMap()
{
    decl String:mapName[128];
    GetCurrentMap(mapName, sizeof(mapName));
    
    char buffers[3][100];
    int splits = ExplodeString(mapName, "_", buffers,
        sizeof(buffers), sizeof(buffers[]));
    // Map name does not follow the "nt_mapname_gamemode" pattern.
    if (splits < 2) {
        return false;
    }
    return StrEqual(buffers[2], payload_id);
}