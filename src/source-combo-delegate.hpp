#pragma once

#include <QStyledItemDelegate>
#include <QComboBox>

/**
 * Table delegate that renders a QComboBox for source selection columns.
 * Used so that combo boxes appear inline in the table without needing
 * setCellWidget for every row (better for large tables).
 *
 * Currently unused — we use setCellWidget directly for simplicity.
 * Kept as a utility for future optimisation if the table grows large.
 */
class SourceComboDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	explicit SourceComboDelegate(const QStringList &sources,
				     QObject *parent = nullptr);

	QWidget *createEditor(QWidget *parent,
			      const QStyleOptionViewItem &option,
			      const QModelIndex &index) const override;

	void setEditorData(QWidget *editor,
			   const QModelIndex &index) const override;

	void setModelData(QWidget *editor, QAbstractItemModel *model,
			  const QModelIndex &index) const override;

	void setSources(const QStringList &sources);

private:
	QStringList m_sources;
};
