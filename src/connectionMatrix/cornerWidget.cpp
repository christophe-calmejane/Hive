/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <hive/modelsLibrary/controllerManager.hpp>

#include <QShortcut>
#include <QPainter>
#include <QApplication>
#include <QMessageBox>

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
	_searchLineEdit.setPlaceholderText("Entity Name Filter (RegEx)");

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
	_layout.addWidget(&_centerContainer, 0, 0);
	_layout.addLayout(&_horizontalLayout, 1, 0);
	_layout.addLayout(&_verticalLayout, 0, 1);
	_layout.setSpacing(2);

	_centerContainer.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	_centerContainerLayout.setContentsMargins(2, 6, 2, 6);
	_centerContainerLayout.addWidget(&_title);
	_centerContainerLayout.addStretch();
	_centerContainerLayout.addWidget(&_legendButton);
	_centerContainerLayout.addWidget(&_searchLineEdit);
	_centerContainerLayout.addWidget(&_removeAllConnectionsButton, 0, Qt::AlignHCenter);
	_centerContainerLayout.addStretch();

	_layout.setRowStretch(0, 1);
	_layout.setRowStretch(1, 0);

	_layout.setColumnStretch(0, 1);
	_layout.setColumnStretch(1, 0);

	_horizontalPlaceholder.setFixedHeight(20);
	_verticalPlaceholder.setFixedWidth(20);
	_title.setAlignment(Qt::AlignHCenter);
	_removeAllConnectionsButton.setToolTip(QCoreApplication::translate("CornerWidget", "Remove all active connections", nullptr));
	// Prevent the button from expanding
	_removeAllConnectionsButton.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	// Connect buttons
	connect(&_legendButton, &qtMate::widgets::FlatIconButton::clicked, this,
		[this]()
		{
			auto dialog = LegendDialog{ _colorName, _isTransposed };
			dialog.exec();
		});
	connect(&_removeAllConnectionsButton, &qtMate::widgets::FlatIconButton::clicked, this,
		[this]()
		{
			if (QMessageBox::Yes == QMessageBox::question(this, {}, "Are you sure you want to remove all established connections?"))
			{
				auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
				manager.foreachEntity(
					[&manager](la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity const& entity)
					{
						try
						{
							auto const& configNode = entity.getCurrentConfigurationNode();
							for (auto const& [streamIndex, streamNode] : configNode.streamInputs)
							{
								auto const& connectionInfo = streamNode.dynamicModel.connectionInfo;
								if (connectionInfo.state != la::avdecc::entity::model::StreamInputConnectionInfo::State::NotConnected)
								{
									// Ignore result handler
									manager.disconnectStream(connectionInfo.talkerStream.entityID, connectionInfo.talkerStream.streamIndex, entityID, streamIndex,
										[](la::avdecc::UniqueIdentifier const talkerEntityID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const listenerEntityID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex, la::avdecc::entity::ControllerEntity::ControlStatus const status)
										{
										});
								}
							}
						}
						catch (...)
						{
						}
					});
			}
		});

	connect(&_searchLineEdit, &QLineEdit::textChanged, this, &CornerWidget::filterChanged);

	connect(&_horizontalExpandButton, &QPushButton::clicked, this, &CornerWidget::verticalExpandClicked);
	connect(&_horizontalCollapseButton, &QPushButton::clicked, this, &CornerWidget::verticalCollapseClicked);

	connect(&_verticalExpandButton, &QPushButton::clicked, this, &CornerWidget::horizontalExpandClicked);
	connect(&_verticalCollapseButton, &QPushButton::clicked, this, &CornerWidget::horizontalCollapseClicked);

	// Shortcuts
	auto* searchShortcut = new QShortcut{ QKeySequence::FindNext, this };
	connect(searchShortcut, &QShortcut::activated, this,
		[this]()
		{
			_searchLineEdit.setFocus(Qt::MouseFocusReason);
			_searchLineEdit.selectAll();
		});
	auto* expandListeners = new QShortcut{ QKeySequence{ "Ctrl+L" }, this };
	connect(expandListeners, &QShortcut::activated, this,
		[this]()
		{
			if (_isTransposed)
			{
				_horizontalExpandButton.click();
			}
			else
			{
				_verticalExpandButton.click();
			}
		});
	auto* collapseListeners = new QShortcut{ QKeySequence{ "Ctrl+Shift+L" }, this };
	connect(collapseListeners, &QShortcut::activated, this,
		[this]()
		{
			if (_isTransposed)
			{
				_horizontalCollapseButton.click();
			}
			else
			{
				_verticalCollapseButton.click();
			}
		});
	auto* expandTalkers = new QShortcut{ QKeySequence{ "Ctrl+T" }, this };
	connect(expandTalkers, &QShortcut::activated, this,
		[this]()
		{
			if (_isTransposed)
			{
				_verticalExpandButton.click();
			}
			else
			{
				_horizontalExpandButton.click();
			}
		});
	auto* collapseTalkers = new QShortcut{ QKeySequence{ "Ctrl+Shift+T" }, this };
	connect(collapseTalkers, &QShortcut::activated, this,
		[this]()
		{
			if (_isTransposed)
			{
				_verticalCollapseButton.click();
			}
			else
			{
				_horizontalCollapseButton.click();
			}
		});

	// Configure settings observers
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->registerSettingObserver(settings::ConnectionMatrix_ChannelMode.name, this);
}

CornerWidget::~CornerWidget()
{
	// Remove settings observers
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->unregisterSettingObserver(settings::ConnectionMatrix_ChannelMode.name, this);
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

QLineEdit* CornerWidget::filterLineEdit() noexcept
{
	return &_searchLineEdit;
}

void CornerWidget::paintEvent(QPaintEvent*)
{
	QPainter painter{ this };

	// Whole section
	{
		auto brush = qtMate::material::color::brush(qtMate::material::color::Name::Gray, qtMate::material::color::isDarkColorScheme() ? qtMate::material::color::Shade::Shade900 : qtMate::material::color::Shade::Shade100);
		painter.fillRect(geometry(), brush);
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
		_title.setText(channelMode ? "<b>Channel Connections</b>" : "<b>Stream Connections</b>");
	}
}

} // namespace connectionMatrix
