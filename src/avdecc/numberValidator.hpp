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

namespace avdecc
{
template<std::uint64_t MaxValue>
class PositiveIntegerValidator : public QValidator
{
public:
	static PositiveIntegerValidator* getSharedInstance() noexcept
	{
		static auto s_instance = PositiveIntegerValidator{};

		return &s_instance;
	}

private:
	virtual State validate(QString& input, int& pos) const override
	{
		auto regex = QRegularExpression{ "^[0-9]+$" };
		auto match = regex.match(input);
		if (match.hasMatch())
		{
			auto ok = false;
			auto value = input.toULongLong(&ok, 10);
			if (ok && value <= MaxValue)
			{
				return Acceptable;
			}
			return Invalid;
		}
		return Intermediate;
	}
	virtual void fixup(QString& input) const override
	{
		// Set the input to '0' if it is invalid
		int pos = 0;
		if (validate(input, pos) != Acceptable)
		{
			input = "0";
		}
	}
};

} // namespace avdecc
