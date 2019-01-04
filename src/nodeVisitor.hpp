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

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#	error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

#include <la/avdecc/controller/internals/avdeccControlledEntityModel.hpp>
#if defined(ENABLE_AVDECC_CUSTOM_ANY)
#	include <la/avdecc/internals/any.hpp>
#else // !ENABLE_AVDECC_CUSTOM_ANY
#	include <any>
#endif // ENABLE_AVDECC_CUSTOM_ANY

#include <QMetaType>

class AnyNode final
{
public:
	AnyNode() noexcept = default;
	template<class Node, typename = std::enable_if_t<std::is_base_of<la::avdecc::controller::model::Node, Node>::value>>
	explicit AnyNode(Node const* const node) noexcept
		: _node(node)
	{
	}

	std::any getNode() const noexcept
	{
		return _node;
	}

	// Defaulted compiler auto-generated methods
	AnyNode(AnyNode&&) = default;
	AnyNode(AnyNode const&) = default;
	AnyNode& operator=(AnyNode const&) = default;
	AnyNode& operator=(AnyNode&&) = default;

private:
	std::any _node{};
};

Q_DECLARE_METATYPE(AnyNode)

class NodeVisitor
{
public:
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::EntityNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::ConfigurationNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::AudioUnitNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::StreamInputNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::StreamOutputNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::AvbInterfaceNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::ClockSourceNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::LocaleNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::StringsNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::StreamPortNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::AudioClusterNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::AudioMapNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::ClockDomainNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::RedundantStreamNode const& node) noexcept = 0;
	virtual void visit(la::avdecc::controller::ControlledEntity const* const entity, la::avdecc::controller::model::MemoryObjectNode const& node) noexcept = 0;

	static void accept(NodeVisitor* const visitor, la::avdecc::controller::ControlledEntity const* const entity, AnyNode const& node) noexcept;

	// Defaulted compiler auto-generated methods
	NodeVisitor() noexcept = default;
	virtual ~NodeVisitor() noexcept = default;
	NodeVisitor(NodeVisitor&&) = default;
	NodeVisitor(NodeVisitor const&) = default;
	NodeVisitor& operator=(NodeVisitor const&) = default;
	NodeVisitor& operator=(NodeVisitor&&) = default;
};
