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

#include "hive/modelsLibrary/networkInterfaceModel.hpp"

#include <vector>

namespace hive
{
namespace modelsLibrary
{
class NetworkInterfaceModel::pImpl : public QObject, public la::avdecc::networkInterface::NetworkInterfaceObserver
{
public:
	pImpl(Model* const model, QObject* parent = nullptr)
		: _model(model)
		, QObject(parent)
	{
	}

	std::optional<std::reference_wrapper<NetworkInterface const>> networkInterface(QModelIndex const& index) const noexcept
	{
		auto const rowIndex = index.row();

		if (isIndexValid(rowIndex))
		{
			return std::cref(_interfaces[rowIndex]);
		}

		return {};
	}

	std::optional<std::reference_wrapper<NetworkInterface const>> networkInterface(std::string const& id) const noexcept
	{
		auto const index = indexOf(id);
		if (index.isValid())
		{
			return networkInterface(index);
		}

		return {};
	}

	int rowCount() const noexcept
	{
		return static_cast<int>(_interfaces.size());
	}

private:
	inline bool isIndexValid(int const index) const noexcept
	{
		return index >= 0 && index < static_cast<int>(_interfaces.size());
	}

	QModelIndex indexOf(std::string const& id) const noexcept
	{
		auto const it = std::find_if(_interfaces.begin(), _interfaces.end(),
			[id](auto const& i)
			{
				return i.id == id;
			});
		if (it == _interfaces.end())
		{
			return {};
		}
		return _model->createIndex(static_cast<int>(std::distance(_interfaces.begin(), it)), 0);
	}

	// la::avdecc::networkInterface::NetworkInterfaceObserver overrides
	virtual void onInterfaceAdded(la::avdecc::networkInterface::Interface const& intfc) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, intfc = intfc]()
			{
				// Only use non-virtual, enabled, Ethernet interfaces
				if (!intfc.isVirtual)
				{
					auto const count = _model->rowCount();
					emit _model->beginInsertRows({}, count, count);
					_interfaces.push_back(NetworkInterface{ intfc.id, intfc.alias, intfc.isEnabled, intfc.isConnected, intfc.type });
					emit _model->endInsertRows();
				}
			});
	}
	virtual void onInterfaceRemoved(la::avdecc::networkInterface::Interface const& intfc) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id]()
			{
				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Remove it
					auto const idx = index.row();
					emit _model->beginRemoveRows({}, idx, idx);
					_interfaces.erase(_interfaces.begin() + idx);
					emit _model->endRemoveRows();
				}
			});
	}
	virtual void onInterfaceEnabledStateChanged(la::avdecc::networkInterface::Interface const& intfc, bool const isEnabled) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, isEnabled]()
			{
				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Change data and emit signal
					auto const idx = index.row();
					_interfaces[idx].isEnabled = isEnabled;
					_model->enabledStateChanged(index, isEnabled);
				}
			},
			Qt::QueuedConnection);
	}
	virtual void onInterfaceConnectedStateChanged(la::avdecc::networkInterface::Interface const& intfc, bool const isConnected) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, isConnected]()
			{
				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Change data and emit signal
					auto const idx = index.row();
					_interfaces[idx].isConnected = isConnected;
					_model->connectedStateChanged(index, isConnected);
				}
			},
			Qt::QueuedConnection);
	}
	virtual void onInterfaceAliasChanged(la::avdecc::networkInterface::Interface const& intfc, std::string const& alias) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, alias]()
			{
				// Search the element
				auto const index = indexOf(id);
				if (index.isValid())
				{
					// Change data and emit signal
					auto const idx = index.row();
					_interfaces[idx].name = alias;
					_model->nameChanged(index, alias);
				}
			},
			Qt::QueuedConnection);
	}
	virtual void onInterfaceIPAddressInfosChanged(la::avdecc::networkInterface::Interface const& /*intfc*/, la::avdecc::networkInterface::Interface::IPAddressInfos const& /*ipAddressInfos*/) noexcept override
	{

	}
	virtual void onInterfaceGateWaysChanged(la::avdecc::networkInterface::Interface const& /*intfc*/, la::avdecc::networkInterface::Interface::Gateways const& /*gateways*/) noexcept override
	{

	}

	// Private Members
	Model* _model{ nullptr };
	std::vector<NetworkInterface> _interfaces{};
	DECLARE_AVDECC_OBSERVER_GUARD(pImpl);
};

NetworkInterfaceModel::NetworkInterfaceModel(Model* const model, QObject* parent)
	: _pImpl{ std::make_unique<pImpl>(model, parent) }
{
	la::avdecc::networkInterface::registerObserver(_pImpl.get());
}

NetworkInterfaceModel::~NetworkInterfaceModel() = default;

std::optional<std::reference_wrapper<NetworkInterfaceModel::NetworkInterface const>> NetworkInterfaceModel::networkInterface(QModelIndex const& index) const noexcept
{
	return _pImpl->networkInterface(index);
}

std::optional<std::reference_wrapper<NetworkInterfaceModel::NetworkInterface const>> NetworkInterfaceModel::networkInterface(std::string const& id) const noexcept
{
	return _pImpl->networkInterface(id);
}

int NetworkInterfaceModel::rowCount() const noexcept
{
	return _pImpl->rowCount();
}
} // namespace modelsLibrary
} // namespace hive
