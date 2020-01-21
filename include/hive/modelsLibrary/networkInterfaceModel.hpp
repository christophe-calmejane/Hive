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

#pragma once

#include <la/avdecc/networkInterfaceHelper.hpp>

#include <QAbstractListModel>

#include <memory>
#include <optional>
#include <string>
#include <functional>

namespace hive
{
namespace modelsLibrary
{
/** Qt Abstract ListModel Proxy for NetworkInterface */
class NetworkInterfaceAbstractListModel : public QAbstractListModel
{
public:
	virtual void nameChanged(QModelIndex const& index, std::string const& name) noexcept = 0;
	virtual void enabledStateChanged(QModelIndex const& index, bool const isEnabled) noexcept = 0;
	virtual void connectedStateChanged(QModelIndex const& index, bool const isConnected) noexcept = 0;

private:
	friend class NetworkInterfaceModel;
};

class NetworkInterfaceModel
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

	using Model = NetworkInterfaceAbstractListModel;

	NetworkInterfaceModel(Model* const model, QObject* parent = nullptr);
	virtual ~NetworkInterfaceModel();

	std::optional<std::reference_wrapper<NetworkInterface const>> networkInterface(QModelIndex const& index) const noexcept;
	std::optional<std::reference_wrapper<NetworkInterface const>> networkInterface(std::string const& id) const noexcept;
	int rowCount() const noexcept;

private:
	class pImpl;
	std::unique_ptr<pImpl> _pImpl{ nullptr };
};

} // namespace modelsLibrary
} // namespace hive
