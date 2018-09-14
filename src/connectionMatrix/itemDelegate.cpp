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

#include "connectionMatrix/itemDelegate.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/paintHelper.hpp"
#include "avdecc/helper.hpp"

#include <QPainter>

namespace connectionMatrix
{

void ItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Highlighted background if needed
	if (option.state & QStyle::State_Selected)
	{
		painter->fillRect(option.rect, option.palette.highlight());
	}
	
	auto const talkerNodeType = index.data(Model::TalkerNodeTypeRole).value<Model::NodeType>();
	auto const listenerNodeType = index.data(Model::ListenerNodeTypeRole).value<Model::NodeType>();
	
	if (talkerNodeType == Model::NodeType::Entity || listenerNodeType == Model::NodeType::Entity)
	{
		drawEntityNoConnection(painter, option.rect);
	}
	else
	{
		
		// TODO, create helper roles that computes everything for us ..
		
		
		auto const talkerRedundantStreamOrder = index.data(Model::TalkerRedundantStreamOrderRole).value<std::int32_t>();
		auto const listenerRedundantStreamOrder = index.data(Model::ListenerRedundantStreamOrderRole).value<std::int32_t>();
	
		// If index is a cross of 2 redundant streams, only the diagonal is connectable
		if (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInputStream && talkerRedundantStreamOrder != listenerRedundantStreamOrder)
		{
			return;
		}
	
		auto const capabilities = index.data(Model::ConnectionCapabilitiesRole).value<Model::ConnectionCapabilities>();
		auto const isRedundant{false};
		
		if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::Connected))
		{
			if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::WrongDomain))
			{
				drawWrongDomainConnectedStream(painter, option.rect, isRedundant);
			}
			else if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::WrongFormat))
			{
				drawWrongFormatConnectedStream(painter, option.rect, isRedundant);
			}
			else
			{
				drawConnectedStream(painter, option.rect, isRedundant);
			}
		}
		else if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::FastConnecting))
		{
			if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::WrongDomain))
			{
				drawWrongDomainFastConnectingStream(painter, option.rect, isRedundant);
			}
			else if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::WrongFormat))
			{
				drawWrongFormatFastConnectingStream(painter, option.rect, isRedundant);
			}
			else
			{
				drawFastConnectingStream(painter, option.rect, isRedundant);
			}
		}
		else if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::PartiallyConnected))
		{
			drawPartiallyConnectedRedundantNode(painter, option.rect);
		}
		else
		{
			if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::WrongDomain))
			{
				drawWrongDomainNotConnectedStream(painter, option.rect, isRedundant);
			}
			else if (la::avdecc::hasFlag(capabilities, Model::ConnectionCapabilities::WrongFormat))
			{
				drawWrongFormatNotConnectedStream(painter, option.rect, isRedundant);
			}
			else
			{
				drawNotConnectedStream(painter, option.rect, isRedundant);
			}
		}
	}
}

QSize ItemDelegate::sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	return {};
}

} // namespace connectionMatrix
