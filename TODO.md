# TODO
## Global
- Enable (via option?) Hive advertising (la_avdecc_controller has to support ADP name/group configuration, and probably partial AEM support)
- Support multiple eth interfaces at the same time to support redundancy (Hive being connected to multiple interfaces at the same time and aggregating information on both interfaces)
  - Have to properly split dynamic/static model in Hive (not only relying on la_avdecc_controller)
  - For each descriptor that have dynamic information, find a way to display them separately in Hive
  - The Entities list will have to properly aggregate entities with the same EID on different networks (and display all possible gptpt and interface index)
- Find something more lightweight than QtWebEngine to display the Changelog
- Auto save the log file in case of a crash
- Save the selected inspector's node when changing inspected entity (and restore it when reselecting it)

## Menu
- Menu: "File/Save log..."
- Menu: "Help/Bug Report"
- Menu: "Help/ChangeLog"

## Entities list
- Icons displaying the type of entity (controller, listener, talker, 3 icons max then)
- Possiblity to copy (right click context menu?) some elements, like the entityID in the list -> already done by Qt, just have to selection row/column and ctrl-c (TODO: Explain that in some tutorial)

## AEM Inspector
- Add Strings descriptor as sub node of Locale
- Right click menu:
  - Export node as aemxml or another format (because aemxml is only for the static part of the AEM)

## Descriptor Inspector
- Improve the "Connection list" for a StreamOutput
  - Add an icon indicating the FastConnecting status
  - Move the right click menu "Clear all ghost connections" from item to the list itself, and implement it
- Right click menu:
  - Copy values (like all EID for ex)

## Connection Matrix
- Right click on a connection (active or not) for which the stream format do not match:
  - "Set listener format from talker" -> Enable this item if, for the current Talker Format, there is a compatible format available on the Listener
  - "Set talker format from listener" -> Enable this item if, for the current Listener Format, there is a compatible format available on the Talker
- Better display error information:
  - When there is a connection error, we should display it wich possibly a red icon (like WrongDomain, but with a tooltip??). Including when there is a fastConnect error (talker acquired for ex.), but in this case the avdecc library should not filter errors in sniffed messages
  - When there is a "mismatch connection" error (listener thinks it's connected but is not in the connections list of the talker), maybe add a new color code?
  - Display when there is a SRP error as well
  - Maybe just use only one new color code (purple) or a new form (triangle?) for when the avdecc connection is established, but there is an error (for all cases above) that we display with a tooltip
- Separate the connection matrix in 2 matrices, one for normal streams and one for CRF?
- Implement colors for entities (the squares)
- Optimize the code for ConnectionMatrixModel::ConnectionMatrixModelPrivate::refreshHeader
- Add feature "Start all Streams" and "Stop all Streams" when right clicking on the header of a entity

## Log window
- Ctrl-F selects current search filter
- When typing in the search filter editBox, ESC removes the focus from it
- Remove Trace/Debug options in Release (only available in Debug)
- Save selected layer/level in configuration (not filter string)

# BUGS
- When an entity arrives on the network, a reselect is made in the entity list, causing a refresh of the inspector. The issue is that if we are changing a value in the AEM, like the name or a format), focus is lost, as well as pending changes
- Connection matrix highlight issue: Should be unhighlighted when the mouse leaves the matrix
- If there is an avdecc error when changing the StreamFormat (or any other comboBox setting), like changing the format on a running stream, the selected item of the combobox is not set back to the previous one
- Display the splashscreen on the same screen than the main windows
- GroupName issue if it's set to "语语语语语语语语语语语语语语语语语语语语语|" (a value is being added at the end of the string)
- If a Talker Stream is in Waiting status and we connect a new listener, it automatically goes into NonWaiting status because we are always sending the connection request without taking Wait flag into account
- ProtocolInterface loaded multiple times during launch (pcap at least)
- When there are many huge devices on the network (Tesira+Hono for ex.) and we reset the controller (reload interface), huge graphics lag (everything arrive at the same time and the mainThread is blocked). Looks like the same issue we previously had with the logger
- Acquire Tesira and quickly select another entity that has no name for Stream in/out, cause a change in the name displayed in the inspector (no filter on onNameChanged??)
- macOS only: ChannelMappings dialog not resizable (https://bugreports.qt.io/browse/QTBUG-41932)
- Properly detect multiple instances of Hive (https://github.com/itay-grudev/SingleApplication)
- Non redundant Talker connected to the primary stream of a redundant Listener (using riedel) causes Hive to not be able to do anything about this stream because it's "not connectable" (single stream cannot be connected to a stream of a redundant pair). Kill ghost connection do not work, neither in the matrix

# Improvements
- Display a reload controller icon in the toolbar

# TO BE SORTED
- Afficher la liste des streams formats tels qu'ils sont retournés par l'entité, et optimiser la liste dans la combobox pour aggréger les up-to avec les autres (si on a up-to 8, ne pas afficher 1, 2, 4, 6...)
- Quand on fait une connexion d'un stream redondant, la liste des listeners connectés (dans le talker) s'update correctement pour le stream primaire, mais pas le secondaire
