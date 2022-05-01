#pragma once

#include <QListView>

class NodeListView : public QListView {
	Q_OBJECT
public:
	NodeListView(QWidget* parent = nullptr);
	virtual ~NodeListView() override;

signals:
	void dropped(QModelIndex const& index);

protected:
	virtual void startDrag(Qt::DropActions supportedActions) override;
};
