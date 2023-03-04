### Payload map-side logic

#### Mapmakers must...
  * Name their map such that it ends with a `_pl`. For example: *nt_mymap_pl* (.vmf, .bsp)
    * This name check is used to determine whether the payload mode should activate for the current map
  * Make sure spawnrooms are inaccessible to the opposing team when active.
  * Abide by the <a href="#plugin-interactions">Plugin interactions</a> rules.

### Concepts

#### Attackers
*Attackers* can push the cart by standing near, the speed of the cart will increase for every *attacker* within range.
If a *defender* is near the cart, it will not move.
Currently, there is **NO** rollback feature.

#### Defenders
*Defenders* win if time runs out.

#### Victory state
*Victory State* for attackers is achieved by the final *path_track* sending an output to a *logic_relay* that will be used as a hook for the plugin.
 
#### Coordinators
*"Coordinators"* are entities created by the plugin at runtime (ie. they should not be placed by mappers) for sending receiving map events, as described in [Entity I/O cheat sheet](#entity-io-cheat-sheet).

#### Spawnrooms
Movable spawns are implemented by using multiple *trigger_teleports*.

Spawn rooms are enabled and disabled by use of a *logic_relay* disabling and enabling the corresponding *trigger_teleports*.

#### Plugin interactions
The plugin is required to do a few things when receiveing a set of outputs from the map:

(TLDR for mappers: see the [I/O cheat sheet](#entity-io-cheat-sheet))
 
* Keep track of which plugin version the map was built for.
  * Payload maps must include one unused *info_teleport_destination*, with the name
    **pl_meta_version_2**. Its purpose is to provide backwards compatibility with maps
    built for earlier versions of this plugin. Missing this entity (or using
    a version differing from the server's) is an error.

* Add time when each capturepoint logic_relay is triggered.
  * This is done with a *FireUser1* call to the *pl_coord_time_control* coordinator,
    and the call activator must be of type func_tracktrain.
  * Good place to do this is in the **OnPass** output of a path_track node of the payload cart's path.

* Announce attacker progress percentage each time a payload path_track is passed.
  * The map needs no triggers for this; it's done automatically by capturing the path nodes'
    *OnPass* output, as long as the pl paths follow a *pl_path_N* naming scheme,
    where N is a whole number and all the nodes follow it incrementally.<br>
    Ie. *pl_path_1, pl_path_2, ...*
  * Max of 128 nodes are supported, anywhere in range 0-127, and they must be
    sequential, ie. pl_path_1 cannot be followed by a pl_path_3.

* Set the attacking team at round beginning.
  * This is done with a *FireUser2* call to the *pl_coord_time_control* coordinator,
    and the call activator must be a player in team Jinrai or team NSF.
  * Good place for this is your initial spawn teleport brush touch activation.

* End the game when the payload goal is reached.
  * This is done with a *FireUser1* call to *pl_coord_team_control* coordinator.

* Enter overtime when round time ends but attackers are still within the payload area.
  * When at least 1 attacker enters the payload push area, send a *FireUser3* to
    *pl_coord_time_control* coordinator. This tells the plugin that attackers
    are qualified to an overtime if the time runs out.
  * When all attackers have left the payload push area, send a *FireUser2* to
    *pl_coord_time_control* coordinator. This tells the plugin that attackers
    are no longer qualified to an overtime if the time runs out.
  * Map needn't worry about the overtime logic, this is handled plugin side,
    as long as the two time control calls above are correctly fired.
  * These time control inputs can be safely fired at any time, even if it isn't overtime yet.

### Entity I/O cheat sheet
The target entities are created dynamically; do not create them in the level.

| I/O target entity | I/O command | Resulting plugin action |
|---|---|---|
| pl_coord_team_control | FireUser1 | Signal that the attacker has won the round. |
| pl_coord_team_control | FireUser2 | If activator of this command is a player (eg. via them touching a trigger brush), assign their team as the attacker. |
| pl_coord_team_control | FireUser3 | No operation. |
| pl_coord_team_control | FireUser4 | No operation. |
| | | |
| pl_coord_time_control | FireUser1 | Increment deadline. Activator of this command must be a func_tracktrain (the payload cart), presumably by triggering the *OnPass* output of a path_track entity. |
| pl_coord_time_control | FireUser2 | All attackers have exited the cart's push area. If overtime, this will end it. |
| pl_coord_time_control | FireUser3 | At least one attacker has entered the cart's push area. This allows for overtime. |
| pl_coord_time_control | FireUser4 | No operation. |

<!-- TODO: move this to trello or issue tracker? 
### Further additions
 * Rollback could be implemented using a delayed OnEndTouchAll and CancelPending 
  * Some additional features that would be a bonus would be things like a modified HUD, but I don't know if a plugin can do that.
    Alternatively we just use a text entity to say the current status of the cart at the bottom of the screen.
-->

# For plugin developers
* The project uses 4 spaces for indent
* Supporting SourceMod range 1.7-1.11 (and probably at least 1.12, once stable)
  * The compiler will emit the following warning:
    `warning 237: coercing functions to and from primitives is unsupported and will be removed in the future`
    This can be safely ignored in the supported SM range (although the code should be refactored eventually to properly address it).
