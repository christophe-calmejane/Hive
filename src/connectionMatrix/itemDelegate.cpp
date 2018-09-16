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
#include <algorithm>

namespace connectionMatrix
{

void ItemDelegate::setTransposed(bool const isTransposed)
{
	_isTransposed = isTransposed;
}

bool ItemDelegate::isTransposed() const
{
	return _isTransposed;
}

void ItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Highlighted background if needed
	if (option.state & QStyle::State_Selected)
	{
		painter->fillRect(option.rect, option.palette.highlight());
	}
	
	auto talkerNodeType = index.model()->headerData(index.row(), Qt::Vertical, Model::NodeTypeRole).value<Model::NodeType>();
	auto listenerNodeType = index.model()->headerData(index.column(), Qt::Horizontal, Model::NodeTypeRole).value<Model::NodeType>();
	
	if (_isTransposed)
	{
		std::swap(talkerNodeType, listenerNodeType);
	}
	
	if (talkerNodeType == Model::NodeType::Entity || listenerNodeType == Model::NodeType::Entity)
	{
		drawEntityNoConnection(painter, option.rect);
	}
	else
	{
		auto talkerRedundantStreamOrder = index.model()->headerData(index.row(), Qt::Vertical, Model::RedundantStreamOrderRole).value<std::int32_t>();
		auto listenerRedundantStreamOrder = index.model()->headerData(index.column(), Qt::Horizontal, Model::RedundantStreamOrderRole).value<std::int32_t>();
	
		if (_isTransposed)
		{
			std::swap(talkerRedundantStreamOrder, listenerRedundantStreamOrder);
		}
	
		// If index is a cross of 2 redundant streams, only the diagonal is connectable
		if (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInputStream && talkerRedundantStreamOrder != listenerRedundantStreamOrder)
		{
			return;
		}
	
		auto const capabilities = index.data(Model::ConnectionCapabilitiesRole).value<Model::ConnectionCapabilities>();
		if (capabilities == Model::ConnectionCapabilities::None)
		{
			return;
		}
		
		auto const isRedundant = !((talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInput)
															 || (talkerNodeType == Model::NodeType::OutputStream && listenerNodeType == Model::NodeType::InputStream));

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
