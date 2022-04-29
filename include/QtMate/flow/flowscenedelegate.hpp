#pragma once

#include <QObject>
#include <QtMate/flow/flowdefs.hpp>

namespace qtMate::flow {

class FlowSceneDelegate : public QObject {
public:
	using QObject::QObject;

	virtual QColor socketTypeColor(FlowSocketType type) const;
	virtual bool canConnect(FlowOutput* output, FlowInput* input) const;
};

} // namespace qtMate::flow
