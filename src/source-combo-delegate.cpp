#include "source-combo-delegate.hpp"

SourceComboDelegate::SourceComboDelegate(const QStringList &sources,
					 QObject *parent)
	: QStyledItemDelegate(parent), m_sources(sources)
{
}

QWidget *
SourceComboDelegate::createEditor(QWidget *parent,
				  const QStyleOptionViewItem &option,
				  const QModelIndex &index) const
{
	Q_UNUSED(option);
	Q_UNUSED(index);

	auto *combo = new QComboBox(parent);
	combo->addItems(m_sources);
	return combo;
}

void SourceComboDelegate::setEditorData(QWidget *editor,
					const QModelIndex &index) const
{
	auto *combo = qobject_cast<QComboBox *>(editor);
	if (!combo)
		return;

	QString value = index.data(Qt::EditRole).toString();
	int idx = combo->findText(value);
	if (idx >= 0)
		combo->setCurrentIndex(idx);
}

void SourceComboDelegate::setModelData(QWidget *editor,
				       QAbstractItemModel *model,
				       const QModelIndex &index) const
{
	auto *combo = qobject_cast<QComboBox *>(editor);
	if (!combo)
		return;

	model->setData(index, combo->currentText(), Qt::EditRole);
}

void SourceComboDelegate::setSources(const QStringList &sources)
{
	m_sources = sources;
}
