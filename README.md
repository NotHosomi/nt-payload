### Table of contents
1. [Payload map-side logic](#payload-map-side-logic)
2. [Mapmakers must...](#mapmakers-must)
3. [Gamemode](#gamemode)
4. [Spawnrooms](#spawnrooms)
5. [Plugin interactions](#plugin-interactions)
6. [Entity I/O cheat sheet](#entity-io-cheat-sheet)
7. [Further additions](#further-additions)

### Payload map-side logic

 For copy-pasting into a map, the spawnroom and the central room contain all required logic
 
 <b>TODO</b>: currently out of date with plugin v. 0.3+!<br>
 Input firing is described under <a href="#plugin-interactions">Plugin Interactions</a> below.
 

### Mapmakers must...
  * make sure spawnrooms are inaccessible to the opposing team when active.
  * abide by the <a href="#plugin-interactions">Plugin Interactions</a> rules.
  


### Gamemode
 ATTACKERS can push the cart by standing near, the speed of the cart will increase for every ATTACKER within range.
 If a DEFENDER is near the cart, it will not move.
 Currently, there is **NO** rollback feature.

 Victory State is achieved by the final path_track sending an output to a logic_relay that will be used as a hook for the plugin to declare ATTACKER victory.
 DEFENDERS win if time runs out.



### Spawnrooms
  Movable spawns are implemented by using multiple trigger_teleports
  Spawn rooms are enabled and disabled by use of a logic_relay disabling and enabling the corresponding trigger_teleports



### Plugin interactions
 The plugin is required to do a few things when receiveing a set of outputs from the map:<br>
 (TLDR: see the [I/O cheat sheet](#entity-io-cheat-sheet))
 
 * Keep track of which plugin version the map was built for.
     * payload maps must include one unused <i>info_teleport_destination</i>, with the name
       <i>pl_meta_version_1</i>. this is currently unused, but ensures backwards compatibility
       without breaking existing maps, should the plugin design ever change in the future.
 
 * Add time when each capturepoint logic_relay is triggered.
     * this is done with a <i>FireUser1</i> call to <i>"pl_coord_time_control"</i> coordinator.
 
 * Announce attacker progress percentage each time a payload path_track is passed.
     * map needs no triggers for this; it's done automatically by capturing the path nodes'
       <i>OnPass</i> output, as long as the pl paths follow a <i>pl_path_N</i> naming scheme,
       where N is a whole number and all the nodes follow it incrementally,
       ie. <i>pl_path_1, pl_path_2, ...</i>
     * max 128 nodes are supported, anywhere in range 0-127, although they do need to be
       sequential.
       
 * Set the attacking team at round beginning.
     * this is done with 1 or more <i>trigger_once</i> brushes named "pl_attackerspawn",
       placed in the attacker spawn such that they trigger as the attackers spawn.
     * max supported brushes (4) is defined by the MAX_SPAWN_BRUSHES plugin define.
 
 * End the game when the logic_relay AttackerWin is triggered.
     * this is done with a <i>FireUser1</i> call to <i>"pl_coord_team_control"</i> coordinator.
 
 * Enter overtime when round time ends but attackers are still within the payload area.
     * when at least 1 attacker enters the payload push area, send a <i>FireUser3</i> to
       <i>pl_coord_time_control</i> coordinator. this tells the plugin that attackers
       are qualified to an overtime if the time runs out.
     * when all attackers have left the payload push area, send a <i>FireUser2</i> to
       <i>pl_coord_time_control</i> coordinator. this tells the plugin that attackers
       are no longer qualified to an overtime if the time runs out.
     * map needn't worry about the overtime logic, this is handled plugin side.
       these inputs can be safely fired even if it isn't overtime yet.

 Some additional features that would be a bonus would be things like a modified HUD, but I don't know if a plugin can do that.
 Alternatively we just use a text entity to say the current status of the cart at the bottom of the screen.
 * (Status messages are currently not implemented, just a bunch of debug messages and the like.)

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

### Further additions
 Rollback could be implemented using a delayed OnEndTouchAll and CancelPending
 HUD and cart notifications could be implemented with displaying text on player's screen
