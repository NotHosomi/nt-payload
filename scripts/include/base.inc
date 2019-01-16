#if defined _base_
    #endinput
#endif
#define _base_

#define PLUGIN_VERSION "0.3"

#define DEBUG_PRINT_MORE_DETAILS
//#define DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS // Uncomment to disable the map name check.

new const String:g_sPluginTag[] = "[PAYLOAD]";
new const String:g_sTeamNames[][] = {
    "none", "spectator", "Jinrai", "NSF"
};

ConVar g_cRoundTime = null;

Handle g_hDeadline = null;
Handle g_hDeadlineCountdown = null;

// Once the deadline ends, count down from this value to defender victory.
const int countdownSecs = 10;

// Format and print Payload announcements with the plugin tag included.
void PayloadMessage(bool onlyOnce = true, const char[] message, any ...)
{
    // TODO figure out how to nicely allocate just needed amount
    int bufLen = strlen(message) + 512;
    decl String:buffer[bufLen];
    VFormat(buffer, bufLen, message, 3);
    // Only continue if this message is different from the last one.
    if (onlyOnce) {
        static char lastMessage[1024];
        bool wasEqual = StrEqual(buffer, lastMessage);
        strcopy(lastMessage, sizeof(lastMessage), buffer);
        if (wasEqual) {
            return;
        }
    }
    PrintToChatAll("%s %s", g_sPluginTag, buffer);
}

void PrintDebug(bool onlyOnce = true, const char[] msg, any ...)
{
#if defined DEBUG_PRINT_MORE_DETAILS
    // TODO figure out how to nicely allocate just needed amount
    int bufLen = strlen(msg) + 512;
    decl String:buffer[bufLen];
    if (VFormat(buffer, bufLen, msg, 2) < 1) {
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
    PrintToChatAll("[debug] %s", buffer);
    PrintToServer("[debug] %s", buffer);
#endif
}

void IncrementDeadline()
{
    // Get round times
    float roundTimeLeft = GameRules_GetPropFloat("m_fRoundTimeLeft");
    // Change game internal round timer
    const int increment = 30; // TODO cvar or something
    GameRules_SetPropFloat("m_fRoundTimeLeft", roundTimeLeft + increment);
    // Change plugin timer
    CreateDeadlineTimer(true, roundTimeLeft + increment);
    PrintDebug(_, "Called CreateDeadlineTimer with %f time", roundTimeLeft + increment);
}

// Delete any existing deadline timers, and create a new one.
// If no arguments are provided, assume a timer expiring at the end
// of the round, according to g_cRoundTime. If custom time is provided,
// note that the timer will take the 10..9..8.. end of round timer
// into account already - you will only need to provide it with the
// desired round time remaining.
void CreateDeadlineTimer(const bool wantCustomTime = false, float customTimeSecs = 0.0)
{
    // Kill deadline timer, if it exists.
    if (g_hDeadline != null) {
        if (g_hDeadlineCountdown != null) {
            LogError("%s Both deadline and countdown timers were active at the same time!",
                g_sPluginTag);
            delete g_hDeadlineCountdown;
            g_hDeadlineCountdown = null;
        }
        delete g_hDeadline;
        g_hDeadline = null;
    }
    // Kill the end of round countdown timer, if it exists.
    else if (g_hDeadlineCountdown != null) {
        delete g_hDeadlineCountdown;
        g_hDeadlineCountdown = null;
    }
    // Timers aren't exactly precise, so give a grace time of ~2 seconds
    // before actual round end for triggering the defender victory.
    const float timerInaccuracy = 3.0;
    // User provided their own round time remaining to use.
    if (wantCustomTime) {
        // Need at least this much time to fit the countdown timer in round.
        float minimumTimerLen = 2 * countdownSecs + timerInaccuracy;
        if (customTimeSecs < minimumTimerLen) {
            LogError("%s customTimeSecs of %f is too small; adjusting to %f",
                g_sPluginTag, customTimeSecs, minimumTimerLen); 
            customTimeSecs = minimumTimerLen;
            // We also need to update game timer if we correct.
            // TODO: kind of messy, this should probably live in IncrementDeadline.
            GameRules_SetPropFloat("m_fRoundTimeLeft", minimumTimerLen);
        }
        g_hDeadline = CreateTimer(customTimeSecs - countdownSecs - timerInaccuracy, Timer_Deadline);
    }
    // Else, assume we want a full round timer.
    else {
        g_hDeadline = CreateTimer(
            g_cRoundTime.FloatValue * 60 - countdownSecs - timerInaccuracy,
            Timer_Deadline);
    }
    if (g_hDeadline == null) {
        LogError("%s Failed to CreateTimer on Event_RoundStart", g_sPluginTag);
    }
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
        PayloadMessage(_, "%s wins by delivering the payload!", g_sTeamNames[winner]);
    }
    else {
        PayloadMessage(_, "%s wins by defending the base!", g_sTeamNames[winner]);
    }
    
    ClearEntHooks();
}

// Toggle respawning with Neotokyo's warmup game state.
void SetRespawning(bool respawnEnabled)
{
    const int respawn = 1, normal = 2;
    int gamestate;

    if (respawnEnabled) {
        gamestate = respawn;
        PayloadMessage(_, "Respawning is now enabled");
    } else {
        gamestate = normal;
        PayloadMessage(_, "Respawning is now disabled");
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