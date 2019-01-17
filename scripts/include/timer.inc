#if defined _timer_
    #endinput
#endif
#define _timer_

// Bool flag to detect if we got interrupted by time extension.
static bool _dirtyDeadlineTimer = false;

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
    // Timers aren't exactly precise, so give a grace time of a few seconds
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

void ClearTimers()
{
    // Clear the timers.
    if (g_hDeadline != null) {
        delete g_hDeadline;
        g_hDeadline = null;
    }
    if (g_hDeadlineCountdown != null) {
        delete g_hDeadlineCountdown;
        g_hDeadlineCountdown = null;
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
    if (g_hDeadlineCountdown != null) {
        LogError("%s g_hDeadlineCountdown was not null!", g_sPluginTag);
        delete g_hDeadlineCountdown;
        g_hDeadlineCountdown = null;
    }
    
    g_hDeadlineCountdown = CreateTimer(1.0, Timer_DeadlineCountdown, _,
        TIMER_FLAG_NO_MAPCHANGE | TIMER_REPEAT);
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