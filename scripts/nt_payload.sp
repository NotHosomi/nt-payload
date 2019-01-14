/*
    This is an accompanying plugin for a custom Neotokyo Payload game mode.
    For map makers: see the comments on usage starting around line 24 in base.inc
    
    An example .vmf with logic_relays implemented:
    https://gist.githubusercontent.com/Rainyan/b1c48f048e15f62999aeb2f66f22e56b/raw/f29762a21ab1b04aecc97005c0ab17a7c500a5ed/nt_payload_plugin-relays.vmf
*/

#pragma semicolon 1

#include <sourcemod>
#include <sdktools>
#include <neotokyo>

#include "include/base.inc"
#include "include/events.inc"
#include "include/teams.inc"
#include "include/timer.inc"

public Plugin myinfo = {
    name = "NEOTOKYO° Payload",
    description = "A custom Payload gamemode for Neotokyo.",
    version = PLUGIN_VERSION,
    author = "Rain, Hosomi",
    url = "https://github.com/NotHosomi/nt-payload"
};