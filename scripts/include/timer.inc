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
    g_hDeadline = null;
    return Plugin_Stop;
}