#pragma once

#include <QWidget>
#include <QtMate/flow/flowdefs.hpp>

class ConnectionEditor : public QWidget
{
	Q_OBJECT
public:
	ConnectionEditor(qtMate::flow::FlowNodeDescriptorMap const& nodes, qtMate::flow::FlowConnectionDescriptors const& connections, QWidget* parent = nullptr);
	virtual ~ConnectionEditor() override;

	qtMate::flow::FlowConnectionDescriptors const& connections() const;

private:
	qtMate::flow::FlowConnectionDescriptors _connections{};
};
