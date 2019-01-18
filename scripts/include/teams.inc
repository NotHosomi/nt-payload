#if defined _teams_
    #endinput
#endif
#define _teams_

static int g_iAttacker = TEAM_NONE;

// Convert the coordinator's attacker enum into a NT team enum.
int CoordEnumToTeamEnum(int coordEnum)
{
    if (coordEnum == TEAMCONTROL_ATTACKER_IS_JINRAI) {
        return TEAM_JINRAI;
    }
    else if (coordEnum == TEAMCONTROL_ATTACKER_IS_NSF) {
        return TEAM_NSF;
    }
    LogError("%s CoordEnumToTeamEnum: invalid team coord enum (%i)",
        g_sPluginTag, coordEnum);
    return TEAM_NONE;
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

    // Map maker has the responsibility to set their
    // trigger to fire once per round to avoid message spam here.
    PayloadMessage("Attacking team is now %s", g_sTeamNames[team]);
}

int GetAttackingTeam()
{
    return g_iAttacker;
}

int GetDefendingTeam()
{
    return GetOppositeTeam(GetAttackingTeam());
}

int GetOppositeTeam(int team)
{
    return team == TEAM_JINRAI ? TEAM_NSF : TEAM_JINRAI;
}