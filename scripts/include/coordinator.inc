#if defined _ntpl_coordinator_
    #endinput
#endif
#define _ntpl_coordinator_

#define MAX_COORDINATOR_INPUTS 4

enum {
    COORDINATOR_TEAMCONTROL = 0,
    COORDINATOR_TIMECONTROL,
    COORDINATOR_ENUM_COUNT
};

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
// These entities should not exist in the map, as they are created at runtime by this plugin!
new const String:g_sCoordinatorTargetname[COORDINATOR_ENUM_COUNT][] = {
    "pl_coord_team_control",
    "pl_coord_time_control"
};

int g_iCoordinatorEnt[COORDINATOR_ENUM_COUNT] = { 0 };

DataPack g_dpCoordFuncs = null;

void StartPayload() {
    CreatePayloadCoordinators();
    CreateDeadlineTimer();
}

void EndPayload() {
    // Clear the timers.
    if (g_hDeadline != null) {
        delete g_hDeadline;
        g_hDeadline = null;
    }
    if (g_hDeadlineCountdown != null) {
        delete g_hDeadlineCountdown;
        g_hDeadlineCountdown = null;
    }

    ClearEntHooks();
}

// Create a networked entity, and hook it for receiving events.
// TODO // is the old coordinator deleted on round end? -H
static void CreatePayloadCoordinators()
{
    // Debug check, TODO: remove this
    for (int i = 0; i < COORDINATOR_ENUM_COUNT; i++) {
        if (g_iCoordinatorEnt[i] > 0) {
            PrintDebug(_, "CreatePayloadCoordinator: \
Called while g_iCoordinatorEnt[%i] > 0", i);
        }
    }
    
    ClearEntHooks();
    
    g_dpCoordFuncs.Reset();
    for (int i = 0; i < COORDINATOR_ENUM_COUNT; i++) {
        PrintDebug(_, "Preparing to dispatch %s", g_sCoordinatorTargetname[i]);
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

// Remove the entity hooks.
static void ClearEntHooks()
{        
    g_dpCoordFuncs.Reset();
    for (int i = 0; i < COORDINATOR_ENUM_COUNT; i++) {
        if (!g_iCoordinatorEnt[i] || !IsValidEntity(g_iCoordinatorEnt[i])) {
            continue;
        }
        EntityOutput entOutFunc = view_as<EntityOutput>(g_dpCoordFuncs.ReadFunction());
        for (int j = 0; j < MAX_COORDINATOR_INPUTS; j++) {
            UnhookSingleEntityOutput(g_iCoordinatorEnt[i], g_sCoordinatorOutputs[j],
                entOutFunc);
        }
        g_iCoordinatorEnt[i] = 0;
    }
}

// Coordinate TeamControl output events sent to coordinator from the map.
public void TeamControl(const char[] output, int caller,
        int activator, float delay)
{
    PrintDebug(_, "TeamControl: %s", output);

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
    PrintDebug(_, "TimeControl: %s", output);
    
    int cmdEnum = GetCoordEnum(output);
    if (cmdEnum == -1) {
        ThrowError("GetCoordEnum failed on output: %s", output);
    }
    else if (cmdEnum == TIMECONTROL_INCREMENT) {
        IncrementDeadline();
        const int increment = 30; // TODO cvar or something
        PayloadMessage(_,
            "Attackers have gained %i more seconds by reaching a checkpoint!",
            increment);
    }
}