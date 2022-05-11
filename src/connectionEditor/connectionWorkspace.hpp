#pragma once

#include <QtMate/flow/flowview.hpp>

class ConnectionWorkspace : public qtMate::flow::FlowView
{
public:
	ConnectionWorkspace(qtMate::flow::FlowScene* scene, QWidget* parent = nullptr);
	virtual ~ConnectionWorkspace() override;

protected:
	virtual void dragEnterEvent(QDragEnterEvent* event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
	virtual void dragMoveEvent(QDragMoveEvent* event) override;
	virtual void dropEvent(QDropEvent* event) override;

private:
	bool _dragEnterAccepted{};
};
