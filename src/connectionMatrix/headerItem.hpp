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

#include <QHeaderView>
#include "connectionMatrix/model.hpp"
#include "avdecc/controllerManager.hpp"

namespace connectionMatrix
{
	
class HeaderItem : public QStandardItem
{
public:
	using StreamMap = std::unordered_map<la::avdecc::entity::model::StreamIndex, std::int32_t>;
	using InterfaceMap = std::unordered_map<la::avdecc::entity::model::AvbInterfaceIndex, std::vector<std::int32_t>>;
	using RelativeParentIndex = std::optional<std::int32_t>;

	HeaderItem(Model::NodeType const nodeType, la::avdecc::UniqueIdentifier const& entityID);

	Model::NodeType nodeType() const;

	la::avdecc::UniqueIdentifier const& entityID() const;

	void setStreamNodeInfo(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex);

	la::avdecc::entity::model::StreamIndex streamIndex() const;
	la::avdecc::entity::model::AvbInterfaceIndex avbInterfaceIndex() const;

	void setRedundantIndex(la::avdecc::controller::model::VirtualIndex const redundantIndex);
	la::avdecc::controller::model::VirtualIndex redundantIndex() const;

	void setRedundantStreamOrder(std::int32_t const redundantStreamOrder);
	std::int32_t redundantStreamOrder() const;

	void setRelativeParentIndex(std::int32_t const relativeParentIndex);
	RelativeParentIndex relativeParentIndex() const;

	void setChildrenCount(std::int32_t const childrenCount);
	std::int32_t childrenCount() const;

	void setStreamMap(StreamMap const& streamMap);
	StreamMap const& streamMap() const;

	void setInterfaceMap(InterfaceMap const& interfaceMap);
	InterfaceMap const& interfaceMap() const;

	virtual QVariant data(int role) const override;

private:
	Model::NodeType const _nodeType;
	la::avdecc::UniqueIdentifier const _entityID;
	la::avdecc::entity::model::StreamIndex _streamIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
	la::avdecc::entity::model::AvbInterfaceIndex _avbInterfaceIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
	la::avdecc::controller::model::VirtualIndex _redundantIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
	std::int32_t _redundantStreamOrder{ -1 };
	RelativeParentIndex _relativeParentIndex{ std::nullopt };
	std::int32_t _childrenCount{ 0 };
	StreamMap _streamMap{};
	InterfaceMap _interfaceMap{};
};

} // namespace connectionMatrix
