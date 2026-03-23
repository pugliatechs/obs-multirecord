#pragma once

#include <QDockWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QTimer>
#include <QLabel>
#include <QDialog>
#include <QVector>

extern "C" {
#include "record-pipeline.h"
}

/**
 * A single row in the recordings table - one source recording entry.
 */
struct RecordingEntry {
	QString videoSourceName;
	QString outputDir;
	QString filenameFormat;
	QString container;
	QString videoEncoderId;
	QString audioEncoderId;
	int videoBitrate = 2500;
	int audioBitrate = 160;
	int fpsNum = 30;
	int fpsDen = 1;

	/* Runtime */
	struct record_pipeline *pipeline = nullptr;
};

/**
 * Dialog for editing a recording entry's settings.
 */
class EntrySettingsDialog : public QDialog {
	Q_OBJECT

public:
	EntrySettingsDialog(RecordingEntry &entry,
			    const QStringList &videoSourceList,
			    const QStringList &videoEncoderIds,
			    const QStringList &videoEncoderNames,
			    const QStringList &audioEncoderIds,
			    const QStringList &audioEncoderNames,
			    QWidget *parent = nullptr);

private slots:
	void onBrowseDir();
	void onAccept();

private:
	RecordingEntry &m_entry;

	QComboBox *m_videoSourceCombo;
	QLineEdit *m_outputDirEdit;
	QComboBox *m_containerCombo;
	QComboBox *m_videoEncoderCombo;
	QComboBox *m_audioEncoderCombo;
	QSpinBox *m_videoBitrateSpin;
	QSpinBox *m_audioBitrateSpin;
	QSpinBox *m_fpsNumSpin;
};

/**
 * Qt dock widget that manages multiple source recordings.
 */
class MultiRecordDock : public QDockWidget {
	Q_OBJECT

public:
	explicit MultiRecordDock(QWidget *parent = nullptr);
	~MultiRecordDock() override;

	void saveConfig();
	void loadConfig();

private slots:
	void onAddEntry();
	void onRemoveSelected();
	void onStartAll();
	void onStopAll();
	void onStartRow(int row);
	void onStopRow(int row);
	void onEditRow(int row);
	void onRefreshSources();
	void onStatusTimer();

private:
	void setupUi();
	void addEntryRow(const RecordingEntry &entry);
	void updateRowDisplay(int row);

	QStringList enumerateVideoSources();
	void enumerateEncoders();

	record_pipeline_config buildPipelineConfig(const RecordingEntry &entry);

	/* Widgets */
	QTableWidget *m_table = nullptr;
	QTimer *m_statusTimer = nullptr;

	QVector<RecordingEntry> m_entries;

	/* Cached lists */
	QStringList m_videoSourceList;
	QStringList m_videoEncoderIds;
	QStringList m_videoEncoderNames;
	QStringList m_audioEncoderIds;
	QStringList m_audioEncoderNames;

	/* Table columns */
	enum Column {
		COL_SOURCE = 0,
		COL_STATUS,
		COL_ACTIONS,
		COL_COUNT,
	};
};
