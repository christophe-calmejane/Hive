#!/usr/bin/env python
import sys
import csv
import json
import re
import requests # pip install requests
from io import StringIO

def download_csv(url):
	response = requests.get(url)
	if response.status_code == 200:
		return response.text
	else:
		raise Exception(f"Failed to download CSV from {url}")

def main(output_file):
	# Dictionary of filter lines and corresponding output names
	filter_dict = {
		"NETGEAR": "Netgear",
		"l-acoustics": "L-Acoustics",
		"AudioScience": "AudioScience",
		"Texas Instruments": "Texas Instruments",
		"d&b audiotechnik GmbH": "d&b Audiotechnik",
		"Meyer Sound Laboratories, Inc.": "Meyer Sound",
		"Apple, Inc.": "Apple",
		"TEKNEMA, INC.": "TEKNEMA",
		"Mark of the Unicorn, Inc.": "MOTU",
		"LUMINEX Lighting Control Equipment": "Luminex",
		"Cisco Systems, Inc": "Cisco",
		"AVID TECHNOLOGY, INC.": "Avid",
		"Extreme Networks Headquarters": "Extreme Networks",
		"Biamp Systems": "Biamp",
	}

	# Download CSV from the given URL
	csv_data = download_csv("https://standards.ieee.org/develop/regauth/oui/oui.csv")

	# Initialize the result dictionary
	result_dict = {}

	# Create the "oui_24" entry
	result_dict["oui_24"] = {}

	# Read and process the CSV data
	csv_reader = csv.reader(StringIO(csv_data))
	matched_vendors = set()

	for row in csv_reader:
		if len(row) >= 3:  # Ensure at least three columns are present
			registry, mac_prefix, vendor_name = row[0], row[1], row[2]

			# Ignore non-MA-L entries
			if registry != "MA-L":
				continue

			# Check if vendor name is in the filter dictionary (case-insensitive)
			for filter_item, output_name in filter_dict.items():
				if re.search(re.escape(filter_item), vendor_name, re.IGNORECASE):
					result_dict["oui_24"]["0x" + mac_prefix] = output_name
					matched_vendors.add(output_name)

	# Print warning for each filter_dict entry without a match
	for filter_item, output_name in filter_dict.items():
		if output_name not in matched_vendors:
			print(f"Warning: No match found for '{filter_item}' in the input file. Output name: {output_name}")

	# Write the result to a JSON file
	with open(output_file, 'w', encoding='utf-8') as json_output_file:
		json.dump(result_dict, json_output_file, indent=2)

if __name__ == "__main__":
	if len(sys.argv) != 2:
		print("Usage: python generate_oui.py <output_file.json>")
		sys.exit(1)
	main(sys.argv[1])
