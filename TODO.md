# TODO

## Network Graph
- [FAIT - A VERIFIER] Affiche incorrectement le nombre de streams d'un lien. 1 talker vers 50 listeners ne fait qu'un seul stream, pas 50
- Ajouter un toggle button pour afficher ou non les streams de media clock
- Trouver un moyen de layout+render uniquement une portion du graphe pour améliorer la lisibilité, par exemple quand on sélectionne un stream, on pourrait carément (temporairement) retirer du graphe toutes les entités/bridges qui ne font pas partie du stream (au lieu de simplement les fade out comme c'est le cas actuellement), peut etre via un bouton "Focus on selected stream" (ou simplement avec un double-clic sur un trait au lieu d'un clic simple). On conserve ESC (ou le bouton "clear stream highlight") pour revenir à l'affichage normal du graphe.

## EventJournal

## DiscoveredEntities

## Global
- Auto save the log file in case of a crash

## Menu
- Menu: "File/Save log..."

## Descriptor Inspector
- Improve the "Connection list" for a StreamOutput
  - Move the right click menu "Clear all ghost connections" from item to the list itself, and implement it

## Connection Matrix
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
