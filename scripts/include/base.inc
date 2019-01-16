#if defined _base_
    #endinput
#endif
#define _base_

#define PLUGIN_VERSION "0.3"

// Uncomment to disable verbose debug messages.
#define DEBUG_PRINT_TO_CHAT
#define DEBUG_PRINT_TO_CONSOLE
#define DEBUG_PRINT_TO_SERVER

// Uncomment to disable the map name check.
//#define DEBUG_ALL_MAPS_ARE_PAYLOAD_MAPS

// Uncomment to keep bot payload going even if all humans disconnected.
// SourceTV bot is always ignored.
//#define DEBUG_BOT_PLAYERS_COUNT_AS_HUMANS

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
#if defined DEBUG_PRINT_TO_CHAT ||\
    defined DEBUG_PRINT_TO_CONSOLE ||\
    defined DEBUG_PRINT_TO_SERVER
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
#if defined DEBUG_PRINT_TO_CHAT
    PrintToChatAll("[debug] %s", buffer);
#endif

#if defined DEBUG_PRINT_TO_CONSOLE
    PrintToConsoleAll("[debug] %s", buffer);
#endif

#if defined DEBUG_PRINT_TO_SERVER
    PrintToServer("[debug] %s", buffer);
#endif
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
        g_hDeadline = CreateTimer(
            customTimeSecs - countdownSecs - timerInaccuracy,
            Timer_Deadline, TIMER_FLAG_NO_MAPCHANGE);
    }
    // Else, assume we want a full round timer.
    else {
        g_hDeadline = CreateTimer(
            g_cRoundTime.FloatValue * 60 - countdownSecs - timerInaccuracy,
            Timer_Deadline, TIMER_FLAG_NO_MAPCHANGE);
    }
    if (g_hDeadline == null) {
        LogError("%s Failed to CreateTimer on Event_RoundStart", g_sPluginTag);
    }
}

// Cue the confetti.
void DeclareVictory(int winner)
{
    EndPayload();

    if (winner != TEAM_JINRAI && winner != TEAM_NSF) {
        PayloadMessage("Error with win condition; payload disabled for this round!");
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