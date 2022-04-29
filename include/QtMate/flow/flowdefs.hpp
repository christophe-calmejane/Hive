#pragma once

#include <QPair>
#include <QString>
#include <QVector>
#include <QHash>
#include <QSet>

namespace qtMate::flow {

using FlowNodeUid = quint64;
using FlowSocketIndex = quint32;
using FlowSocketType = quint32;
using FlowSocketSlot = QPair<FlowNodeUid, FlowSocketIndex>;
using FlowSocketSlots = QSet<FlowSocketSlot>;

// source (output), sink (input)
using FlowConnectionDescriptor = QPair<FlowSocketSlot, FlowSocketSlot>;
using FlowConnectionDescriptors = QSet<FlowConnectionDescriptor>;

inline auto constexpr InvalidFlowSocketSlot = FlowSocketSlot{-1, -1};
inline auto constexpr InvalidFlowConnectionDescriptor = FlowConnectionDescriptor{InvalidFlowSocketSlot, InvalidFlowSocketSlot};

struct FlowSocketDescriptor {
	QString name{};
	FlowSocketType type{};
};

using FlowSocketDescriptors = QVector<FlowSocketDescriptor>;

struct FlowNodeDescriptor {
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
