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

#include "connectionMatrix/itemDelegate.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/paintHelper.hpp"
#include "avdecc/helper.hpp"

#include <QPainter>
#include <algorithm>

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

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
		auto const rowEntityID{ index.model()->headerData(index.row(), Qt::Vertical, Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>() };
		auto const columnEntityID{ index.model()->headerData(index.column(), Qt::Horizontal, Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>() };

		// This is the cross section of the same entity, no connection is possible
		if (rowEntityID == columnEntityID)
		{
			drawNotApplicable(painter, option.rect);
		}
		else
		{
			auto const capabilities = index.data(Model::ConnectionCapabilitiesRole).value<Model::ConnectionCapabilities>();

			if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::Connected))
			{
				drawEntityConnection(painter, option.rect);
			}
			else
			{
				drawEntityNoConnection(painter, option.rect);
			}
		}
	}
	else
	{
		auto talkerRedundantStreamOrder = index.model()->headerData(index.row(), Qt::Vertical, Model::RedundantStreamOrderRole).value<std::int32_t>();
		auto listenerRedundantStreamOrder = index.model()->headerData(index.column(), Qt::Horizontal, Model::RedundantStreamOrderRole).value<std::int32_t>();

		if (_isTransposed)
		{
			std::swap(talkerRedundantStreamOrder, listenerRedundantStreamOrder);
		}

		auto const capabilities = index.data(Model::ConnectionCapabilitiesRole).value<Model::ConnectionCapabilities>();
		if (capabilities == Model::ConnectionCapabilities::None)
		{
			drawNotApplicable(painter, option.rect);
			return;
		}

		auto const isRedundantSummary = talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInput;
		// Draw redundant symbol when both nodes are redundant streams, or one is redundant stream and the other is redundant node
		auto const isRedundantSymbol = (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInputStream) || (talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInputStream) || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInput);

		// Connected
		if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::Connected))
		{
			if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::WrongDomain))
			{
				if (isRedundantSummary)
				{
					drawErrorConnectedRedundantNode(painter, option.rect);
				}
				else
				{
					drawWrongDomainConnectedStream(painter, option.rect, isRedundantSymbol);
				}
			}
			else if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::WrongFormat))
			{
				if (isRedundantSummary)
				{
					drawErrorConnectedRedundantNode(painter, option.rect);
				}
				else
				{
					drawWrongFormatConnectedStream(painter, option.rect, isRedundantSymbol);
				}
			}
			else if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::InterfaceDown))
			{
				// Interface down might not be an error, so don't use drawErrorConnectedRedundantNode even if isRedundantSummary is true
				drawConnectedInterfaceDownStream(painter, option.rect, isRedundantSymbol);
			}
			else
			{
				drawConnectedStream(painter, option.rect, isRedundantSymbol);
			}
		}
		// Fast connecting
		else if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::FastConnecting))
		{
			if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::WrongDomain))
			{
				drawWrongDomainFastConnectingStream(painter, option.rect, isRedundantSymbol);
			}
			else if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::WrongFormat))
			{
				drawWrongFormatFastConnectingStream(painter, option.rect, isRedundantSymbol);
			}
			else
			{
				drawFastConnectingStream(painter, option.rect, isRedundantSymbol);
			}
		}
		// Partially connected
		else if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::PartiallyConnected))
		{
			AVDECC_ASSERT(isRedundantSummary, "This case should only be for Redundant Summary intersection");
			drawPartiallyConnectedRedundantNode(painter, option.rect);
		}
		// Not connected
		else
		{
			if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::WrongDomain))
			{
				if (isRedundantSummary)
				{
					drawErrorNotConnectedRedundantNode(painter, option.rect);
				}
				else
				{
					drawWrongDomainNotConnectedStream(painter, option.rect, isRedundantSymbol);
				}
			}
			else if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::WrongFormat))
			{
				if (isRedundantSummary)
				{
					drawErrorNotConnectedRedundantNode(painter, option.rect);
				}
				else
				{
					drawWrongFormatNotConnectedStream(painter, option.rect, isRedundantSymbol);
				}
			}
			else if (la::avdecc::utils::hasFlag(capabilities, Model::ConnectionCapabilities::InterfaceDown))
			{
				// Interface down might not be an error, so don't use drawErrorNotConnectedRedundantNode even if isRedundantSummary is true
				drawNotConnectedInterfaceDownStream(painter, option.rect, isRedundantSymbol);
			}
			else
			{
				drawNotConnectedStream(painter, option.rect, isRedundantSymbol);
			}
		}
	}
}

QSize ItemDelegate::sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	return {};
}

} // namespace connectionMatrix
