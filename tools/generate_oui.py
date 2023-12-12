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
	# List of filter lines
	filter_list = [
				"NETGEAR",
				"L-Acoustics",
				"AudioScience",
				"Texas Instruments",
				"d&b audiotechnik GmbH",
				"Meyer Sound Laboratories, Inc.",
				"Apple, Inc.",
				"TEKNEMA, INC.",
				"Mark of the Unicorn, Inc.",
				"LUMINEX Lighting Control Equipment",
				"Cisco Systems, Inc",
				"AVID TECHNOLOGY, INC.",
				"Extreme Networks, Inc.",
				"Biamp Systems",
	]

	# Download CSV from the given URL
	csv_data = download_csv("https://standards.ieee.org/develop/regauth/oui/oui.csv")

	# Initialize the result dictionary
	result_dict = {}

	# Create the "oui_24" entry
	result_dict["oui_24"] = {}

	# Read and process the CSV data
	csv_reader = csv.reader(StringIO(csv_data))
	for row in csv_reader:
		if len(row) >= 3:  # Ensure at least three columns are present
			registry = row[0]
			mac_prefix = row[1]
			vendor_name = row[2]

			# Ignore non-MA-L entries
			if registry != "MA-L":
				continue

			# Check if vendor name is in the filter list (case-insensitive)
			if any(re.search(re.escape(filter_item), vendor_name, re.IGNORECASE) for filter_item in filter_list):
				result_dict["oui_24"]["0x" + mac_prefix] = vendor_name

	# Write the result to a JSON file
	with open(output_file, 'w', encoding='utf-8') as json_output_file:
		json.dump(result_dict, json_output_file, indent=2)

if __name__ == "__main__":
	if len(sys.argv) != 2:
		print("Usage: python generate_oui.py <output_file.json>")
		sys.exit(1)
	main(sys.argv[1])
