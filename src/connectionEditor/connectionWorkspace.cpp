#include "connectionworkspace.hpp"

#include <QtMate/flow/flowscene.hpp>
#include <QtMate/flow/flowsocket.hpp>
#include <QtMate/flow/flownode.hpp>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include <QMimeData>

namespace {

bool hasValidMimeData(QMimeData const* mimeData) {
	return mimeData->hasFormat("application/x-node");
}

} // namespace

ConnectionWorkspace::ConnectionWorkspace(qtMate::flow::FlowScene* scene, QWidget* parent)
	: FlowView{scene, parent} {
	setMinimumSize(960, 720);
	setAcceptDrops(true);
}

ConnectionWorkspace::~ConnectionWorkspace() = default;

void ConnectionWorkspace::dragEnterEvent(QDragEnterEvent* event) {
	if (hasValidMimeData(event->mimeData())) {
		_dragEnterAccepted = true;
		event->acceptProposedAction();
	} else {
		_dragEnterAccepted = false;
		QGraphicsView::dragEnterEvent(event);
	}
}
void ConnectionWorkspace::dragLeaveEvent(QDragLeaveEvent* event) {
	if (!_dragEnterAccepted) {
		QGraphicsView::dragLeaveEvent(event);
	}
}

void ConnectionWorkspace::dragMoveEvent(QDragMoveEvent* event) {
	if (hasValidMimeData(event->mimeData())) {
		event->acceptProposedAction();
	} else {
		QGraphicsView::dragMoveEvent(event);
	}
}

void ConnectionWorkspace::dropEvent(QDropEvent* event) {
	if (hasValidMimeData(event->mimeData())) {
		event->acceptProposedAction();
	} else {
		QGraphicsView::dropEvent(event);
	}
}
