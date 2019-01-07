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

#include "connectionMatrix/legend.hpp"
#include "connectionMatrix/paintHelper.hpp"
#include "internals/config.hpp"

#include <QDialog>
#include <QLabel>
#include <QPainter>
#include <QShortcut>

namespace connectionMatrix
{
QString headerTitle(Qt::Orientation const orientation, bool const isTransposed)
{
	QStringList headers{ { "Talkers", "Listeners" } };
	return headers.at((static_cast<int>(orientation) + (isTransposed ? 0 : 1)) % 2);
}

Legend::Legend(QWidget* parent)
	: QWidget{ parent }
{
	// Because the legend is child of the
	_searchLineEdit.setPlaceholderText("Entity Filter (RegEx)");

	// Layout widgets
	_layout.addWidget(&_buttonContainer, 0, 0);
	_layout.addWidget(&_horizontalPlaceholder, 1, 0);
	_layout.addWidget(&_verticalPlaceholder, 0, 1);
	_layout.setSpacing(2);

	_buttonContainer.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	_buttonContainerLayout.setContentsMargins(2, 6, 2, 6);
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
			QDialog dialog;
			QVBoxLayout layout{ &dialog };

			using DrawFunctionType = std::function<void(QPainter*, QRect const&)>;
			auto const separatorDrawFunction = [](QPainter*, QRect const&)
			{
			};

			std::vector<std::pair<DrawFunctionType, QString>> drawFunctions{
				{ static_cast<DrawFunctionType>(nullptr), "Shapes:" }, { static_cast<DrawFunctionType>(std::bind(&drawEntityNoConnection, std::placeholders::_1, std::placeholders::_2)), "Entity connection summary (Not working yet)" }, { static_cast<DrawFunctionType>(std::bind(&drawNotConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connection status for a Simple or Redundant stream" }, { static_cast<DrawFunctionType>(std::bind(&drawNotConnectedStream, std::placeholders::_1, std::placeholders::_2, true)), "Connection status for the individual stream of a Redundant Stream Pair" }, { static_cast<DrawFunctionType>(nullptr), "" }, { static_cast<DrawFunctionType>(nullptr), "Connection status color code:" },
				{ static_cast<DrawFunctionType>(std::bind(&drawNotConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connectable without detectable error" }, { static_cast<DrawFunctionType>(std::bind(&drawWrongDomainNotConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connectable but incompatible AVB domain" }, { static_cast<DrawFunctionType>(std::bind(&drawWrongFormatNotConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connectable but incompatible stream format" }, { static_cast<DrawFunctionType>(std::bind(&drawNotConnectedInterfaceDownStream, std::placeholders::_1, std::placeholders::_2, false)), "Connectable but at least one Network Interface is down" },
				{ static_cast<DrawFunctionType>(std::bind(&drawErrorNotConnectedRedundantNode, std::placeholders::_1, std::placeholders::_2)), "Connectable Redundant Stream Pair but at least one error detected" }, { static_cast<DrawFunctionType>(std::bind(&drawConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connected and no detectable error found" }, { static_cast<DrawFunctionType>(std::bind(&drawWrongDomainConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connected but incompatible AVB domain" }, { static_cast<DrawFunctionType>(std::bind(&drawWrongFormatConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connected but incompatible stream format" },
				{ static_cast<DrawFunctionType>(std::bind(&drawConnectedInterfaceDownStream, std::placeholders::_1, std::placeholders::_2, false)), "Connected but Network Interface is down" }, { static_cast<DrawFunctionType>(std::bind(&drawPartiallyConnectedRedundantNode, std::placeholders::_1, std::placeholders::_2)), "Partially connected Redundant Stream Pair" }, { static_cast<DrawFunctionType>(std::bind(&drawErrorConnectedRedundantNode, std::placeholders::_1, std::placeholders::_2)), "Redundant Stream Pair connected but at least one error detected" },
				//{ static_cast<DrawFunctionType>(std::bind(&drawFastConnectingStream, std::placeholders::_1, std::placeholders::_2, false)), "Listener trying to fast connect" },
				//{ static_cast<DrawFunctionType>(std::bind(&drawWrongDomainFastConnectingStream, std::placeholders::_1, std::placeholders::_2, false)), "Listener trying to fast connect (incompatible AVB domain)" },
				//{ static_cast<DrawFunctionType>(std::bind(&drawWrongFormatFastConnectingStream, std::placeholders::_1, std::placeholders::_2, false)), "Listener trying to fast connect (incompatible stream format)" },
			};

			class IconDrawer : public QWidget
			{
			public:
				IconDrawer(DrawFunctionType const& func)
					: _drawFunction(func)
				{
				}

			private:
				virtual void paintEvent(QPaintEvent*) override
				{
					QPainter painter{ this };
					_drawFunction(&painter, rect());
				}

				DrawFunctionType const& _drawFunction;
			};

			for (auto& drawFunction : drawFunctions)
			{
				auto* hlayout = new QHBoxLayout;
				auto const& iconDrawFunction = drawFunction.first;
				if (iconDrawFunction)
				{
					auto* icon = new IconDrawer{ iconDrawFunction };
					icon->setFixedSize(20, 20);
					hlayout->addWidget(icon);
				}
				auto* label = new QLabel{ drawFunction.second };
				auto font = label->font();
				if (!iconDrawFunction)
				{
					font.setBold(true);
				}
				font.setStyleStrategy(QFont::PreferAntialias);
				label->setFont(font);
				hlayout->addWidget(label);
				layout.addLayout(hlayout);
			}

			QPushButton closeButton{ "Close" };
			connect(&closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
			layout.addWidget(&closeButton);

			dialog.setWindowTitle(hive::internals::applicationShortName + " - " + "Connection matrix legend");
			dialog.exec();
		});

	connect(&_searchLineEdit, &QLineEdit::textChanged, this, &Legend::filterChanged);

	auto* searchShortcut = new QShortcut{ QKeySequence::Find, this };
	connect(searchShortcut, &QShortcut::activated, this,
		[this]()
		{
			_searchLineEdit.setFocus(Qt::MouseFocusReason);
			_searchLineEdit.selectAll();
		});
}

void Legend::setTransposed(bool const isTransposed)
{
	_isTransposed = isTransposed;
	repaint();
}

bool Legend::isTransposed() const
{
	return _isTransposed;
}

void Legend::paintEvent(QPaintEvent*)
{
	QPainter painter{ this };

	// Whole section
	{
		painter.fillRect(geometry(), QColor("#F5F5F5"));
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

} // namespace connectionMatrix
