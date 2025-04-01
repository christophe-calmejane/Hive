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

#pragma once

#include <QValidator>
#include <QRegularExpression>
#include <la/avdecc/internals/uniqueIdentifier.hpp>
#include <la/avdecc/utils.hpp>

namespace avdecc
{
class EUIValidator : public QValidator
{
public:
	static EUIValidator* getSharedInstance() noexcept
	{
		static auto s_instance = EUIValidator{};

		return &s_instance;
	}

private:
	virtual State validate(QString& input, int& pos) const override
	{
		auto hexRegex = QRegularExpression("^0x[0-9A-Fa-f]{1,16}$");
		auto match = hexRegex.match(input);
		if (match.hasMatch())
		{
			try
			{
				la::avdecc::utils::convertFromString<la::avdecc::UniqueIdentifier::value_type>(input.toStdString().c_str());
				return Acceptable;
			}
			catch (const std::invalid_argument&)
			{
				return Invalid;
			}
		}
		return Intermediate;
	}

	virtual void fixup(QString& input) const override
	{
		// Set the input to '0x00000000' if it is invalid
		int pos = 0;
		if (validate(input, pos) != Acceptable)
		{
			input = "0x0000000000000000";
		}
		else
		{
			// Ensure the input always has 16 digits and is in upper case
			input = input.mid(2).rightJustified(16, '0').toUpper();
			input.prepend("0x");
		}
	}
};

} // namespace avdecc
