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

#include "toolkit/flatIconButton.hpp"
#include "settingsManager/settings.hpp"

#include <QWidget>
#include <QLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>

namespace connectionMatrix
{
class CornerWidget : public QWidget, public settings::SettingsManager::Observer
{
	Q_OBJECT
public:
	CornerWidget(QWidget* parent = nullptr);
	~CornerWidget();

	void setTransposed(bool const isTransposed);
	bool isTransposed() const;

	QString filterText() const;

signals:
	void filterChanged(QString const& filter);

	void horizontalExpandClicked();
	void horizontalCollapseClicked();

	void verticalExpandClicked();
	void verticalCollapseClicked();

private:
	virtual void paintEvent(QPaintEvent*) override;

	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override;

private:
	QGridLayout _layout{ this };
	QWidget _buttonContainer{ this };
	QLabel _title{ &_buttonContainer };
	QVBoxLayout _buttonContainerLayout{ &_buttonContainer };
	QPushButton _button{ "Show Legend", &_buttonContainer };
	QLineEdit _searchLineEdit{ &_buttonContainer };

	QHBoxLayout _horizontalLayout;
	qt::toolkit::FlatIconButton _horizontalExpandButton{ "Material Icons", "expand_more" };
	QWidget _horizontalPlaceholder{ this };
	qt::toolkit::FlatIconButton _horizontalCollapseButton{ "Material Icons", "expand_less" };

	QVBoxLayout _verticalLayout;
	qt::toolkit::FlatIconButton _verticalCollapseButton{ "Material Icons", "chevron_left" };
	QWidget _verticalPlaceholder{ this };
	qt::toolkit::FlatIconButton _verticalExpandButton{ "Material Icons", "chevron_right" };

	bool _isTransposed{ false };
};


} // namespace connectionMatrix
