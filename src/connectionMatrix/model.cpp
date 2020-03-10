/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "connectionMatrix/model.hpp"
#include "connectionMatrix/node.hpp"
#include "avdecc/controllerManager.hpp"
#include "avdecc/channelConnectionManager.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"
#include "toolkit/helper.hpp"
#include <deque>
#include <vector>

#if ENABLE_CONNECTION_MATRIX_DEBUG
#	include <QDebug>
#endif

#if ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
#	include "toolkit/material/color.hpp"
#endif

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#	error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

namespace connectionMatrix
{
namespace priv
{
using Nodes = std::deque<Node*>;

// Entity node by entity ID
using NodeMap = std::unordered_map<la::avdecc::UniqueIdentifier, std::unique_ptr<EntityNode>, la::avdecc::UniqueIdentifier::hash>;

// Entity section by entity ID
using EntitySectionMap = std::unordered_map<la::avdecc::UniqueIdentifier, int, la::avdecc::UniqueIdentifier::hash>;

// Unique stream identifier
using StreamKey = std::pair<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::StreamIndex>;

struct StreamKeyHash
{
	std::size_t operator()(StreamKey const& key) const
	{
		return la::avdecc::UniqueIdentifier::hash()(key.first) ^ std::hash<la::avdecc::entity::model::StreamIndex>()(key.second);
	}
};

static auto const InvalidStreamKey = StreamKey{};

// StreamNode by StreamKey
using StreamNodeMap = std::unordered_map<StreamKey, StreamNode*, StreamKeyHash>;

// Unique channel identifier
using ChannelKey = std::pair<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::ClusterIndex>;

struct ChannelKeyHash
{
	std::size_t operator()(ChannelKey const& key) const
	{
		return la::avdecc::UniqueIdentifier::hash()(key.first) ^ std::hash<la::avdecc::entity::model::ClusterIndex>()(key.second);
	}
};

static auto const InvalidChannelKey = ChannelKey{};

// ChannelNode by ChannelKey
using ChannelNodeMap = std::unordered_map<ChannelKey, ChannelNode*, ChannelKeyHash>;

// Section by Node
using NodeSectionMap = std::unordered_map<Node const*, int>;

#if ENABLE_CONNECTION_MATRIX_TOOLTIP

// Converts IntersectionData::Type to string
QString typeToString(Model::IntersectionData::Type const type)
{
	switch (type)
	{
		case Model::IntersectionData::Type::None:
			return "None";
		case Model::IntersectionData::Type::OfflineOutputStream_Entity:
			return "OfflineStream / Entity";
		case Model::IntersectionData::Type::OfflineOutputStream_Redundant:
			return "OfflineStream / Redundant";
		case Model::IntersectionData::Type::OfflineOutputStream_RedundantStream:
			return "OfflineStream / Redundant Stream";
		case Model::IntersectionData::Type::OfflineOutputStream_RedundantChannel:
			return "OfflineStream / Redundant Channel";
		case Model::IntersectionData::Type::OfflineOutputStream_SingleStream:
			return "OfflineStream / Single Stream";
		case Model::IntersectionData::Type::OfflineOutputStream_SingleChannel:
			return "OfflineStream / Single Channel";
		case Model::IntersectionData::Type::Entity_Entity:
			return "Entity / Entity";
		case Model::IntersectionData::Type::Entity_Redundant:
			return "Entity / Redundant";
		case Model::IntersectionData::Type::Entity_RedundantStream:
			return "Entity / Redundant Stream";
		case Model::IntersectionData::Type::Entity_RedundantChannel:
			return "Entity / Redundant Channel";
		case Model::IntersectionData::Type::Entity_SingleStream:
			return "Entity / Single Stream";
		case Model::IntersectionData::Type::Entity_SingleChannel:
			return "Entity / Single Channel";
		case Model::IntersectionData::Type::Redundant_Redundant:
			return "Redundant / Redundant";
		case Model::IntersectionData::Type::Redundant_RedundantStream:
			return "Redundant / Redundant Stream";
		case Model::IntersectionData::Type::Redundant_RedundantChannel:
			return "Redundant / Redundant Channel";
		case Model::IntersectionData::Type::Redundant_SingleStream:
			return "Redundant / Single Stream";
		case Model::IntersectionData::Type::Redundant_SingleChannel:
			return "Redundant / Single Channel";
		case Model::IntersectionData::Type::RedundantStream_RedundantStream:
			return "Redundant Stream / Redundant Stream";
		case Model::IntersectionData::Type::RedundantStream_RedundantStream_Forbidden:
			return "Forbidden Redundant Stream / Redundant Stream";
		case Model::IntersectionData::Type::RedundantStream_SingleStream:
			return "Redundant Stream / Single Stream";
		case Model::IntersectionData::Type::RedundantChannel_RedundantChannel:
			return "Redundant Channel / Redundant Channel";
		case Model::IntersectionData::Type::RedundantChannel_RedundantChannel_Forbidden:
			return "Forbidden Redundant Channel / Redundant Channel";
		case Model::IntersectionData::Type::RedundantChannel_SingleChannel:
			return "Redundant Channel / Single Channel";
		case Model::IntersectionData::Type::SingleStream_SingleStream:
			return "Single Stream / Single Stream";
		case Model::IntersectionData::Type::SingleChannel_SingleChannel:
			return "Single Channel / Single Channel";
		default:
			AVDECC_ASSERT(false, "Unknown Type");
			return "Unknown";
	}
}

// Converts IntersectionData::State to string
QString stateToString(Model::IntersectionData::State const state)
{
	switch (state)
	{
		case Model::IntersectionData::State::NotConnected:
			return "Not Connected";
		case Model::IntersectionData::State::Connected:
			return "Connected";
		case Model::IntersectionData::State::FastConnecting:
			return "Fast Connecting";
		case Model::IntersectionData::State::PartiallyConnected:
			return "Partially Connected";
		default:
			AVDECC_ASSERT(false, "Unknown State");
			return "Unknown";
	}
}

// Converts IntersectionData::Flags to string
QString flagsToString(Model::IntersectionData::Flags const& flags)
{
	QStringList stringList;

	if (flags.test(Model::IntersectionData::Flag::InterfaceDown))
	{
		stringList << "InterfaceDown";
	}

	if (flags.test(Model::IntersectionData::Flag::WrongDomain))
	{
		stringList << "WrongDomain";
	}

	if (flags.test(Model::IntersectionData::Flag::WrongFormat))
	{
		stringList << "WrongFormat";
	}

	return stringList.join(" | ");
}

// Builds string representation of IntersectionData
QString intersectionDataToString(Model::IntersectionData const& intersectionData)
{
	auto const type = typeToString(intersectionData.type);
	auto const state = stateToString(intersectionData.state);
	auto const flags = flagsToString(intersectionData.flags);

	return type + "\n" + state + (flags.isEmpty() ? "" : "\n") + flags;
}
#endif

// Visit node according to mode
void accept(Node* node, Model::Mode const mode, Node::Visitor const& visitor, bool const childrenOnly = false)
{
	if (!AVDECC_ASSERT_WITH_RET(node, "Node should not be null"))
	{
		return;
	}

	switch (mode)
	{
		case Model::Mode::Stream:
			node->accept<Node::StreamHierarchyPolicy>(visitor, childrenOnly);
			break;
		case Model::Mode::Channel:
			node->accept<Node::ChannelHierarchyPolicy>(visitor, childrenOnly);
			break;
		default:
			AVDECC_ASSERT(false, "Unhandled Mode");
			break;
	}
}

// Checks if node is actually a StreamNode and return its associated key, otherwise returns InvalidStreamKey
StreamKey makeStreamKey(Node* node)
{
	if (AVDECC_ASSERT_WITH_RET(node->isStreamNode(), "Node should be of type StreamNode"))
	{
		auto* streamNode = static_cast<StreamNode*>(node);
		return std::make_pair(streamNode->entityID(), streamNode->streamIndex());
	}

	return InvalidStreamKey;
}

// Checks if node is actually a ChannelNode and return its associated key, otherwise returns InvalidChannelKey
ChannelKey makeChannelKey(Node* node)
{
	if (AVDECC_ASSERT_WITH_RET(node->isChannelNode(), "Node should be of type ChannelNode"))
	{
		auto* channelNode = static_cast<ChannelNode*>(node);
		return std::make_pair(channelNode->entityID(), channelNode->clusterIndex());
	}

	return InvalidChannelKey;
}

// Insert stream nodes in map
void insertStreamNodes(StreamNodeMap& map, Node* node)
{
	node->accept<Node::StreamPolicy>(
		[&map](Node* node)
		{
			auto const key = makeStreamKey(node);
			if (key != InvalidStreamKey)
			{
				auto const& [it, result] = map.insert(std::make_pair(key, static_cast<StreamNode*>(node)));
				AVDECC_ASSERT_WITH_RET(result, "Trying to insert the same key twice in the same map");
			}
		});
}

// Remove stream nodes from map
void removeStreamNodes(StreamNodeMap& map, Node* node)
{
	node->accept<Node::StreamPolicy>(
		[&map](Node* node)
		{
			auto const key = makeStreamKey(node);
			auto const it = map.find(key);
			if (AVDECC_ASSERT_WITH_RET(it != std::end(map), "Trying to erase a key that is not in the map"))
			{
				map.erase(key);
			}
		});
}

// Insert channel nodes in map
void insertChannelNodes(ChannelNodeMap& map, Node* node)
{
	node->accept<Node::ChannelPolicy>(
		[&map](Node* node)
		{
			auto const key = makeChannelKey(node);
			if (key != InvalidChannelKey)
			{
				auto const& [it, result] = map.insert(std::make_pair(key, static_cast<ChannelNode*>(node)));
				AVDECC_ASSERT_WITH_RET(result, "Trying to insert the same key twice in the same map");
			}
		});
}

// Remove channel nodes from map
void removeChannelNodes(ChannelNodeMap& map, Node* node)
{
	node->accept<Node::ChannelPolicy>(
		[&map](Node* node)
		{
			auto const key = makeChannelKey(node);
			auto const it = map.find(key);
			if (AVDECC_ASSERT_WITH_RET(it != std::end(map), "Trying to erase a key that is not in the map"))
			{
				map.erase(key);
			}
		});
}

// Flatten node hierarchy
Nodes flattenEntityNode(Node* node, Model::Mode const mode)
{
	auto nodes = Nodes{};

	if (!AVDECC_ASSERT_WITH_RET(node, "Node should not be null"))
	{
		return nodes;
	}

	accept(node, mode,
		[&nodes](Node* node)
		{
			nodes.push_back(node);
		});

	return nodes;
}

// Insert nodes in list, starting a first index
void insertNodes(Nodes& list, Nodes const& nodes, int first)
{
#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const before = list.size();
#endif

	auto const it = std::next(std::begin(list), first);
	list.insert(it, std::begin(nodes), std::end(nodes));

#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const after = list.size();
	qDebug() << "insertNodes" << before << ">" << after;
#endif
}

// Removes [first, last] items from list
void removeNodes(Nodes& list, int first, int last)
{
	auto const from = std::next(std::begin(list), first);
	auto const to = std::next(std::begin(list), last);

	if (!AVDECC_ASSERT_WITH_RET(from < to, "'from' should be < to 'to'"))
	{
		return;
	}

#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const before = list.size();
#endif

	list.erase(from, to);

#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const after = list.size();
	qDebug() << "removeNodes" << before << ">" << after;
#endif
}


// Returns the index where entity should be inserted to keep a sorted list by entityID
int sortedIndexForEntity(Nodes const& list, la::avdecc::UniqueIdentifier const& entityID)
{
	auto index = 0;
	for (auto const* node : list)
	{
		if (node->entityID() > entityID)
		{
			break;
		}
		++index;
	}
	return index;
}

// Build and returns a NodeSectionMap from nodes (quick access map for nodes)
NodeSectionMap buildNodeSectionMap(Nodes const& nodes)
{
	NodeSectionMap sectionMap;

	for (auto section = 0u; section < nodes.size(); ++section)
	{
		auto* node = nodes[section];
		sectionMap.insert(std::make_pair(node, section));
	}

	return sectionMap;
}

// Return the index of a node contained in a NodeSectionMap
int indexOf(NodeSectionMap const& map, Node const* const node)
{
	auto const it = map.find(node);
	if (!AVDECC_ASSERT_WITH_RET(it != std::end(map), "Index not found"))
	{
		return -1;
	}
	return it->second;
}

// Returns cluster channel name.
// It is assumed that if channel == 0, the channel is not displayed
QString clusterChannelName(QString const& clusterName, std::uint16_t const channel)
{
	if (channel > 0)
	{
		return QString{ "%1.%2" }.arg(clusterName).arg(channel);
	}

	return clusterName;
}

} // namespace priv

class ModelPrivate : public QObject
{
	Q_OBJECT
public:
	ModelPrivate(Model* q)
		: q_ptr{ q }
	{
		auto& controllerManager = avdecc::ControllerManager::getInstance();
		// Common signals
		connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ModelPrivate::handleControllerOffline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ModelPrivate::handleEntityOnline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ModelPrivate::handleEntityOffline);
		connect(&controllerManager, &avdecc::ControllerManager::gptpChanged, this, &ModelPrivate::handleGptpChanged);
		connect(&controllerManager, &avdecc::ControllerManager::entityNameChanged, this, &ModelPrivate::handleEntityNameChanged);
		connect(&controllerManager, &avdecc::ControllerManager::avbInterfaceLinkStatusChanged, this, &ModelPrivate::handleAvbInterfaceLinkStatusChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamFormatChanged, this, &ModelPrivate::handleStreamFormatChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamRunningChanged, this, &ModelPrivate::handleStreamRunningChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamInputConnectionChanged, this, &ModelPrivate::handleStreamInputConnectionChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamDynamicInfoChanged, this, &ModelPrivate::handleStreamDynamicInfoChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamInputCountersChanged, this, &ModelPrivate::handleStreamInputCountersChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamOutputCountersChanged, this, &ModelPrivate::handleStreamOutputCountersChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamPortAudioMappingsChanged, this, &ModelPrivate::handleStreamPortAudioMappingsChanged);

		// Stream Mode specific signals
		connect(&controllerManager, &avdecc::ControllerManager::streamNameChanged, this, &ModelPrivate::handleStreamNameChanged);

		// Channel Mode specific signals
		connect(&controllerManager, &avdecc::ControllerManager::compatibilityFlagsChanged, this, &ModelPrivate::handleCompatibilityFlagsChanged);
		connect(&controllerManager, &avdecc::ControllerManager::audioClusterNameChanged, this, &ModelPrivate::handleAudioClusterNameChanged);

		auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
		connect(&channelConnectionManager, &avdecc::ChannelConnectionManager::listenerChannelConnectionsUpdate, this, &ModelPrivate::handleListenerChannelConnectionsUpdate);
	}

#if ENABLE_CONNECTION_MATRIX_DEBUG
	void dump() const
	{
		auto const rows = _intersectionData.size();
		auto const columns = rows > 0 ? _intersectionData[0].size() : 0u;

		qDebug() << "talkers" << _talkerNodes.size();
		qDebug() << "listeners" << _listenerNodes.size();
		qDebug() << "intersections" << rows << "x" << columns;
	}
#endif

#if ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
	void highlightIntersection(int talkerSection, int listenerSection)
	{
		if (!AVDECC_ASSERT_WITH_RET(isValidTalkerSection(talkerSection), "Invalid talker section") || !AVDECC_ASSERT_WITH_RET(isValidListenerSection(listenerSection), "Invalid listener section"))
		{
			return;
		}

		auto& intersectionData = _intersectionData[talkerSection][listenerSection];

		if (!intersectionData.animation)
		{
			intersectionData.animation = new QVariantAnimation{ this };
		}

		intersectionData.animation->setStartValue(qt::toolkit::material::color::value(qt::toolkit::material::color::Name::Red));
		intersectionData.animation->setEndValue(QColor{ Qt::transparent });
		intersectionData.animation->setDuration(1000);
		intersectionData.animation->start();

		connect(intersectionData.animation, &QVariantAnimation::valueChanged,
			[this, talkerSection, listenerSection](QVariant const& /*value*/)
			{
				Q_Q(Model);
				auto const index = q->index(talkerSection, listenerSection);
				emit q->dataChanged(index, index);
			});
	}
#endif

	// Notification wrappers

	void beginInsertTalkerItems(int first, int last)
	{
		Q_Q(Model);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		qDebug() << "beginInsertTalkerItems(" << first << "," << last << ")";
#endif

		if (!_transposed)
		{
			emit q->beginInsertRows({}, first, last);
		}
		else
		{
			emit q->beginInsertColumns({}, first, last);
		}
	}

	void endInsertTalkerItems()
	{
		Q_Q(Model);

		if (!_transposed)
		{
			emit q->endInsertRows();
		}
		else
		{
			emit q->endInsertColumns();
		}
	}

	void beginRemoveTalkerItems(int first, int last)
	{
		Q_Q(Model);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		qDebug() << "beginRemoveTalkerItems(" << first << "," << last << ")";
#endif

		if (!_transposed)
		{
			emit q->beginRemoveRows({}, first, last);
		}
		else
		{
			emit q->beginRemoveColumns({}, first, last);
		}
	}

	void endRemoveTalkerItems()
	{
		Q_Q(Model);

		if (!_transposed)
		{
			emit q->endRemoveRows();
		}
		else
		{
			emit q->endRemoveColumns();
		}
	}

	void beginInsertListenerItems(int first, int last)
	{
		Q_Q(Model);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		qDebug() << "beginInsertListenerItems(" << first << "," << last << ")";
#endif

		if (!_transposed)
		{
			emit q->beginInsertColumns({}, first, last);
		}
		else
		{
			emit q->beginInsertRows({}, first, last);
		}
	}

	void endInsertListenerItems()
	{
		Q_Q(Model);

		if (!_transposed)
		{
			emit q->endInsertColumns();
		}
		else
		{
			emit q->endInsertRows();
		}
	}

	void beginRemoveListenerItems(int first, int last)
	{
		Q_Q(Model);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		qDebug() << "beginRemoveListenerItems(" << first << "," << last << ")";
#endif

		if (!_transposed)
		{
			emit q->beginRemoveColumns({}, first, last);
		}
		else
		{
			emit q->beginRemoveRows({}, first, last);
		}
	}

	void endRemoveListenerItems()
	{
		Q_Q(Model);

		if (!_transposed)
		{
			emit q->endRemoveColumns();
		}
		else
		{
			emit q->endRemoveRows();
		}
	}

	enum class HeaderDirtyFlag
	{
		UpdateLockedState = 1u << 0, /**< Update the Stream Locked State (Stream Input only), or the summary if this is a parent node */
		UpdateIsStreaming = 1u << 1, /**<  Update the Is Streaming State (Stream Output only), or the summary if this is a parent node */
	};
	using HeaderDirtyFlags = la::avdecc::utils::EnumBitfield<HeaderDirtyFlag>;

	enum class IntersectionDirtyFlag
	{
		UpdateConnected = 1u << 0, /**< Update the connected status, or the summary if this is a parent node */
		UpdateFormat = 1u << 1, /**<  Update the matching format status, or the summary if this is a parent node */
		UpdateGptp = 1u << 2, /**< Update the matching gPTP status, or the summary if this is a parent node (WARNING: For intersection of redundant and non-redundant, the complete checks has to be done, since format compatibility is not checked if GM is not the same) */
		UpdateLinkStatus = 1u << 3, /**< Update the link status, or the summary if this is a parent node */
	};
	using IntersectionDirtyFlags = la::avdecc::utils::EnumBitfield<IntersectionDirtyFlag>;

	// Returns IntersectionDirtyFlags with all available flags set
	static IntersectionDirtyFlags allIntersectionDirtyFlags()
	{
		auto flags = IntersectionDirtyFlags{};
		flags.set(IntersectionDirtyFlag::UpdateConnected);
		flags.set(IntersectionDirtyFlag::UpdateFormat);
		flags.set(IntersectionDirtyFlag::UpdateGptp);
		flags.set(IntersectionDirtyFlag::UpdateLinkStatus);
		return flags;
	}

	// Determines intersection type according to talker and listener nodes
	Model::IntersectionData::Type determineIntersectionType(Node* talker, Node* listener) const
	{
		if (!AVDECC_ASSERT_WITH_RET(talker, "Invalid talker") || !AVDECC_ASSERT_WITH_RET(listener, "Invalid listener"))
		{
			return Model::IntersectionData::Type::None;
		}

		if (talker->entityID() == listener->entityID())
		{
			return Model::IntersectionData::Type::None;
		}

		auto const talkerType = talker->type();
		auto const listenerType = listener->type();

		if (talkerType == Node::Type::OfflineOutputStream)
		{
			switch (listenerType)
			{
				case Node::Type::Entity:
					return Model::IntersectionData::Type::OfflineOutputStream_Entity;
				case Node::Type::RedundantInput:
					return Model::IntersectionData::Type::OfflineOutputStream_Redundant;
				case Node::Type::RedundantInputStream:
					return Model::IntersectionData::Type::OfflineOutputStream_RedundantStream;
				case Node::Type::InputStream:
					return Model::IntersectionData::Type::OfflineOutputStream_SingleStream;
				case Node::Type::InputChannel:
					return Model::IntersectionData::Type::OfflineOutputStream_SingleChannel;
				default:
					return Model::IntersectionData::Type::None;
			}
		}

		if (talkerType == Node::Type::Entity && listenerType == Node::Type::Entity)
		{
			return Model::IntersectionData::Type::Entity_Entity;
		}

		if (talkerType == Node::Type::Entity || listenerType == Node::Type::Entity)
		{
			if (talkerType == Node::Type::RedundantOutput || listenerType == Node::Type::RedundantInput)
			{
				return Model::IntersectionData::Type::Entity_Redundant;
			}

			if (talkerType == Node::Type::RedundantOutputStream || listenerType == Node::Type::RedundantInputStream)
			{
				return Model::IntersectionData::Type::Entity_RedundantStream;
			}

			if (talkerType == Node::Type::OutputStream || listenerType == Node::Type::InputStream)
			{
				return Model::IntersectionData::Type::Entity_SingleStream;
			}

			if (talkerType == Node::Type::OutputChannel || listenerType == Node::Type::InputChannel)
			{
				return Model::IntersectionData::Type::Entity_SingleChannel;
			}
		}

		if (talkerType == Node::Type::RedundantOutput && listenerType == Node::Type::RedundantInput)
		{
			return Model::IntersectionData::Type::Redundant_Redundant;
		}

		if (talkerType == Node::Type::RedundantOutput || listenerType == Node::Type::RedundantInput)
		{
			if (talkerType == Node::Type::RedundantOutputStream || listenerType == Node::Type::RedundantInputStream)
			{
				return Model::IntersectionData::Type::Redundant_RedundantStream;
			}

			if (talkerType == Node::Type::OutputStream || listenerType == Node::Type::InputStream)
			{
				return Model::IntersectionData::Type::Redundant_SingleStream;
			}

			if (talkerType == Node::Type::OutputChannel || listenerType == Node::Type::InputChannel)
			{
				return Model::IntersectionData::Type::Redundant_SingleChannel;
			}
		}

		if (talkerType == Node::Type::RedundantOutputStream && listenerType == Node::Type::RedundantInputStream)
		{
			if (talker->index() == listener->index())
			{
				return Model::IntersectionData::Type::RedundantStream_RedundantStream;
			}
			else
			{
				return Model::IntersectionData::Type::RedundantStream_RedundantStream_Forbidden;
			}
		}

		if (talkerType == Node::Type::RedundantOutputStream || listenerType == Node::Type::RedundantInputStream)
		{
			if (talkerType == Node::Type::OutputStream || listenerType == Node::Type::InputStream)
			{
				return Model::IntersectionData::Type::RedundantStream_SingleStream;
			}
		}

		if (talkerType == Node::Type::OutputStream && listenerType == Node::Type::InputStream)
		{
			return Model::IntersectionData::Type::SingleStream_SingleStream;
		}

		if (talkerType == Node::Type::OutputChannel && listenerType == Node::Type::InputChannel)
		{
			return Model::IntersectionData::Type::SingleChannel_SingleChannel;
		}

		AVDECC_ASSERT_WITH_RET(false, "Not handled");

		return Model::IntersectionData::Type::None;
	}

	// Initializes intersection data
	void initializeIntersectionData(Node* talker, Node* listener, Model::IntersectionData& intersectionData)
	{
		AVDECC_ASSERT(talker, "Invalid talker");
		AVDECC_ASSERT(listener, "Invalid listener");

		intersectionData.talker = talker;
		intersectionData.listener = listener;

		intersectionData.type = determineIntersectionType(talker, listener);
		intersectionData.flags = {};

		// Compute everything for initial state
		computeIntersectionData(intersectionData, allIntersectionDirtyFlags());
	}

	// Updates header data for the given dirtyFlags
	void computeHeaderData(Node* const node, HeaderDirtyFlags const dirtyFlags)
	{
		auto const isMilan = node->entityNode()->isMilan();

		// LockedState is only valid for Milan devices
		if (isMilan && dirtyFlags.test(HeaderDirtyFlag::UpdateLockedState))
		{
			switch (node->type())
			{
				case Node::Type::Entity:
					// Nothing to do, for now
					break;
				case Node::Type::RedundantInput:
				{
					auto summaryLockedState = Node::TriState::Unknown;
					node->accept(
						[&summaryLockedState](Node* node)
						{
							AVDECC_ASSERT(node->isRedundantStreamNode(), "Should be a RedundantStreamNode");
							auto const state = static_cast<StreamNode&>(*node).lockedState();
							switch (state)
							{
								case Node::TriState::Unknown: // Don't change summary value if we don't know this state
									break;
								case Node::TriState::False: // Summary should display UnLock if at least one node is not Locked
									summaryLockedState = state;
									break;
								case Node::TriState::True: // Summary should display Lock if all nodes are Locked
									// Only force to False is summary is currently False. If it's True or Unknown, force to True
									summaryLockedState = (summaryLockedState == Node::TriState::False) ? Node::TriState::False : Node::TriState::True;
									break;
								default:
									AVDECC_ASSERT(false, "Unknown State");
									break;
							}
						},
						true);
					AVDECC_ASSERT(node->isRedundantNode(), "Should be a RedundantNode");
					static_cast<RedundantNode&>(*node).setLockedState(summaryLockedState);
					break;
				}
				case Node::Type::InputStream:
				case Node::Type::RedundantInputStream:
				{
					AVDECC_ASSERT(node->isStreamNode(), "Should be a StreamNode");
					static_cast<StreamNode&>(*node).computeLockedState();
					break;
				}
				case Node::Type::InputChannel:
					// Nothing to do, for now
					break;
				default:
					AVDECC_ASSERT(false, "UpdateLockedState flag should not be set for other types");
					break;
			}
		}

		// IsStreaming is only valid for Milan devices
		if (isMilan && dirtyFlags.test(HeaderDirtyFlag::UpdateIsStreaming))
		{
			switch (node->type())
			{
				case Node::Type::Entity:
					// Nothing to do, for now
					break;
				case Node::Type::RedundantOutput:
				{
					auto summaryStreaming = false;
					node->accept(
						[&summaryStreaming](Node* node)
						{
							AVDECC_ASSERT(node->isRedundantStreamNode(), "Should be a RedundantStreamNode");
							auto const isStreaming = static_cast<StreamNode&>(*node).isStreaming();
							// Summary should display as Streaming if at least one node is Streaming
							summaryStreaming |= isStreaming;
						},
						true);
					AVDECC_ASSERT(node->isRedundantNode(), "Should be a RedundantNode");
					static_cast<RedundantNode&>(*node).setIsStreaming(summaryStreaming);
					break;
				}
				case Node::Type::OutputStream:
				case Node::Type::RedundantOutputStream:
				{
					AVDECC_ASSERT(node->isStreamNode(), "Should be a StreamNode");
					static_cast<StreamNode&>(*node).computeIsStreaming();
					break;
				}
				case Node::Type::OutputChannel:
					// Nothing to do, for now
					break;
				default:
					AVDECC_ASSERT(false, "UpdateIsStreaming flag should not be set for other types");
					break;
			}
		}
	}

	// Updates intersection data for the given dirtyFlags
	void computeIntersectionData(Model::IntersectionData& intersectionData, IntersectionDirtyFlags const dirtyFlags)
	{
		try
		{
			auto const talkerType = intersectionData.talker->type();
			auto const listenerType = intersectionData.listener->type();

			auto const talkerEntityID = intersectionData.talker->entityID();
			auto const listenerEntityID = intersectionData.listener->entityID();

			switch (intersectionData.type)
			{
				case Model::IntersectionData::Type::OfflineOutputStream_Redundant:
				{
					// This is a summary intersection, always update all flags
					intersectionData.flags.clear();

					RedundantNode* redundantNode = static_cast<RedundantNode*>(intersectionData.listener);

					// Reset intersection connection info
					intersectionData.state = Model::IntersectionData::State::NotConnected;
					intersectionData.smartConnectableStreams.clear();

					//  Summary is said to be Connected if at least one stream is connected
					auto areConnected = false;
					auto fastConnecting = false;

					for (auto i = 0; i < redundantNode->childrenCount(); ++i)
					{
						auto const* const redundantStreamNode = static_cast<StreamNode const*>(redundantNode->childAt(i));
						if (AVDECC_ASSERT_WITH_RET(redundantStreamNode, "StreamNode should be valid"))
						{
							AVDECC_ASSERT(redundantStreamNode->isRedundantStreamNode(), "Should be a redundant node");
							auto connectableStream = Model::IntersectionData::SmartConnectableStream{};

							auto const& streamInputConnectionInfo = redundantStreamNode->streamInputConnectionInformation();

							// Check if this stream is connected to an offline talker
							if (streamInputConnectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected && _talkerNodeMap.count(streamInputConnectionInfo.talkerStream.entityID) == 0)
							{
								auto const isConnectedToTalker = streamInputConnectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected;
								auto const isFastConnectingToTalker = streamInputConnectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::FastConnecting;

								// Add stream to smartConnectableStreams
								intersectionData.smartConnectableStreams.push_back(Model::IntersectionData::SmartConnectableStream{ streamInputConnectionInfo.talkerStream, { listenerEntityID, redundantStreamNode->streamIndex() }, isConnectedToTalker, isFastConnectingToTalker });

								// Update State
								areConnected |= isConnectedToTalker;
								fastConnecting |= isFastConnectingToTalker;
							}
						}
					}

					// Update State
					if (areConnected)
					{
						intersectionData.state = Model::IntersectionData::State::Connected;
					}

					break;
				}

				case Model::IntersectionData::Type::OfflineOutputStream_RedundantStream:
				case Model::IntersectionData::Type::OfflineOutputStream_SingleStream:
				{
					auto const* listenerStreamNode = static_cast<StreamNode*>(intersectionData.listener);

					intersectionData.flags = Model::IntersectionData::Flags{};

					// Connected
					if (dirtyFlags.test(IntersectionDirtyFlag::UpdateConnected))
					{
						// If ListenerNode is RedundantInput, get the real StreamNode
						if (listenerType == Node::Type::RedundantInput)
						{
							auto const talkerRedundantStreamOrder = intersectionData.talker->index();
							auto* listenerRedundantNode = static_cast<RedundantNode*>(intersectionData.listener);
							listenerStreamNode = static_cast<StreamNode*>(listenerRedundantNode->childAt(talkerRedundantStreamOrder));
							AVDECC_ASSERT(listenerStreamNode->isRedundantStreamNode(), "Should be a redundant node");
						}
						else if (listenerType != Node::Type::InputStream && listenerType != Node::Type::RedundantInputStream)
						{
							AVDECC_ASSERT(false, "Unhandled");
							return;
						}

						auto const& streamInputConnectionInfo = listenerStreamNode->streamInputConnectionInformation();

						// Reset intersection connection info
						intersectionData.state = Model::IntersectionData::State::NotConnected;
						intersectionData.smartConnectableStreams.clear();

						// Check if this stream is connected to an offline talker
						if (streamInputConnectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected && _talkerNodeMap.count(streamInputConnectionInfo.talkerStream.entityID) == 0)
						{
							auto const isConnectedToTalker = streamInputConnectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected;
							auto const isFastConnectingToTalker = streamInputConnectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::FastConnecting;

							// Add stream to smartConnectableStreams
							intersectionData.smartConnectableStreams.push_back(Model::IntersectionData::SmartConnectableStream{ streamInputConnectionInfo.talkerStream, { listenerEntityID, listenerStreamNode->streamIndex() }, isConnectedToTalker, isFastConnectingToTalker });

							// Update State
							intersectionData.state = Model::IntersectionData::State::Connected;
						}
					}

					break;
				}

				case Model::IntersectionData::Type::Entity_Entity:
				case Model::IntersectionData::Type::Entity_Redundant:
				case Model::IntersectionData::Type::Entity_RedundantStream:
				case Model::IntersectionData::Type::Entity_SingleStream:
				{
					// This is a summary intersection, always update all flags
					intersectionData.flags.clear();

					break;
				}

				case Model::IntersectionData::Type::Redundant_Redundant:
				{
					// This is a summary intersection, always update all flags
					intersectionData.flags.clear();

					auto* talker = static_cast<RedundantNode*>(intersectionData.talker);
					auto* listener = static_cast<RedundantNode*>(intersectionData.listener);

					// Build the list of smart connectable streams:
					intersectionData.smartConnectableStreams.clear();

					auto atLeastOneInterfaceDown = false;
					auto atLeastOneConnected = false;
					auto allConnected = true;
					auto allCompatibleDomain = true;
					auto allCompatibleFormat = true;

					AVDECC_ASSERT(talker->childrenCount() == listener->childrenCount(), "Talker and listener should have the same child count");
					AVDECC_ASSERT(listener->childrenCount() == 2, "Milan redundancy is limited to 2 streams per redundant pair");

					for (auto i = 0; i < talker->childrenCount(); ++i)
					{
						// Avdecc library guarantees that the order of the redundant pair is always Primary, then Secondary
						auto const* const talkerStreamNode = static_cast<StreamNode*>(talker->childAt(i));
						auto const* const listenerStreamNode = static_cast<StreamNode*>(listener->childAt(i));

						auto const talkerInterfaceLinkStatus = talkerStreamNode->interfaceLinkStatus();
						auto const listenerInterfaceLinkStatus = listenerStreamNode->interfaceLinkStatus();
						atLeastOneInterfaceDown |= talkerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down || listenerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down;

						auto const talkerStream = la::avdecc::entity::model::StreamIdentification{ talkerEntityID, talkerStreamNode->streamIndex() };
						auto const isConnectedToTalker = avdecc::helper::isConnectedToTalker(talkerStream, listenerStreamNode->streamInputConnectionInformation());
						auto const isFastConnectingToTalker = avdecc::helper::isFastConnectingToTalker(talkerStream, listenerStreamNode->streamInputConnectionInformation());

						auto const connected = isConnectedToTalker || isFastConnectingToTalker;
						atLeastOneConnected |= connected;
						allConnected &= connected;

						auto const talkerGrandMasterID = talkerStreamNode->grandMasterID();
						auto const listenerGrandMasterID = listenerStreamNode->grandMasterID();
						allCompatibleDomain &= talkerGrandMasterID == listenerGrandMasterID;

						auto const talkerStreamFormat = talkerStreamNode->streamFormat();
						auto const listenerStreamFormat = listenerStreamNode->streamFormat();
						allCompatibleFormat &= la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerStreamFormat, talkerStreamFormat);

						intersectionData.smartConnectableStreams.push_back(Model::IntersectionData::SmartConnectableStream{ { talkerStreamNode->entityID(), talkerStreamNode->streamIndex() }, { listenerStreamNode->entityID(), listenerStreamNode->streamIndex() }, isConnectedToTalker, isFastConnectingToTalker });
					}

					// Update flags
					if (atLeastOneInterfaceDown)
					{
						intersectionData.flags.set(Model::IntersectionData::Flag::InterfaceDown);
					}

					if (!allCompatibleDomain)
					{
						intersectionData.flags.set(Model::IntersectionData::Flag::WrongDomain);
					}

					if (!allCompatibleFormat)
					{
						intersectionData.flags.set(Model::IntersectionData::Flag::WrongFormat);
					}

					// Update State
					if (allConnected)
					{
						intersectionData.state = Model::IntersectionData::State::Connected;
					}
					else if (atLeastOneConnected)
					{
						intersectionData.state = Model::IntersectionData::State::PartiallyConnected;
					}
					else
					{
						intersectionData.state = Model::IntersectionData::State::NotConnected;
					}

					break;
				}

				case Model::IntersectionData::Type::Redundant_SingleStream:
				{
					// This is a summary intersection, always update all flags
					intersectionData.flags.clear();

					RedundantNode* redundantNode = nullptr;
					StreamNode* nonRedundantStreamNode = nullptr;

					// Determine the redundant and non-redundant nodes
					if (talkerType == Node::Type::RedundantOutput)
					{
						redundantNode = static_cast<RedundantNode*>(intersectionData.talker);
						nonRedundantStreamNode = static_cast<StreamNode*>(intersectionData.listener);
					}
					else if (listenerType == Node::Type::RedundantInput)
					{
						redundantNode = static_cast<RedundantNode*>(intersectionData.listener);
						nonRedundantStreamNode = static_cast<StreamNode*>(intersectionData.talker);
					}
					else
					{
						AVDECC_ASSERT(false, "Unhandled");
					}

					// Build the list of smart connectable streams:
					//  - If the Talker is the redundant device, only get the first stream with a matching domain
					//  - If the Listener is the redundant device, get all streams with a matching domain (so we can fully connect a device in cable redundancy mode)
					//  - If a stream is already connected, always add it to the list
					intersectionData.smartConnectableStreams.clear();

					// Also check for Connection state and Domain/Format compatibility in the loop
					//  - Summary is said to be Connected if at least one stream is connected
					//  - Summary is never said to be InterfaceDown (makes no sense since one has to be Up otherwise we wouldn't get messages from the device, and we only have one redundant device here)
					//  - Summary is said to be WrongDomain if both streams have different domains or if a connection is established while on a different domain
					//  - Summary is said to be WrongFormat if format do not match (any of the streams, Milan redundancy impose that both Streams of the pair have the same format)
					auto areConnected = false;
					auto fastConnecting = false;
					auto atLeastOneMatchingDomain = false;
					auto allConnectionsHaveMatchingDomain = true;
					auto isCompatibleFormat = true;
					auto countConnections = size_t{ 0u };
					auto possibleSmartConnectableStreams = decltype(intersectionData.smartConnectableStreams){};

					for (auto i = 0; i < redundantNode->childrenCount(); ++i)
					{
						auto const* const redundantStreamNode = static_cast<StreamNode const*>(redundantNode->childAt(i));
						if (AVDECC_ASSERT_WITH_RET(redundantStreamNode, "StreamNode should be valid"))
						{
							AVDECC_ASSERT(redundantStreamNode->isRedundantStreamNode(), "Should be a redundant node");
							auto connectableStream = Model::IntersectionData::SmartConnectableStream{};
							auto const* listenerStreamInputConnectionInfo = static_cast<la::avdecc::entity::model::StreamInputConnectionInfo const*>(nullptr);
							auto talkerStreamFormat = la::avdecc::entity::model::StreamFormat{};
							auto listenerStreamFormat = la::avdecc::entity::model::StreamFormat{};

							// Get information based on which node is redundant
							if (talkerType == Node::Type::RedundantOutput)
							{
								connectableStream.talkerStream = { talkerEntityID, redundantStreamNode->streamIndex() };
								connectableStream.listenerStream = { listenerEntityID, nonRedundantStreamNode->streamIndex() };
								listenerStreamInputConnectionInfo = &nonRedundantStreamNode->streamInputConnectionInformation();
								talkerStreamFormat = redundantStreamNode->streamFormat();
								listenerStreamFormat = nonRedundantStreamNode->streamFormat();
							}
							else if (listenerType == Node::Type::RedundantInput)
							{
								connectableStream.talkerStream = { talkerEntityID, nonRedundantStreamNode->streamIndex() };
								connectableStream.listenerStream = { listenerEntityID, redundantStreamNode->streamIndex() };
								listenerStreamInputConnectionInfo = &redundantStreamNode->streamInputConnectionInformation();
								talkerStreamFormat = nonRedundantStreamNode->streamFormat();
								listenerStreamFormat = redundantStreamNode->streamFormat();
							}

							// Get Connection State
							connectableStream.isConnected = avdecc::helper::isConnectedToTalker(connectableStream.talkerStream, *listenerStreamInputConnectionInfo);
							connectableStream.isFastConnecting = avdecc::helper::isFastConnectingToTalker(connectableStream.talkerStream, *listenerStreamInputConnectionInfo);
							areConnected |= connectableStream.isConnected;
							fastConnecting |= connectableStream.isFastConnecting;

							// Get Format Compatibility
							isCompatibleFormat &= la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerStreamFormat, talkerStreamFormat);

							// Get Domain Compatibility
							auto const sameDomain = redundantStreamNode->grandMasterID() == nonRedundantStreamNode->grandMasterID();
							atLeastOneMatchingDomain |= sameDomain;
							if (sameDomain || connectableStream.isConnected || connectableStream.isFastConnecting)
							{
								if (connectableStream.isConnected || connectableStream.isFastConnecting)
								{
									// Always add a Connected Stream to the smartConnectable list
									intersectionData.smartConnectableStreams.push_back(connectableStream);
									allConnectionsHaveMatchingDomain &= sameDomain;
									++countConnections;
								}
								else
								{
									// Stream is not connected, we'll decide later if we can add it to the smartConnectable list
									possibleSmartConnectableStreams.push_back(connectableStream);
								}
							}
						}
					}

					// Process possible smartConnectable streams now
					for (auto const& connectableStream : possibleSmartConnectableStreams)
					{
						// Only add to the smart connection list if it's the only one in the list, or if the redundant device is a Listener (for cable redundancy)
						if (intersectionData.smartConnectableStreams.empty() || listenerType == Node::Type::RedundantInput)
						{
							intersectionData.smartConnectableStreams.push_back(connectableStream);
						}
					}

					// Update flags
					if (!atLeastOneMatchingDomain || !allConnectionsHaveMatchingDomain)
					{
						intersectionData.flags.set(Model::IntersectionData::Flag::WrongDomain);
					}

					if (!isCompatibleFormat)
					{
						intersectionData.flags.set(Model::IntersectionData::Flag::WrongFormat);
					}

					// Update State
					if (areConnected)
					{
						if (countConnections == intersectionData.smartConnectableStreams.size())
						{
							intersectionData.state = Model::IntersectionData::State::Connected;
						}
						else
						{
							intersectionData.state = Model::IntersectionData::State::PartiallyConnected;
						}
					}
					else if (fastConnecting)
					{
						// FastConnecting state should only be possible when the non-redundant device is a Listener, as it can be a non-Milan device
						// If the redundant device is a Listener, then it has to be a Milan device, in which case FastConnecting state no longer exists
						intersectionData.state = Model::IntersectionData::State::FastConnecting;
					}
					else
					{
						intersectionData.state = Model::IntersectionData::State::NotConnected;
					}

					break;
				}

				case Model::IntersectionData::Type::Redundant_RedundantStream:
				{
					// Duplicate information from the only allowed redundant stream node
					auto talkerSection = -1;
					auto listenerSection = -1;
					auto const* redundantNode = static_cast<RedundantNode const*>(nullptr);
					auto const* streamNode = static_cast<StreamNode const*>(nullptr);

					if (talkerType == Node::Type::RedundantOutput)
					{
						redundantNode = static_cast<RedundantNode const*>(intersectionData.talker);
						streamNode = static_cast<StreamNode const*>(intersectionData.listener);

						// Get Talker's StreamNode matching Listener's StreamNode (diagonally matching)
						auto const* const otherStreamNode = redundantNode->childAt(streamNode->index());

						// Get the indexes for the Intersection Data we'll copy data from (Which is a RedundantStream_RedundantStream node)
						talkerSection = priv::indexOf(_talkerNodeSectionMap, otherStreamNode);
						listenerSection = priv::indexOf(_listenerNodeSectionMap, streamNode);
					}
					else if (listenerType == Node::Type::RedundantInput)
					{
						redundantNode = static_cast<RedundantNode const*>(intersectionData.listener);
						streamNode = static_cast<StreamNode const*>(intersectionData.talker);

						// Get Listener's StreamNode matching Talker's StreamNode (diagonally matching)
						auto const* const otherStreamNode = redundantNode->childAt(streamNode->index());

						// Get the indexes for the Intersection Data we'll copy data from (Which is a RedundantStream_RedundantStream node)
						talkerSection = priv::indexOf(_talkerNodeSectionMap, streamNode);
						listenerSection = priv::indexOf(_listenerNodeSectionMap, otherStreamNode);
					}
					else
					{
						AVDECC_ASSERT(false, "Unhandled");
					}

					if (!AVDECC_ASSERT_WITH_RET(isValidTalkerSection(talkerSection), "Invalid talker section") || !AVDECC_ASSERT_WITH_RET(isValidListenerSection(listenerSection), "Invalid listener section"))
					{
						break;
					}

					// Get the IntersectionData source node we'll copy the data from
					auto const& sourceIntersectionData = _intersectionData[talkerSection][listenerSection];
					AVDECC_ASSERT(sourceIntersectionData.type == Model::IntersectionData::Type::RedundantStream_RedundantStream, "Intersection should be RedundantStream_RedundantStream");

					intersectionData.state = sourceIntersectionData.state;
					intersectionData.flags = sourceIntersectionData.flags;
					intersectionData.smartConnectableStreams = sourceIntersectionData.smartConnectableStreams;

					break;
				}

				case Model::IntersectionData::Type::RedundantStream_RedundantStream:
				case Model::IntersectionData::Type::RedundantStream_RedundantStream_Forbidden:
				case Model::IntersectionData::Type::RedundantStream_SingleStream:
				case Model::IntersectionData::Type::SingleStream_SingleStream:
				{
					auto const* talkerStreamNode = static_cast<StreamNode*>(intersectionData.talker);
					auto const* listenerStreamNode = static_cast<StreamNode*>(intersectionData.listener);

					auto const talkerInterfaceLinkStatus = talkerStreamNode->interfaceLinkStatus();
					auto const listenerInterfaceLinkStatus = listenerStreamNode->interfaceLinkStatus();

					auto const talkerGrandMasterID = talkerStreamNode->grandMasterID();
					auto const listenerGrandMasterID = listenerStreamNode->grandMasterID();

					auto const interfaceDown = talkerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down || listenerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down;

					intersectionData.flags = computeStreamIntersectionFlags(talkerStreamNode, listenerStreamNode);

					// Connected
					if (dirtyFlags.test(IntersectionDirtyFlag::UpdateConnected))
					{
						// If TalkerNode is RedundantOutput, get the real StreamNode
						if (talkerType == Node::Type::RedundantOutput)
						{
							auto const listenerRedundantStreamOrder = intersectionData.listener->index();
							auto* talkerRedundantNode = static_cast<RedundantNode*>(intersectionData.talker);
							talkerStreamNode = static_cast<StreamNode*>(talkerRedundantNode->childAt(listenerRedundantStreamOrder));
							AVDECC_ASSERT(talkerStreamNode->isRedundantStreamNode(), "Should be a redundant node");
						}
						else if (talkerType != Node::Type::OutputStream && talkerType != Node::Type::RedundantOutputStream)
						{
							AVDECC_ASSERT(false, "Unhandled");
							return;
						}

						// If ListenerNode is RedundantInput, get the real StreamNode
						if (listenerType == Node::Type::RedundantInput)
						{
							auto const talkerRedundantStreamOrder = intersectionData.talker->index();
							auto* listenerRedundantNode = static_cast<RedundantNode*>(intersectionData.listener);
							listenerStreamNode = static_cast<StreamNode*>(listenerRedundantNode->childAt(talkerRedundantStreamOrder));
							AVDECC_ASSERT(listenerStreamNode->isRedundantStreamNode(), "Should be a redundant node");
						}
						else if (listenerType != Node::Type::InputStream && listenerType != Node::Type::RedundantInputStream)
						{
							AVDECC_ASSERT(false, "Unhandled");
							return;
						}

						auto const& streamInputConnectionInfo = listenerStreamNode->streamInputConnectionInformation();
						auto const talkerStream = la::avdecc::entity::model::StreamIdentification{ talkerEntityID, talkerStreamNode->streamIndex() };

						auto const isConnectedToTalker = avdecc::helper::isConnectedToTalker(talkerStream, streamInputConnectionInfo);
						auto const isFastConnectingToTalker = avdecc::helper::isFastConnectingToTalker(talkerStream, streamInputConnectionInfo);

						if (isConnectedToTalker || isFastConnectingToTalker)
						{
							intersectionData.state = Model::IntersectionData::State::Connected;
						}
						else
						{
							intersectionData.state = Model::IntersectionData::State::NotConnected;
						}

						// Build the list of smart connectable streams:
						intersectionData.smartConnectableStreams.clear();
						intersectionData.smartConnectableStreams.push_back(Model::IntersectionData::SmartConnectableStream{ { talkerEntityID, talkerStreamNode->streamIndex() }, { listenerEntityID, listenerStreamNode->streamIndex() }, isConnectedToTalker, isFastConnectingToTalker });
					}

					break;
				}

				case Model::IntersectionData::Type::SingleChannel_SingleChannel:
				{
					auto const* const talkerChannelNode = static_cast<ChannelNode*>(intersectionData.talker);
					auto const* const listenerChannelNode = static_cast<ChannelNode*>(intersectionData.listener);

					// get the stream indices
					auto& channelConnectionManager = avdecc::ChannelConnectionManager::getInstance();
					auto const streamConnectionIndices = channelConnectionManager.getStreamIndexPairUsedByAudioChannelConnection(talkerEntityID, talkerChannelNode->channelIdentification(), listenerEntityID, listenerChannelNode->channelIdentification());

					if (!streamConnectionIndices.empty())
					{
						intersectionData.smartConnectableStreams.clear();
						auto combinedFlags = Model::IntersectionData::Flags{};
						auto allConnected = true;
						for (auto const& streamConnection : streamConnectionIndices)
						{
							auto talkerStreamIndex = streamConnection.first.streamIndex;
							auto listenerStreamIndex = streamConnection.second.streamIndex;

							auto const* talkerStreamNode = ModelPrivate::talkerStreamNode(talkerEntityID, talkerStreamIndex);
							auto const* listenerStreamNode = ModelPrivate::listenerStreamNode(listenerEntityID, listenerStreamIndex);

							combinedFlags |= computeStreamIntersectionFlags(talkerStreamNode, listenerStreamNode);

							auto const talkerStream = la::avdecc::entity::model::StreamIdentification{ talkerEntityID, talkerStreamIndex };
							auto const connected = avdecc::helper::isConnectedToTalker(talkerStream, listenerStreamNode->streamInputConnectionInformation());
							allConnected &= connected;
						}

						intersectionData.flags = combinedFlags;

						if (allConnected)
						{
							intersectionData.state = Model::IntersectionData::State::Connected;
						}
						else
						{
							intersectionData.state = Model::IntersectionData::State::PartiallyConnected;
						}
					}
					else
					{
						intersectionData.flags = {};
						intersectionData.state = Model::IntersectionData::State::NotConnected;
					}

					break;
				}

				default:
					break;
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	Model::IntersectionData::Flags computeStreamIntersectionFlags(StreamNode const* const talkerStreamNode, StreamNode const* const listenerStreamNode) const
	{
		auto flags = Model::IntersectionData::Flags{};

		auto const talkerInterfaceLinkStatus = talkerStreamNode->interfaceLinkStatus();
		auto const listenerInterfaceLinkStatus = listenerStreamNode->interfaceLinkStatus();

		auto const talkerGrandMasterID = talkerStreamNode->grandMasterID();
		auto const listenerGrandMasterID = listenerStreamNode->grandMasterID();

		auto const interfaceDown = talkerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down || listenerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down;

		// InterfaceDown
		if (interfaceDown)
		{
			flags.set(Model::IntersectionData::Flag::InterfaceDown);
		}
		else
		{
			flags.reset(Model::IntersectionData::Flag::InterfaceDown);
		}

		// WrongDomain
		if (talkerGrandMasterID == listenerGrandMasterID)
		{
			flags.reset(Model::IntersectionData::Flag::WrongDomain);
		}
		else
		{
			flags.set(Model::IntersectionData::Flag::WrongDomain);
		}

		// WrongFormat
		auto const talkerStreamFormat = talkerStreamNode->streamFormat();
		auto const listenerStreamFormat = listenerStreamNode->streamFormat();

		if (la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerStreamFormat, talkerStreamFormat))
		{
			flags.reset(Model::IntersectionData::Flag::WrongFormat);
		}
		else
		{
			flags.set(Model::IntersectionData::Flag::WrongFormat);
		}

		return flags;
	}

	// Cache update helpers

	void rebuildTalkerSectionCache()
	{
		_talkerNodeSectionMap = priv::buildNodeSectionMap(_talkerNodes);
	}

	void rebuildListenerSectionCache()
	{
		_listenerNodeSectionMap = priv::buildNodeSectionMap(_listenerNodes);
	}

	// Build talker node hierarchy
	EntityNode* buildTalkerNode(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		try
		{
			auto const configurationIndex = configurationNode.descriptorIndex;

			auto const isMilan = controlledEntity.getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan);
			auto* entity = EntityNode::create(entityID, isMilan);
			entity->setName(avdecc::helper::smartEntityName(controlledEntity));

			auto const fillStreamOutputNode = [&controlledEntity](auto& node, auto const configurationIndex, auto const streamIndex, auto const avbInterfaceIndex, auto const& streamOutputNode, auto const& avbInterfaceNode)
			{
				node.setName(avdecc::helper::outputStreamName(controlledEntity, streamIndex));
				node.setStreamFormat(streamOutputNode.dynamicModel->streamFormat);
				node.setGrandMasterID(avbInterfaceNode.dynamicModel->gptpGrandmasterID);
				node.setGrandMasterDomain(avbInterfaceNode.dynamicModel->gptpDomainNumber);
				node.setInterfaceLinkStatus(controlledEntity.getAvbInterfaceLinkStatus(avbInterfaceIndex));
				node.setRunning(controlledEntity.isStreamOutputRunning(configurationIndex, streamIndex));
				if (streamOutputNode.dynamicModel->counters)
				{
					auto const& counters = *streamOutputNode.dynamicModel->counters;
					{
						auto const it = counters.find(la::avdecc::entity::StreamOutputCounterValidFlag::StreamStart);
						if (it != counters.end())
						{
							node.setStreamStartCounter(it->second);
						}
					}
					{
						auto const it = counters.find(la::avdecc::entity::StreamOutputCounterValidFlag::StreamStop);
						if (it != counters.end())
						{
							node.setStreamStopCounter(it->second);
						}
					}
				}
			};

			// Redundant streams
			for (auto const& [redundantIndex, redundantNode] : configurationNode.redundantStreamOutputs)
			{
				auto* redundantOutput = RedundantNode::createOutputNode(*entity, redundantIndex);
				redundantOutput->setName(avdecc::helper::redundantOutputName(redundantIndex));

				for (auto const& [streamIndex, streamNode] : redundantNode.redundantStreams)
				{
					auto const avbInterfaceIndex{ streamNode->staticModel->avbInterfaceIndex };
					auto const& avbInterfaceNode = controlledEntity.getAvbInterfaceNode(configurationIndex, avbInterfaceIndex);

					auto* redundantOutputStream = StreamNode::createRedundantOutputNode(*redundantOutput, streamIndex, avbInterfaceIndex);
					auto const* const streamOutputNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(streamNode);
					fillStreamOutputNode(*redundantOutputStream, configurationIndex, streamIndex, avbInterfaceIndex, *streamOutputNode, avbInterfaceNode);
				}
			}

			// Single streams
			for (auto const& [streamIndex, streamNode] : configurationNode.streamOutputs)
			{
				if (!streamNode.isRedundant)
				{
					auto const avbInterfaceIndex{ streamNode.staticModel->avbInterfaceIndex };
					auto const& avbInterfaceNode = controlledEntity.getAvbInterfaceNode(configurationIndex, avbInterfaceIndex);

					auto* outputStream = StreamNode::createOutputNode(*entity, streamIndex, avbInterfaceIndex);
					fillStreamOutputNode(*outputStream, configurationIndex, streamIndex, avbInterfaceIndex, streamNode, avbInterfaceNode);
				}
			}

			// Channels for Milan compatible entities only
			if (isMilan)
			{
				for (auto const& [audioUnitIndex, audioUnitNode] : configurationNode.audioUnits)
				{
					for (auto const& [streamPortIndex, streamPortNode] : audioUnitNode.streamPortOutputs)
					{
						// Save ChannelOffset for this StreamPort
						entity->setStreamPortOutputClusterOffset(streamPortIndex, streamPortNode.staticModel->baseCluster);

						if (streamPortNode.staticModel->hasDynamicAudioMap)
						{
							// Save current mappings (we want all mappings, including redundant)
							entity->setOutputAudioMappings(streamPortIndex, streamPortNode.dynamicModel->dynamicAudioMap);
						}
						else
						{
							la::avdecc::entity::model::AudioMappings staticMappings;
							for (auto const& mapKV : streamPortNode.audioMaps)
							{
								staticMappings.insert(staticMappings.end(), mapKV.second.staticModel->mappings.begin(), mapKV.second.staticModel->mappings.end());
							}
							entity->setOutputAudioMappings(streamPortIndex, staticMappings);
						}

						// Process all Clusters
						for (auto const& [clusterIndex, clusterNode] : streamPortNode.audioClusters)
						{
							auto const* const staticModel = clusterNode.staticModel;
							for (auto channel = (uint16_t)0u; channel < staticModel->channelCount; ++channel)
							{
								auto channelIdentification = avdecc::ChannelIdentification{ configurationNode.descriptorIndex, clusterIndex, channel, avdecc::ChannelConnectionDirection::InputToOutput, audioUnitIndex, streamPortIndex, streamPortNode.staticModel->baseCluster };

								auto* outputChannel = ChannelNode::createOutputNode(*entity, channelIdentification);
								auto const clusterName = avdecc::helper::objectName(&controlledEntity, streamPortNode.audioClusters.at(clusterIndex));
								auto const channelName = priv::clusterChannelName(clusterName, channel);
								outputChannel->setName(channelName);
							}
						}
					}
				}
			}

			return entity;
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const& e)
		{
			LOG_HIVE_ERROR(QString("Cannot build TalkerNode for EntityID=%1: %2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(e.what()));
			return nullptr;
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
			LOG_HIVE_ERROR(QString("Cannot build TalkerNode for EntityID=%1").arg(avdecc::helper::uniqueIdentifierToString(entityID)));
			return nullptr;
		}
	}

	// Build listener node hierarchy
	EntityNode* buildListenerNode(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		try
		{
			auto const configurationIndex = configurationNode.descriptorIndex;

			auto const isMilan = controlledEntity.getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan);
			auto* entity = EntityNode::create(entityID, isMilan);
			entity->setName(avdecc::helper::smartEntityName(controlledEntity));

			auto const fillStreamInputNode = [&controlledEntity](auto& node, auto const configurationIndex, auto const streamIndex, auto const avbInterfaceIndex, auto const& streamInputNode, auto const& avbInterfaceNode)
			{
				node.setName(avdecc::helper::inputStreamName(controlledEntity, streamIndex));
				node.setStreamFormat(streamInputNode.dynamicModel->streamFormat);
				node.setGrandMasterID(avbInterfaceNode.dynamicModel->gptpGrandmasterID);
				node.setGrandMasterDomain(avbInterfaceNode.dynamicModel->gptpDomainNumber);
				node.setInterfaceLinkStatus(controlledEntity.getAvbInterfaceLinkStatus(avbInterfaceIndex));
				node.setRunning(controlledEntity.isStreamInputRunning(configurationIndex, streamIndex));
				node.setStreamInputConnectionInformation(streamInputNode.dynamicModel->connectionInfo);
				if (streamInputNode.dynamicModel->counters)
				{
					auto const& counters = *streamInputNode.dynamicModel->counters;
					{
						auto const it = counters.find(la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked);
						if (it != counters.end())
						{
							node.setMediaLockedCounter(it->second);
						}
					}
					{
						auto const it = counters.find(la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked);
						if (it != counters.end())
						{
							node.setMediaUnlockedCounter(it->second);
						}
					}
				}
				if (streamInputNode.dynamicModel->streamDynamicInfo && (*streamInputNode.dynamicModel->streamDynamicInfo).probingStatus)
				{
					node.setProbingStatus(*(*streamInputNode.dynamicModel->streamDynamicInfo).probingStatus);
				}
			};

			// Redundant streams
			for (auto const& [redundantIndex, redundantNode] : configurationNode.redundantStreamInputs)
			{
				auto* redundantInput = RedundantNode::createInputNode(*entity, redundantIndex);
				redundantInput->setName(avdecc::helper::redundantInputName(redundantIndex));

				for (auto const& [streamIndex, streamNode] : redundantNode.redundantStreams)
				{
					auto const avbInterfaceIndex{ streamNode->staticModel->avbInterfaceIndex };
					auto const& avbInterfaceNode = controlledEntity.getAvbInterfaceNode(configurationIndex, avbInterfaceIndex);

					auto* redundantInputStream = StreamNode::createRedundantInputNode(*redundantInput, streamIndex, avbInterfaceIndex);
					redundantInputStream->setName(avdecc::helper::inputStreamName(controlledEntity, streamIndex));

					auto const* const streamInputNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(streamNode);
					fillStreamInputNode(*redundantInputStream, configurationIndex, streamIndex, avbInterfaceIndex, *streamInputNode, avbInterfaceNode);
				}
			}

			// Single streams
			for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
			{
				if (!streamNode.isRedundant)
				{
					auto const avbInterfaceIndex{ streamNode.staticModel->avbInterfaceIndex };
					auto const& avbInterfaceNode = controlledEntity.getAvbInterfaceNode(configurationIndex, avbInterfaceIndex);

					auto* inputStream = StreamNode::createInputNode(*entity, streamIndex, avbInterfaceIndex);
					fillStreamInputNode(*inputStream, configurationIndex, streamIndex, avbInterfaceIndex, streamNode, avbInterfaceNode);
				}
			}

			// Channels for Milan compatible entities only
			if (isMilan)
			{
				for (auto const& [audioUnitIndex, audioUnitNode] : configurationNode.audioUnits)
				{
					for (auto const& [streamPortIndex, streamPortNode] : audioUnitNode.streamPortInputs)
					{
						if (streamPortNode.staticModel->hasDynamicAudioMap)
						{
							// Save ChannelOffset for this StreamPort
							entity->setStreamPortInputClusterOffset(streamPortIndex, streamPortNode.staticModel->baseCluster);

							// Save current mappings (we want all mappings, including redundant)
							entity->setInputAudioMappings(streamPortIndex, streamPortNode.dynamicModel->dynamicAudioMap);

							// Process all Clusters
							for (auto const& [clusterIndex, clusterNode] : streamPortNode.audioClusters)
							{
								auto const* const staticModel = clusterNode.staticModel;
								for (auto channel = (uint16_t)0u; channel < staticModel->channelCount; ++channel)
								{
									auto channelIdentification = avdecc::ChannelIdentification{ configurationNode.descriptorIndex, clusterIndex, channel, avdecc::ChannelConnectionDirection::InputToOutput, audioUnitIndex, streamPortIndex, streamPortNode.staticModel->baseCluster };

									auto* inputChannel = ChannelNode::createInputNode(*entity, channelIdentification);
									auto const clusterName = avdecc::helper::objectName(&controlledEntity, streamPortNode.audioClusters.at(clusterIndex));
									auto const channelName = priv::clusterChannelName(clusterName, channel);
									inputChannel->setName(channelName);
								}
							}
						}
					}
				}
			}

			return entity;
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const& e)
		{
			LOG_HIVE_ERROR(QString("Cannot build ListenerNode for EntityID=%1: %2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(e.what()));
			return nullptr;
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
			LOG_HIVE_ERROR(QString("Cannot build ListenerNode for EntityID=%1").arg(avdecc::helper::uniqueIdentifierToString(entityID)));
			return nullptr;
		}
	}

	void insertOfflineOutputStreamNode()
	{
		if (_mode == Model::Mode::Stream)
		{
			insertTalkerNode(_offlineOutputStreamNode.get());
		}
	}

	// Insert a talker node hierarchy in the model
	template<class NodeType>
	std::enable_if_t<std::is_same_v<NodeType, EntityNode> | std::is_same_v<NodeType, OfflineOutputStreamNode>> insertTalkerNode(NodeType* node)
	{
		if (!AVDECC_ASSERT_WITH_RET(node, "Node should not be null"))
		{
			return;
		}

		auto const entityID = node->entityID();

		auto const flattendedNodes = priv::flattenEntityNode(node, _mode);
		auto const childrenCount = static_cast<int>(flattendedNodes.size()) - 1;

		// Always return if count is negative
		if (!AVDECC_ASSERT_WITH_RET(childrenCount >= 0, "children count should never be negative"))
		{
			return;
		}

		if constexpr (std::is_same_v<NodeType, EntityNode>)
		{
			// This entity has nothing to display in this mode
			if (childrenCount <= 0)
			{
				return;
			}
		}

		auto const first = priv::sortedIndexForEntity(_talkerNodes, entityID);
		auto const last = first + childrenCount;

		beginInsertTalkerItems(first, last);

		priv::insertNodes(_talkerNodes, flattendedNodes, first);

		rebuildTalkerSectionCache();

		// Insert new talker rows
		auto const it = std::next(std::begin(_intersectionData), first);
		_intersectionData.insert(it, childrenCount + 1, {});

		// Update intersection matrix (Start from the end so that children are initialized before parents)
		for (auto talkerSection = last; talkerSection >= first; --talkerSection)
		{
			auto& row = _intersectionData[talkerSection];
			row.resize(_listenerNodes.size());

			auto* talker = _talkerNodes[talkerSection];
			for (auto listenerSection = _listenerNodes.size(); listenerSection > 0u; --listenerSection)
			{
				auto* listener = _listenerNodes[listenerSection - 1];
				initializeIntersectionData(talker, listener, row[listenerSection - 1]);
			}
		}

#if ENABLE_CONNECTION_MATRIX_DEBUG
		dump();
#endif

		endInsertTalkerItems();
	}

	// Insert a listener node hierarchy in the model
	void insertListenerNode(EntityNode* node)
	{
		if (!AVDECC_ASSERT_WITH_RET(node, "Node should not be null"))
		{
			return;
		}

		auto const entityID = node->entityID();

		auto const flattendedNodes = priv::flattenEntityNode(node, _mode);
		auto const childrenCount = static_cast<int>(flattendedNodes.size()) - 1;

		// This entity has nothing to display in this mode
		if (childrenCount <= 0)
		{
			return;
		}

		auto const first = priv::sortedIndexForEntity(_listenerNodes, entityID);
		auto const last = first + childrenCount;

		beginInsertListenerItems(first, last);

		priv::insertNodes(_listenerNodes, flattendedNodes, first);

		rebuildListenerSectionCache();

		// Update intersection matrix (Start from the end so that children are initialized before parents)
		for (auto talkerSection = _talkerNodes.size(); talkerSection > 0u; --talkerSection)
		{
			auto& row = _intersectionData[talkerSection - 1];

			// Insert new listener columns
			auto const it = std::next(std::begin(row), first);
			row.insert(it, childrenCount + 1, {});

			auto* talker = _talkerNodes[talkerSection - 1];
			for (auto listenerSection = last; listenerSection >= first; --listenerSection)
			{
				auto* listener = _listenerNodes[listenerSection];
				initializeIntersectionData(talker, listener, row[listenerSection]);
			}
		}

#if ENABLE_CONNECTION_MATRIX_DEBUG
		dump();
#endif

		endInsertListenerItems();
	}

	// Remove complete talker node hierarchy from entityID
	void removeTalker(EntityNode* node)
	{
		auto const flattendedNodes = priv::flattenEntityNode(node, _mode);
		auto const childrenCount = static_cast<int>(flattendedNodes.size()) - 1;

		if (childrenCount <= 0)
		{
			return;
		}

		auto const first = priv::indexOf(_talkerNodeSectionMap, node);
		auto const last = first + childrenCount;

		beginRemoveTalkerItems(first, last);

		priv::removeNodes(_talkerNodes, first, last + 1 /* entity */);

		rebuildTalkerSectionCache();

		_intersectionData.erase(std::next(std::begin(_intersectionData), first), std::next(std::begin(_intersectionData), last + 1));

#if ENABLE_CONNECTION_MATRIX_DEBUG
		dump();
#endif

		endRemoveTalkerItems();
	}

	// Remove complete listener node hierarchy from entityID
	void removeListener(EntityNode* node)
	{
		auto const flattendedNodes = priv::flattenEntityNode(node, _mode);
		auto const childrenCount = static_cast<int>(flattendedNodes.size()) - 1;

		if (childrenCount <= 0)
		{
			return;
		}

		auto const first = priv::indexOf(_listenerNodeSectionMap, node);
		auto const last = first + childrenCount;

		beginRemoveListenerItems(first, last);

		priv::removeNodes(_listenerNodes, first, last + 1 /* entity */);

		rebuildListenerSectionCache();

		for (auto talkerSection = 0u; talkerSection < _talkerNodes.size(); ++talkerSection)
		{
			auto& row = _intersectionData[talkerSection];

			row.erase(std::next(std::begin(row), first), std::next(std::begin(row), last + 1));
		}

#if ENABLE_CONNECTION_MATRIX_DEBUG
		dump();
#endif

		endRemoveListenerItems();
	}

	// Returns talker node at section if valid, nullptr otherwise
	Node* talkerNode(int section) const
	{
		if (!isValidTalkerSection(section))
		{
			return nullptr;
		}

		return _talkerNodes[section];
	}

	// Returns listener node at section if valid, nullptr otherwise
	Node* listenerNode(int section) const
	{
		if (!isValidListenerSection(section))
		{
			return nullptr;
		}

		return _listenerNodes[section];
	}

	// avdecc::ControllerManager slots

	void handleControllerOffline()
	{
		resetModel();
	}

	void handleEntityOnline(la::avdecc::UniqueIdentifier const entityID)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity && AVDECC_ASSERT_WITH_RET(!controlledEntity->gotFatalEnumerationError(), "An entity should not be set online if it had an enumeration error"))
			{
				auto const entityCapabilities = controlledEntity->getEntity().getEntityCapabilities();

				if (!entityCapabilities.test(la::avdecc::entity::EntityCapability::AemSupported))
				{
					return;
				}

				auto const& entityNode = controlledEntity->getEntityNode();
				auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);

				// Talker
				if (controlledEntity->getEntity().getTalkerCapabilities().test(la::avdecc::entity::TalkerCapability::Implemented) && !configurationNode.streamOutputs.empty())
				{
					if (auto* node = buildTalkerNode(*controlledEntity, entityID, configurationNode))
					{
						_talkerNodeMap.insert(std::make_pair(entityID, node));

						priv::insertStreamNodes(_talkerStreamNodeMap, node);
						priv::insertChannelNodes(_talkerChannelNodeMap, node);

						insertTalkerNode(node);
					}
				}

				// Listener
				if (controlledEntity->getEntity().getListenerCapabilities().test(la::avdecc::entity::ListenerCapability::Implemented) && !configurationNode.streamInputs.empty())
				{
					if (auto* node = buildListenerNode(*controlledEntity, entityID, configurationNode))
					{
						// Insert nodes in cache for quick access
						_listenerNodeMap.insert(std::make_pair(entityID, node));

						priv::insertStreamNodes(_listenerStreamNodeMap, node);
						priv::insertChannelNodes(_listenerChannelNodeMap, node);

						insertListenerNode(node);
					}
				}
			}

			// Trigger "special offline streams" intersection update
			if (_mode == Model::Mode::Stream)
			{
				talkerIntersectionDataChanged(_offlineOutputStreamNode.get(), false, true, IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateConnected });
			}
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// Ignore exception
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleEntityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		if (auto* node = talkerNodeFromEntityID(entityID))
		{
			removeTalker(node);

			// Remove from cache
			priv::removeStreamNodes(_talkerStreamNodeMap, node);
			priv::removeChannelNodes(_talkerChannelNodeMap, node);
			_talkerNodeMap.erase(entityID);
		}

		if (auto* node = listenerNodeFromEntityID(entityID))
		{
			removeListener(node);

			// Remove from cache
			priv::removeStreamNodes(_listenerStreamNodeMap, node);
			priv::removeChannelNodes(_listenerChannelNodeMap, node);
			_listenerNodeMap.erase(entityID);
		}

		// Trigger "special offline streams" intersection update
		if (_mode == Model::Mode::Stream)
		{
			talkerIntersectionDataChanged(_offlineOutputStreamNode.get(), false, true, IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateConnected });
		}
	}

	void handleGptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
	{
		// Event affecting the whole entity (all streams, Input and Output)
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateGptp };

		if (auto* talker = talkerNodeFromEntityID(entityID))
		{
			talker->accept(avbInterfaceIndex,
				[this, grandMasterID, grandMasterDomain, dirtyFlags, talker, entityID](StreamNode* node)
				{
					node->setGrandMasterID(grandMasterID);
					node->setGrandMasterDomain(grandMasterDomain);

					if (_mode == Model::Mode::Stream)
					{
						talkerIntersectionDataChanged(node, true, false, dirtyFlags);
					}
					else
					{
						updateTalkerIntersectionChannels(entityID, dirtyFlags, talker, node);
					}
				});
		}

		if (auto* listener = listenerNodeFromEntityID(entityID))
		{
			listener->accept(avbInterfaceIndex,
				[this, grandMasterID, grandMasterDomain, dirtyFlags, listener, entityID](StreamNode* node)
				{
					node->setGrandMasterID(grandMasterID);
					node->setGrandMasterDomain(grandMasterDomain);

					if (_mode == Model::Mode::Stream)
					{
						listenerIntersectionDataChanged(node, true, false, dirtyFlags);
					}
					else
					{
						updateListenerIntersectionChannels(entityID, dirtyFlags, listener, node);
					}
				});
		}
	}

	void handleEntityNameChanged(la::avdecc::UniqueIdentifier const entityID)
	{
		// Event affecting the whole entity (but just the entity node as Talker and Listener)
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				auto const name = avdecc::helper::smartEntityName(*controlledEntity);

				if (auto* node = talkerNodeFromEntityID(entityID))
				{
					node->setName(name);

					// Always notify the view, EntityName is displayed in CBR and SBR modes
					talkerHeaderDataChanged(node, true, false, {});
				}

				if (auto* node = listenerNodeFromEntityID(entityID))
				{
					node->setName(name);

					listenerHeaderDataChanged(node, false, {});
				}
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleAvbInterfaceLinkStatusChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus)
	{
		// Event affecting the whole entity (all streams, Input and Output)
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateLinkStatus };

		if (auto* talker = talkerNodeFromEntityID(entityID))
		{
			talker->accept(avbInterfaceIndex,
				[this, linkStatus, dirtyFlags, talker, entityID](StreamNode* node)
				{
					node->setInterfaceLinkStatus(linkStatus);

					if (_mode == Model::Mode::Stream)
					{
						talkerIntersectionDataChanged(node, true, false, dirtyFlags);
					}
					else
					{
						updateTalkerIntersectionChannels(entityID, dirtyFlags, talker, node);
					}
				});
		}

		if (auto* listener = listenerNodeFromEntityID(entityID))
		{
			listener->accept(avbInterfaceIndex,
				[this, linkStatus, dirtyFlags, listener, entityID](StreamNode* node)
				{
					node->setInterfaceLinkStatus(linkStatus);

					if (_mode == Model::Mode::Stream)
					{
						listenerIntersectionDataChanged(node, true, false, dirtyFlags);
					}
					else
					{
						updateListenerIntersectionChannels(entityID, dirtyFlags, listener, node);
					}
				});
		}
	}

	void handleStreamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
	{
		// Event affecting a single stream node (either Input or Output), but having repercussion on parent intersection "summary" nodes
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateFormat };

		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			if (auto* talker = talkerNodeFromEntityID(entityID))
			{
				if (auto* node = talkerStreamNode(entityID, streamIndex))
				{
					node->setStreamFormat(streamFormat);

					if (_mode == Model::Mode::Stream)
					{
						talkerIntersectionDataChanged(node, true, false, dirtyFlags);
					}
					else
					{
						updateTalkerIntersectionChannels(entityID, dirtyFlags, talker, node);
					}
				}
				else
				{
					LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamFormatChanged: Invalid StreamOutputIndex: TalkerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
				}
			}
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			if (auto* listener = listenerNodeFromEntityID(entityID))
			{
				if (auto* node = listenerStreamNode(entityID, streamIndex))
				{
					node->setStreamFormat(streamFormat);

					if (_mode == Model::Mode::Stream)
					{
						listenerIntersectionDataChanged(node, true, false, dirtyFlags);
					}
					else
					{
						updateListenerIntersectionChannels(entityID, dirtyFlags, listener, node);
					}
				}
				else
				{
					LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamFormatChanged: Invalid StreamInputIndex: ListenerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
				}
			}
		}
	}

	void handleStreamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
	{
		// Event affecting a single stream node (either Input or Output)
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			if (auto* node = talkerStreamNode(entityID, streamIndex))
			{
				node->setRunning(isRunning);

				// Notify view based on current mode, since we have a StreamNode here
				talkerHeaderDataChanged(node, _mode == Model::Mode::Stream, false, {});
#pragma message("TODO: Find affected Channels and for each, call talkerHeaderDataChanged(node, _mode == Model::Mode::Channel, false, {UpdateStreamRunning});")
			}
			else
			{
				LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamRunningChanged: Invalid StreamOutputIndex: TalkerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
			}
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			if (auto* node = listenerStreamNode(entityID, streamIndex))
			{
				node->setRunning(isRunning);

				listenerHeaderDataChanged(node, false, {});
#pragma message("TODO: Find affected Channels and for each, call listenerHeaderDataChanged(node, _mode == Model::Mode::Channel, false, {UpdateStreamRunning});")
			}
			else
			{
				LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamRunningChanged: Invalid StreamInputIndex: ListenerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
			}
		}
	}

	void handleStreamInputConnectionChanged(la::avdecc::entity::model::StreamIdentification const& stream, la::avdecc::entity::model::StreamInputConnectionInfo const& info)
	{
		// Event affecting a single stream intersection, but having repercussion on parent intersection "summary" nodes
		auto const entityID = stream.entityID;
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateConnected };

		if (auto* listener = listenerNodeFromEntityID(entityID))
		{
			if (auto* node = listenerStreamNode(entityID, stream.streamIndex))
			{
				node->setStreamInputConnectionInformation(info);

				if (_mode == Model::Mode::Stream)
				{
					listenerIntersectionDataChanged(node, true, true, dirtyFlags);
				}
				else
				{
					updateListenerIntersectionChannels(entityID, dirtyFlags, listener, node);
				}

				listenerHeaderDataChanged(node, true, HeaderDirtyFlags{ HeaderDirtyFlag::UpdateLockedState });
#pragma message("TODO: Find affected Channels and for each, call listenerHeaderDataChanged(node, _mode == Model::Mode::Channel, false, {UpdateLockedState});")
			}
			else
			{
				LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamInputConnectionChanged: Invalid StreamIndex: ListenerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(stream.streamIndex));
			}
		}
	}

	void handleStreamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const /*configurationIndex*/, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex)
	{
		// Event affecting a single stream node (either Input or Output)
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
				{
					auto const name = avdecc::helper::outputStreamName(*controlledEntity, streamIndex);

					if (auto* node = talkerStreamNode(entityID, streamIndex))
					{
						node->setName(name);

						// Only notify the view in SBR mode, StreamName is not displayed in CBR mode
						talkerHeaderDataChanged(node, _mode == Model::Mode::Stream, false, {});
					}
					else
					{
						LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamNameChanged: Invalid StreamOutputIndex: TalkerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
					}
				}
				else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
				{
					auto const name = avdecc::helper::inputStreamName(*controlledEntity, streamIndex);

					if (auto* node = listenerStreamNode(entityID, streamIndex))
					{
						node->setName(name);

						listenerHeaderDataChanged(node, false, {});
					}
					else
					{
						LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamNameChanged: Invalid StreamInputIndex: ListenerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
					}
				}
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleStreamDynamicInfoChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamDynamicInfo const& info)
	{
		// Event affecting a single stream node (Input)
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
				{
					if (auto* node = listenerStreamNode(entityID, streamIndex))
					{
						if (info.probingStatus)
						{
							if (node->setProbingStatus(*info.probingStatus))
							{
								listenerHeaderDataChanged(node, true, HeaderDirtyFlags{ HeaderDirtyFlag::UpdateLockedState });
#pragma message("TODO: Find affected Channels and for each, call listenerHeaderDataChanged(node, _mode == Model::Mode::Channel, false, {UpdateLockedState});")
							}
						}
					}
					else
					{
						LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamInfoChanged: Invalid StreamInputIndex: ListenerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
					}
				}
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleStreamInputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamInputCounters const& counters)
	{
		// Event affecting a single stream node (Input)
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				if (auto* node = listenerStreamNode(entityID, streamIndex))
				{
					auto changed = false;

					for (auto const [flag, counter] : counters)
					{
						switch (flag)
						{
							case la::avdecc::entity::StreamInputCounterValidFlag::MediaLocked:
								node->setMediaLockedCounter(counter);
								changed = true;
								break;
							case la::avdecc::entity::StreamInputCounterValidFlag::MediaUnlocked:
								node->setMediaUnlockedCounter(counter);
								changed = true;
								break;
							default:
								break;
						}
					}

					if (changed)
					{
						listenerHeaderDataChanged(node, true, HeaderDirtyFlags{ HeaderDirtyFlag::UpdateLockedState });
#pragma message("TODO: Find affected Channels and for each, call listenerHeaderDataChanged(node, _mode == Model::Mode::Channel, false, {UpdateLockedState});")
					}
				}
				else
				{
					LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamInputCountersChanged: Invalid StreamInputIndex: ListenerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
				}
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleStreamOutputCountersChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamOutputCounters const& counters)
	{
		// Event affecting a single stream node (Output)
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				if (auto* node = talkerStreamNode(entityID, streamIndex))
				{
					auto changed = false;

					for (auto const [flag, counter] : counters)
					{
						switch (flag)
						{
							case la::avdecc::entity::StreamOutputCounterValidFlag::StreamStart:
								node->setStreamStartCounter(counter);
								changed = true;
								break;
							case la::avdecc::entity::StreamOutputCounterValidFlag::StreamStop:
								node->setStreamStopCounter(counter);
								changed = true;
								break;
							default:
								break;
						}
					}

					if (changed)
					{
						// Notify view based on current mode, since we have a StreamNode here
						talkerHeaderDataChanged(node, _mode == Model::Mode::Stream, true, HeaderDirtyFlags{ HeaderDirtyFlag::UpdateIsStreaming });
#pragma message("TODO: Find affected Channels and for each, call talkerHeaderDataChanged(node, _mode == Model::Mode::Channel, false, {UpdateIsStreaming});")
					}
				}
				else
				{
					LOG_HIVE_ERROR(QString("connectionMatrix::Model::StreamOutputCountersChanged: Invalid StreamOutputIndex: TalkerID=%1 StreamIndex=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));
				}
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleStreamPortAudioMappingsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex)
	{
		// Event affecting multiple channel nodes (either Input or Output)
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateConnected, IntersectionDirtyFlag::UpdateFormat, IntersectionDirtyFlag::UpdateGptp, IntersectionDirtyFlag::UpdateLinkStatus };

				if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
				{
					if (auto* talker = talkerNodeFromEntityID(entityID))
					{
						// Save current mappings (we want all mappings, including redundant)
						talker->setOutputAudioMappings(streamPortIndex, controlledEntity->getStreamPortOutputAudioMappings(streamPortIndex));
						// No need to trigger a refresh here, the same event is already handled by ChannelConnectionManager
					}
				}
				else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
				{
					if (auto* listener = listenerNodeFromEntityID(entityID))
					{
						// Save current mappings (we want all mappings, including redundant)
						listener->setInputAudioMappings(streamPortIndex, controlledEntity->getStreamPortInputAudioMappings(streamPortIndex));
						// No need to trigger a refresh here, the same event is already handled by ChannelConnectionManager
					}
				}
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleCompatibilityFlagsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::ControlledEntity::CompatibilityFlags compatibilityFlags)
	{
		auto const isMilan = compatibilityFlags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan);

		// Check if entity was Milan compatible, but isn't anymore
		if (!isMilan)
		{
#pragma message("TODO: Handle this properly (see comments below)")
			/*
			When an entity looses it's Milan Compatibility Flag (and only this flag, not the other existing or future flags), we want it to be removed from the CBR view but NOT from the SBR view.
			So we have to clear the CBR cached nodes, the channel nodes from the EntityNode, and force a refresh (do the reverse of what is done in buildTalkerNode and buildListenerNode)
			See void Model::setMode(Mode const mode), we probably want to do what's inside this method, but for just this entity
			*/
#if 1
			// Quick hack, always trigger entity offline/online
			handleEntityOffline(entityID);
			handleEntityOnline(entityID);
#else
			if (auto* node = talkerNodeFromEntityID(entityID))
			{
				// Was Milan
				if (node->isMilan())
				{
					// Remove from Channel cache
					priv::removeChannelNodes(_talkerChannelNodeMap, node);
					... Other things to do
				}
			}

			if (auto* node = listenerNodeFromEntityID(entityID))
			{
				if (node->isMilan())
				{
					// Remove from Channel cache
					priv::removeChannelNodes(_listenerChannelNodeMap, node);
					... Other things to do
				}
			}
#endif
		}
	}

	void handleAudioClusterNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName)
	{
		try
		{
#pragma message("TODO: cache the current configuration in the node to avoid locking the controller from the main thread")
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			auto const& entityNode = controlledEntity->getEntityNode();

			// We're only interested in the current configuration
			if (entityNode.dynamicModel->currentConfiguration != configurationIndex)
			{
				return;
			}

			// ClusterIndex numbering is shared among all STREAM_PORTS (including input/output), meaning we have to check for which kind (input or output) it belongs to

			if (hasTalkerCluster(entityID, audioClusterIndex))
			{
				if (auto* channelNode = talkerChannelNode(entityID, audioClusterIndex))
				{
					auto const channelName = priv::clusterChannelName(audioClusterName, channelNode->channelIndex());
					channelNode->setName(channelName);

					// Only notify the view in CBR mode, ClusterName is not displayed in SBR mode
					talkerHeaderDataChanged(channelNode, _mode == Model::Mode::Channel, false, {});
				}
			}

			if (hasListenerCluster(entityID, audioClusterIndex))
			{
				if (auto* channelNode = listenerChannelNode(entityID, audioClusterIndex))
				{
					auto const channelName = priv::clusterChannelName(audioClusterName, channelNode->channelIndex());
					channelNode->setName(channelName);

					listenerHeaderDataChanged(channelNode, false, {});
				}
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	// avdecc::ChannelConnectionManager slots
	void handleListenerChannelConnectionsUpdate(std::set<std::pair<la::avdecc::UniqueIdentifier, avdecc::ChannelIdentification>> const& channels)
	{
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateConnected };

		for (auto const& [entityID, channelInfo] : channels)
		{
			auto* listenerNode = listenerChannelNode(entityID, channelInfo.clusterIndex);

			if (_mode == Model::Mode::Channel)
			{
				listenerIntersectionDataChanged(listenerNode, true, false, dirtyFlags);
			}
		}
	}

private:
	void updateTalkerIntersectionChannels(la::avdecc::UniqueIdentifier const entityID, IntersectionDirtyFlags const dirtyFlags, EntityNode* const talker, StreamNode* const node)
	{
		auto const& mappings = talker->getOutputAudioMappings();
		for (auto const& [streamPortIndex, audioUnitMappings] : mappings)
		{
			auto const clusterOffset = talker->getStreamPortOutputClusterOffset(streamPortIndex);

			// One stream can have multiple Channels
			for (auto const& mapping : audioUnitMappings)
			{
				if (mapping.streamIndex == node->streamIndex())
				{
					if (auto* channelNode = talkerChannelNode(entityID, clusterOffset + mapping.clusterOffset))
					{
						talkerIntersectionDataChanged(channelNode, true, false, dirtyFlags);
						qDebug() << "updateTalkerIntersectionChannels: Update Channel #" << (clusterOffset + mapping.clusterOffset);
					}
				}
			}
		}
	}

	void updateListenerIntersectionChannels(la::avdecc::UniqueIdentifier const entityID, IntersectionDirtyFlags const dirtyFlags, EntityNode* const listener, StreamNode* const node)
	{
		auto const& mappings = listener->getInputAudioMappings();
		for (auto const& [streamPortIndex, audioUnitMappings] : mappings)
		{
			auto const clusterOffset = listener->getStreamPortInputClusterOffset(streamPortIndex);

			// One stream can have multiple Channels
			for (auto const& mapping : audioUnitMappings)
			{
				if (mapping.streamIndex == node->streamIndex())
				{
					if (auto* channelNode = listenerChannelNode(entityID, clusterOffset + mapping.clusterOffset))
					{
						listenerIntersectionDataChanged(channelNode, true, false, dirtyFlags);
						qDebug() << "updateListenerIntersectionChannels: Update Channel #" << (clusterOffset + mapping.clusterOffset);
					}
				}
			}
		}
	}

	// Returns talker section for node
	int talkerNodeSection(Node* const node) const
	{
		return priv::indexOf(_talkerNodeSectionMap, node);
	}

	// Returns listener section for node
	int listenerNodeSection(Node* const node) const
	{
		return priv::indexOf(_listenerNodeSectionMap, node);
	}

	// Returns talker EntityNode for a given entityID
	EntityNode* talkerNodeFromEntityID(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto const it = _talkerNodeMap.find(entityID);
		if (it == std::end(_talkerNodeMap))
		{
			return nullptr;
		}
		auto* node = it->second.get();
		if (!AVDECC_ASSERT_WITH_RET(node->type() == Node::Type::Entity, "Invalid type"))
		{
			return nullptr;
		}
		return static_cast<EntityNode*>(node);
	}

	// Returns listener EntityNode for a given entityID
	EntityNode* listenerNodeFromEntityID(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto const it = _listenerNodeMap.find(entityID);
		if (it == std::end(_listenerNodeMap))
		{
			return nullptr;
		}
		auto* node = it->second.get();
		if (!AVDECC_ASSERT_WITH_RET(node->type() == Node::Type::Entity, "Invalid type"))
		{
			return nullptr;
		}
		return static_cast<EntityNode*>(node);
	}

	// Returns talker StreamNode for a given entityID + streamIndex
	StreamNode* talkerStreamNode(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const key = std::make_pair(entityID, streamIndex);
		auto const it = _talkerStreamNodeMap.find(key);
		if (!AVDECC_ASSERT_WITH_RET(it != std::end(_talkerStreamNodeMap), "Invalid stream"))
		{
			return nullptr;
		}
		return it->second;
	}

	// Returns listener StreamNode for a given entityID + streamIndex
	StreamNode* listenerStreamNode(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const key = std::make_pair(entityID, streamIndex);
		auto const it = _listenerStreamNodeMap.find(key);
		if (!AVDECC_ASSERT_WITH_RET(it != std::end(_listenerStreamNodeMap), "Invalid stream"))
		{
			return nullptr;
		}
		return it->second;
	}

	bool hasTalkerCluster(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::ClusterIndex const& audioClusterIndex) const
	{
		auto const key = std::make_pair(entityID, audioClusterIndex);
		auto const it = _talkerChannelNodeMap.find(key);
		return it != std::end(_talkerChannelNodeMap);
	}

	bool hasListenerCluster(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::ClusterIndex const& audioClusterIndex) const
	{
		auto const key = std::make_pair(entityID, audioClusterIndex);
		auto const it = _listenerChannelNodeMap.find(key);
		return it != std::end(_listenerChannelNodeMap);
	}

	// Returns talker ChannelNode for a given entityID + audioClusterIndex
	ChannelNode* talkerChannelNode(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::ClusterIndex const& audioClusterIndex) const
	{
		auto const key = std::make_pair(entityID, audioClusterIndex);
		auto const it = _talkerChannelNodeMap.find(key);
		if (!AVDECC_ASSERT_WITH_RET(it != std::end(_talkerChannelNodeMap), "Invalid channel"))
		{
			return nullptr;
		}
		return it->second;
	}

	// Returns listener ChannelNode for a given entityID + audioClusterIndex
	ChannelNode* listenerChannelNode(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::ClusterIndex const& audioClusterIndex) const
	{
		auto const key = std::make_pair(entityID, audioClusterIndex);
		auto const it = _listenerChannelNodeMap.find(key);
		if (!AVDECC_ASSERT_WITH_RET(it != std::end(_listenerChannelNodeMap), "Invalid channel"))
		{
			return nullptr;
		}
		return it->second;
	}

private:
	// Returns intersection model index for talkerSection and listenerSection (automatically transposed if required)
	QModelIndex createIndex(int const talkerSection, int const listenerSection) const
	{
		Q_Q(const Model);

		if (!_transposed)
		{
			return q->createIndex(talkerSection, listenerSection);
		}
		else
		{
			return q->createIndex(listenerSection, talkerSection);
		}
	}

	// Recomputes (according to dirtyFlags) intersection data for talkerSection and listenerSection and notifies that it has changed
	void intersectionDataChanged(int const talkerSection, int const listenerSection, IntersectionDirtyFlags const dirtyFlags)
	{
		Q_Q(Model);

		auto& data = _intersectionData[talkerSection][listenerSection];

		computeIntersectionData(data, dirtyFlags);

		auto const index = createIndex(talkerSection, listenerSection);
		emit q->dataChanged(index, index);

#if ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
		highlightIntersection(talkerSection, listenerSection);
#endif
	}

	// Recomputes talker intersection data, possibily recomputing its parent and/or children according to desired dirtyFlags
	// Children are updated first, then the node, then the parents
	void talkerIntersectionDataChanged(Node* talker, bool const andParents, bool const andChildren, IntersectionDirtyFlags const dirtyFlags)
	{
		if (!AVDECC_ASSERT_WITH_RET(talker, "Invalid talker"))
		{
			return;
		}

		auto const talkerSection = talkerNodeSection(talker);
		if (!AVDECC_ASSERT_WITH_RET(talkerSection != -1, "Called with a Node that doesn't exist for current Matrix Mode, a check like 'if (_mode == Model::Mode::Stream)' is probably missing"))
		{
			return;
		}

		// Update the children
		if (andChildren)
		{
			talker->accept(
				[=](Node* child)
				{
					talkerIntersectionDataChanged(child, false, andChildren, dirtyFlags);
				},
				true);
		}

		// Then, update the node intersection
		for (auto listenerSection = static_cast<int>(_listenerNodes.size()); listenerSection > 0; --listenerSection)
		{
			intersectionDataChanged(talkerSection, listenerSection - 1, dirtyFlags);
		}

		// Finally, recursively update the parents
		if (andParents)
		{
			auto* node = talker;
			while (auto* parent = node->parent())
			{
				talkerIntersectionDataChanged(parent, andParents, false, dirtyFlags);
				node = parent;
			}
		}
	}

	// Recomputes listener intersection data, possibily recomputing its parent and/or children according to desired dirtyFlags
	// Children are updated first, then the node, then the parents
	void listenerIntersectionDataChanged(Node* listener, bool const andParents, bool const andChildren, IntersectionDirtyFlags const dirtyFlags)
	{
		if (!AVDECC_ASSERT_WITH_RET(listener, "Invalid listener"))
		{
			return;
		}

		// Update the children
		if (andChildren)
		{
			listener->accept(
				[=](Node* child)
				{
					listenerIntersectionDataChanged(child, false, andChildren, dirtyFlags);
				},
				true);
		}

		// Then, update the node intersection
		auto const listenerSection = listenerNodeSection(listener);
		for (auto talkerSection = static_cast<int>(_talkerNodes.size()); talkerSection > 0; --talkerSection)
		{
			intersectionDataChanged(talkerSection - 1, listenerSection, dirtyFlags);
		}

		// Finally, recursively update the parents
		if (andParents)
		{
			auto* node = listener;
			while (auto* parent = node->parent())
			{
				listenerIntersectionDataChanged(parent, andParents, false, dirtyFlags);
				node = parent;
			}
		}
	}

	// Recomputes talker header data, possibily recomputing its parent according to desired dirtyFlags
	// The node is updated first, then the parents
	void talkerHeaderDataChanged(Node* const talker, bool const notifyView, bool const andParents, HeaderDirtyFlags const dirtyFlags)
	{
		// Compute header flags
		if (!dirtyFlags.empty())
		{
			computeHeaderData(talker, dirtyFlags);
		}

		// Notifies that talker header data has changed
		if (notifyView)
		{
			Q_Q(Model);

			// Update the node header
			auto section = talkerNodeSection(talker);

#if ENABLE_CONNECTION_MATRIX_DEBUG
			qDebug() << "talkerHeaderDataChanged(" << section << ")";
#endif

			emit q->headerDataChanged(talkerOrientation(), section, section);
		}

		// Finally, recursively update the parents
		if (andParents)
		{
			auto* node = talker;
			while (auto* parent = node->parent())
			{
				talkerHeaderDataChanged(parent, notifyView, andParents, dirtyFlags);
				node = parent;
			}
		}
	}

	// Recomputes listener header data, possibily recomputing its parent according to desired dirtyFlags
	// The node is updated first, then the parents
	void listenerHeaderDataChanged(Node* const listener, bool const notifyView, bool const andParents, HeaderDirtyFlags const dirtyFlags)
	{
		// Compute header flags
		if (!dirtyFlags.empty())
		{
			computeHeaderData(listener, dirtyFlags);
		}

		if (notifyView)
		{
			Q_Q(Model);

			// Update the node header
			auto section = listenerNodeSection(listener);

#if ENABLE_CONNECTION_MATRIX_DEBUG
			qDebug() << "listenerHeaderDataChanged(" << section << ")";
#endif

			emit q->headerDataChanged(listenerOrientation(), section, section);
		}

		// Finally, recursively update the parents
		if (andParents)
		{
			auto* node = listener;
			while (auto* parent = node->parent())
			{
				listenerHeaderDataChanged(parent, notifyView, andParents, dirtyFlags);
				node = parent;
			}
		}
	}

	template<class NodeType>
	void listenerHeaderDataChanged(NodeType* const listener, bool const andParents, HeaderDirtyFlags const dirtyFlags)
	{
		// Notifies that listener header data has changed
		auto notifyView = false;

		// Always notify the view, EntityNode is available in CBR and SBR modes
		if constexpr (std::is_same_v<NodeType, EntityNode>)
		{
			notifyView = true;
		}

		// In SBR mode, only notify view if we have a StreamNode
		if (_mode == Model::Mode::Stream)
		{
			if constexpr (std::is_same_v<NodeType, StreamNode>)
			{
				notifyView = true;
			}
		}

		// In CBR mode, only notify view if we have a ChannelNode
		if (_mode == Model::Mode::Channel)
		{
			if constexpr (std::is_same_v<NodeType, ChannelNode>)
			{
				notifyView = true;
			}
		}

		listenerHeaderDataChanged(listener, notifyView, andParents, dirtyFlags);
	}

	// Returns talker header orientation
	Qt::Orientation talkerOrientation() const
	{
		return !_transposed ? Qt::Vertical : Qt::Horizontal;
	}

	// Returns listener header orientation
	Qt::Orientation listenerOrientation() const
	{
		return !_transposed ? Qt::Horizontal : Qt::Vertical;
	}

	// Returns the number of talker sections
	int talkerSectionCount() const
	{
		return static_cast<int>(_talkerNodes.size());
	}

	// Returns the number of listener sections
	int listenerSectionCount() const
	{
		return static_cast<int>(_listenerNodes.size());
	}

	// Extract talker section from index
	int talkerIndex(QModelIndex const& index) const
	{
		return !_transposed ? index.row() : index.column();
	}

	// Extract listener section from index
	int listenerIndex(QModelIndex const& index) const
	{
		return !_transposed ? index.column() : index.row();
	}

	// Returns true if section is a valid talker section, i.e. within [0, talkerSectionCount[
	bool isValidTalkerSection(int section) const
	{
		return section >= 0 && section < talkerSectionCount();
	}

	// Returns true if section is a valid listener section, i.e. within [0, listenerSectionCount[
	bool isValidListenerSection(int section) const
	{
		return section >= 0 && section < listenerSectionCount();
	}

	// Returns talker header data for sections (Qt::DisplayRole) at section
	QString talkerHeaderData(int section) const
	{
		if (!isValidTalkerSection(section))
		{
			return {};
		}

		return _talkerNodes[section]->name();
	}

	// Returns listener header data for sections (Qt::DisplayRole) at section
	QString listenerHeaderData(int section) const
	{
		if (!isValidListenerSection(section))
		{
			return {};
		}

		return _listenerNodes[section]->name();
	}

	void clearCachedData()
	{
		_talkerNodes.clear();
		_listenerNodes.clear();
		_talkerNodeSectionMap.clear();
		_listenerNodeSectionMap.clear();
		_intersectionData.clear();
	}

	void resetModel()
	{
		Q_Q(Model);

		emit q->beginResetModel();
		_talkerNodeMap.clear();
		_listenerNodeMap.clear();

		_talkerStreamNodeMap.clear();
		_listenerStreamNodeMap.clear();

		_talkerChannelNodeMap.clear();
		_listenerChannelNodeMap.clear();

		clearCachedData();
		emit q->endResetModel();

		q->setMode(q->mode(), true);
	}

private:
	Model* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(Model);

	Model::Mode _mode{ Model::Mode::None };
	bool _transposed{ false };

	// OfflineOutputStream special node
	std::unique_ptr<OfflineOutputStreamNode> _offlineOutputStreamNode{ OfflineOutputStreamNode::create() };

	// EntityNode per entityID (persistent)
	priv::NodeMap _talkerNodeMap;
	priv::NodeMap _listenerNodeMap;

	// Stream nodes by StreamKey
	priv::StreamNodeMap _talkerStreamNodeMap;
	priv::StreamNodeMap _listenerStreamNodeMap;

	// Channel nodes by ChannelKey
	priv::ChannelNodeMap _talkerChannelNodeMap;
	priv::ChannelNodeMap _listenerChannelNodeMap;

	// Flattened nodes (cache)
	priv::Nodes _talkerNodes;
	priv::Nodes _listenerNodes;

	// Node section quick access map (cache)
	priv::NodeSectionMap _talkerNodeSectionMap;
	priv::NodeSectionMap _listenerNodeSectionMap;

	// Talker major intersection data matrix (cache)
	std::deque<std::deque<Model::IntersectionData>> _intersectionData;
};

Model::Model(QObject* parent)
	: QAbstractTableModel{ parent }
	, d_ptr{ new ModelPrivate{ this } }
{
}

Model::~Model() = default;

int Model::rowCount(QModelIndex const&) const
{
	Q_D(const Model);

	if (!d->_transposed)
	{
		return d->talkerSectionCount();
	}
	else
	{
		return d->listenerSectionCount();
	}
}

int Model::columnCount(QModelIndex const&) const
{
	Q_D(const Model);

	if (!d->_transposed)
	{
		return d->listenerSectionCount();
	}
	else
	{
		return d->talkerSectionCount();
	}
}

QVariant Model::data([[maybe_unused]] QModelIndex const& index, [[maybe_unused]] int role) const
{
#if ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
	if (role == Qt::BackgroundRole)
	{
		auto const& intersectionData = this->intersectionData(index);
		if (intersectionData.animation)
		{
			return intersectionData.animation->currentValue();
		}
	}
#endif

#if ENABLE_CONNECTION_MATRIX_TOOLTIP
	if (role == Qt::ToolTipRole)
	{
		auto const& intersectionData = this->intersectionData(index);
		return priv::intersectionDataToString(intersectionData);
	}
#endif

	return {};
}

QVariant Model::headerData(int section, Qt::Orientation orientation, int role) const
{
	Q_D(const Model);

	if (role == Qt::DisplayRole)
	{
		if (!d->_transposed)
		{
			if (orientation == Qt::Vertical)
			{
				return d->talkerHeaderData(section);
			}
			else
			{
				return d->listenerHeaderData(section);
			}
		}
		else
		{
			if (orientation == Qt::Vertical)
			{
				return d->listenerHeaderData(section);
			}
			else
			{
				return d->talkerHeaderData(section);
			}
		}
	}

	return {};
}

Node* Model::node(int section, Qt::Orientation orientation) const
{
	Q_D(const Model);

	if (!d->_transposed)
	{
		if (orientation == Qt::Vertical)
		{
			return d->talkerNode(section);
		}
		else
		{
			return d->listenerNode(section);
		}
	}
	else
	{
		if (orientation == Qt::Vertical)
		{
			return d->listenerNode(section);
		}
		else
		{
			return d->talkerNode(section);
		}
	}
}


int Model::section(Node* node, Qt::Orientation orientation) const
{
	Q_D(const Model);

	if (!d->_transposed)
	{
		if (orientation == Qt::Vertical)
		{
			return priv::indexOf(d->_talkerNodeSectionMap, node);
		}
		else
		{
			return priv::indexOf(d->_listenerNodeSectionMap, node);
		}
	}
	else
	{
		if (orientation == Qt::Vertical)
		{
			return priv::indexOf(d->_listenerNodeSectionMap, node);
		}
		else
		{
			return priv::indexOf(d->_talkerNodeSectionMap, node);
		}
	}
}

Model::IntersectionData const& Model::intersectionData(QModelIndex const& index) const
{
	Q_D(const Model);

	auto const talkerSection = d->talkerIndex(index);
	auto const listenerSection = d->listenerIndex(index);

	if (!AVDECC_ASSERT_WITH_RET(d->isValidTalkerSection(talkerSection), "invalid talker section") || !AVDECC_ASSERT_WITH_RET(d->isValidListenerSection(listenerSection), "invalid listener section"))
	{
		static auto const s_dummyData = Model::IntersectionData{};
		return s_dummyData;
	}

	return d->_intersectionData[talkerSection][listenerSection];
}

void Model::setMode(Mode const mode, bool const force)
{
	Q_D(Model);

	if (mode != d->_mode || force)
	{
		emit beginResetModel();

		d->_mode = mode;
		d->clearCachedData();

		emit endResetModel();

		// Special Offline Output Stream node
		d->insertOfflineOutputStreamNode();

		// Rebuild the cache data for all known entities
		for (auto const& [entityID, entityNode] : d->_talkerNodeMap)
		{
			d->insertTalkerNode(entityNode.get());
		}

		for (auto const& [entityID, entityNode] : d->_listenerNodeMap)
		{
			d->insertListenerNode(entityNode.get());
		}
	}
}

Model::Mode Model::mode() const
{
	Q_D(const Model);
	return d->_mode;
}

void Model::setTransposed(bool const transposed)
{
	Q_D(Model);

	if (transposed != d->_transposed)
	{
		emit beginResetModel();
		d->_transposed = transposed;
		emit endResetModel();
	}
}

bool Model::isTransposed() const
{
	Q_D(const Model);
	return d->_transposed;
}

void Model::forceRefreshHeaders()
{
	emit headerDataChanged(Qt::Horizontal, 0, columnCount());
	emit headerDataChanged(Qt::Vertical, 0, rowCount());
}

void Model::accept(Node* node, Visitor const& visitor, bool const childrenOnly) const
{
	Q_D(const Model);
	priv::accept(node, d->_mode, visitor, childrenOnly);
}

} // namespace connectionMatrix

#include "model.moc"
