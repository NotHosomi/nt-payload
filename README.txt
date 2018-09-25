
  +----------------------+
  |PAYLOAD MAP-SIDE LOGIC|
  +----------------------+

 For copy-pasting into a map, the spawnroom and the central room contain all required logic


 
 

 MAPMAKERS MUST
  make sure spawnrooms are inaccessible to the opposing team when active.
  


  +-- GAMEMODE --+
 ATTACKERS can push the cart by standing near, the speed of the cart will increase for every ATTACKER within range.
 If a DEFENDER is near the cart, it will not move.
 Currently, there is **NO** rollback feature.

 Victory State is achieved by the final path_track sending an output to a logic_relay that will be used as a hook for the plugin to declare ATTACKER victory.
 DEFENDERS win if time runs out.



  +-- SPAWNROOMS --+
  Movable spawns are implemented by using multiple trigger_teleports



  +-- PLUGIN INTERACTIONS --+
 @Rain
 I would like for the plugin to do a few things when recieveing a set of outputs from the map
>Add time when each capturepoint logic_relay is triggered
>End the game when the logic_relay AttackerWin is triggered

 Some additional features that would be a bonus would be things like a modified HUD, but I don't know if a plugin can do that.
 Alternatively we just use a text entity to say the current status of the cart at the bottom of the screen, which is all map-side logic.


  +-- Further additions --+
 Rollback could be implemented using a delayed OnEndTouchAll and CancelPending
 Teleport-based spawnrooms will be designed soon
 HUD and cart notifications could be implemented with displaying text on player's screen
