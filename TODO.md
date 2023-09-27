# TODO
## Global
- Support multiple eth interfaces at the same time to support redundancy (Hive being connected to multiple interfaces at the same time and aggregating information on both interfaces)
  - Have to properly split dynamic/static model in Hive (not only relying on la_avdecc_controller)
  - For each descriptor that have dynamic information, find a way to display them separately in Hive
  - The Entities list will have to properly aggregate entities with the same EID on different networks (and display all possible gptpt and interface index)
- Auto save the log file in case of a crash

## Menu
- Menu: "File/Save log..."

## Descriptor Inspector
- Improve the "Connection list" for a StreamOutput
  - Move the right click menu "Clear all ghost connections" from item to the list itself, and implement it

## Connection Matrix
- Better display error information:
  - When there is a "mismatch connection" error (listener thinks it's connected but is not in the connections list of the talker), maybe add a new color code?
  - Display when there is a SRP error as well
  - Maybe just use only one new color code (purple) or a new form (triangle?) for when the avdecc connection is established, but there is an error (for all cases above) that we display with a tooltip
- Separate the connection matrix in 2 matrices, one for normal streams and one for CRF?
- Add feature "Start all Streams" and "Stop all Streams" when right clicking on the header of a entity

## Log window
- Ctrl-F selects current search filter
- When typing in the search filter editBox, ESC removes the focus from it
- Remove Trace/Debug options in Release (only available in Debug)
- Save selected layer/level in configuration (not filter string)

# BUGS
- GroupName issue if it's set to "语语语语语语语语语语语语语语语语语语语语语|" (a value is being added at the end of the string)
- If a Talker Stream is in Waiting status and we connect a new listener, it automatically goes into NonWaiting status because we are always sending the connection request without taking Wait flag into account
- ProtocolInterface loaded multiple times during launch (pcap at least)

# TO BE SORTED
- Afficher la liste des streams formats tels qu'ils sont retournés par l'entité, et optimiser la liste dans la combobox pour aggréger les up-to avec les autres (si on a up-to 8, ne pas afficher 1, 2, 4, 6...)
- Quand on fait une connexion d'un stream redondant, la liste des listeners connectés (dans le talker) s'update correctement pour le stream primaire, mais pas le secondaire
