/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QGraphicsItem>
#include <QVariantAnimation>
#include <QtMate/flow/flowDefs.hpp>

namespace qtMate::flow
{
class FlowNodeHeader;

class FlowNode : public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_PROPERTY(bool collapsed READ isCollapsed NOTIFY collapsedChanged);

public:
	FlowNode(FlowSceneDelegate* delegate, FlowNodeUid const& uid, FlowNodeDescriptor const& descriptor, QGraphicsItem* parent = nullptr);
	virtual ~FlowNode() override;

	FlowNodeUid const& uid() const;
	QString const& name() const;
	FlowInputs const& inputs() const;
	FlowOutputs const& outputs() const;

	bool isCollapsed() const;

	FlowInput* input(FlowSocketIndex index) const;
	FlowOutput* output(FlowSocketIndex index) const;

	bool hasConnectedInput() const;
	bool hasConnectedOutput() const;

	enum
	{
		Type = UserType + 1
	};
	virtual int type() const override;

	virtual QRectF boundingRect() const override;

	QRectF animatedBoundingRect() const;
	QRectF fixedBoundingRect() const;

	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget = nullptr) override;

signals:
	void collapsedChanged();

protected:
	virtual QVariant itemChange(GraphicsItemChange change, QVariant const& value) override;
	virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

private:
	void toggleCollapsed();
	void setCollapsed(bool collapsed);
	void handleItemPositionHasChanged();
	void handleItemSelectionHasChanged();
	void updateSockets();

	float collapseRatio(bool animated) const;

private:
	FlowSceneDelegate* _delegate{};
	FlowNodeUid _uid{};
	QString _name{};
	FlowNodeHeader* _header{};
	FlowInputs _inputs{};
	FlowOutputs _outputs{};
	bool _collapsed{};
	float _collapseRatio{ 1.f };
	QVariantAnimation _collapseAnimation{};
};

} // namespace qtMate::flow
