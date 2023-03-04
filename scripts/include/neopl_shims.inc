#if defined _ntpl_shims_
    #endinput
#endif
#define _ntpl_shims_

// This file contains compatibility shims for supporting older SM versions.


// Backported from SourceMod/SourcePawn SDK for SM < 1.9 compatibility.
// Used here under GPLv3 license: https://www.sourcemod.net/license.php
// SourceMod (C)2004-2008 AlliedModders LLC.  All rights reserved.
#if SOURCEMOD_V_MAJOR <= 1 && SOURCEMOD_V_MINOR < 9
/**
 * Sends a message to every client's console.
 *
 * @param format        Formatting rules.
 * @param ...           Variable number of format parameters.
 */
stock void PrintToConsoleAll(const char[] format, any ...)
{
    char buffer[254];

    for (int i = 1; i <= MaxClients; i++)
    {
        if (IsClientInGame(i))
        {
            SetGlobalTransTarget(i);
            VFormat(buffer, sizeof(buffer), format, 2);
            PrintToConsole(i, "%s", buffer);
        }
    }
}
#endif