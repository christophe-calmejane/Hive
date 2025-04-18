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

#include "mappingMatrix.hpp"

#include <QtMate/flow/flowScene.hpp>
#include <QtMate/flow/flowSceneDelegate.hpp>
#include <QtMate/flow/flowView.hpp>
#include <QtMate/flow/flowNode.hpp>
#include <QtMate/flow/flowInput.hpp>
#include <QtMate/flow/flowOutput.hpp>

#include <QPushButton>
#include <QDialog>
#include <QGridLayout>

#include <utility>
#include <vector>
#include <string>

namespace mappingMatrix
{
class MappingMatrix : public QWidget
{
public:
	enum SocketType
	{
		Input,
		Output,
	};

	class Delegate : public qtMate::flow::FlowSceneDelegate
	{
	public:
		using FlowSceneDelegate::FlowSceneDelegate;

		virtual bool canConnect(qtMate::flow::FlowOutput* output, qtMate::flow::FlowInput* input) const override
		{
			return true;
		}

		virtual QColor socketTypeColor(qtMate::flow::FlowSocketType type) const
		{
			return 0x2196F3;
		}
	};


	MappingMatrix(Outputs const& outputs, Inputs const& inputs, Connections const& connections, QWidget* parent = nullptr)
		: QWidget{ parent }
	{
		auto* delegate = new Delegate{ this };
		auto* scene = new qtMate::flow::FlowScene{ delegate, this };
		auto* view = new qtMate::flow::FlowView{ scene, this };
		view->setDragMode(QGraphicsView::ScrollHandDrag);

		auto* layout = new QHBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(view);

		// Get notified by the scene when connections are created/destroyed
		connect(scene, &qtMate::flow::FlowScene::connectionCreated, this,
			[&](auto const& descriptor)
			{
				_connections.insert(descriptor);
			});

		connect(scene, &qtMate::flow::FlowScene::connectionDestroyed, this,
			[&](auto const& descriptor)
			{
				_connections.remove(descriptor);
			});


		auto const createNodesAndConnections = [&]()
		{
			// CAUTION: as the spec tells that outputs and inputs nodes are identified by their index in their own list,
			// which is not compatible with qtMate::flow API which requires a unique identifier for each node,
			// we must generate this unique identifier ourselves and also keep the offset to the first input node
			// in order to be able to translate connections back and forth.
			auto id = 0;
			_offset = static_cast<decltype(_offset)>(outputs.size());

			// Create output nodes
			_outputNodes.reserve(outputs.size());
			for (auto const& output : outputs)
			{
				auto descriptor = qtMate::flow::FlowNodeDescriptor{};
				descriptor.name = QString::fromStdString(output.name);
				for (auto const& socket : output.sockets)
				{
					descriptor.outputs.push_back({ QString::fromStdString(socket), SocketType::Output });
				}

				auto* node = scene->createNode(id++, descriptor);
				node->setFlag(QGraphicsItem::ItemIsMovable, false);
				_outputNodes.emplaceBack(node);
			}

			// Create input nodes
			for (auto const& input : inputs)
			{
				auto descriptor = qtMate::flow::FlowNodeDescriptor{};
				descriptor.name = QString::fromStdString(input.name);
				for (auto const& socket : input.sockets)
				{
					descriptor.inputs.push_back({ QString::fromStdString(socket), SocketType::Input });
				}

				auto* node = scene->createNode(id++, descriptor);
				node->setFlag(QGraphicsItem::ItemIsMovable, false);
				_inputNodes.emplaceBack(node);
			}

			// Create connections
			for (auto const& connection : connections)
			{
				scene->createConnection(convert(connection));
			}
		};

		auto const layoutNodes = [&]()
		{
			auto const paddingX{ 150.f };
			auto const paddingY{ 5.f };

			auto outputNodeX{ 0.f };
			auto outputNodeY{ 0.f };

			auto inputNodeX{ 0.f };
			auto inputNodeY{ 0.f };

			auto animateTo = [&](qtMate::flow::FlowNode* node, float x, float y)
			{
				if (!_firstLayoutExecuted)
				{
					node->setPos(x, y);
					return;
				}

				auto animation = new QVariantAnimation{ this };
				animation->setDuration(400);
				animation->setEasingCurve(QEasingCurve::Type::OutQuart);
				animation->setStartValue(node->pos());
				animation->setEndValue(QPointF{ x, y });

				connect(animation, &QVariantAnimation::valueChanged, this,
					[node, uid = node](QVariant const& value)
					{
						node->setPos(value.toPointF());
					});

				animation->start(QAbstractAnimation::DeletionPolicy::DeleteWhenStopped);
			};


			for (auto* node : _outputNodes)
			{
				animateTo(node, outputNodeX, outputNodeY);

				auto const boundingRect = node->fixedBoundingRect();
				outputNodeY += boundingRect.height() + paddingY;
				inputNodeX = std::max(inputNodeX, static_cast<float>(boundingRect.width()) + paddingX);
			}

			for (auto* node : _inputNodes)
			{
				animateTo(node, inputNodeX, inputNodeY);

				auto const boundingRect = node->fixedBoundingRect();
				inputNodeY += boundingRect.height() + paddingY;
			}
		};

		connect(scene, &qtMate::flow::FlowScene::layoutRequested, this, layoutNodes);

		createNodesAndConnections();

		_firstLayoutExecuted = false;
		layoutNodes();
		_firstLayoutExecuted = true;
	}


	Connections connections() const
	{
		auto result = Connections{};
		for (auto const& connection : _connections)
		{
			result.emplace_back(convert(connection));
		}
		return result;
	}

	// Convert connection from flow to mapping
	Connection convert(qtMate::flow::FlowConnectionDescriptor const& descriptor) const
	{
		auto const source = SlotID{ static_cast<SlotID::first_type>(descriptor.first.first), static_cast<SlotID::second_type>(descriptor.first.second) };
		auto const sink = SlotID{ static_cast<SlotID::first_type>(descriptor.second.first - _offset), static_cast<SlotID::first_type>(descriptor.second.second) };
		return Connection{ source, sink };
	}

	// Convert connection from mapping to flow
	qtMate::flow::FlowConnectionDescriptor convert(Connection const& connection) const
	{
		auto const source = qtMate::flow::FlowSocketSlot{ static_cast<qtMate::flow::FlowSocketSlot::first_type>(connection.first.first), static_cast<qtMate::flow::FlowSocketSlot::second_type>(connection.first.second) };
		auto const sink = qtMate::flow::FlowSocketSlot{ static_cast<qtMate::flow::FlowSocketSlot::first_type>(connection.second.first + _offset), static_cast<qtMate::flow::FlowSocketSlot::second_type>(connection.second.second) };
		return qtMate::flow::FlowConnectionDescriptor{ source, sink };
	}

private:
	int _offset{}; // we need an offset to identify output nodes
	qtMate::flow::FlowConnectionDescriptors _connections{};

	QVector<qtMate::flow::FlowNode*> _outputNodes{};
	QVector<qtMate::flow::FlowNode*> _inputNodes{};

	bool _firstLayoutExecuted{ false };
};

MappingMatrixDialog::MappingMatrixDialog(QString const& title, Outputs const& outputs, Inputs const& inputs, Connections const& connections, QWidget* parent)
#ifdef Q_OS_WIN32
	: QDialog(parent, Qt::Dialog | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint) // Because Qt::Tool is ugly on windows and '?' needs to be hidden (currently not supported)
#else
	: QDialog(parent, Qt::Tool | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
#endif
	, _matrix{ new MappingMatrix{ outputs, inputs, connections, this } }
{
	setWindowTitle(title);

	auto* apply = new QPushButton{ "Apply", this };
	auto* cancel = new QPushButton{ "Cancel", this };

	auto* layout = new QGridLayout{ this };
	layout->addWidget(_matrix, 0, 0, 1, 2);
	layout->addWidget(apply, 1, 0);
	layout->addWidget(cancel, 1, 1);

	connect(apply, &QPushButton::clicked, this, &QDialog::accept);
	connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
}

Connections MappingMatrixDialog::connections() const
{
	return _matrix->connections();
}

} // namespace mappingMatrix
