/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <nlohmann/json.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <cstring> // strerror
#include <cerrno> // errno
#include <iomanip> // setw

using json = nlohmann::json;

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		std::cout << "Missing parameters" << std::endl << "Usage: <Input File (*.ave;*.aem;*.ans)> <Output File (*.json)>" << std::endl;
		return 1;
	}

	auto const* const inputFile = argv[1];
	auto const* const outputFile = argv[2];

	// Try to open the input file
	auto ifs = std::ifstream{ inputFile, std::ios::binary | std::ios::in };

	// Failed to open file for reading
	if (!ifs.is_open())
	{
		std::cout << "Cannot open input file '" << std::string{ inputFile } << "': " << std::strerror(errno) << std::endl;
		return 1;
	}

	auto object = json{};
	try
	{
		object = json::from_msgpack(ifs);
	}
	catch (json::type_error const& e)
	{
		std::cout << "Cannot parse input file '" << std::string{ inputFile } << "': " << e.what() << std::endl;
		return 2;
	}
	catch (json::parse_error const& e)
	{
		std::cout << "Cannot parse input file '" << std::string{ inputFile } << "': " << e.what() << std::endl;
		return 2;
	}
	catch (json::out_of_range const& e)
	{
		std::cout << "Cannot parse input file '" << std::string{ inputFile } << "': " << e.what() << std::endl;
		return 2;
	}
	catch (json::other_error const& e)
	{
		std::cout << "Cannot parse input file '" << std::string{ inputFile } << "': " << e.what() << std::endl;
		return 2;
	}
	catch (json::exception const& e)
	{
		std::cout << "Cannot parse input file '" << std::string{ inputFile } << "': " << e.what() << std::endl;
		return 2;
	}

	// Try to open the output file
	auto ofs = std::ofstream{ outputFile, std::ios::binary | std::ios::out };

	// Failed to open file to writting
	if (!ofs.is_open())
	{
		std::cout << "Cannot open output file '" << std::string{ outputFile } << "': " << std::strerror(errno) << std::endl;
		return 3;
	}

	ofs << std::setw(4) << object << std::endl;

	std::cout << "Successfully converted file" << std::endl;

	return 0;
}
