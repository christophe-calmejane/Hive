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

// Debugging options
//#define ENABLE_CONNECTION_MATRIX_DEBUG 1
#define ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED 1
#define ENABLE_CONNECTION_MATRIX_TOOLTIP 1

#include <QAbstractTableModel>
#include <la/avdecc/utils.hpp>
#include <la/avdecc/internals/entityModel.hpp>

// Always disable debugging options in Release
#ifndef DEBUG
#	undef ENABLE_CONNECTION_MATRIX_DEBUG
#	undef ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
#	undef ENABLE_CONNECTION_MATRIX_TOOLTIP
#	define ENABLE_CONNECTION_MATRIX_DEBUG 0
#	define ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED 0
#	define ENABLE_CONNECTION_MATRIX_TOOLTIP 0
#endif // !DEBUG

#if ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
#	include <QVariantAnimation>
#	include <QColor>
#endif

#include <vector>

namespace connectionMatrix
{
class Node;
class ModelPrivate;
class Model : public QAbstractTableModel
{
public:
	enum class Mode
	{
		None,
		Stream,
		Channel,
	};

	struct IntersectionData
	{
		enum class Type
		{
			// None
			None,

			// OfflineOutputStream
			OfflineOutputStream_Entity, // Summary kind
			OfflineOutputStream_Redundant, // Summary kind
			OfflineOutputStream_RedundantStream,
			OfflineOutputStream_RedundantChannel,
			OfflineOutputStream_SingleStream,
			OfflineOutputStream_SingleChannel,

			// Entity
			Entity_Entity, // Summary kind
			Entity_Redundant, // Summary kind
			Entity_RedundantStream, // Summary kind
			Entity_RedundantChannel, // Summary kind
			Entity_SingleStream, // Summary kind
			Entity_SingleChannel, // Summary kind

			// Redundant
			Redundant_Redundant, // Summary kind
			Redundant_RedundantStream, // Duplicate kind
			Redundant_RedundantChannel, // Duplicate kind
			Redundant_SingleStream, // Summary kind
			Redundant_SingleChannel, // Summary kind

			// RedundantStream
			RedundantStream_RedundantStream,
			RedundantStream_RedundantStream_Forbidden,
			RedundantStream_SingleStream,

			// RedundantChannel
			RedundantChannel_RedundantChannel,
			RedundantChannel_RedundantChannel_Forbidden,
			RedundantChannel_SingleChannel,

			// Stream
			SingleStream_SingleStream,

			// Channel
			SingleChannel_SingleChannel,
		};

		enum class State
		{
			NotConnected, /**< Stream is not connected */
			Connected, /**< Stream is connected */
			FastConnecting, /**< Stream is fast connecting */
			PartiallyConnected, /**< Some but not all of a redundant stream are connected */
		};

		enum class Flag
		{
			InterfaceDown = 1u << 0, /**< The AVB interface is down (or at least one for the intersection of 2 RedundantNodes) */
			WrongDomain = 1u << 1, /**< The AVB domains do not match (connection is possible, but stream reservation will fail) */
			WrongFormat = 1u << 2, /**< The Stream format do not match (connection is possible, but the audio won't be decoded by the listener) */
			MediaLocked = 1u << 3, /**< The Stream is Connected and Media Locked (Milan only) */
		};
		using Flags = la::avdecc::utils::EnumBitfield<Flag>;

		struct SmartConnectableStream
		{
			la::avdecc::entity::model::StreamIdentification talkerStream{};
			la::avdecc::entity::model::StreamIdentification listenerStream{};
			bool isConnected{ false };
			bool isFastConnecting{ false };
		};

		Node* talker{ nullptr };
		Node* listener{ nullptr };

		Type type{ Type::None };
		State state{ State::NotConnected };
		Flags flags{};
		std::vector<SmartConnectableStream> smartConnectableStreams{};

#if ENABLE_CONNECTION_MATRIX_HIGHLIGHT_DATA_CHANGED
		QVariantAnimation* animation{ nullptr };
#endif
	};

	Model(QObject* parent = nullptr);
	virtual ~Model();

	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual int columnCount(QModelIndex const& parent = {}) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	// Returns node for the given section and orientation, nullptr otherwise
	Node* node(int section, Qt::Orientation orientation) const;

	// Returns section for the given node and orientation, -1 otherwise
	int section(Node* node, Qt::Orientation orientation) const;

	// Returns intersection data for the given index
	IntersectionData const& intersectionData(QModelIndex const& index) const;

	// Set the model mode
	void setMode(Mode const mode, bool const force = false);

	// Returns the mode of the model
	Mode mode() const;

	// Set the transpose state of the model (default false, rows = talkers, columns = listeners)
	void setTransposed(bool const transposed);

	// Returns the transpose state of the model
	bool isTransposed() const;

	// Force a refresh of the headers
	void forceRefreshHeaders();

	// Visitor pattern that performs a hierarchy traversal according with respect of the current mode
	using Visitor = std::function<void(Node*)>;
	void accept(Node* node, Visitor const& visitor, bool const childrenOnly = false) const;

private:
	QScopedPointer<ModelPrivate> d_ptr;
	Q_DECLARE_PRIVATE(Model);
};

} // namespace connectionMatrix
