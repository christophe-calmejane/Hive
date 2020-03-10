/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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
#include <QVector>
#include "toolkit/material/color.hpp"

namespace connectionMatrix
{
class HeaderView final : public QHeaderView
{
public:
	struct SectionState
	{
		bool expanded{ true };
		bool visible{ true };
	};

	HeaderView(bool const isListenersHeader, Qt::Orientation const orientation, QWidget* parent = nullptr);

	bool isListenersHeader() const noexcept;

	void setAlwaysShowArrowTip(bool const show);
	void setAlwaysShowArrowEnd(bool const show);
	void setTransposed(bool const isTransposed);
	void setColor(qt::toolkit::material::color::Name const name);

	// Retrieves the current sectionState for each section
	QVector<SectionState> saveSectionState() const;

	// Applies sectionState, sectionState count must match the number of section
	void restoreSectionState(QVector<SectionState> const& sectionState);

	// Set filter regexp that applies to entity
	// i.e the complete entity hierarchy is visible (with respect of the current collapse/expand state) if the entity name matches pattern
	void setFilterPattern(QRegExp const& pattern);

	// Expand all child nodes of each entity
	void expandAll();

	// Collapse all child nodes of each entity
	void collapseAll();

private:
	void handleSectionClicked(int logicalIndex);
	void handleSectionInserted(QModelIndex const& parent, int first, int last);
	void handleSectionRemoved(QModelIndex const& parent, int first, int last);
	void handleModelReset();
	void handleEditMappingsClicked(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamPortType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::StreamIndex const streamIndex);
	void updateSectionVisibility(int const logicalIndex);
	void applyFilterPattern();

	// QHeaderView overrides
	virtual void setModel(QAbstractItemModel* model) override;
	virtual QSize sizeHint() const override;
	virtual void paintSection(QPainter* painter, QRect const& rect, int logicalIndex) const override;
	virtual void contextMenuEvent(QContextMenuEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
	virtual void leaveEvent(QEvent* event) override;

private:
	bool const _isListenersHeader{ false };
	QVector<SectionState> _sectionState;
	QRegExp _pattern;

	bool _alwaysShowArrowTip{ false };
	bool _alwaysShowArrowEnd{ false };
	bool _isTransposed{ false };
	qt::toolkit::material::color::Name _colorName{ qt::toolkit::material::color::DefaultColor };
};

} // namespace connectionMatrix
