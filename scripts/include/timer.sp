#if defined _timer_
    #endinput
#endif
#define _timer_

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
