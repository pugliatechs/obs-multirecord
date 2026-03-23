#include "multi-record-dock.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QMainWindow>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QToolButton>
#include <QStyle>

/* ------------------------------------------------------------------ */
/* PairSettingsDialog                                                  */
/* ------------------------------------------------------------------ */

static constexpr const char *CONTAINERS[] = {"mkv", "mp4", "mov", "ts"};
static constexpr int NUM_CONTAINERS = 4;

PairSettingsDialog::PairSettingsDialog(
	RecordingEntry &entry,
	const QStringList &sourceList,
	const QStringList &videoEncoderIds,
	const QStringList &videoEncoderNames,
	const QStringList &audioEncoderIds,
	const QStringList &audioEncoderNames,
	QWidget *parent)
	: QDialog(parent), m_entry(entry)
{
	setWindowTitle("Recording Pair Settings");
	setMinimumWidth(420);

	auto *mainLayout = new QVBoxLayout(this);

	/* -- Sources group -- */
	auto *srcGroup = new QGroupBox("Sources", this);
	auto *srcForm = new QFormLayout(srcGroup);

	m_videoSourceCombo = new QComboBox();
	m_videoSourceCombo->addItems(sourceList);
	int vIdx = sourceList.indexOf(entry.videoSourceName);
	if (vIdx >= 0)
		m_videoSourceCombo->setCurrentIndex(vIdx);
	srcForm->addRow("Video Source:", m_videoSourceCombo);

	m_audioSourceCombo = new QComboBox();
	m_audioSourceCombo->addItem("(same as video)");
	m_audioSourceCombo->addItems(sourceList);
	if (!entry.audioSourceName.isEmpty()) {
		int aIdx = sourceList.indexOf(entry.audioSourceName);
		if (aIdx >= 0)
			m_audioSourceCombo->setCurrentIndex(aIdx + 1);
	}
	srcForm->addRow("Audio Source:", m_audioSourceCombo);

	mainLayout->addWidget(srcGroup);

	/* -- Output group -- */
	auto *outGroup = new QGroupBox("Output", this);
	auto *outForm = new QFormLayout(outGroup);

	auto *dirWidget = new QWidget();
	auto *dirLayout = new QHBoxLayout(dirWidget);
	dirLayout->setContentsMargins(0, 0, 0, 0);
	m_outputDirEdit = new QLineEdit(entry.outputDir);
	m_outputDirEdit->setPlaceholderText("Recording directory...");
	auto *browseBtn = new QPushButton("...");
	browseBtn->setFixedWidth(30);
	dirLayout->addWidget(m_outputDirEdit);
	dirLayout->addWidget(browseBtn);
	connect(browseBtn, &QPushButton::clicked, this,
		&PairSettingsDialog::onBrowseDir);
	outForm->addRow("Directory:", dirWidget);

	m_containerCombo = new QComboBox();
	for (int i = 0; i < NUM_CONTAINERS; i++)
		m_containerCombo->addItem(CONTAINERS[i]);
	int cIdx = m_containerCombo->findText(entry.container);
	if (cIdx >= 0)
		m_containerCombo->setCurrentIndex(cIdx);
	outForm->addRow("Container:", m_containerCombo);

	mainLayout->addWidget(outGroup);

	/* -- Encoder group -- */
	auto *encGroup = new QGroupBox("Encoding", this);
	auto *encForm = new QFormLayout(encGroup);

	m_videoEncoderCombo = new QComboBox();
	for (int i = 0; i < videoEncoderIds.size(); i++)
		m_videoEncoderCombo->addItem(videoEncoderNames[i],
					     videoEncoderIds[i]);
	int veIdx = videoEncoderIds.indexOf(entry.videoEncoderId);
	if (veIdx >= 0)
		m_videoEncoderCombo->setCurrentIndex(veIdx);
	encForm->addRow("Video Encoder:", m_videoEncoderCombo);

	m_videoBitrateSpin = new QSpinBox();
	m_videoBitrateSpin->setRange(100, 100000);
	m_videoBitrateSpin->setSuffix(" kbps");
	m_videoBitrateSpin->setValue(entry.videoBitrate);
	encForm->addRow("Video Bitrate:", m_videoBitrateSpin);

	m_audioEncoderCombo = new QComboBox();
	for (int i = 0; i < audioEncoderIds.size(); i++)
		m_audioEncoderCombo->addItem(audioEncoderNames[i],
					     audioEncoderIds[i]);
	int aeIdx = audioEncoderIds.indexOf(entry.audioEncoderId);
	if (aeIdx >= 0)
		m_audioEncoderCombo->setCurrentIndex(aeIdx);
	encForm->addRow("Audio Encoder:", m_audioEncoderCombo);

	m_audioBitrateSpin = new QSpinBox();
	m_audioBitrateSpin->setRange(32, 512);
	m_audioBitrateSpin->setSuffix(" kbps");
	m_audioBitrateSpin->setValue(entry.audioBitrate);
	encForm->addRow("Audio Bitrate:", m_audioBitrateSpin);

	m_fpsNumSpin = new QSpinBox();
	m_fpsNumSpin->setRange(1, 240);
	m_fpsNumSpin->setSuffix(" fps");
	m_fpsNumSpin->setValue(entry.fpsNum);
	encForm->addRow("Frame Rate:", m_fpsNumSpin);

	mainLayout->addWidget(encGroup);

	/* -- Buttons -- */
	auto *buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this,
		&PairSettingsDialog::onAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	mainLayout->addWidget(buttons);
}

void PairSettingsDialog::onBrowseDir()
{
	QString dir = QFileDialog::getExistingDirectory(
		this, "Select Recording Directory", m_outputDirEdit->text());
	if (!dir.isEmpty())
		m_outputDirEdit->setText(dir);
}

void PairSettingsDialog::onAccept()
{
	if (m_videoSourceCombo->currentText().isEmpty()) {
		QMessageBox::warning(this, "Settings",
				     "Please select a video source.");
		return;
	}
	if (m_outputDirEdit->text().isEmpty()) {
		QMessageBox::warning(this, "Settings",
				     "Please set an output directory.");
		return;
	}

	m_entry.videoSourceName = m_videoSourceCombo->currentText();
	if (m_audioSourceCombo->currentIndex() == 0)
		m_entry.audioSourceName = "";
	else
		m_entry.audioSourceName = m_audioSourceCombo->currentText();

	m_entry.outputDir = m_outputDirEdit->text();
	m_entry.container = m_containerCombo->currentText();
	m_entry.videoEncoderId =
		m_videoEncoderCombo->currentData().toString();
	m_entry.audioEncoderId =
		m_audioEncoderCombo->currentData().toString();
	m_entry.videoBitrate = m_videoBitrateSpin->value();
	m_entry.audioBitrate = m_audioBitrateSpin->value();
	m_entry.fpsNum = m_fpsNumSpin->value();
	m_entry.fpsDen = 1;

	accept();
}

/* ------------------------------------------------------------------ */
/* MultiRecordDock                                                    */
/* ------------------------------------------------------------------ */

MultiRecordDock::MultiRecordDock(QWidget *parent)
	: QDockWidget(parent)
{
	setObjectName("MultiRecordDock");
	/* Hide our title bar - obs_frontend_add_dock_by_id provides its own */
	setTitleBarWidget(new QWidget(this));
	setupUi();

	onRefreshSources();

	m_statusTimer = new QTimer(this);
	connect(m_statusTimer, &QTimer::timeout, this,
		&MultiRecordDock::onStatusTimer);
	m_statusTimer->start(1000);

	loadConfig();
}

MultiRecordDock::~MultiRecordDock()
{
	saveConfig();

	for (auto &entry : m_entries) {
		if (entry.pipeline) {
			record_pipeline_destroy(entry.pipeline);
			entry.pipeline = nullptr;
		}
	}
}

/* ------------------------------------------------------------------ */
/* UI Setup                                                           */
/* ------------------------------------------------------------------ */

void MultiRecordDock::setupUi()
{
	auto *container = new QWidget(this);
	auto *mainLayout = new QVBoxLayout(container);
	mainLayout->setContentsMargins(4, 4, 4, 4);
	mainLayout->setSpacing(4);

	/* Table - compact: 4 columns */
	m_table = new QTableWidget(0, COL_COUNT, container);
	QStringList headers;
	headers << "Video" << "Audio" << "Status" << "";
	m_table->setHorizontalHeaderLabels(headers);

	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->verticalHeader()->hide();
	m_table->verticalHeader()->setDefaultSectionSize(30);
	m_table->setShowGrid(false);
	m_table->setAlternatingRowColors(true);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setFocusPolicy(Qt::NoFocus);

	/* Column sizing */
	auto *hdr = m_table->horizontalHeader();
	hdr->setSectionResizeMode(COL_VIDEO_SOURCE, QHeaderView::Stretch);
	hdr->setSectionResizeMode(COL_AUDIO_SOURCE, QHeaderView::Stretch);
	hdr->setSectionResizeMode(COL_STATUS, QHeaderView::ResizeToContents);
	hdr->setSectionResizeMode(COL_ACTIONS, QHeaderView::ResizeToContents);
	hdr->setMinimumSectionSize(40);

	mainLayout->addWidget(m_table, 1);

	/* Button bar */
	auto *btnLayout = new QHBoxLayout();
	btnLayout->setSpacing(4);

	auto *addBtn = new QPushButton("Add");
	addBtn->setToolTip("Add recording pair");

	auto *removeBtn = new QPushButton("Remove");
	removeBtn->setToolTip("Remove selected pair");

	auto *startAllBtn = new QPushButton("Rec All");
	startAllBtn->setToolTip("Start all recordings");

	auto *stopAllBtn = new QPushButton("Stop All");
	stopAllBtn->setToolTip("Stop all recordings");

	btnLayout->addWidget(addBtn);
	btnLayout->addWidget(removeBtn);
	btnLayout->addStretch();
	btnLayout->addWidget(startAllBtn);
	btnLayout->addWidget(stopAllBtn);

	mainLayout->addLayout(btnLayout);
	setWidget(container);

	connect(addBtn, &QPushButton::clicked, this,
		&MultiRecordDock::onAddPair);
	connect(removeBtn, &QPushButton::clicked, this,
		&MultiRecordDock::onRemoveSelected);
	connect(startAllBtn, &QPushButton::clicked, this,
		&MultiRecordDock::onStartAll);
	connect(stopAllBtn, &QPushButton::clicked, this,
		&MultiRecordDock::onStopAll);

	/* Double-click row to edit settings */
	connect(m_table, &QTableWidget::cellDoubleClicked, this,
		[this](int row, int) { onEditRow(row); });
}

/* ------------------------------------------------------------------ */
/* Row display                                                        */
/* ------------------------------------------------------------------ */

void MultiRecordDock::addEntryRow(const RecordingEntry &entry)
{
	int row = m_table->rowCount();
	m_table->insertRow(row);

	/* Video source label */
	auto *videoItem = new QTableWidgetItem(
		entry.videoSourceName.isEmpty() ? "(none)"
						: entry.videoSourceName);
	m_table->setItem(row, COL_VIDEO_SOURCE, videoItem);

	/* Audio source label */
	QString audioText = entry.audioSourceName.isEmpty()
				    ? "(same)"
				    : entry.audioSourceName;
	auto *audioItem = new QTableWidgetItem(audioText);
	m_table->setItem(row, COL_AUDIO_SOURCE, audioItem);

	/* Status label */
	auto *statusLabel = new QLabel("Idle");
	statusLabel->setAlignment(Qt::AlignCenter);
	statusLabel->setContentsMargins(4, 0, 4, 0);
	m_table->setCellWidget(row, COL_STATUS, statusLabel);

	/* Action buttons: settings, start/stop */
	auto *actWidget = new QWidget();
	auto *actLayout = new QHBoxLayout(actWidget);
	actLayout->setContentsMargins(2, 0, 2, 0);
	actLayout->setSpacing(2);

	auto *settingsBtn = new QToolButton();
	settingsBtn->setIcon(
		settingsBtn->style()->standardIcon(QStyle::SP_FileDialogDetailedView));
	settingsBtn->setToolTip("Settings");
	settingsBtn->setFixedSize(24, 24);

	auto *startBtn = new QToolButton();
	startBtn->setIcon(
		startBtn->style()->standardIcon(QStyle::SP_MediaPlay));
	startBtn->setToolTip("Start recording");
	startBtn->setFixedSize(24, 24);

	auto *stopBtn = new QToolButton();
	stopBtn->setIcon(
		stopBtn->style()->standardIcon(QStyle::SP_MediaStop));
	stopBtn->setToolTip("Stop recording");
	stopBtn->setFixedSize(24, 24);
	stopBtn->setEnabled(false);

	actLayout->addWidget(settingsBtn);
	actLayout->addWidget(startBtn);
	actLayout->addWidget(stopBtn);
	m_table->setCellWidget(row, COL_ACTIONS, actWidget);

	connect(settingsBtn, &QToolButton::clicked, this,
		[this, row]() { onEditRow(row); });
	connect(startBtn, &QToolButton::clicked, this,
		[this, row]() { onStartRow(row); });
	connect(stopBtn, &QToolButton::clicked, this,
		[this, row]() { onStopRow(row); });
}

void MultiRecordDock::updateRowDisplay(int row)
{
	if (row < 0 || row >= m_entries.size())
		return;

	const auto &e = m_entries[row];

	auto *videoItem = m_table->item(row, COL_VIDEO_SOURCE);
	if (videoItem)
		videoItem->setText(e.videoSourceName.isEmpty()
					  ? "(none)"
					  : e.videoSourceName);

	auto *audioItem = m_table->item(row, COL_AUDIO_SOURCE);
	if (audioItem)
		audioItem->setText(e.audioSourceName.isEmpty()
					  ? "(same)"
					  : e.audioSourceName);
}

/* ------------------------------------------------------------------ */
/* Source / Encoder enumeration                                       */
/* ------------------------------------------------------------------ */

QStringList MultiRecordDock::enumerateSources()
{
	QStringList list;

	/* Only enumerate actual sources (not scenes) that have
	 * video or audio output capability. */
	auto cb = [](void *param, obs_source_t *source) -> bool {
		auto *l = static_cast<QStringList *>(param);
		uint32_t flags = obs_source_get_output_flags(source);
		if (flags & (OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO)) {
			const char *name = obs_source_get_name(source);
			if (name && *name)
				l->append(QString::fromUtf8(name));
		}
		return true;
	};
	obs_enum_sources(cb, &list);

	list.sort(Qt::CaseInsensitive);
	list.removeDuplicates();
	return list;
}

void MultiRecordDock::enumerateEncoders()
{
	m_videoEncoderIds.clear();
	m_videoEncoderNames.clear();
	m_audioEncoderIds.clear();
	m_audioEncoderNames.clear();

	auto vcb = [](void *param, const char *id, const char *name) -> bool {
		auto *dock = static_cast<MultiRecordDock *>(param);
		dock->m_videoEncoderIds.append(QString::fromUtf8(id));
		dock->m_videoEncoderNames.append(QString::fromUtf8(name));
		return true;
	};
	record_pipeline_enum_video_encoders(vcb, this);

	auto acb = [](void *param, const char *id, const char *name) -> bool {
		auto *dock = static_cast<MultiRecordDock *>(param);
		dock->m_audioEncoderIds.append(QString::fromUtf8(id));
		dock->m_audioEncoderNames.append(QString::fromUtf8(name));
		return true;
	};
	record_pipeline_enum_audio_encoders(acb, this);
}

/* ------------------------------------------------------------------ */
/* Slots                                                              */
/* ------------------------------------------------------------------ */

void MultiRecordDock::onAddPair()
{
	RecordingEntry entry;
	entry.container = "mkv";
	entry.videoEncoderId = "obs_x264";
	entry.audioEncoderId = "ffmpeg_aac";
	entry.videoBitrate = 2500;
	entry.audioBitrate = 160;

	config_t *cfg = obs_frontend_get_profile_config();
	if (cfg) {
		const char *path =
			config_get_string(cfg, "SimpleOutput", "FilePath");
		if (path)
			entry.outputDir = QString::fromUtf8(path);
	}

	/* Open settings dialog for the new pair */
	PairSettingsDialog dlg(entry, m_sourceList,
			       m_videoEncoderIds, m_videoEncoderNames,
			       m_audioEncoderIds, m_audioEncoderNames,
			       this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	m_entries.append(entry);
	addEntryRow(entry);
}

void MultiRecordDock::onRemoveSelected()
{
	int row = m_table->currentRow();
	if (row < 0 || row >= m_entries.size())
		return;

	auto &entry = m_entries[row];
	if (entry.pipeline) {
		record_pipeline_destroy(entry.pipeline);
		entry.pipeline = nullptr;
	}

	m_entries.remove(row);
	m_table->removeRow(row);
}

void MultiRecordDock::onStartAll()
{
	for (int i = 0; i < m_entries.size(); i++) {
		if (!m_entries[i].pipeline ||
		    record_pipeline_get_state(m_entries[i].pipeline) ==
			    PIPELINE_IDLE) {
			onStartRow(i);
		}
	}
}

void MultiRecordDock::onStopAll()
{
	for (int i = 0; i < m_entries.size(); i++) {
		if (m_entries[i].pipeline)
			onStopRow(i);
	}
}

record_pipeline_config
MultiRecordDock::buildPipelineConfig(const RecordingEntry &entry)
{
	struct record_pipeline_config cfg;
	record_pipeline_config_init(&cfg);

	bfree(cfg.video_source_name);
	cfg.video_source_name =
		bstrdup(entry.videoSourceName.toUtf8().constData());

	bfree(cfg.audio_source_name);
	cfg.audio_source_name =
		entry.audioSourceName.isEmpty()
			? NULL
			: bstrdup(entry.audioSourceName.toUtf8().constData());

	bfree(cfg.output_dir);
	cfg.output_dir = bstrdup(entry.outputDir.toUtf8().constData());

	bfree(cfg.container);
	cfg.container = bstrdup(entry.container.toUtf8().constData());

	bfree(cfg.video_encoder_id);
	cfg.video_encoder_id =
		bstrdup(entry.videoEncoderId.toUtf8().constData());

	bfree(cfg.audio_encoder_id);
	cfg.audio_encoder_id =
		bstrdup(entry.audioEncoderId.toUtf8().constData());

	cfg.video_bitrate = entry.videoBitrate;
	cfg.audio_bitrate = entry.audioBitrate;
	cfg.width = entry.width;
	cfg.height = entry.height;
	cfg.fps_num = entry.fpsNum;
	cfg.fps_den = entry.fpsDen;

	return cfg;
}

void MultiRecordDock::onStartRow(int row)
{
	if (row < 0 || row >= m_entries.size())
		return;

	auto &entry = m_entries[row];

	if (entry.videoSourceName.isEmpty()) {
		QMessageBox::warning(this, "Multi Recorder",
				     "Please configure this pair first.");
		return;
	}

	if (entry.outputDir.isEmpty()) {
		QMessageBox::warning(this, "Multi Recorder",
				     "Please set an output directory.");
		return;
	}

	if (entry.pipeline) {
		record_pipeline_destroy(entry.pipeline);
		entry.pipeline = nullptr;
	}

	record_pipeline_config cfg = buildPipelineConfig(entry);
	entry.pipeline = record_pipeline_create(&cfg);
	record_pipeline_config_free(&cfg);

	if (!record_pipeline_start(entry.pipeline)) {
		const char *err = record_pipeline_get_error(entry.pipeline);
		QMessageBox::critical(
			this, "Multi Recorder",
			QString("Failed to start recording: %1")
				.arg(err ? err : "unknown error"));
		record_pipeline_destroy(entry.pipeline);
		entry.pipeline = nullptr;
	}
}

void MultiRecordDock::onStopRow(int row)
{
	if (row < 0 || row >= m_entries.size())
		return;

	auto &entry = m_entries[row];
	if (entry.pipeline) {
		record_pipeline_stop(entry.pipeline);
		record_pipeline_destroy(entry.pipeline);
		entry.pipeline = nullptr;
	}
}

void MultiRecordDock::onEditRow(int row)
{
	if (row < 0 || row >= m_entries.size())
		return;

	auto &entry = m_entries[row];

	/* Don't allow editing while recording */
	if (entry.pipeline &&
	    record_pipeline_get_state(entry.pipeline) == PIPELINE_RECORDING) {
		QMessageBox::information(
			this, "Multi Recorder",
			"Stop recording before editing settings.");
		return;
	}

	PairSettingsDialog dlg(entry, m_sourceList,
			       m_videoEncoderIds, m_videoEncoderNames,
			       m_audioEncoderIds, m_audioEncoderNames,
			       this);
	if (dlg.exec() == QDialog::Accepted)
		updateRowDisplay(row);
}

void MultiRecordDock::onRefreshSources()
{
	m_sourceList = enumerateSources();
	enumerateEncoders();
}

void MultiRecordDock::onStatusTimer()
{
	for (int row = 0; row < m_entries.size(); row++) {
		auto *statusLabel = qobject_cast<QLabel *>(
			m_table->cellWidget(row, COL_STATUS));

		auto *actWidget = m_table->cellWidget(row, COL_ACTIONS);
		QToolButton *startBtn = nullptr;
		QToolButton *stopBtn = nullptr;
		if (actWidget) {
			auto btns = actWidget->findChildren<QToolButton *>();
			if (btns.size() >= 3) {
				/* [0]=settings, [1]=start, [2]=stop */
				startBtn = btns[1];
				stopBtn = btns[2];
			}
		}

		const auto &entry = m_entries[row];
		enum record_pipeline_state state = PIPELINE_IDLE;
		if (entry.pipeline)
			state = record_pipeline_get_state(entry.pipeline);

		QString statusText;
		QString statusStyle;
		bool canStart = false;
		bool canStop = false;

		switch (state) {
		case PIPELINE_IDLE:
			statusText = "Idle";
			statusStyle = "";
			canStart = true;
			break;
		case PIPELINE_STARTING:
			statusText = "Starting";
			statusStyle = "color: orange;";
			break;
		case PIPELINE_RECORDING:
			statusText = "REC";
			statusStyle =
				"color: #ff4444; font-weight: bold;";
			canStop = true;
			break;
		case PIPELINE_STOPPING:
			statusText = "Stopping";
			statusStyle = "color: orange;";
			break;
		case PIPELINE_ERROR:
			statusText = "Error";
			statusStyle = "color: #ff4444;";
			canStart = true;
			break;
		}

		if (statusLabel) {
			statusLabel->setText(statusText);
			statusLabel->setStyleSheet(statusStyle);
		}
		if (startBtn)
			startBtn->setEnabled(canStart);
		if (stopBtn)
			stopBtn->setEnabled(canStop);
	}
}

/* ------------------------------------------------------------------ */
/* Config persistence                                                 */
/* ------------------------------------------------------------------ */

void MultiRecordDock::saveConfig()
{
	config_t *cfg = obs_frontend_get_profile_config();
	if (!cfg)
		return;

	config_set_int(cfg, "MultiRecord", "NumEntries", m_entries.size());

	for (int i = 0; i < m_entries.size(); i++) {
		const auto &e = m_entries[i];
		QString prefix = QString("MultiRecord.Entry%1.").arg(i);

		auto setStr = [&](const char *key, const QString &val) {
			config_set_string(cfg, "MultiRecord",
					  (prefix + key).toUtf8().constData(),
					  val.toUtf8().constData());
		};
		auto setInt = [&](const char *key, int val) {
			config_set_int(cfg, "MultiRecord",
				       (prefix + key).toUtf8().constData(),
				       val);
		};

		setStr("VideoSource", e.videoSourceName);
		setStr("AudioSource", e.audioSourceName);
		setStr("OutputDir", e.outputDir);
		setStr("Container", e.container);
		setStr("VideoEncoder", e.videoEncoderId);
		setStr("AudioEncoder", e.audioEncoderId);
		setInt("VideoBitrate", e.videoBitrate);
		setInt("AudioBitrate", e.audioBitrate);
		setInt("Width", e.width);
		setInt("Height", e.height);
		setInt("FpsNum", e.fpsNum);
		setInt("FpsDen", e.fpsDen);
	}

	config_save(cfg);
}

void MultiRecordDock::loadConfig()
{
	config_t *cfg = obs_frontend_get_profile_config();
	if (!cfg)
		return;

	int numEntries =
		(int)config_get_int(cfg, "MultiRecord", "NumEntries");
	if (numEntries <= 0)
		return;

	for (int i = 0; i < numEntries; i++) {
		QString prefix = QString("MultiRecord.Entry%1.").arg(i);

		auto getStr = [&](const char *key) -> QString {
			const char *val = config_get_string(
				cfg, "MultiRecord",
				(prefix + key).toUtf8().constData());
			return val ? QString::fromUtf8(val) : QString();
		};
		auto getInt = [&](const char *key) -> int {
			return (int)config_get_int(
				cfg, "MultiRecord",
				(prefix + key).toUtf8().constData());
		};

		RecordingEntry entry;
		entry.videoSourceName = getStr("VideoSource");
		entry.audioSourceName = getStr("AudioSource");
		entry.outputDir = getStr("OutputDir");
		entry.container = getStr("Container");
		entry.videoEncoderId = getStr("VideoEncoder");
		entry.audioEncoderId = getStr("AudioEncoder");
		entry.videoBitrate = getInt("VideoBitrate");
		entry.audioBitrate = getInt("AudioBitrate");
		entry.width = getInt("Width");
		entry.height = getInt("Height");
		entry.fpsNum = getInt("FpsNum");
		entry.fpsDen = getInt("FpsDen");

		if (entry.container.isEmpty())
			entry.container = "mkv";
		if (entry.videoEncoderId.isEmpty())
			entry.videoEncoderId = "obs_x264";
		if (entry.audioEncoderId.isEmpty())
			entry.audioEncoderId = "ffmpeg_aac";
		if (entry.videoBitrate <= 0)
			entry.videoBitrate = 2500;
		if (entry.audioBitrate <= 0)
			entry.audioBitrate = 160;
		if (entry.fpsNum <= 0)
			entry.fpsNum = 30;
		if (entry.fpsDen <= 0)
			entry.fpsDen = 1;

		m_entries.append(entry);
		addEntryRow(entry);
	}
}

/* ------------------------------------------------------------------ */
/* C-linkage registration function (called from plugin-main.c)        */
/* ------------------------------------------------------------------ */

extern "C" void multi_record_dock_register(void)
{
	auto *main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window)
		return;

	auto *dock = new MultiRecordDock(main_window);

	if (!obs_frontend_add_dock_by_id("multi-record-dock",
					 "Multi Recorder", dock)) {
		main_window->addDockWidget(Qt::BottomDockWidgetArea, dock);
		dock->setFloating(true);
		dock->hide();

		auto *viewMenu =
			main_window->findChild<QMenu *>("menuDocks");
		if (viewMenu)
			viewMenu->addAction(dock->toggleViewAction());
	}
}
