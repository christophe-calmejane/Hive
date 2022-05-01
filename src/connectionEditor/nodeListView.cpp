#include "nodelistview.hpp"

#include <QDrag>
#include <QMimeData>

NodeListView::NodeListView(QWidget* parent)
	: QListView{parent} {
	setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
	setDragDropMode(QAbstractItemView::DragDropMode::DragOnly);
	setDragEnabled(true);
	setMinimumWidth(250);
}

NodeListView::~NodeListView() = default;

void NodeListView::startDrag(Qt::DropActions supportedActions) {
	auto* drag = new QDrag{this};

	auto const indexes = selectedIndexes();
	Q_ASSERT(indexes.count() == 1);

	auto* mimeData = model()->mimeData(indexes);
	drag->setMimeData(mimeData);

	if (mimeData->hasImage()) {
		auto const image = qvariant_cast<QImage>(mimeData->imageData());
		drag->setPixmap(QPixmap::fromImage(image));
	}

	if (drag->exec(supportedActions) != Qt::DropAction::IgnoreAction) {
		emit dropped(indexes.first());
	}
}
