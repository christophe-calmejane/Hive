/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "connectionMatrix/cornerWidget.hpp"
#include "connectionMatrix/legendDialog.hpp"
#include <QShortcut>
#include <QPainter>

namespace connectionMatrix
{
QString headerTitle(Qt::Orientation const orientation, bool const isTransposed)
{
	QStringList headers{ { "Talkers", "Listeners" } };
	return headers.at((static_cast<int>(orientation) + (isTransposed ? 0 : 1)) % 2);
}

CornerWidget::CornerWidget(QWidget* parent)
	: QWidget{ parent }
{
	_searchLineEdit.setPlaceholderText("Entity Filter (RegEx)");

	_horizontalExpandButton.setToolTip("Expand");
	_horizontalCollapseButton.setToolTip("Collapse");

	_verticalCollapseButton.setToolTip("Collapse");
	_verticalExpandButton.setToolTip("Expand");

	_horizontalLayout.addWidget(&_horizontalExpandButton);
	_horizontalLayout.addWidget(&_horizontalCollapseButton);
	_horizontalLayout.addWidget(&_horizontalPlaceholder, 1);

	_verticalLayout.addWidget(&_verticalPlaceholder, 1);
	_verticalLayout.addWidget(&_verticalCollapseButton);
	_verticalLayout.addWidget(&_verticalExpandButton);

	// Layout widgets
	_layout.addWidget(&_buttonContainer, 0, 0);
	_layout.addLayout(&_horizontalLayout, 1, 0);
	_layout.addLayout(&_verticalLayout, 0, 1);
	_layout.setSpacing(2);

	_buttonContainer.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	_buttonContainerLayout.setContentsMargins(2, 6, 2, 6);
	_buttonContainerLayout.addWidget(&_title);
	_buttonContainerLayout.addStretch();
	_buttonContainerLayout.addWidget(&_button);
	_buttonContainerLayout.addWidget(&_searchLineEdit);
	_buttonContainerLayout.addStretch();

	_layout.setRowStretch(0, 1);
	_layout.setRowStretch(1, 0);

	_layout.setColumnStretch(0, 1);
	_layout.setColumnStretch(1, 0);

	_horizontalPlaceholder.setFixedHeight(20);
	_verticalPlaceholder.setFixedWidth(20);

	// Connect button
	connect(&_button, &QPushButton::clicked, this,
		[this]()
		{
			auto dialog = LegendDialog{ _colorName, _isTransposed };
			dialog.exec();
		});

	connect(&_searchLineEdit, &QLineEdit::textChanged, this, &CornerWidget::filterChanged);

	connect(&_horizontalExpandButton, &QPushButton::clicked, this, &CornerWidget::verticalExpandClicked);
	connect(&_horizontalCollapseButton, &QPushButton::clicked, this, &CornerWidget::verticalCollapseClicked);

	connect(&_verticalExpandButton, &QPushButton::clicked, this, &CornerWidget::horizontalExpandClicked);
	connect(&_verticalCollapseButton, &QPushButton::clicked, this, &CornerWidget::horizontalCollapseClicked);

	auto* searchShortcut = new QShortcut{ QKeySequence::Find, this };
	connect(searchShortcut, &QShortcut::activated, this,
		[this]()
		{
			_searchLineEdit.setFocus(Qt::MouseFocusReason);
			_searchLineEdit.selectAll();
		});

	// Configure settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.registerSettingObserver(settings::ConnectionMatrix_ChannelMode.name, this);
}

CornerWidget::~CornerWidget()
{
	// Remove settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.unregisterSettingObserver(settings::ConnectionMatrix_ChannelMode.name, this);
}

void CornerWidget::setColor(qtMate::material::color::Name const name)
{
	_colorName = name;
	update();
}

void CornerWidget::setTransposed(bool const isTransposed)
{
	_isTransposed = isTransposed;
	update();
}

bool CornerWidget::isTransposed() const
{
	return _isTransposed;
}

QString CornerWidget::filterText() const
{
	return _searchLineEdit.text();
}

void CornerWidget::paintEvent(QPaintEvent*)
{
	QPainter painter{ this };

	// Whole section
	{
		painter.fillRect(geometry(), 0xf5f5f5);
	}

	// Horizontal section
	{
		painter.save();

		painter.setRenderHint(QPainter::Antialiasing, true);
		auto font = painter.font();
		font.setBold(true);
		painter.setFont(font);

		QTextOption options;
		options.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
		painter.drawText(_horizontalPlaceholder.geometry(), headerTitle(Qt::Horizontal, _isTransposed), options);

		painter.restore();
	}

	// Vertical section
	{
		painter.save();

		auto rect = _verticalPlaceholder.geometry();
		painter.translate(rect.bottomLeft());
		painter.rotate(-90);
		auto const drawRect = QRect(0, 0, rect.height(), rect.width());

		painter.setRenderHint(QPainter::Antialiasing, true);
		auto font = painter.font();
		font.setBold(true);
		painter.setFont(font);

		QTextOption options;
		options.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
		painter.drawText(drawRect, headerTitle(Qt::Vertical, _isTransposed), options);

		painter.restore();
	}
}

void CornerWidget::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::ConnectionMatrix_ChannelMode.name)
	{
		auto const channelMode = value.toBool();
		_title.setText(channelMode ? "<b>Channel Based Connections</b>" : "<b>Stream Based Connections</b>");
	}
}

} // namespace connectionMatrix
