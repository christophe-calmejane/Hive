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
#include <la/avdecc/networkInterfaceHelper.hpp>
#include "toolkit/material/color.hpp"

class NetworkInterfaceModelPrivate : public QObject, private la::avdecc::networkInterface::NetworkInterfaceObserver
{
public:
	NetworkInterfaceModelPrivate(NetworkInterfaceModel* q)
		: q_ptr{ q }
	{
	}

private:
	// la::avdecc::networkInterface::NetworkInterfaceObserver overrides
	void onInterfaceAdded(la::avdecc::networkInterface::Interface const& intfc) noexcept
	{
		QMetaObject::invokeMethod(this,
			[this, intfc]()
			{
				Q_Q(NetworkInterfaceModel);

				// Only use non-virtual, enabled, Ethernet interfaces
				if (intfc.type == la::avdecc::networkInterface::Interface::Type::Ethernet && !intfc.isVirtual)
				{
					auto const count = q->rowCount();
					emit q->beginInsertRows({}, count, count);
					_interfaces.push_back(Data{ intfc.id, intfc.alias, intfc.isEnabled, intfc.isConnected });
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
				auto const index = q->indexOf(id);
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
				auto const index = q->indexOf(id);
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
				auto const index = q->indexOf(id);
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

QModelIndex NetworkInterfaceModel::indexOf(std::string const& id) const noexcept
{
	Q_D(const NetworkInterfaceModel);

	auto const it = std::find_if(d->_interfaces.begin(), d->_interfaces.end(),
		[id](auto const& i)
		{
			return i.id == id;
		});
	if (it == d->_interfaces.end())
	{
		return {};
	}
	return createIndex(static_cast<int>(std::distance(d->_interfaces.begin(), it)), 0);
}

bool NetworkInterfaceModel::isEnabled(QModelIndex const& index) const noexcept
{
	return (flags(index) & Qt::ItemIsEnabled) == Qt::ItemIsEnabled;
}

int NetworkInterfaceModel::rowCount(QModelIndex const& parent) const
{
	Q_D(const NetworkInterfaceModel);
	return static_cast<int>(d->_interfaces.size());
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
			{
				return QString::fromStdString(d->_interfaces.at(idx).id);
			}
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
