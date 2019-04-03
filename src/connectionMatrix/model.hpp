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

#pragma once

#include <QAbstractTableModel>
#include <la/avdecc/utils.hpp>

#define ENABLE_CONNECTION_MATRIX_DEBUG 1

#if ENABLE_CONNECTION_MATRIX_DEBUG
#include <QDebug>
#include <QVariantAnimation>
#include <QColor>
#endif

namespace connectionMatrix
{

class Node;

class ModelPrivate;
class Model : public QAbstractTableModel
{
public:
	struct IntersectionData
	{
		enum class Type
		{
			// None
			None,

			// Entity
			Entity_Entity,
			Entity_Redundant,
			Entity_RedundantStream,
			Entity_SingleStream,

			// Redundant
			Redundant_Redundant,
			Redundant_RedundantStream,
			Redundant_SingleStream,

			// RedundantStream
			RedundantStream_RedundantStream,
			RedundantStream_SingleStream,

			// Stream
			SingleStream_SingleStream,
		};

		enum class Capability
		{
			InterfaceDown = 1u << 0, /**< The AVB interface is down (or at least one for the intersection of 2 RedundantNodes) */
			WrongDomain = 1u << 1, /**< The AVB domains do not match (connection is possible, but stream reservation will fail) */
			WrongFormat = 1u << 2, /**< The Stream format do not match (connection is possible, but the audio won't be decoded by the listener) */
			Connected = 1u << 3, /**< Stream is connected (Mutually exclusive with FastConnecting and PartiallyConnected) */
			FastConnecting = 1u << 4, /**< Stream is fast connecting (Mutually exclusive with Connected and PartiallyConnected) */
			PartiallyConnected = 1u << 5, /**< Some, but not all of a redundant streams tuple, are connected (Mutually exclusive with Connected and FastConnecting) */
		};
		using Capabilities = la::avdecc::utils::EnumBitfield<Capability>;

		Type type{ Type::None };

		Node* talker{ nullptr };
		Node* listener{ nullptr };

		Capabilities capabilities{};

		// Additional redundant intersection data
		// PrimaryChildStatus // see formal PartiallyConnected
		// SecondaryChildStatus // see formal PartiallyConnected

#if ENABLE_CONNECTION_MATRIX_DEBUG
		QVariantAnimation* animation{ nullptr };
#endif
	};

	Model(QObject* parent = nullptr);
	virtual ~Model();
	
	virtual int rowCount(QModelIndex const& parent = {}) const override;
	virtual int columnCount(QModelIndex const& parent = {} ) const override;
	virtual QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	
	// Returns node for the given section and orientation, nullptr otherwise
	Node* node(int section, Qt::Orientation orientation) const;
	
	// Returns section for the given node and orientation, -1 otherwise
	int section(Node* node, Qt::Orientation orientation) const;

	// Returns intersection data for the given index
	IntersectionData const& intersectionData(QModelIndex const& index) const;
	
	// Set the transpose state of the model (default false, rows = talkers, columns = listeners)
	void setTransposed(bool const transposed);

	// Returns the transpose state of the model
	bool isTransposed() const;
	
private:
	QScopedPointer<ModelPrivate> d_ptr;
	Q_DECLARE_PRIVATE(Model);
};

} // namespace connectionMatrix
