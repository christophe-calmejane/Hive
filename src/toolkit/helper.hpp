#pragma once

#include <QDebug>
#include <QString>

namespace qt
{
namespace toolkit
{
inline QString pointerToString(void* ptr)
{
	return QString{ "0x%1" }.arg(reinterpret_cast<quintptr>(ptr), QT_POINTER_SIZE * 2, 16, QChar{ '0' });
}

} // namespace toolkit
} // namespace qt
