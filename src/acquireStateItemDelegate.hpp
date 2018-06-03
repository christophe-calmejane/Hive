#include <QStyledItemDelegate>
#include <QPixmap>
#include <array>

class AcquireStateItemDelegate : public QStyledItemDelegate
{
public:
	AcquireStateItemDelegate(QObject* parent = nullptr);
	
	virtual void paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const override;
	
private:
	std::array<QPixmap, 3> _pixmaps
	{
		{
			QPixmap{":/unlocked.png"},
			QPixmap{":/locked.png"},
			QPixmap{":/locked_by_other.png"}
		},
	};
};
