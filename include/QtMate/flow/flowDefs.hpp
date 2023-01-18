/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QPair>
#include <QString>
#include <QVector>
#include <QHash>
#include <QSet>

namespace qtMate::flow
{
using FlowNodeUid = quint64;
using FlowSocketIndex = quint32;
using FlowSocketType = quint32;
using FlowSocketSlot = QPair<FlowNodeUid, FlowSocketIndex>;
using FlowSocketSlots = QSet<FlowSocketSlot>;

// source (output), sink (input)
using FlowConnectionDescriptor = QPair<FlowSocketSlot, FlowSocketSlot>;
using FlowConnectionDescriptors = QSet<FlowConnectionDescriptor>;

inline auto constexpr InvalidFlowNodeUid = static_cast<FlowNodeUid>(-1);
inline auto constexpr InvalidFlowSocketIndex = static_cast<FlowSocketIndex>(-1);
inline auto constexpr InvalidFlowSocketSlot = FlowSocketSlot{ InvalidFlowNodeUid, InvalidFlowSocketIndex };
inline auto constexpr InvalidFlowConnectionDescriptor = FlowConnectionDescriptor{ InvalidFlowSocketSlot, InvalidFlowSocketSlot };

struct FlowSocketDescriptor
{
	QString name{};
	FlowSocketType type{};
};

using FlowSocketDescriptors = QVector<FlowSocketDescriptor>;

struct FlowNodeDescriptor
{
	QString name{};
	FlowSocketDescriptors inputs{};
	FlowSocketDescriptors outputs{};
};

using FlowNodeDescriptorMap = QHash<FlowNodeUid, FlowNodeDescriptor>;

class FlowNode;

class FlowSocket;
class FlowInput;
class FlowOutput;

using FlowInputs = QVector<FlowInput*>;
using FlowOutputs = QVector<FlowOutput*>;

class FlowLink;
using FlowLinks = QSet<FlowLink*>;

class FlowConnection;
using FlowConnections = QSet<FlowConnection*>;

class FlowScene;
class FlowSceneDelegate;

class FlowView;

} // namespace qtMate::flow
