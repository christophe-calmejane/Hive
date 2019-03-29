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

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#	error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

namespace connectionMatrix
{

namespace priv
{

using Nodes = std::vector<Node*>;

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

enum class IntersectionDirtyFlag
{
	UpdateConnected = 1u << 0, /**< Update the connected status, or the summary if this is a parent node */
	UpdateFormat = 1u << 1, /**<  Update the matching format status, or the summary if this is a parent node */
	UpdateGptp = 1u << 2, /**< Update the matching gPTP status, or the summary if this is a parent node (WARNING: For intersection of redundant and non-redundant, the complete checks has to be done, since format compatibility is not checked if GM is not the same) */
	UpdateLinkStatus = 1u << 3, /**< Update the link status, or the summary if this is a parent node */
};
using IntersectionDirtyFlags = la::avdecc::utils::EnumBitfield<IntersectionDirtyFlag>;

// Flatten node hierarchy and insert all nodes in list
void insertNodes(std::vector<Node*>& list, Node* node)
{
	if (!node)
	{
		assert(false);
		return;
	}
	
#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const before = list.size();
#endif
	
	node->accept([&list](Node* node)
	{
		list.push_back(node);
	});
	
#if ENABLE_CONNECTION_MATRIX_DEBUG
	auto const after = list.size();
	qDebug() << "insertNodes" << before << ">" << after;
#endif
}

// Range vector removal
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

// Builds and returns an EntitySectionMap from nodes
EntitySectionMap buildEntitySectionMap(Nodes const& nodes)
{
	EntitySectionMap sectionMap;
	
	for (auto section = 0u; section < nodes.size(); ++section)
	{
		auto* node = nodes[section];
		if (node->type() == Node::Type::Entity)
		{
#if ENABLE_CONNECTION_MATRIX_DEBUG
			qDebug() << "buildEntitySectionMap" << node->name() << "at section" << section;
#endif
		
			auto const [it, result] = sectionMap.insert(std::make_pair(node->entityID(), section));
			
			assert(result);
		}
	}
	
	return sectionMap;
}

// Build and returns a StreamSectionMap from nodes
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

// Build and returns a NodeSectionMap from nodes
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

// Returns entity name of entityID
QString entityName(la::avdecc::UniqueIdentifier const& entityID)
{
	QString name;

	auto& manager = avdecc::ControllerManager::getInstance();
	if (auto controlledEntity = manager.getControlledEntity(entityID))
	{
		name = avdecc::helper::smartEntityName(*controlledEntity);
	}
	
	if (name.isNull())
	{
		name = avdecc::helper::uniqueIdentifierToString(entityID);
	}
	
	return name;
}

// Returns redundant output name at redundantIndex
QString redundantOutputName(la::avdecc::controller::model::VirtualIndex const redundantIndex)
{
	return QString{ "Redundant Stream Output %1" }.arg(QString::number(redundantIndex));
}

// Returns redundant input name at redundantIndex
QString redundantInputName(la::avdecc::controller::model::VirtualIndex const redundantIndex)
{
	return QString{ "Redundant Stream Input %1" }.arg(QString::number(redundantIndex));
}

Model::IntersectionData::Type determineIntersectionType(Node* talker, Node* listener)
{
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

void initializeStaticIntersectionData(Node* talker, Node* listener, Model::IntersectionData& intersectionData)
{
	assert(talker);
	assert(listener);

	intersectionData.type = determineIntersectionType(talker, listener);
	intersectionData.talker = talker;
	intersectionData.listener = listener;
}

Model::IntersectionData::Capabilities computeIntersectionCapabilities(Model::IntersectionData const& intersectionData, IntersectionDirtyFlags const dirtyFlags)
{
	auto talkerEntityID = intersectionData.talker->entityID();
	auto listenerEntityID = intersectionData.listener->entityID();

	if (talkerEntityID == listenerEntityID)
	{
		return {};
	}

	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		auto talkerEntity = manager.getControlledEntity(talkerEntityID);
		auto listenerEntity = manager.getControlledEntity(listenerEntityID);
	}
	catch (...)
	{
	}

	return {};
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
		auto const columns = rows > 0 ? _intersectionData.at(0).size() : 0u;
			
		qDebug() << "talkers" << _talkerNodes.size();
		qDebug() << "listeners" << _listenerNodes.size();
		qDebug() << "capabilities" << rows << "x" << columns;
	}
#endif

#if ENABLE_CONNECTION_MATRIX_DEBUG
	void highlightIntersection(int talkerSection, int listenerSection)
	{
		assert(isValidTalkerSection(talkerSection));
		assert(isValidListenerSection(listenerSection));

		auto& intersectionData = _intersectionData.at(talkerSection).at(listenerSection);

		if (!intersectionData.animation)
		{
			intersectionData.animation = new QVariantAnimation{ this };
		}

		intersectionData.animation->setStartValue(QColor{ Qt::red });
		intersectionData.animation->setEndValue(QColor{ Qt::transparent });
		intersectionData.animation->setDuration(500);
		intersectionData.animation->start();

		connect(intersectionData.animation, &QVariantAnimation::valueChanged, [this, talkerSection, listenerSection](QVariant const& value)
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
	
	// Insertion / removal helpers
	
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
	
	Node* buildTalkerNode(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		auto* entityNode = EntityNode::create(entityID);
		entityNode->setName(priv::entityName(entityID));
	
		// Redundant streams
		for (auto const& [redundantIndex, redundantNode] : configurationNode.redundantStreamOutputs)
		{
			auto* redundantOutputNode = RedundantNode::createOutputNode(*entityNode, redundantIndex);
			redundantOutputNode->setName(priv::redundantOutputName(redundantIndex));

			for (auto const& [streamIndex, streamNode] : redundantNode.redundantStreams)
			{
				auto const avbInterfaceIndex{ streamNode->staticModel->avbInterfaceIndex };
				
				auto* redundantOutputStreamNode = StreamNode::createRedundantOutputNode(*redundantOutputNode, streamIndex, avbInterfaceIndex);
				redundantOutputStreamNode->setName(avdecc::helper::outputStreamName(controlledEntity, streamIndex));
				redundantOutputStreamNode->setRunning(avdecc::helper::isOutputStreamRunning(controlledEntity, streamIndex));
			}
		}

		// Single streams
		for (auto const& [streamIndex, streamNode] : configurationNode.streamOutputs)
		{
			if (!streamNode.isRedundant)
			{
				auto const avbInterfaceIndex{ streamNode.staticModel->avbInterfaceIndex };
				
				auto* outputStreamNode = StreamNode::createOutputNode(*entityNode, streamIndex, avbInterfaceIndex);
				outputStreamNode->setName(avdecc::helper::outputStreamName(controlledEntity, streamIndex));
				outputStreamNode->setRunning(avdecc::helper::isOutputStreamRunning(controlledEntity, streamIndex));
			}
		}
		
		return entityNode;
	}
	
	Node* buildListenerNode(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		auto* entityNode = EntityNode::create(entityID);
		entityNode->setName(priv::entityName(entityID));
		
		// Redundant streams
		for (auto const& [redundantIndex, redundantNode] : configurationNode.redundantStreamInputs)
		{
			auto* redundantInputNode = RedundantNode::createInputNode(*entityNode, redundantIndex);
			redundantInputNode->setName(priv::redundantInputName(redundantIndex));

			for (auto const& [streamIndex, streamNode] : redundantNode.redundantStreams)
			{
				auto const avbInterfaceIndex{ streamNode->staticModel->avbInterfaceIndex };
				
				auto* redundantInputStreamNode = StreamNode::createRedundantInputNode(*redundantInputNode, streamIndex, avbInterfaceIndex);
				redundantInputStreamNode->setName(avdecc::helper::inputStreamName(controlledEntity, streamIndex));
				redundantInputStreamNode->setRunning(avdecc::helper::isInputStreamRunning(controlledEntity, streamIndex));
			}
		}

		// Single streams
		for (auto const& [streamIndex, streamNode] : configurationNode.streamInputs)
		{
			if (!streamNode.isRedundant)
			{
				auto const avbInterfaceIndex{ streamNode.staticModel->avbInterfaceIndex };
				
				auto* inputStreamNode = StreamNode::createInputNode(*entityNode, streamIndex, avbInterfaceIndex);
				inputStreamNode->setName(avdecc::helper::inputStreamName(controlledEntity, streamIndex));
				inputStreamNode->setRunning(avdecc::helper::isInputStreamRunning(controlledEntity, streamIndex));
			}
		}
		
		return entityNode;
	}
	
	void addTalker(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		auto* node = buildTalkerNode(controlledEntity, entityID, configurationNode);

		auto const childrenCount = priv::absoluteChildrenCount(node);
		
		auto const first = talkerSectionCount();
		auto const last = first + childrenCount;
		
		beginInsertTalkerItems(first, last);

		_talkerNodeMap.insert(std::make_pair(entityID, node));
		
		priv::insertNodes(_talkerNodes, node);
		
		rebuildTalkerSectionCache();

		// Update capabilities matrix
		_intersectionData.resize(last + 1);
		for (auto talkerSection = first; talkerSection <= last; ++talkerSection)
		{
			auto& row = _intersectionData.at(talkerSection);
			row.resize(_listenerNodes.size());
			
			auto* talker = _talkerNodes.at(talkerSection);
			for (auto listenerSection = 0u; listenerSection < _listenerNodes.size(); ++listenerSection)
			{
				auto* listener = _listenerNodes.at(listenerSection);
				priv::initializeStaticIntersectionData(talker, listener, row.at(listenerSection));
			}
		}
		
#if ENABLE_CONNECTION_MATRIX_DEBUG
		dump();
#endif
		
		endInsertTalkerItems();
	}
	
	void addListener(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::model::ConfigurationNode const& configurationNode)
	{
		auto* node = buildListenerNode(controlledEntity, entityID, configurationNode);
		
		auto const childrenCount = priv::absoluteChildrenCount(node);
		
		auto const first = listenerSectionCount();
		auto const last = first + childrenCount;
		
		beginInsertListenerItems(first, last);

		_listenerNodeMap.insert(std::make_pair(entityID, node));
		
		priv::insertNodes(_listenerNodes, node);

		rebuildListenerSectionCache();
		
		// Update capabilities matrix
		for (auto talkerSection = 0u; talkerSection < _talkerNodes.size(); ++talkerSection)
		{
			auto& row = _intersectionData.at(talkerSection);
			row.resize(_listenerNodes.size());
			
			auto* talker = _talkerNodes.at(talkerSection);
			for (auto listenerSection = first; listenerSection <= last; ++listenerSection)
			{
				auto* listener = _listenerNodes.at(listenerSection);
				priv::initializeStaticIntersectionData(talker, listener, row.at(listenerSection));
			}
		}

#if ENABLE_CONNECTION_MATRIX_DEBUG
		dump();
#endif
		
		endInsertListenerItems();
	}
	
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
			
			_intersectionData.erase(
				std::next(std::begin(_intersectionData), first),
				std::next(std::begin(_intersectionData), last + 1));

#if ENABLE_CONNECTION_MATRIX_DEBUG
			dump();
#endif
			
			endRemoveTalkerItems();
		}
	}
	
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
				auto& row = _intersectionData.at(talkerSection);
				
				row.erase(
					std::next(std::begin(row), first),
					std::next(std::begin(row), last + 1));
			}

#if ENABLE_CONNECTION_MATRIX_DEBUG
			dump();
#endif

			endRemoveListenerItems();
		}
	}
	
	Node* talkerNode(int section) const
	{
		if (!isValidTalkerSection(section))
		{
			return nullptr;
		}
		
		return _talkerNodes.at(section);
	}
	
	Node* listenerNode(int section) const
	{
		if (!isValidListenerSection(section))
		{
			return nullptr;
		}
		
		return _listenerNodes.at(section);
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
			
			auto const section = talkerStreamSection(entityID, streamIndex);
			talkerHeaderDataChanged(section);
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			auto* node = listenerStreamNode(entityID, streamIndex);
			node->setRunning(isRunning);
			
			auto const section = listenerStreamSection(entityID, streamIndex);
			listenerHeaderDataChanged(section);
		}
	}

	void handleStreamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& state)
	{
		auto const entityID = state.listenerStream.entityID;
		auto const streamIndex = state.listenerStream.streamIndex;

		auto* listener = listenerStreamNode(entityID, streamIndex);

		listenerIntersectionDataChanged(listener, true, true);
	}

	void handleStreamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
	{
		if (hasTalker(entityID))
		{
			auto* talker = talkerStreamNode(entityID, streamIndex);
			talkerIntersectionDataChanged(talker, true, false);
		}

		if (hasListener(entityID))
		{
			auto* listener = listenerStreamNode(entityID, streamIndex);
			listenerIntersectionDataChanged(listener, true, false);
		}
	}

	void handleGptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
	{
		if (hasTalker(entityID))
		{
			talkerAvbInterfaceDataChanged(entityID, avbInterfaceIndex);
		}

		if (hasListener(entityID))
		{
			listenerAvbInterfaceDataChanged(entityID, avbInterfaceIndex);
		}
	}

	void handleEntityNameChanged(la::avdecc::UniqueIdentifier const entityID)
	{
		auto const name = priv::entityName(entityID);
		
		setTalkerName(entityID, name);
		setListenerName(entityID, name);
	}

	void handleStreamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex)
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);
		if (controlledEntity)
		{
			if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
			{
				auto const name = avdecc::helper::outputStreamName(*controlledEntity, streamIndex);
				setTalkerStreamName(entityID, streamIndex, name);
			}
			else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
			{
				auto const name = avdecc::helper::inputStreamName(*controlledEntity, streamIndex);
				setListenerStreamName(entityID, streamIndex, name);
			}
		}
	}

	void handleAvbInterfaceLinkStatusChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const linkStatus)
	{
		if (hasTalker(entityID))
		{
			talkerAvbInterfaceDataChanged(entityID, avbInterfaceIndex);
		}

		if (hasListener(entityID))
		{
			listenerAvbInterfaceDataChanged(entityID, avbInterfaceIndex);
		}
	}
	
private:
	bool hasTalker(la::avdecc::UniqueIdentifier const entityID) const
	{
		return _talkerNodeMap.count(entityID) == 1;
	}

	bool hasListener(la::avdecc::UniqueIdentifier const entityID) const
	{
		return _listenerNodeMap.count(entityID) == 1;
	}

	void setTalkerSectionName(int section, QString const& name)
	{
		if (!isValidTalkerSection(section))
		{
			return;
		}
		
		auto* node = _talkerNodes.at(section);
		if (node->name() != name)
		{
			node->setName(name);
			emit talkerHeaderDataChanged(section);
		}
	}
	
	void setListenerSectionName(int section, QString const& name)
	{
		if (!isValidListenerSection(section))
		{
			return;
		}
		
		auto* node = _listenerNodes.at(section);
		if (node->name() != name)
		{
			node->setName(name);
			emit listenerHeaderDataChanged(section);
		}
	}
	
	int talkerSection(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto* node = talkerNodeFromEntityID(entityID);
		return priv::indexOf(_talkerNodeSectionMap, node);
	}
	
	int listenerSection(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto* node = listenerNodeFromEntityID(entityID);
		return priv::indexOf(_listenerNodeSectionMap, node);
	}
	
	void setTalkerName(la::avdecc::UniqueIdentifier const& entityID, QString const& name)
	{
		auto const section = talkerSection(entityID);
		setTalkerSectionName(section, name);
	}
	
	void setListenerName(la::avdecc::UniqueIdentifier const& entityID, QString const& name)
	{
		auto const section = listenerSection(entityID);
		setListenerSectionName(section, name);
	}
	
	Node* talkerNodeFromEntityID(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto const it = _talkerNodeMap.find(entityID);
		assert(it != std::end(_talkerNodeMap));
		return it->second.get();
	}
	
	Node* listenerNodeFromEntityID(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto const it = _listenerNodeMap.find(entityID);
		assert(it != std::end(_listenerNodeMap));
		return it->second.get();
	}
	
	StreamNode* talkerStreamNode(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const section = talkerStreamSection(entityID, streamIndex);
		auto* node = _talkerNodes.at(section);
		assert(node->isStreamNode());
		return static_cast<StreamNode*>(node);
	}
	
	StreamNode* listenerStreamNode(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const section = listenerStreamSection(entityID, streamIndex);
		auto* node = _listenerNodes.at(section);
		assert(node->isStreamNode());
		return static_cast<StreamNode*>(node);
	}

	int talkerStreamSection(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const key = std::make_pair(entityID, streamIndex);
		auto const it = _talkerStreamSectionMap.find(key);
		assert(it != std::end(_talkerStreamSectionMap));
		return it->second;
	}
	
	int listenerStreamSection(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		auto const key = std::make_pair(entityID, streamIndex);
		auto const it = _listenerStreamSectionMap.find(key);
		assert(it != std::end(_listenerStreamSectionMap));
		return it->second;
	}
	
	void setTalkerStreamName(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name)
	{
		auto const section = talkerStreamSection(entityID, streamIndex);
		setTalkerSectionName(section, name);
	}
	
	void setListenerStreamName(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& name)
	{
		auto const section = listenerStreamSection(entityID, streamIndex);
		setListenerSectionName(section, name);
	}

private:
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

	void intersectionDataChanged(int const talkerSection, int const listenerSection)
	{
		Q_Q(Model);

		auto& data = _intersectionData.at(talkerSection).at(listenerSection);

		priv::IntersectionDirtyFlags allBits;
		allBits.assign(0xff);

		data.capabilities = priv::computeIntersectionCapabilities(data, allBits);

		auto const index = createIndex(talkerSection, listenerSection);
		emit q->dataChanged(index, index);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		highlightIntersection(talkerSection, listenerSection);
#endif
	}

	void talkerIntersectionDataChanged(Node* talker, bool const andParents, bool const andChildren)
	{
		assert(talker);

		// Recursively update the parents
		if (andParents)
		{
			auto* node = talker;
			while (auto* parent = node->parent())
			{
				talkerIntersectionDataChanged(parent, andParents, false);
				node = parent;
			}
		}

		// Update the children
		if (andChildren)
		{
			talker->accept([=](Node* child)
			{
				if (child != talker)
				{
					talkerIntersectionDataChanged(child, false, andChildren);
				}
			});
		}

		auto const talkerSection = priv::indexOf(_talkerNodeSectionMap, talker);
		for (auto listenerSection = 0u; listenerSection < _listenerNodes.size(); ++listenerSection)
		{
			intersectionDataChanged(talkerSection, listenerSection);
		}
	}

	void listenerIntersectionDataChanged(Node* listener, bool const andParents, bool const andChildren)
	{
		assert(listener);

		// Recursively update the parents
		if (andParents)
		{
			auto* node = listener;
			while (auto* parent = node->parent())
			{
				listenerIntersectionDataChanged(parent, andParents, false);
				node = parent;
			}
		}

		// Update the children
		if (andChildren)
		{
			listener->accept([=](Node* child)
			{
				if (child != listener)
				{
					listenerIntersectionDataChanged(child, false, andChildren);
				}
			});
		}

		auto const listenerSection = priv::indexOf(_listenerNodeSectionMap, listener);
		for (auto talkerSection = 0u; talkerSection < _talkerNodes.size(); ++talkerSection)
		{
			intersectionDataChanged(talkerSection, listenerSection);
		}
	}

	void talkerAvbInterfaceDataChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex)
	{
		auto* talker = talkerNodeFromEntityID(entityID);

		talker->accept([this, avbInterfaceIndex](Node* node)
		{
			if (node->isStreamNode())
			{
				if (static_cast<StreamNode*>(node)->avbInterfaceIndex() == avbInterfaceIndex)
				{
					talkerIntersectionDataChanged(node, true, false);
				}
			}
		});
	}

	void listenerAvbInterfaceDataChanged(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex)
	{
		auto* listener = listenerNodeFromEntityID(entityID);

		listener->accept([this, avbInterfaceIndex](Node* node)
		{
			if (node->isStreamNode())
			{
				if (static_cast<StreamNode*>(node)->avbInterfaceIndex() == avbInterfaceIndex)
				{
					listenerIntersectionDataChanged(node, true, false);
				}
			}
		});
	}

	void talkerHeaderDataChanged(int section)
	{
		Q_Q(Model);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		qDebug() << "talkerHeaderDataChanged(" << section << ")";
#endif
		emit q->headerDataChanged(talkerOrientation(), section, section);
	}

	void listenerHeaderDataChanged(int section)
	{
		Q_Q(Model);

#if ENABLE_CONNECTION_MATRIX_DEBUG
		qDebug() << "listenerHeaderDataChanged(" << section << ")";
#endif
		emit q->headerDataChanged(listenerOrientation(), section, section);
	}

	Qt::Orientation talkerOrientation() const
	{
		return !_transposed ? Qt::Vertical : Qt::Horizontal;
	}

	Qt::Orientation listenerOrientation() const
	{
		return !_transposed ? Qt::Horizontal : Qt::Vertical;
	}
	
	int talkerSectionCount() const
	{
		return static_cast<int>(_talkerNodes.size());
	}
	
	int listenerSectionCount() const
	{
		return static_cast<int>(_listenerNodes.size());
	}
	
	int talkerIndex(QModelIndex const& index) const
	{
		return !_transposed ? index.row() : index.column();
	}
	
	int listenerIndex(QModelIndex const& index) const
	{
		return !_transposed ? index.column() : index.row();
	}
	
	bool isValidTalkerSection(int section) const
	{
		return section >= 0 && section < talkerSectionCount();
	}
	
	bool isValidListenerSection(int section) const
	{
		return section >= 0 && section < listenerSectionCount();
	}
	
	QString talkerHeaderData(int section) const
	{
		if (!isValidTalkerSection(section))
		{
			return {};
		}

		return _talkerNodes.at(section)->name();
	}
	
	QString listenerHeaderData(int section) const
	{
		if (!isValidListenerSection(section))
		{
			return {};
		}

		return _listenerNodes.at(section)->name();
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
	std::vector<std::vector<Model::IntersectionData>> _intersectionData;
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
#if ENABLE_CONNECTION_MATRIX_DEBUG
	if (role == Qt::BackgroundRole)
	{
		auto const& data = intersectionData(index);
		if (data.animation)
		{
			return data.animation->currentValue();
		}
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
	
	return d->_intersectionData.at(talkerSection).at(listenerSection);
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
