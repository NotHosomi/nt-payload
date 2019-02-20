### Table of contents
1. [Payload map-side logic](#payload-map-side-logic)
2. [Mapmakers must...](#mapmakers-must)
3. [Gamemode](#gamemode)
4. [Spawnrooms](#spawnrooms)
5. [Plugin interactions](#plugin-interactions)
6. [Entity I/O cheat sheet](#entity-io-cheat-sheet)
7. [Further additions](#further-additions)

### Payload map-side logic

  <b>TODO/FIXME</b>: currently out of date with plugin logic!<br>
 <strike>For copy-pasting into a map, the spawnroom and the central room contain all required logic.</strike>
 
 Input firing is described under <a href="#plugin-interactions">Plugin interactions</a> below.

### Mapmakers must...
  * Make sure spawnrooms are inaccessible to the opposing team when active.
  * Abide by the <a href="#plugin-interactions">Plugin interactions</a> rules.

### Gamemode
 <i>Attackers</i> can push the cart by standing near, the speed of the cart will increase for every <i>attacker</i> within range.
 If a <i>defender</i> is near the cart, it will not move.
 Currently, there is **NO** rollback feature.

 <b>Victory State</b> is achieved by the final <i>path_track</i> sending an output to a <i>logic_relay</i> that will be used as a hook for the plugin to declare <i>attacker victory</i>.
 
 <i>Defenders</i> win if time runs out.

### Spawnrooms
  Movable spawns are implemented by using multiple <i>trigger_teleports</i>.
  Spawn rooms are enabled and disabled by use of a <i>logic_relay</i> disabling and enabling the corresponding <i>trigger_teleports</i>.

### Plugin interactions
 The plugin is required to do a few things when receiveing a set of outputs from the map:<br>
 (TLDR: see the [I/O cheat sheet](#entity-io-cheat-sheet))
 
 * Keep track of which plugin version the map was built for.
     * Payload maps must include one unused <i>info_teleport_destination</i>, with the name
       <i>pl_meta_version_1</i>. This is currently unused, but ensures backwards compatibility
       without breaking existing maps, should the plugin design ever change in the future.
 
 * Add time when each capturepoint logic_relay is triggered.
     * This is done with a <i>FireUser1</i> call to <i>"pl_coord_time_control"</i> coordinator.
 
 * Announce attacker progress percentage each time a payload path_track is passed.
     * Map needs no triggers for this; it's done automatically by capturing the path nodes'
       <i>OnPass</i> output, as long as the pl paths follow a <i>pl_path_N</i> naming scheme,
       where N is a whole number and all the nodes follow it incrementally.<br>
       Ie. <i>pl_path_1, pl_path_2, ...</i>
     * Max 128 nodes are supported, anywhere in range 0-127, and they must be
       sequential.
       
 * Set the attacking team at round beginning.
     * This is done with 1 or more <i>trigger_once</i> brushes named <i>pl_attackerspawn</i>,
       placed in the attacker spawn such that they trigger as the attackers spawn.
     * Max supported <i>pl_attackerspawn</i> brushes (4) is defined by the MAX_SPAWN_BRUSHES plugin define.
 
 * End the game when the logic_relay AttackerWin is triggered.
     * This is done with a <i>FireUser1</i> call to <i>"pl_coord_team_control"</i> coordinator.
 
 * Enter overtime when round time ends but attackers are still within the payload area.
     * When at least 1 attacker enters the payload push area, send a <i>FireUser3</i> to
       <i>pl_coord_time_control</i> coordinator. This tells the plugin that attackers
       are qualified to an overtime if the time runs out.
     * When all attackers have left the payload push area, send a <i>FireUser2</i> to
       <i>pl_coord_time_control</i> coordinator. This tells the plugin that attackers
       are no longer qualified to an overtime if the time runs out.
     * Map needn't worry about the overtime logic, this is handled plugin side.
       These inputs can be safely fired even if it isn't overtime yet.

 <!-- TODO: move this to trello or issue tracker?
 Some additional features that would be a bonus would be things like a modified HUD, but I don't know if a plugin can do that.
 Alternatively we just use a text entity to say the current status of the cart at the bottom of the screen.-->

### Entity I/O cheat sheet
The target entities are created dynamically; do not create them in the level.

| I/O target entity | I/O command | Resulting plugin action |
|---|---|---|
| pl_coord_team_control | FireUser1 | Signal that the attacker has won the round. |
| pl_coord_team_control | FireUser2 | No operation. |
| pl_coord_team_control | FireUser3 | No operation. |
| pl_coord_team_control | FireUser4 | No operation. |
| | | |
| pl_coord_time_control | FireUser1 | Increment deadline. |
| pl_coord_time_control | FireUser2 | All attackers have exited the cart's push area. If overtime, this will end it. |
| pl_coord_time_control | FireUser3 | At least one attacker has entered the cart's push area. This allows for overtime. |
| pl_coord_time_control | FireUser4 | No operation. |

<!-- TODO: move this to trello or issue tracker? -->
### Further additions
 Rollback could be implemented using a delayed OnEndTouchAll and CancelPending
 HUD and cart notifications could be implemented with displaying text on player's screen
