/*
    This is an accompanying plugin for a custom Neotokyo Payload game mode.
    For map makers:
        Please see the repository for documentation and example maps:
        https://github.com/NotHosomi/nt-payload
*/

#pragma semicolon 1

#include <sourcemod>
#include <sdkhooks>
#include <sdktools>
#include <neotokyo>

#include "include/neopl_shims.inc"
#include "include/neopl_base.inc"
#include "include/neopl_coordinator.inc"
#include "include/neopl_events.inc"
#include "include/neopl_teams.inc"
#include "include/neopl_timer.inc"

public Plugin myinfo = {
    name = "NEOTOKYO° Payload",
    description = "A custom Payload gamemode for Neotokyo.",
    version = PLUGIN_VERSION,
    author = "Rain, Hosomi",
    url = "https://github.com/NotHosomi/nt-payload"
};