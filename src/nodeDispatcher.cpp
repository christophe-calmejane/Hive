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

#include "nodeDispatcher.hpp"
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <type_traits>

template<typename NodeType, typename std::enable_if_t<std::is_pointer<NodeType>::value && std::is_base_of<la::avdecc::controller::model::Node, std::remove_pointer_t<NodeType>>::value, int> = 0>
auto createNodeDispatchFunctor() noexcept
{
	return [](NodeDispatcher* const dispatchor, la::avdecc::controller::ControlledEntity const* const entity, std::any const& node)
	{
		try
		{
			dispatchor->dispatch(entity, *std::any_cast<NodeType>(node));
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

void NodeDispatcher::accept(NodeDispatcher* const dispatchor, la::avdecc::controller::ControlledEntity const* const entity, AnyNode const& node) noexcept
{
	static std::unordered_map<std::type_index, std::function<void(NodeDispatcher*, la::avdecc::controller::ControlledEntity const*, std::any const&)>> s_dispatch{};
	if (s_dispatch.empty())
	{
		// Create the validator dispatch table
		// EntityNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::EntityNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::EntityNode const*>();
		// ConfigurationNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::ConfigurationNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::ConfigurationNode const*>();
		// AudioUnitNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::AudioUnitNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::AudioUnitNode const*>();
		// StreamNodes
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::StreamInputNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::StreamInputNode const*>();
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::StreamOutputNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::StreamOutputNode const*>();
		// JackNodes
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::JackInputNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::JackInputNode const*>();
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::JackOutputNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::JackOutputNode const*>();
		// AvbInterfaceNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::AvbInterfaceNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::AvbInterfaceNode const*>();
		// ClockSourceNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::ClockSourceNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::ClockSourceNode const*>();
		// MemoryObjectNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::MemoryObjectNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::MemoryObjectNode const*>();
		// LocaleNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::LocaleNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::LocaleNode const*>();
		// StringsNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::StringsNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::StringsNode const*>();
		// StreamPortNodes
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::StreamPortInputNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::StreamPortInputNode const*>();
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::StreamPortOutputNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::StreamPortOutputNode const*>();
		// ExternalPortNode
		// InternalPortNode
		// AudioClusterNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::AudioClusterNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::AudioClusterNode const*>();
		// AudioMapNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::AudioMapNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::AudioMapNode const*>();
		// ControlNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::ControlNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::ControlNode const*>();
		// ClockDomainNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::ClockDomainNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::ClockDomainNode const*>();
		// TimingNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::TimingNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::TimingNode const*>();
		// PtpInstanceNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::PtpInstanceNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::PtpInstanceNode const*>();
		// PtpPortNode
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::PtpPortNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::PtpPortNode const*>();
		// RedundantStreamNodes
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::RedundantStreamInputNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::RedundantStreamInputNode const*>();
		s_dispatch[std::type_index(typeid(la::avdecc::controller::model::RedundantStreamOutputNode const*))] = createNodeDispatchFunctor<la::avdecc::controller::model::RedundantStreamOutputNode const*>();
	}


	auto const& nodeAny = node.getNode();
	auto const& it = s_dispatch.find(nodeAny.type());
	if (it == s_dispatch.end())
	{
		AVDECC_ASSERT(it != s_dispatch.end(), "Node not handled (should be added to s_dispatch map)");
		return;
	}
	auto const& dispatchHandler = it->second;
	dispatchHandler(dispatchor, entity, nodeAny);
}
