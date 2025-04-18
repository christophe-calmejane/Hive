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

#include "avdecc/helper.hpp"

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <QtMate/widgets/flatIconButton.hpp>

#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>


class TalkerStreamConnectionWidget : public QWidget
{
	Q_OBJECT
public:
	TalkerStreamConnectionWidget(la::avdecc::entity::model::StreamIdentification talkerConnection, la::avdecc::entity::model::StreamIdentification listenerConnection, QWidget* parent = nullptr);
	void updateData();

private:
	la::avdecc::entity::model::StreamIdentification const _talkerConnection{};
	la::avdecc::entity::model::StreamIdentification const _listenerConnection{};

	QHBoxLayout _layout{ this };

	QLabel _streamConnectionLabel{ this };
	QLabel _entityNameLabel{ this };

	qtMate::widgets::FlatIconButton _disconnectButton{ "Material Icons", "block", this };
};
