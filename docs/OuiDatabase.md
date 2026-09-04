# OUI Database

Hive displays a vendor name instead of a raw hexadecimal number whenever it knows the organization owning the prefix of an EUI-64 (entity ID) or a MAC address, typically to name a switch discovered on the network. The lookup table is a JSON resource embedded in the application (`resources/oui.json`, referenced by `resources/main.qrc`), generated from the public IEEE registries by `tools/generate_oui.py`.

## Resource file format

```json
{
  "oui_24": {
    "0x00000C": "Cisco"
  },
  "oui_28": {
    "0x480BB2D": "M2Lab"
  },
  "oui_36": {
    "0x0050C29C9": "Luminex"
  }
}
```

Each table maps a prefix (hexadecimal string, one entry per IEEE assignment) to the display name. The key holds the prefix bits right-aligned, so it has exactly 6, 7 and 9 hex digits respectively. This is directly what `la::avdecc::UniqueIdentifier::getVendorID<la::avdecc::OuiType>()` returns for the matching `OuiType`, which is what `hive::modelsLibrary::helper::getVendorName()` looks up (each table in turn, then falls back to the hexadecimal representation of the OUI-24).

Nothing in an EUI-64 tells how many of its leading bits the IEEE assigned, so the three tables have to be probed one after the other. This is not ambiguous: the IEEE either assigns a whole 24 bits block to one organization (MA-L), or subdivides it into smaller blocks, so a given identifier can only ever match one of the tables.

## Generation

```bash
pip install requests
python3 tools/generate_oui.py resources/oui.json           # downloads the IEEE registries
python3 tools/generate_oui.py resources/oui.json a.csv ...  # uses previously downloaded CSV files
```

The IEEE web application firewall rejects any request whose user-agent contains "python-requests", hence the explicit user-agent set by `download_csv()` (a rejected download reports an HTTP 418 status).

The IEEE publishes one CSV file per registry, all sharing the same columns (`Registry`, `Assignment`, `Organization Name`, `Organization Address`). The script reads the `Registry` column of each row to decide which table an assignment belongs to, so the CSV files can be passed in any order (and a single file could even contain several registries):

| Registry | IEEE file | Prefix size | Destination table |
| --- | --- | --- | --- |
| MA-L | `oui/oui.csv` | 24 bits | `oui_24` |
| MA-M | `oui28/mam.csv` | 28 bits | `oui_28` |
| MA-S | `oui36/oui36.csv` | 36 bits | `oui_36` |
| IAB | `iab/iab.csv` | 36 bits | `oui_36` |

Notes on the registries:

- MA-M (28 bits) and MA-S (36 bits) are the block sizes a vendor buys when it doesn't need the 16 million addresses of an MA-L. Most large vendors own MA-L blocks only, so `oui_28` and `oui_36` are expected to stay small.
- IAB is the legacy name of the same 36 bits assignment (the registry was closed to new assignments in 2014, but existing IABs remain valid and in use), which is why both feed the same table.
- CID (`cid/cid.csv`) is not used: those 24 bits identifiers are explicitly not meant for globally unique addresses, so they never appear as the prefix of an entity ID.

## Vendor filtering

Merging all the IEEE registries would produce a ~2.4 MB resource (about 58000 distinct assignments), so the script only keeps the organizations listed in the `filter_dict` dictionary at the top of `main()`. Each entry maps a (case-insensitive) substring searched in the IEEE `Organization Name` column to the name Hive displays, which also normalizes the various legal spellings ("Mark of the Unicorn, Inc." becomes "MOTU").

Adding a vendor is a one-line addition to `filter_dict` followed by a regeneration. Two things to watch for:

- IEEE organization names change over time (an entry becoming unmatched is reported as a warning at the end of the run), so prefer the shortest unambiguous substring.
- A substring that is too short may capture unrelated companies (`Luminex` alone would also match "Luminex Corporation", a biotech company owning an IAB block).

The generated file is sorted by key, so regenerating it after an IEEE update produces a diff limited to the actual registry changes.
