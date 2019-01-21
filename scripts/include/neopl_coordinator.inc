#if defined _ntpl_coordinator_
    #endinput
#endif
#define _ntpl_coordinator_

#define MAX_COORDINATOR_INPUTS 4
#define MAX_PATH_NODES 128

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
    TIMECONTROL_OVERTIME_DISQUALIFY,
    TIMECONTROL_OVERTIME_QUALIFY,
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
        // TimeControl: All attackers have exited the cart's push area. If overtime, this will end it.
        "OnUser2",
        // TeamControl: Signal that the attacker has won the round.
        // TimeControl: At least one attacker has entered the cart's push area. This allows for overtime.
        "OnUser3",
        // TeamControl: No operation.
        // TimeControl: No operation.
        "OnUser4"
};
// Sanity check for outputs; cannot have more than we define.
#if sizeof(g_sCoordinatorOutputs) > MAX_COORDINATOR_INPUTS
#error "Too many coordinator outputs."
#endif
// Target names that the logic_relays should fire into to communicate with this plugin.
// These entities should not exist in the map, as they are created at runtime by this plugin!
new const String:g_sCoordinatorTargetname[COORDINATOR_ENUM_COUNT][] = {
    "pl_coord_team_control",
    "pl_coord_time_control"
};
// This could be any networked entity with a targetname field;
// I chose teleport destination because it has no discernible side effects.
new const String:g_sCoordinatorClassName[] = "info_teleport_destination";
// Payload path nodes are recognized by this naming, followed by an
// increasing number, ie. pl_path_1, pl_path_2...
// Node link is allowed to start anywhere in range 0-127,
// althought starting with 1 or 0 is probably most readable.
// Max amount of nodes: 128.
new const String:g_sPayloadPathNode[] = "pl_path_";
// Metadata about the payload version a map was built for.
// Payload map must have a info_teleport_destination with its
// name set to "pl_meta_version_1". Currently this does nothing,
// but is meant for possible future backwards compatibility,
// if the coordinator design ever changes.
new const String:g_sPayloadMetaNodeName[] = "pl_meta_version_";

int g_iCoordinatorEnt[COORDINATOR_ENUM_COUNT] = { 0 };

int g_iPathNodes[MAX_PATH_NODES] = { 0 };
static int iStartNode = -1;

DataPack g_dpCoordFuncs = null;

void StartPayload() {
    CreatePayloadCoordinators();
    CreateEntHooks();
    iStartNode = LookupPathNodes();
    if (iStartNode == -1) {
        PrintDebug(_, "Path nodes lookup failed. Is this a valid payload map?");
    }
    CreateDeadlineTimer();
}

void EndPayload() {
    ClearTimers();
    ClearEntHooks();
    ClearPayloadCoordinators();
    ClearPathNodes();
}

int LookupMetaNodeVersion()
{
    int maxEdicts = GetMaxEntities();
    decl String:entName[26]; // strlen("info_teleport_destination") + 1
    int version = -1;
    for (int i = 1; i <= maxEdicts; i++) {
        if (!IsValidEntity(i)) {
            continue;
        }
        if (!GetEntityClassname(i, entName, sizeof(entName))) {
            continue;
        }
        if (!StrEqual(entName, "info_teleport_destination")) {
            continue;
        }
        if (GetEntPropString(i, Prop_Data, "m_iName",
            entName, sizeof(entName)) < 1)
        {
            continue;
        }
        if (!StrContains(g_sPayloadMetaNodeName, entName)) {
            continue;
        }
        const int charPos = sizeof(g_sPayloadMetaNodeName) - 1;
        version = StringToInt(entName[charPos]);
        break;
    }
    return version;
}

// TODO: run only once per map to avoid expensive string operations
// Iterate map edicts, and find all path_track nodes
// following the payload naming scheme.
// If nodes were found, return the index of the lowest node.
// If nodes were not found or there was a problem with them, return -1.
int LookupPathNodes()
{
    int maxEdicts = GetMaxEntities();
    int lowestNode = maxEdicts + 1;
    decl String:entName[12]; // strlen("path_track") + 1
    for (int i = 1; i <= maxEdicts; i++) {
        if (!IsValidEntity(i)) {
            continue;
        }
        if (!GetEntityClassname(i, entName, sizeof(entName))) {
            continue;
        }
        if (!StrEqual(entName, "path_track")) {
            continue;
        }
        if (GetEntPropString(i, Prop_Data, "m_iName",
            entName, sizeof(entName)) < 1)
        {
            continue;
        }
        if (!StrContains(entName, g_sPayloadPathNode)) {
            continue;
        }
        const int charPos = sizeof(g_sPayloadPathNode) - 1;
        if (!IsCharNumeric(entName[charPos])) {
            continue;
        }
        int index = StringToInt(entName[charPos]);
        if (g_iPathNodes[index] != 0) {
            // Don't throw because this can happen due to mapper error.
            PrintDebug(_, "Node already exists for index %i", index);
            return -1;
        }
        g_iPathNodes[index] = i;
        if (index < lowestNode) {
            lowestNode = index;
        }
    }
    return lowestNode > maxEdicts ? -1 : lowestNode;
}

void ClearPathNodes()
{
    for (int i = 0; i < sizeof(g_iPathNodes); i++) {
        g_iPathNodes[i] = 0;
    }
}

// Create a networked entity, and hook it for receiving events.
static void CreatePayloadCoordinators()
{
    // Debug check, TODO: remove this
    for (int i = 0; i < COORDINATOR_ENUM_COUNT; i++) {
        if (g_iCoordinatorEnt[i] > 0) {
            PrintDebug(_, "CreatePayloadCoordinator: \
Called while g_iCoordinatorEnt[%i] > 0", i);
        }
    }
    for (int i = 0; i < COORDINATOR_ENUM_COUNT; i++) {
        PrintDebug(_, "Preparing to dispatch %s", g_sCoordinatorTargetname[i]);
        g_iCoordinatorEnt[i] = CreateEntityByName(g_sCoordinatorClassName);
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
    }
}

static void CreateEntHooks()
{
    // Hook the coordinator output.
    g_dpCoordFuncs.Reset();
    for (int i = 0; i < COORDINATOR_ENUM_COUNT; i++) {
        EntityOutput entOutFunc = view_as<EntityOutput>(g_dpCoordFuncs.ReadFunction());
        for (int j = 0; j < MAX_COORDINATOR_INPUTS; j++) {
            HookSingleEntityOutput(g_iCoordinatorEnt[i], g_sCoordinatorOutputs[j],
                entOutFunc, false);
        }
    }
    HookEntityOutput("path_track", "OnPass", OnPass);
}

int GetNumPathNodes()
{
    if (iStartNode == -1) {
        return 0;
    }
    else if (iStartNode >= sizeof(g_iPathNodes)) {
        ThrowError("Start node out of range");
    }
    int numNodes = 0;
    for (int i = iStartNode; i < sizeof(g_iPathNodes); i++) {
        if (g_iPathNodes[i] == 0) {
            break;
        }
        numNodes++;
    }
    return numNodes;
}

public void OnPass(const char[] output, int caller, int activator, float delay)
{
    int numNodes = GetNumPathNodes();
    if (numNodes != 0) {
        // Iterate through the nodes, starting from second node
        // because we don't want to AnnounceNodePass for the
        // first node that happens right on new round start.
        for (int i = iStartNode + 1; i < numNodes; i++) {
            if (caller != g_iPathNodes[i]) {
                continue;
            }
            // Add one because AnnounceNodePass starts
            // node count from 1, not zero.
            AnnounceNodePass(i - iStartNode + 1, numNodes);
        }
    }
}

static void ClearPayloadCoordinators()
{
    for (int i = 0; i < COORDINATOR_ENUM_COUNT; i++) {
        if (g_iCoordinatorEnt[i] == 0) {
            continue;
        }
        if (IsValidEntity(g_iCoordinatorEnt[i])) {
            decl String:entName[sizeof(g_sCoordinatorClassName)];
            if (!GetEntityClassname(g_iCoordinatorEnt[i],
                entName, sizeof(entName))) {
                LogError(
                    "%s Failed to fetch previous coordinator classname from %i",
                    g_sPluginTag, i);
            }
            else {
                PrintDebug(_, "Got valid coord %i with classname %s", i, entName);
                if (StrEqual(entName, g_sCoordinatorClassName)) {
                    if (!AcceptEntityInput(g_iCoordinatorEnt[i], "kill")) {
                        LogError("%s Failed to kill previous coordinator %s",
                            g_sPluginTag, g_sCoordinatorClassName);
                    }
                    else {
                        PrintDebug(_, "Coordinator %i kill ok", i);
                    }
                }
            }
        }
        g_iCoordinatorEnt[i] = 0;
    }
}

// Convert the received output string into coordinator int enum value.
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
        if (g_iCoordinatorEnt[i] == 0 || !IsValidEntity(g_iCoordinatorEnt[i])) {
            continue;
        }
        EntityOutput entOutFunc = view_as<EntityOutput>(g_dpCoordFuncs.ReadFunction());
        for (int j = 0; j < MAX_COORDINATOR_INPUTS; j++) {
            UnhookSingleEntityOutput(g_iCoordinatorEnt[i], g_sCoordinatorOutputs[j],
                entOutFunc);
        }
    }
    if (!UnhookEntityOutput("path_track", "OnPass", OnPass)) {
        PrintDebug(_, "No OnPass hook was found to unhook");
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
        // Only award extra time if not in overtime.
        if (g_hOvertimePoll == null) {
            IncrementDeadline();
            PayloadMessage(
                "Attackers have gained %i more seconds by reaching a checkpoint!",
                g_cDeadlineExtendTime.IntValue);
        }
    }
    else if (cmdEnum == TIMECONTROL_OVERTIME_DISQUALIFY) {
        SetIsQualifiedForOvertime(false);
    }
    else if (cmdEnum == TIMECONTROL_OVERTIME_QUALIFY) {
        SetIsQualifiedForOvertime(true);
    }
}
