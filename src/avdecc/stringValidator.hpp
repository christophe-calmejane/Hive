/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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
#include <la/avdecc/internals/entityModelTypes.hpp>
#include <la/avdecc/internals/entityModelControlValues.hpp>

namespace avdecc
{
template<int MaxLength>
class FixedSizeStringValidator : public QValidator
{
public:
	virtual State validate(QString& input, int& /*pos*/) const override
	{
		return input.toUtf8().length() <= MaxLength ? State::Acceptable : State::Invalid;
	}
};

class AvdeccStringValidator : public FixedSizeStringValidator<la::avdecc::entity::model::AvdeccFixedString::MaxLength>
{
public:
	static AvdeccStringValidator* getSharedInstance() noexcept
	{
		static auto s_instance = AvdeccStringValidator{};

		return &s_instance;
	}
};

class ControlUTF8StringValidator : public FixedSizeStringValidator<la::avdecc::entity::model::UTF8StringValueStatic::MaxLength>
{
public:
	static ControlUTF8StringValidator* getSharedInstance() noexcept
	{
		static auto s_instance = ControlUTF8StringValidator{};

		return &s_instance;
	}
};

} // namespace avdecc
