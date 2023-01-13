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

namespace qtMate
{
namespace graph
{
class InputSocketItem;
class OutputSocketItem;

class NodeItem : public QGraphicsItem
{
public:
	NodeItem(int id, QString const& text, QGraphicsItem* parent = nullptr);

	virtual int type() const override;

	void addInput(QString const& text);
	void addOutput(QString const& text);

	InputSocketItem* inputAt(int index) const;
	OutputSocketItem* outputAt(int index) const;

	virtual QRectF boundingRect() const override;

private:
	virtual void paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget) override;
	virtual QVariant itemChange(GraphicsItemChange change, QVariant const& value) override;

	void updateGeometry();
	void propagateChanges();

private:
	int _id{ -1 };
	QString _text{};

	float _width{ 0.f };
	float _height{ 0.f };

	QVector<InputSocketItem*> _inputs;
	QVector<OutputSocketItem*> _outputs;
};

} // namespace graph
} // namespace qtMate
