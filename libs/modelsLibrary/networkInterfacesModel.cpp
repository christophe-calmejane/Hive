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

#include "hive/modelsLibrary/networkInterfacesModel.hpp"

#include <la/avdecc/utils.hpp>

#include <vector>

namespace hive
{
namespace modelsLibrary
{
class NetworkInterfacesModel::pImpl : public QObject, public la::networkInterface::NetworkInterfaceHelper::Observer
{
public:
	pImpl(Model* const model, QObject* parent = nullptr)
		: QObject(parent)
		, _model(model)
	{
	}

	std::optional<std::reference_wrapper<NetworkInterface const>> networkInterface(std::size_t const index) const noexcept
	{
		if (isIndexValid(index))
		{
			return std::cref(_interfaces[index]);
		}

		return {};
	}

	std::optional<std::reference_wrapper<NetworkInterface const>> networkInterface(std::string const& id) const noexcept
	{
		if (auto const index = indexOf(id))
		{
			return networkInterface(*index);
		}

		return {};
	}

	std::size_t networkInterfacesCount() const noexcept
	{
		return _interfaces.size();
	}

	void insertOfflineInterface() noexcept
	{
		auto const count = _model->rowCount();
		emit _model->beginInsertRows({}, count, count);
		_interfaces.push_back(NetworkInterface{ OfflineInterfaceName, OfflineInterfaceName, true, true, la::networkInterface::Interface::Type::Loopback });
		emit _model->endInsertRows();
	}

private:
	inline bool isIndexValid(std::size_t const index) const noexcept
	{
		return index < _interfaces.size();
	}

	std::optional<std::size_t> indexOf(std::string const& id) const noexcept
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
		return std::distance(_interfaces.begin(), it);
	}

	// la::avdecc::networkInterface::NetworkInterfaceObserver overrides
	virtual void onInterfaceAdded(la::networkInterface::Interface const& intfc) noexcept override
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
	virtual void onInterfaceRemoved(la::networkInterface::Interface const& intfc) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id]()
			{
				// Search the element
				if (auto const index = indexOf(id))
				{
					// Remove it
					auto const idx = *index;
					emit _model->beginRemoveRows({}, idx, idx);
					_interfaces.erase(_interfaces.begin() + idx);
					emit _model->endRemoveRows();
				}
			});
	}
	virtual void onInterfaceEnabledStateChanged(la::networkInterface::Interface const& intfc, bool const isEnabled) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, isEnabled]()
			{
				// Search the element
				if (auto const index = indexOf(id))
				{
					// Change data and emit signal
					auto const idx = *index;
					_interfaces[idx].isEnabled = isEnabled;
					la::avdecc::utils::invokeProtectedMethod(&Model::enabledStateChanged, _model, idx, isEnabled);
				}
			});
	}
	virtual void onInterfaceConnectedStateChanged(la::networkInterface::Interface const& intfc, bool const isConnected) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, isConnected]()
			{
				// Search the element
				if (auto const index = indexOf(id))
				{
					// Change data and emit signal
					auto const idx = *index;
					_interfaces[idx].isConnected = isConnected;
					la::avdecc::utils::invokeProtectedMethod(&Model::connectedStateChanged, _model, idx, isConnected);
				}
			});
	}
	virtual void onInterfaceAliasChanged(la::networkInterface::Interface const& intfc, std::string const& alias) noexcept override
	{
		QMetaObject::invokeMethod(this,
			[this, id = intfc.id, alias]()
			{
				// Search the element
				if (auto const index = indexOf(id))
				{
					// Change data and emit signal
					auto const idx = *index;
					_interfaces[idx].name = alias;
					la::avdecc::utils::invokeProtectedMethod(&Model::nameChanged, _model, idx, alias);
				}
			});
	}
	virtual void onInterfaceIPAddressInfosChanged(la::networkInterface::Interface const& /*intfc*/, la::networkInterface::Interface::IPAddressInfos const& /*ipAddressInfos*/) noexcept override
	{
		// Don't care
	}
	virtual void onInterfaceGateWaysChanged(la::networkInterface::Interface const& /*intfc*/, la::networkInterface::Interface::Gateways const& /*gateways*/) noexcept override
	{
		// Don't care
	}

	// Private Members
	Model* _model{ nullptr };
	std::vector<NetworkInterface> _interfaces{};
};

std::string const NetworkInterfacesModel::OfflineInterfaceName = "Offline";

NetworkInterfacesModel::NetworkInterfacesModel() noexcept
	: _pImpl{ nullptr }
{
}

NetworkInterfacesModel::NetworkInterfacesModel(Model* const model, bool const addOfflineInterface, QObject* parent)
	: _pImpl{ std::make_unique<pImpl>(model, parent) }
{
	if (addOfflineInterface)
	{
		_pImpl->insertOfflineInterface();
	}
	la::networkInterface::NetworkInterfaceHelper::getInstance().registerObserver(_pImpl.get());
}

NetworkInterfacesModel::~NetworkInterfacesModel()
{
	la::networkInterface::NetworkInterfaceHelper::getInstance().unregisterObserver(_pImpl.get());
}

std::optional<std::reference_wrapper<NetworkInterfacesModel::NetworkInterface const>> NetworkInterfacesModel::networkInterface(std::size_t const index) const noexcept
{
	return _pImpl->networkInterface(index);
}

std::optional<std::reference_wrapper<NetworkInterfacesModel::NetworkInterface const>> NetworkInterfacesModel::networkInterface(std::string const& id) const noexcept
{
	return _pImpl->networkInterface(id);
}

std::size_t NetworkInterfacesModel::networkInterfacesCount() const noexcept
{
	return _pImpl->networkInterfacesCount();
}
} // namespace modelsLibrary
} // namespace hive
