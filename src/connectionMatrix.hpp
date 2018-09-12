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

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

#include "toolkit/matrixTreeView.hpp"
#include <la/avdecc/avdecc.hpp>
#include <set>
#include <QAbstractItemDelegate>
#include "avdecc/controllerManager.hpp"

namespace connectionMatrix
{

enum class ConnectionCapabilities
{
	None = 0,
	WrongDomain = 1u << 0,
	WrongFormat = 1u << 1,
	Connectable = 1u << 2, /**< Stream connectable (might be connected, or not) */
	Connected = 1u << 3, /**< Stream is connected (Mutually exclusive with FastConnecting and PartiallyConnected) */
	FastConnecting = 1u << 4, /**< Stream is fast connecting (Mutually exclusive with Connected and PartiallyConnected) */
	PartiallyConnected = 1u << 5, /**< Some, but not all of a redundant streams tuple, are connected (Mutually exclusive with Connected and FastConnecting) */
};

struct UserData
{
	enum class Type
	{
		None = 0,
		EntityNode,
		InputStreamNode,
		OutputStreamNode,
		RedundantInputNode,
		RedundantOutputNode,
		RedundantInputStreamNode,
		RedundantOutputStreamNode,
	};
	Type type{ Type::None };
	la::avdecc::UniqueIdentifier entityID{};
	la::avdecc::entity::model::StreamIndex streamIndex{ la::avdecc::entity::model::StreamIndex(-1) }; // The entity stream index (real index)
	la::avdecc::controller::model::VirtualIndex redundantIndex{ la::avdecc::controller::model::VirtualIndex(-1) }; // The entity redundant stream index (virtual index)
	std::int32_t redundantStreamOrder{ -1 }; // Stream order in case of redundant one (Primary = 0, Secondary = 1, Tertiary = 2, ...)
};

constexpr bool operator==(UserData const& lhs, UserData const& rhs)
{
	return lhs.type == rhs.type && lhs.entityID == rhs.entityID && lhs.streamIndex == rhs.streamIndex;
}

class ConnectionMatrixModel final : public qt::toolkit::MatrixModel
{
public:
	ConnectionMatrixModel(QObject* parent = nullptr);
	virtual ~ConnectionMatrixModel();

private:
	//using MatrixModel::beginAppendRows;
	//using MatrixModel::appendRow;
	//using MatrixModel::endAppendRows;
	//using MatrixModel::beginAppendColumns;
	//using MatrixModel::appendColumn;
	//using MatrixModel::endAppendColumns;

	// QAbstractTableModel overrides
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

	friend class ConnectionMatrixItemDelegate;
	friend class ConnectionMatrixView;
	class ConnectionMatrixModelPrivate;
	ConnectionMatrixModelPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(ConnectionMatrixModel);
};

class ConnectionMatrixItemDelegate final : public QAbstractItemDelegate
{
public:
private:
	// QAbstractItemDelegate overrides
	virtual void paint(QPainter *painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override;
	virtual QSize sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const override;
};

class ConnectionMatrixView final : public qt::toolkit::MatrixTreeView
{
public:
	ConnectionMatrixView(QWidget* parent = nullptr);
	
private:
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual bool eventFilter(QObject* object, QEvent* event) override;

	Q_SLOT void onHeaderCustomContextMenuRequested(QPoint const& pos);
};

static void drawConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawWrongDomainConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawWrongFormatConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawWrongDomainFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawWrongFormatFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawWrongDomainNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawWrongFormatNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant);
static void drawPartiallyConnectedRedundantNode(QPainter* painter, QRect const& rect, bool const isRedundant = false);
static void drawEntityNoConnection(QPainter* painter, QRect const& rect);

} // namespace connectionMatrix

// Define bitfield enum traits for ConnectionCapabilities
template<>
struct la::avdecc::enum_traits<connectionMatrix::ConnectionCapabilities>
{
	static constexpr bool is_bitfield = true;
};

Q_DECLARE_METATYPE(connectionMatrix::UserData);
