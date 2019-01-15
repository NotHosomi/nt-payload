#if defined _timer_
    #endinput
#endif
#define _timer_

// Bool flag to detect if we got interrupted by time extension.
static bool _dirtyDeadlineTimer = false;

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
    if (g_hDeadlineCountdown != null) {
        LogError("%s g_hDeadlineCountdown was not null!", g_sPluginTag);
        delete g_hDeadlineCountdown;
        g_hDeadlineCountdown = null;
    }
    
    g_hDeadlineCountdown = CreateTimer(1.0, Timer_DeadlineCountdown, _, TIMER_REPEAT);
    _dirtyDeadlineTimer = true;
    if (g_hDeadlineCountdown == null) {
        LogError("%s Failed to create timer g_hDeadlineCountdown", g_sPluginTag);
    }
    
    g_hDeadline = null;
    return Plugin_Stop;
}

public Action Timer_DeadlineCountdown(Handle timer)
{
    static int countdown = countdownSecs;
    // Countdown was interrupted before it finished;
    // revert value back to max.
    if (_dirtyDeadlineTimer) {
        countdown = countdownSecs;
        _dirtyDeadlineTimer = false;
    }
    else if (countdown == 0) {
        g_hDeadlineCountdown = null;
        countdown = countdownSecs;
        _dirtyDeadlineTimer = false;
        DeclareVictory(GetDefendingTeam());
        return Plugin_Stop;
    }
    PayloadMessage("Time remaining: %i", countdown--);
    return Plugin_Continue;
}