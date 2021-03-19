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

#pragma once

#include <la/avdecc/networkInterfaceHelper.hpp>

#include <QAbstractListModel>

#include <optional>
#include <string>
#include <memory>

namespace hive
{
namespace modelsLibrary
{
class NetworkInterfacesAbstractListModel;

/** NetworkInterfaces model itself (core engine) */
class NetworkInterfacesModel
{
public:
	struct NetworkInterface
	{
		std::string id{};
		std::string name{};
		bool isEnabled{ false };
		bool isConnected{ false };
		la::avdecc::networkInterface::Interface::Type interfaceType{ la::avdecc::networkInterface::Interface::Type::None };
	};

	using Model = NetworkInterfacesAbstractListModel;

	NetworkInterfacesModel(Model* const model, QObject* parent = nullptr);
	virtual ~NetworkInterfacesModel();

	std::optional<std::reference_wrapper<NetworkInterface const>> networkInterface(std::size_t const index) const noexcept;
	std::optional<std::reference_wrapper<NetworkInterface const>> networkInterface(std::string const& id) const noexcept;
	std::size_t networkInterfacesCount() const noexcept;

private:
	class pImpl;
	std::unique_ptr<pImpl> _pImpl; //{ nullptr }; NSDMI for unique_ptr not supported by gcc (for incomplete type)
};

/** Qt Abstract ListModel Proxy for NetworkInterfaces */
class NetworkInterfacesAbstractListModel : public QAbstractListModel
{
public:
	// Notifications for NetworkInterfacesModel changes, guaranteed to be called from Qt Main Thread
	virtual void nameChanged(std::size_t const /*index*/, std::string const& /*name*/) noexcept {}
	virtual void enabledStateChanged(std::size_t const /*index*/, bool const /*isEnabled*/) noexcept {}
	virtual void connectedStateChanged(std::size_t const /*index*/, bool const /*isConnected*/) noexcept {}

private:
	friend class NetworkInterfacesModel;
};

} // namespace modelsLibrary
} // namespace hive
