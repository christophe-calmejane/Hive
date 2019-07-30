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

#include "networkInterfaceModel.hpp"
#include "toolkit/material/color.hpp"
#include "avdecc/helper.hpp"
#include "errorItemDelegate.hpp"

class NetworkInterfaceModelPrivate : public QObject, private la::avdecc::networkInterface::NetworkInterfaceObserver
{
public:
	NetworkInterfaceModelPrivate(NetworkInterfaceModel* q)
		: q_ptr{ q }
	{
	}

private:
	QModelIndex indexOf(std::string const& id) const noexcept
	{
		Q_Q(const NetworkInterfaceModel);

		auto const it = std::find_if(_interfaces.begin(), _interfaces.end(),
			[id](auto const& i)
			{
				return i.id == id;
			});
		if (it == _interfaces.end())
		{
			return {};
		}
		return q->createIndex(static_cast<int>(std::distance(_interfaces.begin(), it)), 0);
	}

	// la::avdecc::networkInterface::NetworkInterfaceObserver overrides
	void onInterfaceAdded(la::avdecc::networkInterface::Interface const& intfc) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, intfc = intfc]()
			{
				Q_Q(NetworkInterfaceModel);

				// Only use non-virtual, enabled, Ethernet interfaces
				if (!intfc.isVirtual)
				{
					auto const count = q->rowCount();
					emit q->beginInsertRows({}, count, count);
					_interfaces.push_back(Data{ intfc.id, intfc.alias, intfc.isEnabled, intfc.isConnected, intfc.type });
					emit q->endInsertRows();
				}
			});
	}
	void onInterfaceRemoved(la::avdecc::networkInterface::Interface const& intfc) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id]()
			{
				Q_Q(NetworkInterfaceModel);

				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Remove it
					auto const idx = index.row();
					emit q->beginRemoveRows({}, idx, idx);
					_interfaces.remove(idx);
					emit q->endRemoveRows();
				}
			});
	}
	void onInterfaceEnabledStateChanged(la::avdecc::networkInterface::Interface const& intfc, bool const isEnabled) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, isEnabled]()
			{
				Q_Q(NetworkInterfaceModel);

				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Change data and emit signal
					auto const idx = index.row();
					_interfaces[idx].isEnabled = isEnabled;
					auto const index = q->createIndex(idx, 0);
					emit q->dataChanged(index, index);
				}
			},
			Qt::QueuedConnection);
	}
	void onInterfaceConnectedStateChanged(la::avdecc::networkInterface::Interface const& intfc, bool const isConnected) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, isConnected]()
			{
				Q_Q(NetworkInterfaceModel);

				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Change data and emit signal
					auto const idx = index.row();
					_interfaces[idx].isConnected = isConnected;
					auto const index = q->createIndex(idx, 0);
					emit q->dataChanged(index, index);
				}
			},
			Qt::QueuedConnection);
	}

	NetworkInterfaceModel* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(NetworkInterfaceModel);

	struct Data
	{
		std::string id{};
		std::string name{};
		bool isEnabled{ false };
		bool isConnected{ false };
		la::avdecc::networkInterface::Interface::Type interfaceType{ la::avdecc::networkInterface::Interface::Type::None };
	};

	// Private Members
	QVector<Data> _interfaces{};
	DECLARE_AVDECC_OBSERVER_GUARD(NetworkInterfaceModelPrivate);
};

NetworkInterfaceModel::NetworkInterfaceModel(QObject* parent)
	: QAbstractListModel{ parent }
	, d_ptr{ new NetworkInterfaceModelPrivate{ this } }
{
	la::avdecc::networkInterface::registerObserver(d_ptr.get());
}

NetworkInterfaceModel::~NetworkInterfaceModel() = default;

bool NetworkInterfaceModel::isEnabled(QString const& id) const noexcept
{
	Q_D(const NetworkInterfaceModel);

	auto const it = std::find_if(d->_interfaces.begin(), d->_interfaces.end(),
		[id = id.toStdString()](auto const& i)
		{
			return i.id == id;
		});
	if (it == d->_interfaces.end())
	{
		return false;
	}
	return it->isEnabled;
}

la::avdecc::networkInterface::Interface::Type NetworkInterfaceModel::interfaceType(QModelIndex const& index) const noexcept
{
	Q_D(const NetworkInterfaceModel);

	auto const idx = index.row();
	if (idx >= 0 && idx < rowCount())
	{
		return d->_interfaces.at(idx).interfaceType;
	}

	return la::avdecc::networkInterface::Interface::Type::None;
}

int NetworkInterfaceModel::rowCount(QModelIndex const& parent) const
{
	Q_D(const NetworkInterfaceModel);
	return d->_interfaces.count();
}

QVariant NetworkInterfaceModel::data(QModelIndex const& index, int role) const
{
	Q_D(const NetworkInterfaceModel);

	auto const idx = index.row();
	if (idx >= 0 && idx < rowCount())
	{
		switch (role)
		{
			case Qt::DisplayRole:
				return QString::fromStdString(d->_interfaces.at(idx).name);
			case ErrorItemDelegate::ErrorRole:
			{
				auto const& intfc = d->_interfaces.at(idx);
				return intfc.isEnabled && !intfc.isConnected;
			}
			case Qt::ForegroundRole:
			{
				auto const& intfc = d->_interfaces.at(idx);
				if (!intfc.isEnabled)
				{
					return qt::toolkit::material::color::value(qt::toolkit::material::color::Name::Gray);
				}
				else if (!intfc.isConnected)
				{
					return qt::toolkit::material::color::value(qt::toolkit::material::color::Name::Red);
				}
				else
				{
					return QColor{ Qt::black };
				}
			}
			case Qt::UserRole:
				return QString::fromStdString(d->_interfaces.at(idx).id);
			case Qt::WhatsThisRole:
			{
				auto const& intfc = d->_interfaces.at(idx);
				return QString{ "%1#%2" }.arg(la::avdecc::utils::to_integral(intfc.interfaceType)).arg(intfc.id.c_str());
			}
			case Qt::DecorationRole:
				return avdecc::helper::interfaceTypeIcon(d->_interfaces.at(idx).interfaceType);
		}
	}
	return {};
}

Qt::ItemFlags NetworkInterfaceModel::flags(QModelIndex const& index) const
{
	Q_D(const NetworkInterfaceModel);

	auto const idx = index.row();
	if (idx >= 0 && idx < rowCount())
	{
		if (d->_interfaces.at(idx).isEnabled)
		{
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		}
	}
	return {};
}
