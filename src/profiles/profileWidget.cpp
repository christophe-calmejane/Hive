/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "profileWidget.hpp"

#include <QtMate/material/color.hpp>
#include <QtMate/material/colorPalette.hpp>

#include <QGuiApplication>
#include <QStyleHints>
#include <QStyle>
#include <QAccessible>
#include <QAccessibleWidget>

#include <mutex>

namespace profiles
{
namespace
{
/**
* @brief Accessible interface of a ProfileWidget.
* @details The default interface of a QFrame describes a border with a few labels, while the widget is a clickable card.
*          Expose it as a button (named after the profile title, described by the profile description) that assistive
*          technologies can press. The labels being the visual representation of the button (the icon one even displays
*          the name of a glyph of the icons font), they are not exposed as children.
*/
class ProfileWidgetAccessibleInterface final : public QAccessibleWidget
{
public:
	explicit ProfileWidgetAccessibleInterface(ProfileWidget* const widget)
		: QAccessibleWidget{ widget, QAccessible::Button }
	{
	}

	// QAccessibleInterface overrides
	virtual int childCount() const override
	{
		return 0;
	}

	virtual QAccessibleInterface* child(int /*index*/) const override
	{
		return nullptr;
	}

	virtual int indexOfChild(QAccessibleInterface const* /*child*/) const override
	{
		return -1;
	}

	// QAccessibleActionInterface overrides
	virtual QStringList actionNames() const override
	{
		return QStringList{ QAccessibleActionInterface::pressAction() } + QAccessibleWidget::actionNames();
	}

	virtual void doAction(QString const& actionName) override
	{
		if (actionName == QAccessibleActionInterface::pressAction())
		{
			emit static_cast<ProfileWidget*>(widget())->clicked();
			return;
		}
		QAccessibleWidget::doAction(actionName);
	}
};

QAccessibleInterface* profileWidgetAccessibleFactory(QString const& className, QObject* const object)
{
	if (object != nullptr && object->isWidgetType() && className == QLatin1StringView{ ProfileWidget::staticMetaObject.className() })
	{
		return new ProfileWidgetAccessibleInterface{ static_cast<ProfileWidget*>(object) };
	}
	return nullptr;
}
} // namespace

ProfileWidget::ProfileWidget(QString const& title, QString const& description, QString const& icon, QWidget* parent)
	: QFrame{ parent }
{
	// Install the factory of our accessible interface before anything queries the default one (it would be cached)
	static auto s_accessibleFactoryInstalled = std::once_flag{};
	std::call_once(s_accessibleFactoryInstalled,
		[]()
		{
			QAccessible::installFactory(&profileWidgetAccessibleFactory);
		});
	setAccessibleName(title);
	setAccessibleDescription(description);

	_layout.addWidget(&_title, 0, 1);
	_layout.addWidget(&_description, 1, 1);
	_layout.addWidget(&_icon, 0, 0, -1, 1);

	_title.setText(title);
	_title.setStyleSheet("font-size: 20px; font-weight: bold;");

	_description.setText(description);

	_icon.setStyleSheet("font-size: 40px; font-family: 'Material Icons';");
	_icon.setText(icon);
	_icon.setFixedWidth(60);

	auto const setStyle = [this]()
	{
		auto const themeColorIndex = qtMate::material::color::Palette::index(qtMate::material::color::DefaultColor);
		auto const colorName = qtMate::material::color::Palette::name(themeColorIndex);
		auto const themeBackgroundColor = qtMate::material::color::value(colorName);
		auto const themeForegroundTextColor = qtMate::material::color::foregroundValue(colorName);
		auto const complementaryColor = qtMate::material::color::complementaryValue(colorName);
		auto const complementaryForegroundTextColor = qtMate::material::color::foregroundComplementaryValue(colorName);

		auto const style = QString{ R"(
			profiles--ProfileWidget {
				background-color: %1;
				color: %2;
			}
			profiles--ProfileWidget:hover {
				background-color: %3;
				color: red;
			}
			QLabel[hover=true] {
				background-color: %3;
				color: %4;
			}
		)" }
												 .arg(themeBackgroundColor.name())
												 .arg(themeForegroundTextColor.name())
												 .arg(complementaryColor.name())
												 .arg(complementaryForegroundTextColor.name());
		setStyleSheet(style);
	};

	// Set the style
	setStyle();

	// Color Scheme changed
	connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
		[setStyle](Qt::ColorScheme scheme)
		{
			// Update the style
			setStyle();
		});
}

void ProfileWidget::enterEvent(QEnterEvent* event)
{
	// Set the hover property to true
	_title.setProperty("hover", true);
	_title.style()->unpolish(&_title);
	_title.style()->polish(&_title);
	_description.setProperty("hover", true);
	_description.style()->unpolish(&_description);
	_description.style()->polish(&_description);
	_icon.setProperty("hover", true);
	_icon.style()->unpolish(&_icon);
	_icon.style()->polish(&_icon);
}

void ProfileWidget::leaveEvent(QEvent* event)
{
	// Reset the hover property
	_title.setProperty("hover", false);
	_title.style()->unpolish(&_title);
	_title.style()->polish(&_title);
	_description.setProperty("hover", false);
	_description.style()->unpolish(&_description);
	_description.style()->polish(&_description);
	_icon.setProperty("hover", false);
	_icon.style()->unpolish(&_icon);
	_icon.style()->polish(&_icon);
}

void ProfileWidget::mouseReleaseEvent(QMouseEvent* event)
{
	emit clicked();
	QWidget::mouseReleaseEvent(event);
}
} // namespace profiles
