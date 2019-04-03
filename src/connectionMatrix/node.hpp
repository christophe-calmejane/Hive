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

#pragma once

#include "avdecc/helper.hpp"

namespace connectionMatrix
{

class Node;
class EntityNode;
class RedundantNode;
class RedundantStreamNode;
class StreamNode;

class Node
{
	friend class ModelPrivate;
public:
	enum class Type
	{
		None,
		Entity,
		OutputStream,
		InputStream,
		RedundantOutput,
		RedundantInput,
		RedundantOutputStream,
		RedundantInputStream,
	};
	
	virtual ~Node() = default;
	
	// Returns node type
	Type type() const;

	// Returns true if node type is Entity
	bool isEntityNode() const;

	// Returns true if node type is either RedundantOutput or RedundantInput
	bool isRedundantNode() const;
	
	// Returns true if node type is either RedundantOutputStream or RedundantInputStream
	bool isRedundantStreamNode() const;

	// Returns true if node type is either OutputStream, InputStream, RedundantOutputStream or RedundantInputStream
	bool isStreamNode() const;
	
	// Returns the entity ID
	la::avdecc::UniqueIdentifier const& entityID() const;
	
	// Returns the parent node
	Node* parent() const;
	
	// Returns true if this node has a parent (false for an entity)
	bool hasParent() const;

	// Returns the name of this node (entity name, stream name, .. etc)
	QString const& name() const;
	
	// Returns the index of the node in its parent childen
	int index() const;
	
	// Returns the index child in this node children list, -1 if the child is not related
	int indexOf(Node* child) const;
	
	// Returns the child node at index, null if not found
	Node* childAt(int index) const;

	// Returns the number of children
	int childrenCount() const;
	
	// Visitor pattern that performs a complete hierarchy traversal
	using Visitor = std::function<void(Node*)>;
	void accept(Visitor const& visitor) const;

protected:
	Node(Type const type, la::avdecc::UniqueIdentifier const& entityID, Node* parent);

	void setName(QString const& name);
	
protected:
	// Node type
	Type const _type;
	
	// Associated entity ID
	la::avdecc::UniqueIdentifier const _entityID;
	
	// Pointer to the parent node (should be null for entities only)
	Node* const _parent;
	
	// Node name
	QString _name;
	
	// Holds the children
	std::vector<std::unique_ptr<Node>> _children;
};

class EntityNode : public Node
{
	friend class ModelPrivate;
public:
	static EntityNode* create(la::avdecc::UniqueIdentifier const& entityID);

	// Visitor pattern that performs is called on every stream node that matches avbInterfaceIndex
	using AvbInterfaceIndexVisitor = std::function<void(StreamNode*)>;
	void accept(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, AvbInterfaceIndexVisitor const& visitor) const;

protected:
	EntityNode(la::avdecc::UniqueIdentifier const& entityID);
};

class RedundantNode : public Node
{
	friend class ModelPrivate;
public:
	static RedundantNode* createOutputNode(EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex);
	static RedundantNode* createInputNode(EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex);
	
	la::avdecc::controller::model::VirtualIndex const& redundantIndex() const;
	
protected:
	RedundantNode(Type const type, EntityNode& parent, la::avdecc::controller::model::VirtualIndex const redundantIndex);
	
protected:
	la::avdecc::controller::model::VirtualIndex const _redundantIndex;
};

class StreamNode : public Node
{
	friend class ModelPrivate;
public:
	static StreamNode* createOutputNode(EntityNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);
	static StreamNode* createInputNode(EntityNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);
	
	static StreamNode* createRedundantOutputNode(RedundantNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);
	static StreamNode* createRedundantInputNode(RedundantNode& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);
	
	la::avdecc::entity::model::StreamIndex const& streamIndex() const;
	la::avdecc::entity::model::AvbInterfaceIndex const& avbInterfaceIndex() const;
	la::avdecc::entity::model::StreamFormat const& streamFormat() const;
	la::avdecc::UniqueIdentifier const& grandMasterID() const;
	std::uint8_t const& grandMasterDomain() const;
	la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const& interfaceLinkStatus() const;
	bool isRunning() const;
	
protected:
	StreamNode(Type const type, Node& parent, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);

	void setStreamFormat(la::avdecc::entity::model::StreamFormat const streamFormat);
	void setGrandMasterID(la::avdecc::UniqueIdentifier const grandMasterID);
	void setGrandMasterDomain(std::uint8_t const grandMasterDomain);
	void setInterfaceLinkStatus(la::avdecc::controller::ControlledEntity::InterfaceLinkStatus const interfaceLinkStatus);
	void setRunning(bool isRunning);
	
protected:
	la::avdecc::entity::model::StreamIndex const _streamIndex;
	la::avdecc::entity::model::AvbInterfaceIndex const _avbInterfaceIndex;
	la::avdecc::entity::model::StreamFormat _streamFormat{ la::avdecc::entity::model::StreamFormat::getNullStreamFormat() };
	la::avdecc::UniqueIdentifier _grandMasterID;
	std::uint8_t _grandMasterDomain;
	la::avdecc::controller::ControlledEntity::InterfaceLinkStatus _interfaceLinkStatus{ la::avdecc::controller::ControlledEntity::InterfaceLinkStatus::Unknown };
	bool _isRunning{ true };
};

} // namespace connectionMatrix
