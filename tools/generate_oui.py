#!/usr/bin/env python3
import sys
import csv
import json
import requests # pip install requests
from io import StringIO

# IEEE public registry CSV files, downloaded when no local file is given on the command line
REGISTRY_URLS = [
	"https://standards-oui.ieee.org/oui/oui.csv", # MA-L (OUI-24)
	"https://standards-oui.ieee.org/oui28/mam.csv", # MA-M (OUI-28)
	"https://standards-oui.ieee.org/oui36/oui36.csv", # MA-S (OUI-36)
	"https://standards-oui.ieee.org/iab/iab.csv", # IAB (OUI-36, legacy registry closed in 2014 but still valid)
]

# Maps the IEEE "Registry" column to the JSON table an assignment belongs to
REGISTRY_TABLES = {
	"MA-L": "oui_24",
	"MA-M": "oui_28",
	"MA-S": "oui_36",
	"IAB": "oui_36",
}

# Number of hex digits expected in the "Assignment" column, for each table
TABLE_PREFIX_LENGTHS = {
	"oui_24": 6,
	"oui_28": 7,
	"oui_36": 9,
}

def download_csv(url):
	# The IEEE web application firewall rejects the default requests user-agent, so identify ourselves explicitly
	response = requests.get(url, headers={"User-Agent": "Hive-OUI-Generator/1.0"})
	if response.status_code == 200:
		return response.text
	else:
		raise Exception(f"Failed to download CSV from {url} (HTTP status {response.status_code})")

def find_vendor_name(organization_name, filter_dict):
	"""Returns the display name to use for the given IEEE organization name, or None if that organization is not filtered in."""
	lowered_organization_name = organization_name.lower()
	for filter_item, output_name in filter_dict.items():
		if filter_item.lower() in lowered_organization_name:
			return output_name
	return None

def process_csv(csv_data, filter_dict, result_dict, matched_vendors):
	"""Adds every matching assignment of the given IEEE CSV content to the relevant table of result_dict."""
	csv_reader = csv.reader(StringIO(csv_data))

	for row in csv_reader:
		if len(row) < 3: # Ensure at least three columns are present
			continue

		registry, assignment, organization_name = row[0], row[1], row[2]

		# Ignore the header line and any registry we don't handle (MA-M, CID)
		table = REGISTRY_TABLES.get(registry)
		if table is None:
			continue

		# Check if the organization name is filtered in (case-insensitive)
		output_name = find_vendor_name(organization_name, filter_dict)
		if output_name is None:
			continue

		# Assignments are the hex representation of the prefix, without any separator
		expected_length = TABLE_PREFIX_LENGTHS[table]
		if len(assignment) != expected_length:
			print(f"Warning: Ignoring {registry} assignment '{assignment}' of '{organization_name}': expected {expected_length} hex digits, got {len(assignment)}")
			continue

		result_dict[table]["0x" + assignment.upper()] = output_name
		matched_vendors.add(output_name)

def main(output_file, csv_data_list):
	# Dictionary of filter lines and corresponding output names
	filter_dict = {
		"NETGEAR": "Netgear",
		"l-acoustics": "L-Acoustics",
		"AudioScience": "AudioScience",
		"Texas Instruments": "Texas Instruments",
		"d&b audiotechnik GmbH": "d&b audiotechnik",
		"Meyer Sound Laboratories, Inc.": "Meyer Sound",
		"Apple, Inc.": "Apple",
		"TEKNEMA, INC.": "TEKNEMA",
		"Mark of the Unicorn, Inc.": "MOTU",
		"LUMINEX Lighting Control Equipment": "Luminex",
		"Cisco Systems, Inc": "Cisco",
		"AVID TECHNOLOGY, INC.": "Avid",
		"Extreme Networks": "Extreme Networks",
		"Biamp Systems": "Biamp",
		"Adamson Systems Engineering": "Adamson",
		"AllDSP GmbH & Co. KG": "AllDSP",
		"JOYNED": "JOYNED",
		"M2Lab Ltd.": "M2Lab",
	}

	# Initialize the result dictionary with one empty table per supported prefix length
	result_dict = {table: {} for table in TABLE_PREFIX_LENGTHS}

	# Read and process each CSV data, the "Registry" column of each row telling which table it belongs to
	matched_vendors = set()
	for csv_data in csv_data_list:
		process_csv(csv_data, filter_dict, result_dict, matched_vendors)

	# Print warning for each filter_dict entry without a match
	for filter_item, output_name in filter_dict.items():
		if output_name not in matched_vendors:
			print(f"Warning: No match found for '{filter_item}' in the input files. Output name: {output_name}")

	# Write the result to a JSON file, sorting the keys so the generated file is stable across runs
	with open(output_file, 'w', encoding='utf-8') as json_output_file:
		json.dump(result_dict, json_output_file, indent=2, sort_keys=True)

if __name__ == "__main__":
	if len(sys.argv) < 2:
		print("Usage: python generate_oui.py <output_file.json> [csv_file...]")
		print("       Without any csv_file, the IEEE MA-L, MA-S and IAB registries are downloaded.")
		sys.exit(1)
	# Read the provided files or download the IEEE registries
	if len(sys.argv) > 2:
		csv_data_list = []
		for csv_file_path in sys.argv[2:]:
			with open(csv_file_path, 'r', encoding='utf-8') as csv_file:
				csv_data_list.append(csv_file.read())
	else:
		csv_data_list = [download_csv(url) for url in REGISTRY_URLS]

	main(sys.argv[1], csv_data_list)
