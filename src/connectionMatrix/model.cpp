/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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
#include "avdecc/helper.hpp"
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
using NodeMap = std::unordered_map<la::avdecc::UniqueIdentifier, std::unique_ptr<Node>, la::avdecc::UniqueIdentifier::hash>;

// Entity section by entity ID
using EntitySectionMap = std::unordered_map<la::avdecc::UniqueIdentifier, int, la::avdecc::UniqueIdentifier::hash>;

// Stream identifier by entity ID and index
using StreamSectionKey = std::pair<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::StreamIndex>;

struct StreamSectionKeyHash
{
	std::size_t operator()(StreamSectionKey const& key) const
	{
		return la::avdecc::UniqueIdentifier::hash()(key.first) ^ std::hash<int>()(key.second);
	}
};

// Stream section by entity ID and index
using StreamSectionMap = std::unordered_map<StreamSectionKey, int, StreamSectionKeyHash>;

// index by node
using NodeSectionMap = std::unordered_map<Node*, int>;

#if ENABLE_CONNECTION_MATRIX_TOOLTIP

// Converts IntersectionData::Type to string
QString typeToString(Model::IntersectionData::Type const type)
{
	switch (type)
	{
		case Model::IntersectionData::Type::None:
			return "None";
		case Model::IntersectionData::Type::Entity_Entity:
			return "Entity / Entity";
		case Model::IntersectionData::Type::Entity_Redundant:
			return "Entity / Redundant";
		case Model::IntersectionData::Type::Entity_RedundantStream:
			return "Entity / Redundant Stream";
		case Model::IntersectionData::Type::Entity_SingleStream:
			return "Entity / Single Stream";
		case Model::IntersectionData::Type::Redundant_Redundant:
			return "Redundant / Redundant";
		case Model::IntersectionData::Type::Redundant_RedundantStream:
			return "Redundant / Redundant Stream";
		case Model::IntersectionData::Type::Redundant_SingleStream:
			return "Redundant / Single Stream";
		case Model::IntersectionData::Type::RedundantStream_RedundantStream:
			return "Redundant Stream / Redundant Stream";
		case Model::IntersectionData::Type::RedundantStream_SingleStream:
			return "Redundant Stream / Single Stream";
		case Model::IntersectionData::Type::SingleStream_SingleStream:
			return "Single Stream / Single Stream";
		default:
			assert(false);
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
			assert(false);
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

// Flatten node hierarchy and insert all nodes in list starting at first
void insertNodes(Nodes& list, Node* node, int first)
{
	if (!node)
	{
		assert(false);
		return;
	}

#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const before = list.size();
#endif

	auto nodes = Nodes{};
	node->accept(
		[&nodes](Node* node)
		{
			nodes.push_back(node);
		});

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

	assert(from < to);

#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const before = list.size();
#endif

	list.erase(from, to);

#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const after = list.size();
	qDebug() << "removeNodes" << before << ">" << after;
#endif
}

// Returns the total number of children in node hierarchy
int absoluteChildrenCount(Node* node)
{
	if (!node)
	{
		assert(false);
		return 0;
	}

	auto count = 0;

	for (auto i = 0; i < node->childrenCount(); ++i)
	{
		auto* child = node->childAt(i);
		count += 1 + absoluteChildrenCount(child);
	}

	return count;
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

// Build and returns a StreamSectionMap from nodes (quick access map for stream nodes)
StreamSectionMap buildStreamSectionMap(Nodes const& nodes)
{
	StreamSectionMap sectionMap;

	for (auto section = 0u; section < nodes.size(); ++section)
	{
		auto* node = nodes[section];
		if (node->isStreamNode())
		{
			auto const entityID = node->entityID();
			auto const streamIndex = static_cast<StreamNode*>(node)->streamIndex();

#if ENABLE_CONNECTION_MATRIX_DEBUG
			qDebug() << "buildStreamSectionMap" << node->name() << ", stream" << streamIndex << "at section" << section;
#endif

			auto const key = std::make_pair(entityID, streamIndex);
			auto const [it, result] = sectionMap.insert(std::make_pair(key, section));

			assert(result);
		}
	}

	return sectionMap;
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
int indexOf(NodeSectionMap const& map, Node* node)
{
	auto const it = map.find(node);
	assert(it != std::end(map));
	return it->second;
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
		connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ModelPrivate::handleControllerOffline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ModelPrivate::handleEntityOnline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ModelPrivate::handleEntityOffline);
		connect(&controllerManager, &avdecc::ControllerManager::streamRunningChanged, this, &ModelPrivate::handleStreamRunningChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamConnectionChanged, this, &ModelPrivate::handleStreamConnectionChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamFormatChanged, this, &ModelPrivate::handleStreamFormatChanged);
		connect(&controllerManager, &avdecc::ControllerManager::gptpChanged, this, &ModelPrivate::handleGptpChanged);
		connect(&controllerManager, &avdecc::ControllerManager::entityNameChanged, this, &ModelPrivate::handleEntityNameChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamNameChanged, this, &ModelPrivate::handleStreamNameChanged);
		connect(&controllerManager, &avdecc::ControllerManager::avbInterfaceLinkStatusChanged, this, &ModelPrivate::handleAvbInterfaceLinkStatusChanged);
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
		assert(isValidTalkerSection(talkerSection));
		assert(isValidListenerSection(listenerSection));

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
			[this, talkerSection, listenerSection](QVariant const& value)
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
		IntersectionDirtyFlags flags;
		flags.set(IntersectionDirtyFlag::UpdateConnected);
		flags.set(IntersectionDirtyFlag::UpdateFormat);
		flags.set(IntersectionDirtyFlag::UpdateGptp);
		flags.set(IntersectionDirtyFlag::UpdateLinkStatus);
		return flags;
	}

	// Returns true if domains are compatible
	constexpr bool isCompatibleDomain(la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const& talkerLinkStatus, la::avdecc::UniqueIdentifier const& talkerGrandMasterID, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const& listenerLinkStatus, la::avdecc::UniqueIdentifier const& listenerGrandMasterID) const noexcept
	{
		// Check both have the same grandmaster
		return talkerGrandMasterID == listenerGrandMasterID;
	}

	// Determines intersection type according to talker and listener nodes
	Model::IntersectionData::Type determineIntersectionType(Node* talker, Node* listener) const
	{
		assert(talker);
		assert(listener);

		if (talker->entityID() == listener->entityID())
		{
			return Model::IntersectionData::Type::None;
		}

		auto const talkerType = talker->type();
		auto const listenerType = listener->type();

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
		}

		if (talkerType == Node::Type::RedundantOutputStream && listenerType == Node::Type::RedundantInputStream)
		{
			if (talker->index() == listener->index())
			{
				return Model::IntersectionData::Type::RedundantStream_RedundantStream;
			}
			else
			{
				return Model::IntersectionData::Type::None;
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

		assert(false);

		return Model::IntersectionData::Type::None;
	}

	// Initializes intersection data
	void initializeIntersectionData(Node* talker, Node* listener, Model::IntersectionData& intersectionData)
	{
		assert(talker);
		assert(listener);

		intersectionData.talker = talker;
		intersectionData.listener = listener;

		intersectionData.type = determineIntersectionType(talker, listener);
		intersectionData.flags = {};

		// Compute everything for initial state
		computeIntersectionFlags(intersectionData, allIntersectionDirtyFlags());
	}

	// Updates intersection data for the given dirtyFlags
	void computeIntersectionFlags(Model::IntersectionData& intersectionData, IntersectionDirtyFlags const dirtyFlags)
	{
		try
		{
			auto const talkerType = intersectionData.talker->type();
			auto const listenerType = intersectionData.listener->type();

			auto const talkerEntityID = intersectionData.talker->entityID();
			auto const listenerEntityID = intersectionData.listener->entityID();

			switch (intersectionData.type)
			{
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

					auto atLeastOneInterfaceDown = false;
					auto atLeastOneConnected = false;
					auto allConnected = true;
					auto allCompatibleDomain = true;
					auto allCompatibleFormat = true;

					assert(talker->childrenCount() == listener->childrenCount());

					for (auto i = 0; i < talker->childrenCount(); ++i)
					{
						auto const* const talkerStreamNode = static_cast<StreamNode*>(talker->childAt(i));
						auto const* const listenerStreamNode = static_cast<StreamNode*>(listener->childAt(i));

						auto const talkerInterfaceLinkStatus = talkerStreamNode->interfaceLinkStatus();
						auto const listenerInterfaceLinkStatus = listenerStreamNode->interfaceLinkStatus();
						atLeastOneInterfaceDown |= talkerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down || listenerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down;

						auto const talkerStream = la::avdecc::entity::model::StreamIdentification{ talkerEntityID, talkerStreamNode->streamIndex() };
						auto const isConnectedToTalker = avdecc::helper::isConnectedToTalker(talkerStream, listenerStreamNode->streamConnectionState());
						auto const isFastConnectingToTalker = avdecc::helper::isFastConnectingToTalker(talkerStream, listenerStreamNode->streamConnectionState());

						auto const connected = isConnectedToTalker || isFastConnectingToTalker;
						atLeastOneConnected |= connected;
						allConnected &= connected;

						auto const talkerGrandMasterID = talkerStreamNode->grandMasterID();
						auto const listenerGrandMasterID = listenerStreamNode->grandMasterID();
						allCompatibleDomain &= isCompatibleDomain(talkerInterfaceLinkStatus, talkerGrandMasterID, listenerInterfaceLinkStatus, listenerGrandMasterID);

						auto const talkerStreamFormat = talkerStreamNode->streamFormat();
						auto const listenerStreamFormat = listenerStreamNode->streamFormat();
						allCompatibleFormat &= la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerStreamFormat, talkerStreamFormat);
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
						assert(false);
					}

					// Build the list of smart connectable streams:
					//  - If the Talker is the redundant device, only get the first stream with a matching domain
					//  - If the Listener is the redundant device, get all streams with a matching domain (so we can fully connect a device in cable redundancy mode)
					//  - If a stream is already connected, always add it to the list
					intersectionData.redundantSmartConnectableStreams.clear();

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

					for (auto i = 0; i < redundantNode->childrenCount(); ++i)
					{
						if (auto* redundantStreamNode = static_cast<StreamNode*>(redundantNode->childAt(i)))
						{
							assert(redundantStreamNode->isRedundantStreamNode());
							auto connectableStreams = Model::IntersectionData::ConnectableStreams{};
							auto const* listenerStreamConnectionState = static_cast<la::avdecc::entity::model::StreamConnectionState const*>(nullptr);
							auto talkerStreamFormat = la::avdecc::entity::model::StreamFormat{};
							auto listenerStreamFormat = la::avdecc::entity::model::StreamFormat{};

							// Get information based on which node is redundant
							if (talkerType == Node::Type::RedundantOutput)
							{
								connectableStreams = std::make_pair(redundantStreamNode->streamIndex(), nonRedundantStreamNode->streamIndex());
								listenerStreamConnectionState = &nonRedundantStreamNode->streamConnectionState();
								talkerStreamFormat = redundantStreamNode->streamFormat();
								listenerStreamFormat = nonRedundantStreamNode->streamFormat();
							}
							else if (listenerType == Node::Type::RedundantInput)
							{
								connectableStreams = std::make_pair(nonRedundantStreamNode->streamIndex(), redundantStreamNode->streamIndex());
								listenerStreamConnectionState = &redundantStreamNode->streamConnectionState();
								talkerStreamFormat = nonRedundantStreamNode->streamFormat();
								listenerStreamFormat = redundantStreamNode->streamFormat();
							}

							// Get Connection State
							auto const talkerStream = la::avdecc::entity::model::StreamIdentification{ talkerEntityID, connectableStreams.first };
							auto const streamConnected = avdecc::helper::isConnectedToTalker(talkerStream, *listenerStreamConnectionState);
							auto const streamFastConnecting = avdecc::helper::isFastConnectingToTalker(talkerStream, *listenerStreamConnectionState);
							areConnected |= streamConnected;
							fastConnecting |= streamFastConnecting;

							// Get Format Compatibility
							isCompatibleFormat &= la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerStreamFormat, talkerStreamFormat);

							// Get Domain Compatibility
							auto const sameDomain = redundantStreamNode->grandMasterID() == nonRedundantStreamNode->grandMasterID();
							atLeastOneMatchingDomain |= sameDomain;
							if (sameDomain || streamConnected || streamFastConnecting)
							{
								// Only add to the smart connection list if it's the first matching domain, or if the redundant device is a Listener (for cable redundancy)
								if (intersectionData.redundantSmartConnectableStreams.empty() || listenerType == Node::Type::RedundantInput)
								{
									intersectionData.redundantSmartConnectableStreams.push_back(connectableStreams);
								}
								if (streamConnected || streamFastConnecting)
								{
									allConnectionsHaveMatchingDomain &= sameDomain;
								}
							}
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
						intersectionData.state = Model::IntersectionData::State::Connected;
					}
					else if (fastConnecting)
					{
						// FastConnecting state should only be possible then the non-redundant device is a Listener, as it can be a non-Milan device
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
					// Duplicate information from the only possible redundant stream node
					auto talkerSection = -1;
					auto listenerSection = -1;

					if (talkerType == Node::Type::RedundantOutput)
					{
						auto* redundantNode = static_cast<RedundantNode*>(intersectionData.talker);
						auto* streamNode = static_cast<StreamNode*>(intersectionData.listener);

						auto const otherStreamNode = redundantNode->childAt(streamNode->index());

						talkerSection = priv::indexOf(_talkerNodeSectionMap, otherStreamNode);
						listenerSection = priv::indexOf(_listenerNodeSectionMap, streamNode);
					}
					else if (listenerType == Node::Type::RedundantInput)
					{
						auto* redundantNode = static_cast<RedundantNode*>(intersectionData.listener);
						auto* streamNode = static_cast<StreamNode*>(intersectionData.talker);

						auto const otherStreamNode = redundantNode->childAt(streamNode->index());

						talkerSection = priv::indexOf(_talkerNodeSectionMap, streamNode);
						listenerSection = priv::indexOf(_listenerNodeSectionMap, otherStreamNode);
					}
					else
					{
						assert(false);
					}

					assert(isValidTalkerSection(talkerSection));
					assert(isValidListenerSection(listenerSection));

					// Simply copy data from the source
					auto const& sourceIntersectionData = _intersectionData[talkerSection][listenerSection];

					intersectionData.state = sourceIntersectionData.state;
					intersectionData.flags = sourceIntersectionData.flags;

					break;
				}

				case Model::IntersectionData::Type::RedundantStream_RedundantStream:
				case Model::IntersectionData::Type::RedundantStream_SingleStream:
				case Model::IntersectionData::Type::SingleStream_SingleStream:
				{
					auto const* const talkerStreamNode = static_cast<StreamNode*>(intersectionData.talker);
					auto const* const listenerStreamNode = static_cast<StreamNode*>(intersectionData.listener);

					auto const talkerInterfaceLinkStatus = talkerStreamNode->interfaceLinkStatus();
					auto const listenerInterfaceLinkStatus = listenerStreamNode->interfaceLinkStatus();

					auto const talkerGrandMasterID = talkerStreamNode->grandMasterID();
					auto const listenerGrandMasterID = listenerStreamNode->grandMasterID();

					auto const interfaceDown = talkerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down || listenerInterfaceLinkStatus == la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Down;

					// InterfaceDown
					if (dirtyFlags.test(IntersectionDirtyFlag::UpdateLinkStatus))
					{
						if (interfaceDown)
						{
							intersectionData.flags.set(Model::IntersectionData::Flag::InterfaceDown);
						}
						else
						{
							intersectionData.flags.reset(Model::IntersectionData::Flag::InterfaceDown);
						}
					}

					// WrongDomain
					if (dirtyFlags.test(IntersectionDirtyFlag::UpdateGptp))
					{
						if (isCompatibleDomain(talkerInterfaceLinkStatus, talkerGrandMasterID, listenerInterfaceLinkStatus, listenerGrandMasterID))
						{
							intersectionData.flags.reset(Model::IntersectionData::Flag::WrongDomain);
						}
						else
						{
							intersectionData.flags.set(Model::IntersectionData::Flag::WrongDomain);
						}
					}

					// WrongFormat
					if (dirtyFlags.test(IntersectionDirtyFlag::UpdateFormat))
					{
						auto const talkerStreamFormat = talkerStreamNode->streamFormat();
						auto const listenerStreamFormat = listenerStreamNode->streamFormat();

						if (la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerStreamFormat, talkerStreamFormat))
						{
							intersectionData.flags.reset(Model::IntersectionData::Flag::WrongFormat);
						}
						else
						{
							intersectionData.flags.set(Model::IntersectionData::Flag::WrongFormat);
						}
					}

					// Connected
					if (dirtyFlags.test(IntersectionDirtyFlag::UpdateConnected))
					{
						StreamNode* talkerStreamNode = nullptr;

						if (talkerType == Node::Type::RedundantOutput)
						{
							auto const listenerRedundantStreamOrder = static_cast<std::size_t>(intersectionData.listener->index());
							auto* talkerRedundantNode = static_cast<RedundantNode*>(intersectionData.talker);
							talkerStreamNode = static_cast<StreamNode*>(talkerRedundantNode->childAt(listenerRedundantStreamOrder));
							assert(talkerStreamNode->isRedundantStreamNode());
						}
						else if (talkerType == Node::Type::OutputStream || talkerType == Node::Type::RedundantOutputStream)
						{
							talkerStreamNode = static_cast<StreamNode*>(intersectionData.talker);
						}
						else
						{
							assert(false);
							return;
						}

						StreamNode* listenerStreamNode = nullptr;

						if (listenerType == Node::Type::RedundantInput)
						{
							auto const talkerRedundantStreamOrder = static_cast<std::size_t>(intersectionData.talker->index());
							auto* listenerRedundantNode = static_cast<RedundantNode*>(intersectionData.listener);
							listenerStreamNode = static_cast<StreamNode*>(listenerRedundantNode->childAt(talkerRedundantStreamOrder));
							assert(listenerStreamNode->isRedundantStreamNode());
						}
						else if (listenerType == Node::Type::InputStream || listenerType == Node::Type::RedundantInputStream)
						{
							listenerStreamNode = static_cast<StreamNode*>(intersectionData.listener);
						}
						else
						{
							assert(false);
							return;
						}

						assert(talkerStreamNode);
						assert(listenerStreamNode);

						auto const& streamConnectionState = listenerStreamNode->streamConnectionState();
						auto const talkerStream = la::avdecc::entity::model::StreamIdentification{ talkerEntityID, talkerStreamNode->streamIndex() };

						auto const isConnectedToTalker = avdecc::helper::isConnectedToTalker(talkerStream, streamConnectionState);
						auto const isFastConnectingToTalker = avdecc::helper::isFastConnectingToTalker(talkerStream, streamConnectionState);

						if (isConnectedToTalker || isFastConnectingToTalker)
						{
							intersectionData.state = Model::IntersectionData::State::Connected;
						}
						else
						{
							intersectionData.state = Model::IntersectionData::State::NotConnected;
						}
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

	// Cache update helpers

	void rebuildTalkerSectionCache()
	{
		_talkerStreamSectionMap = priv::buildStreamSectionMap(_talkerNodes);
		_talkerNodeSectionMap = priv::buildNodeSectionMap(_talkerNodes);
	}

	void rebuildListenerSectionCache()
	{
		_listenerStreamSectionMap = priv::buildStreamSectionMap(_listenerNodes);
		_listenerNodeSectionMap = priv::buildNodeSectionMap(_listenerNodes);
	}

	// Build talker node hierarchy
	Node* buildTalkerNode(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		try
		{
			auto const& entityNode = controlledEntity.getEntityNode();
			auto const currentConfigurationIndex = entityNode.dynamicModel->currentConfiguration;

			auto* entity = EntityNode::create(entityID);
			entity->setName(avdecc::helper::smartEntityName(controlledEntity));

			// Redundant streams
			for (auto const& [redundantIndex, redundantNode] : configurationNode.redundantStreamOutputs)
			{
				auto* redundantOutput = RedundantNode::createOutputNode(*entity, redundantIndex);
				redundantOutput->setName(avdecc::helper::redundantOutputName(redundantIndex));

				for (auto const& [streamIndex, streamNode] : redundantNode.redundantStreams)
				{
					auto const avbInterfaceIndex{ streamNode->staticModel->avbInterfaceIndex };
					auto const& avbInterfaceNode = controlledEntity.getAvbInterfaceNode(currentConfigurationIndex, avbInterfaceIndex);

					auto* redundantOutputStream = StreamNode::createRedundantOutputNode(*redundantOutput, streamIndex, avbInterfaceIndex);
					redundantOutputStream->setName(avdecc::helper::outputStreamName(controlledEntity, streamIndex));

					auto const* const streamOutputNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(streamNode);
					redundantOutputStream->setStreamFormat(streamOutputNode->dynamicModel->streamInfo.streamFormat);

					redundantOutputStream->setGrandMasterID(avbInterfaceNode.dynamicModel->avbInfo.gptpGrandmasterID);
					redundantOutputStream->setGrandMasterDomain(avbInterfaceNode.dynamicModel->avbInfo.gptpDomainNumber);
					redundantOutputStream->setInterfaceLinkStatus(controlledEntity.getAvbInterfaceLinkStatus(avbInterfaceIndex));
					redundantOutputStream->setRunning(controlledEntity.isStreamOutputRunning(currentConfigurationIndex, streamIndex));
				}
			}

			// Single streams
			for (auto const& [streamIndex, streamNode] : configurationNode.streamOutputs)
			{
				if (!streamNode.isRedundant)
				{
					auto const avbInterfaceIndex{ streamNode.staticModel->avbInterfaceIndex };
					auto const& streamNode = controlledEntity.getStreamOutputNode(currentConfigurationIndex, streamIndex);
					auto const& avbInterfaceNode = controlledEntity.getAvbInterfaceNode(currentConfigurationIndex, avbInterfaceIndex);

					auto* outputStream = StreamNode::createOutputNode(*entity, streamIndex, avbInterfaceIndex);
					outputStream->setName(avdecc::helper::outputStreamName(controlledEntity, streamIndex));
					outputStream->setStreamFormat(streamNode.dynamicModel->streamInfo.streamFormat);
					outputStream->setGrandMasterID(avbInterfaceNode.dynamicModel->avbInfo.gptpGrandmasterID);
					outputStream->setGrandMasterDomain(avbInterfaceNode.dynamicModel->avbInfo.gptpDomainNumber);
					outputStream->setInterfaceLinkStatus(controlledEntity.getAvbInterfaceLinkStatus(avbInterfaceIndex));
					outputStream->setRunning(controlledEntity.isStreamOutputRunning(currentConfigurationIndex, streamIndex));
				}
			}

			return entity;
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
			return nullptr;
		}
	}

	// Build listener node hierarchy
	Node* buildListenerNode(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		try
		{
			auto const& entityNode = controlledEntity.getEntityNode();
			auto const currentConfigurationIndex = entityNode.dynamicModel->currentConfiguration;

			auto* entity = EntityNode::create(entityID);
			entity->setName(avdecc::helper::smartEntityName(controlledEntity));

			// Redundant streams
			for (auto const& [redundantIndex, redundantNode] : configurationNode.redundantStreamInputs)
			{
				auto* redundantInput = RedundantNode::createInputNode(*entity, redundantIndex);
				redundantInput->setName(avdecc::helper::redundantInputName(redundantIndex));

				for (auto const& [streamIndex, streamNode] : redundantNode.redundantStreams)
				{
					auto const avbInterfaceIndex{ streamNode->staticModel->avbInterfaceIndex };
					auto const& avbInterfaceNode = controlledEntity.getAvbInterfaceNode(currentConfigurationIndex, avbInterfaceIndex);

					auto* redundantInputStream = StreamNode::createRedundantInputNode(*redundantInput, streamIndex, avbInterfaceIndex);
					redundantInputStream->setName(avdecc::helper::inputStreamName(controlledEntity, streamIndex));

					auto const* const streamInputNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(streamNode);
					redundantInputStream->setStreamFormat(streamInputNode->dynamicModel->streamInfo.streamFormat);

					redundantInputStream->setGrandMasterID(avbInterfaceNode.dynamicModel->avbInfo.gptpGrandmasterID);
					redundantInputStream->setGrandMasterDomain(avbInterfaceNode.dynamicModel->avbInfo.gptpDomainNumber);
					redundantInputStream->setInterfaceLinkStatus(controlledEntity.getAvbInterfaceLinkStatus(avbInterfaceIndex));
					redundantInputStream->setRunning(controlledEntity.isStreamInputRunning(currentConfigurationIndex, streamIndex));

					// Only for listeners
					redundantInputStream->setStreamConnectionState(streamInputNode->dynamicModel->connectionState);
				}
			}

			// Single streams
			for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
			{
				if (!streamNode.isRedundant)
				{
					auto const avbInterfaceIndex{ streamNode.staticModel->avbInterfaceIndex };
					auto const& streamNode = controlledEntity.getStreamInputNode(currentConfigurationIndex, streamIndex);
					auto const& avbInterfaceNode = controlledEntity.getAvbInterfaceNode(currentConfigurationIndex, avbInterfaceIndex);

					auto* inputStream = StreamNode::createInputNode(*entity, streamIndex, avbInterfaceIndex);
					inputStream->setName(avdecc::helper::inputStreamName(controlledEntity, streamIndex));
					inputStream->setStreamFormat(streamNode.dynamicModel->streamInfo.streamFormat);
					inputStream->setGrandMasterID(avbInterfaceNode.dynamicModel->avbInfo.gptpGrandmasterID);
					inputStream->setGrandMasterDomain(avbInterfaceNode.dynamicModel->avbInfo.gptpDomainNumber);
					inputStream->setInterfaceLinkStatus(controlledEntity.getAvbInterfaceLinkStatus(avbInterfaceIndex));
					inputStream->setRunning(controlledEntity.isStreamInputRunning(currentConfigurationIndex, streamIndex));

					// Only for listeners
					inputStream->setStreamConnectionState(streamNode.dynamicModel->connectionState);
				}
			}

			return entity;
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
			return nullptr;
		}
	}

	// Build and add talker node hierarchy
	void addTalker(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		auto* node = buildTalkerNode(controlledEntity, entityID, configurationNode);

		auto const childrenCount = priv::absoluteChildrenCount(node);

		auto const first = priv::sortedIndexForEntity(_talkerNodes, entityID);
		auto const last = first + childrenCount;

		beginInsertTalkerItems(first, last);

		_talkerNodeMap.insert(std::make_pair(entityID, node));

		priv::insertNodes(_talkerNodes, node, first);

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

	// Build and add listener node hierarchy
	void addListener(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		auto* node = buildListenerNode(controlledEntity, entityID, configurationNode);

		auto const childrenCount = priv::absoluteChildrenCount(node);

		auto const first = priv::sortedIndexForEntity(_listenerNodes, entityID);
		auto const last = first + childrenCount;

		beginInsertListenerItems(first, last);

		_listenerNodeMap.insert(std::make_pair(entityID, node));

		priv::insertNodes(_listenerNodes, node, first);

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
	void removeTalker(la::avdecc::UniqueIdentifier const& entityID)
	{
		if (auto* node = talkerNodeFromEntityID(entityID))
		{
			auto const childrenCount = priv::absoluteChildrenCount(node);

			auto const first = priv::indexOf(_talkerNodeSectionMap, node);
			auto const last = first + childrenCount;

			beginRemoveTalkerItems(first, last);

			priv::removeNodes(_talkerNodes, first, last + 1 /* entity */);

			rebuildTalkerSectionCache();

			_talkerNodeMap.erase(entityID);

			_intersectionData.erase(std::next(std::begin(_intersectionData), first), std::next(std::begin(_intersectionData), last + 1));

#if ENABLE_CONNECTION_MATRIX_DEBUG
			dump();
#endif

			endRemoveTalkerItems();
		}
	}

	// Remove complete listener node hierarchy from entityID
	void removeListener(la::avdecc::UniqueIdentifier const& entityID)
	{
		if (auto* node = listenerNodeFromEntityID(entityID))
		{
			auto const childrenCount = priv::absoluteChildrenCount(node);

			auto const first = priv::indexOf(_listenerNodeSectionMap, node);
			auto const last = first + childrenCount;

			beginRemoveListenerItems(first, last);

			priv::removeNodes(_listenerNodes, first, last + 1 /* entity */);

			rebuildListenerSectionCache();

			_listenerNodeMap.erase(entityID);

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
		Q_Q(Model);

		emit q->beginResetModel();
		_talkerNodeMap.clear();
		_listenerNodeMap.clear();
		_talkerNodes.clear();
		_listenerNodes.clear();
		_talkerStreamSectionMap.clear();
		_listenerStreamSectionMap.clear();
		_talkerNodeSectionMap.clear();
		_listenerNodeSectionMap.clear();
		_intersectionData.clear();
		emit q->endResetModel();
	}

	void handleEntityOnline(la::avdecc::UniqueIdentifier const entityID)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity && AVDECC_ASSERT_WITH_RET(!controlledEntity->gotFatalEnumerationError(), "An entity should not be set online if it had an enumeration error"))
			{
				if (!controlledEntity->getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
				{
					return;
				}

				auto const& entityNode = controlledEntity->getEntityNode();
				auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);

				// Talker
				if (controlledEntity->getEntity().getTalkerCapabilities().test(la::avdecc::entity::TalkerCapability::Implemented) && !configurationNode.streamOutputs.empty())
				{
					addTalker(*controlledEntity, entityID, configurationNode);
				}

				// Listener
				if (controlledEntity->getEntity().getListenerCapabilities().test(la::avdecc::entity::ListenerCapability::Implemented) && !configurationNode.streamInputs.empty())
				{
					addListener(*controlledEntity, entityID, configurationNode);
				}
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
		if (hasTalker(entityID))
		{
			removeTalker(entityID);
		}

		if (hasListener(entityID))
		{
			removeListener(entityID);
		}
	}

	void handleStreamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
	{
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			auto* node = talkerStreamNode(entityID, streamIndex);
			node->setRunning(isRunning);
			emit talkerHeaderDataChanged(node);
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			auto* node = listenerStreamNode(entityID, streamIndex);
			node->setRunning(isRunning);
			emit listenerHeaderDataChanged(node);
		}
	}

	void handleStreamConnectionChanged(la::avdecc::entity::model::StreamConnectionState const& state)
	{
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateConnected };
		auto* listener = listenerStreamNode(state.listenerStream.entityID, state.listenerStream.streamIndex);
		listener->setStreamConnectionState(state);
		listenerIntersectionDataChanged(listener, true, true, dirtyFlags);
	}

	void handleStreamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
	{
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateLinkStatus };

		if (hasTalker(entityID))
		{
			auto* node = talkerStreamNode(entityID, streamIndex);
			node->setStreamFormat(streamFormat);

			talkerIntersectionDataChanged(node, true, false, dirtyFlags);
		}

		if (hasListener(entityID))
		{
			auto* node = listenerStreamNode(entityID, streamIndex);
			node->setStreamFormat(streamFormat);

			listenerIntersectionDataChanged(node, true, false, dirtyFlags);
		}
	}

	void handleGptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
	{
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateGptp };

		if (hasTalker(entityID))
		{
			auto* talker = talkerNodeFromEntityID(entityID);

			talker->accept(avbInterfaceIndex,
				[this, grandMasterID, grandMasterDomain, dirtyFlags](StreamNode* node)
				{
					node->setGrandMasterID(grandMasterID);
					node->setGrandMasterDomain(grandMasterDomain);

					talkerIntersectionDataChanged(node, true, false, dirtyFlags);
				});
		}

		if (hasListener(entityID))
		{
			auto* listener = listenerNodeFromEntityID(entityID);

			listener->accept(avbInterfaceIndex,
				[this, grandMasterID, grandMasterDomain, dirtyFlags](StreamNode* node)
				{
					node->setGrandMasterID(grandMasterID);
					node->setGrandMasterDomain(grandMasterDomain);

					listenerIntersectionDataChanged(node, true, false, dirtyFlags);
				});
		}
	}

	void handleEntityNameChanged(la::avdecc::UniqueIdentifier const entityID)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				auto const name = avdecc::helper::smartEntityName(*controlledEntity);

				if (hasTalker(entityID))
				{
					auto* node = talkerNodeFromEntityID(entityID);
					node->setName(name);

					talkerHeaderDataChanged(node);
				}

				if (hasListener(entityID))
				{
					auto* node = listenerNodeFromEntityID(entityID);
					node->setName(name);

					listenerHeaderDataChanged(node);
				}
			}
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	void handleStreamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity)
			{
				if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
				{
					auto const name = avdecc::helper::outputStreamName(*controlledEntity, streamIndex);

					auto* node = talkerStreamNode(entityID, streamIndex);
					node->setName(name);

					talkerHeaderDataChanged(node);
				}
				else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
				{
					auto const name = avdecc::helper::inputStreamName(*controlledEntity, streamIndex);

					auto* node = listenerStreamNode(entityID, streamIndex);
					node->setName(name);

					listenerHeaderDataChanged(node);
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
		auto const dirtyFlags = IntersectionDirtyFlags{ IntersectionDirtyFlag::UpdateLinkStatus };

		if (hasTalker(entityID))
		{
			auto* talker = talkerNodeFromEntityID(entityID);

			talker->accept(avbInterfaceIndex,
				[this, linkStatus, dirtyFlags](StreamNode* node)
				{
					node->setInterfaceLinkStatus(linkStatus);

					talkerIntersectionDataChanged(node, true, false, dirtyFlags);
				});
		}

		if (hasListener(entityID))
		{
			auto* listener = listenerNodeFromEntityID(entityID);

			listener->accept(avbInterfaceIndex,
				[this, linkStatus, dirtyFlags](StreamNode* node)
				{
					node->setInterfaceLinkStatus(linkStatus);

					listenerIntersectionDataChanged(node, true, false, dirtyFlags);
				});
		}
	}

private:
	// Returns true if a talker exists for entityID
	bool hasTalker(la::avdecc::UniqueIdentifier const entityID) const
	{
		return _talkerNodeMap.count(entityID) == 1;
	}

	// Returns true if a listener exists for entityID
	bool hasListener(la::avdecc::UniqueIdentifier const entityID) const
	{
		return _listenerNodeMap.count(entityID) == 1;
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

	// Returns talker stream section for entityID + streamIndex
	int talkerStreamSection(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const key = std::make_pair(entityID, streamIndex);
		auto const it = _talkerStreamSectionMap.find(key);
		assert(it != std::end(_talkerStreamSectionMap));
		return it->second;
	}

	// Returns listener stream section for entityID + streamIndex
	int listenerStreamSection(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const key = std::make_pair(entityID, streamIndex);
		auto const it = _listenerStreamSectionMap.find(key);
		assert(it != std::end(_listenerStreamSectionMap));
		return it->second;
	}

	// Returns talker EntityNode for a given entityID
	EntityNode* talkerNodeFromEntityID(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto const it = _talkerNodeMap.find(entityID);
		assert(it != std::end(_talkerNodeMap));
		auto* node = it->second.get();
		assert(node->type() == Node::Type::Entity);
		return static_cast<EntityNode*>(node);
	}

	// Returns listener EntityNode for a given entityID
	EntityNode* listenerNodeFromEntityID(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto const it = _listenerNodeMap.find(entityID);
		assert(it != std::end(_listenerNodeMap));
		auto* node = it->second.get();
		assert(node->type() == Node::Type::Entity);
		return static_cast<EntityNode*>(node);
	}

	// Returns talker StreamNode for a given entityID + streamIndex
	StreamNode* talkerStreamNode(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const section = talkerStreamSection(entityID, streamIndex);
		auto* node = _talkerNodes[section];
		assert(node->isStreamNode());
		return static_cast<StreamNode*>(node);
	}

	// Returns listener StreamNode for a given entityID + streamIndex
	StreamNode* listenerStreamNode(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const section = listenerStreamSection(entityID, streamIndex);
		auto* node = _listenerNodes[section];
		assert(node->isStreamNode());
		return static_cast<StreamNode*>(node);
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
	void intersectionDataChanged(int const talkerSection, int const listenerSection, IntersectionDirtyFlags dirtyFlags)
	{
		Q_Q(Model);

		auto& data = _intersectionData[talkerSection][listenerSection];

		computeIntersectionFlags(data, dirtyFlags);

		auto const index = createIndex(talkerSection, listenerSection);
		emit q->dataChanged(index, index);

#if ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
		highlightIntersection(talkerSection, listenerSection);
#endif
	}

	// Recomputes talker intersection data, possibily recomputing its parent and/or children according desired dirtyFlags
	// Children are updated first, then the node, then the parents
	void talkerIntersectionDataChanged(Node* talker, bool const andParents, bool const andChildren, IntersectionDirtyFlags dirtyFlags)
	{
		assert(talker);

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
		auto const talkerSection = talkerNodeSection(talker);
		for (auto listenerSection = _listenerNodes.size(); listenerSection > 0u; --listenerSection)
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

	// Recomputes listener intersection data, possibily recomputing its parent and/or children according desired dirtyFlags
	// Children are updated first, then the node, then the parents
	void listenerIntersectionDataChanged(Node* listener, bool const andParents, bool const andChildren, IntersectionDirtyFlags dirtyFlags)
	{
		assert(listener);

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
		for (auto talkerSection = _talkerNodes.size(); talkerSection > 0; --talkerSection)
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

	// Notifies that talker header data has changed
	void talkerHeaderDataChanged(Node* const node)
	{
		Q_Q(Model);

		auto section = talkerNodeSection(node);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		qDebug() << "talkerHeaderDataChanged(" << section << ")";
#endif

		emit q->headerDataChanged(talkerOrientation(), section, section);
	}

	// Notifies that listener header data has changed
	void listenerHeaderDataChanged(Node* const node)
	{
		Q_Q(Model);

		auto section = listenerNodeSection(node);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		qDebug() << "listenerHeaderDataChanged(" << section << ")";
#endif

		emit q->headerDataChanged(listenerOrientation(), section, section);
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

private:
	Model* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(Model);

	bool _transposed{ false };

	// Entity nodes by entity ID
	priv::NodeMap _talkerNodeMap;
	priv::NodeMap _listenerNodeMap;

	// Flattened nodes
	priv::Nodes _talkerNodes;
	priv::Nodes _listenerNodes;

	// Stream section quick access map
	priv::StreamSectionMap _talkerStreamSectionMap;
	priv::StreamSectionMap _listenerStreamSectionMap;

	// Node section quick access map
	priv::NodeSectionMap _talkerNodeSectionMap;
	priv::NodeSectionMap _listenerNodeSectionMap;

	// Talker major intersection data matrix
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

QVariant Model::data(QModelIndex const& index, int role) const
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

	assert(d->isValidTalkerSection(talkerSection));
	assert(d->isValidListenerSection(listenerSection));

	return d->_intersectionData[talkerSection][listenerSection];
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

} // namespace connectionMatrix

#include "model.moc"
