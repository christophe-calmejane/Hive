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

#pragma once

#include <QStandardItemModel>
#include <la/avdecc/utils.hpp>

namespace connectionMatrix
{
class ModelPrivate;
class Model final : public QStandardItemModel
{
public:
	enum class NodeType
	{
		None,
		Entity,
		OutputStream,
		InputStream,
		RedundantOutput,
		RedundantInput,
		RedundantOutputStream,
		RedundantInputStream,
	};

	enum class ConnectionCapabilities
	{
		None = 0,
		InterfaceDown = 1u << 0, /**< The AVB interface is down (or at least one for the intersection of 2 RedundantNodes) */
		WrongDomain = 1u << 1, /**< The AVB domains do not match (connection is possible, but stream reservation will fail) */
		WrongFormat = 1u << 2, /**< The Stream format do not match (connection is possible, but the audio won't be decoded by the listener) */
		Connectable = 1u << 3, /**< Stream connectable (might be connected, or not) */
		Connected = 1u << 4, /**< Stream is connected (Mutually exclusive with FastConnecting and PartiallyConnected) */
		FastConnecting = 1u << 5, /**< Stream is fast connecting (Mutually exclusive with Connected and PartiallyConnected) */
		PartiallyConnected = 1u << 6, /**< Some, but not all of a redundant streams tuple, are connected (Mutually exclusive with Connected and FastConnecting) */
	};

	enum ItemDataRole
	{
		/** Header Data */
		NodeTypeRole = Qt::UserRole + 1, // Model::NodeType
		EntityIDRole, // la::avdecc::UniqueIdentifier
		StreamIndexRole, // la::avdecc::entity::model::StreamIndex
		RedundantIndexRole, // la::avdecc::controller::model::VirtualIndex
		RedundantStreamOrderRole, // std::int32_t
		StreamWaitingRole, // bool

		RelativeParentIndexRole, // std::optional<std::int32_t>, (std::nullopt if orphan)
		ChildrenCountRole, // std::int32_t
		FilterRole, // QString proxy to the associated entity's DisplayRole

		/** Intersection Data */
		ConnectionCapabilitiesRole, // ConnectionCapabilities
	};

	Model(QObject* parent = nullptr);
	virtual ~Model();

private:
	ModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(Model);
};

} // namespace connectionMatrix

Q_DECLARE_METATYPE(connectionMatrix::Model::NodeType)
Q_DECLARE_METATYPE(connectionMatrix::Model::ConnectionCapabilities)

// Define bitfield enum traits for Model::ConnectionCapabilities
template<>
struct la::avdecc::utils::enum_traits<connectionMatrix::Model::ConnectionCapabilities>
{
	static constexpr bool is_bitfield = true;
};
