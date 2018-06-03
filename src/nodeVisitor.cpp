/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nodeVisitor.hpp"
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <type_traits>

template<typename NodeType, typename std::enable_if_t<std::is_pointer<NodeType>::value && std::is_base_of<la::avdecc::controller::model::Node, std::remove_pointer_t<NodeType>>::value, int> = 0>
auto createNodeVisitDispatchFunctor() noexcept
{
	return[](NodeVisitor* const visitor, la::avdecc::controller::ControlledEntity const* const entity, std::any const& node)
	{
		try
		{
			visitor->visit(entity, *std::any_cast<NodeType>(node));
		}
		catch (std::bad_any_cast const&)
		{
			AVDECC_ASSERT(false, "Node does not match expected type!");
		}
		catch (...)
		{
		}
	};
}

void NodeVisitor::accept(NodeVisitor* const visitor, la::avdecc::controller::ControlledEntity const* const entity, AnyNode const& node) noexcept
{
	static std::unordered_map<std::type_index, std::function<void(NodeVisitor*, la::avdecc::controller::ControlledEntity const*, std::any const&)>> s_visitDispatch{};
	if (s_visitDispatch.empty())
	{
		// Create the validator dispatch table
		// EntityNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::EntityNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::EntityNode const*>();
		// ConfigurationNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::ConfigurationNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::ConfigurationNode const*>();
		// AudioUnitNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::AudioUnitNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::AudioUnitNode const*>();
		// StreamNodes
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::StreamInputNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::StreamInputNode const*>();
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::StreamOutputNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::StreamOutputNode const*>();
		// JackNode
		// AvbInterfaceNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::AvbInterfaceNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::AvbInterfaceNode const*>();
		// ClockSourceNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::ClockSourceNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::ClockSourceNode const*>();
		// MemoryObjectNode
		// LocaleNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::LocaleNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::LocaleNode const*>();
		// StringsNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::StringsNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::StringsNode const*>();
		// StreamPortNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::StreamPortNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::StreamPortNode const*>();
		// ExternalPortNode
		// InternalPortNode
		// AudioClusterNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::AudioClusterNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::AudioClusterNode const*>();
		// AudioMapNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::AudioMapNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::AudioMapNode const*>();
		// ClockDomainNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::ClockDomainNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::ClockDomainNode const*>();
		// RedundantStreamNodes
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::RedundantStreamNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::RedundantStreamNode const*>();
	}


	auto const& nodeAny = node.getNode();
	auto const& it = s_visitDispatch.find(nodeAny.type());
	if (it == s_visitDispatch.end())
	{
		AVDECC_ASSERT(it != s_visitDispatch.end(), "Node not handled (should be added to s_visitDispatch map)");
		return;
	}
	auto const& visitHandler = it->second;
	visitHandler(visitor, entity, nodeAny);
}
