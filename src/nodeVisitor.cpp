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

#include "nodeVisitor.hpp"
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <type_traits>

template<typename NodeType, typename std::enable_if_t<std::is_pointer<NodeType>::value && std::is_base_of<la::avdecc::controller::model::Node, std::remove_pointer_t<NodeType>>::value, int> = 0>
auto createNodeVisitDispatchFunctor() noexcept
{
	return [](NodeVisitor* const visitor, la::avdecc::controller::ControlledEntity const* const entity, bool const isActiveConfiguration, std::any const& node)
	{
		try
		{
			visitor->visit(entity, isActiveConfiguration, *std::any_cast<NodeType>(node));
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

void NodeVisitor::accept(NodeVisitor* const visitor, la::avdecc::controller::ControlledEntity const* const entity, bool const isActiveConfiguration, AnyNode const& node) noexcept
{
	static std::unordered_map<std::type_index, std::function<void(NodeVisitor*, la::avdecc::controller::ControlledEntity const*, bool const, std::any const&)>> s_visitDispatch{};
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
		// JackNodes
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::JackInputNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::JackInputNode const*>();
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::JackOutputNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::JackOutputNode const*>();
		// AvbInterfaceNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::AvbInterfaceNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::AvbInterfaceNode const*>();
		// ClockSourceNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::ClockSourceNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::ClockSourceNode const*>();
		// MemoryObjectNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::MemoryObjectNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::MemoryObjectNode const*>();
		// LocaleNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::LocaleNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::LocaleNode const*>();
		// StringsNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::StringsNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::StringsNode const*>();
		// StreamPortNodes
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::StreamPortInputNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::StreamPortInputNode const*>();
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::StreamPortOutputNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::StreamPortOutputNode const*>();
		// ExternalPortNode
		// InternalPortNode
		// AudioClusterNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::AudioClusterNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::AudioClusterNode const*>();
		// AudioMapNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::AudioMapNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::AudioMapNode const*>();
		// ControlNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::ControlNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::ControlNode const*>();
		// ClockDomainNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::ClockDomainNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::ClockDomainNode const*>();
		// TimingNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::TimingNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::TimingNode const*>();
		// PtpInstanceNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::PtpInstanceNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::PtpInstanceNode const*>();
		// PtpPortNode
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::PtpPortNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::PtpPortNode const*>();
		// RedundantStreamNodes
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::RedundantStreamInputNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::RedundantStreamInputNode const*>();
		s_visitDispatch[std::type_index(typeid(la::avdecc::controller::model::RedundantStreamOutputNode const*))] = createNodeVisitDispatchFunctor<la::avdecc::controller::model::RedundantStreamOutputNode const*>();
	}


	auto const& nodeAny = node.getNode();
	auto const& it = s_visitDispatch.find(nodeAny.type());
	if (it == s_visitDispatch.end())
	{
		AVDECC_ASSERT(it != s_visitDispatch.end(), "Node not handled (should be added to s_visitDispatch map)");
		return;
	}
	auto const& visitHandler = it->second;
	visitHandler(visitor, entity, isActiveConfiguration, nodeAny);
}
