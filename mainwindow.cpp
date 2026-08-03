#include "mainwindow.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QMessageBox>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();

    m_processor = new FileProcessor();
    m_processor->moveToThread(&m_workerThread);

    // Кнопки управления
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(m_resumeBtn, &QPushButton::clicked, this, &MainWindow::onResume);
    connect(m_searchBtn, &QPushButton::clicked, this, &MainWindow::onSearch);

    // Сигналы от обработчика
    connect(m_processor, &FileProcessor::fileProgress,
            this, &MainWindow::onFileProgress);
    connect(m_processor, &FileProcessor::fileCompleted,
            this, &MainWindow::onFileCompleted);
    connect(m_processor, &FileProcessor::allFinished,
            this, &MainWindow::onAllFinished);
    connect(m_processor, &FileProcessor::processingPaused,
            this, &MainWindow::onProcessingPaused);
    connect(m_processor, &FileProcessor::processingResumed,
            this, &MainWindow::onProcessingResumed);

    m_workerThread.start();

    // Таймер опроса (если включён)
    connect(&m_pollTimer, &QTimer::timeout, this, &MainWindow::onTimerTick);
}

MainWindow::~MainWindow()
{
    stopWorker();
    m_workerThread.quit();
    m_workerThread.wait();
    delete m_processor;
}

void MainWindow::setupUi()
{
    setWindowTitle("XOR File Modifier");

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // Группа настроек
    QGroupBox *settingsGroup = new QGroupBox("Настройки");
    QFormLayout *form = new QFormLayout(settingsGroup);

    m_inputPathEdit = new QLineEdit(QDir::currentPath());
    QPushButton *browseInputBtn = new QPushButton("...");
    QHBoxLayout *inputLay = new QHBoxLayout();
    inputLay->addWidget(m_inputPathEdit);
    inputLay->addWidget(browseInputBtn);
    form->addRow("Путь для поиска:", inputLay);

    m_maskEdit = new QLineEdit("*.txt");
    form->addRow("Маска файлов:", m_maskEdit);

    m_outputPathEdit = new QLineEdit(QDir::currentPath() + "/output");
    QPushButton *browseOutBtn = new QPushButton("...");
    QHBoxLayout *outLay = new QHBoxLayout();
    outLay->addWidget(m_outputPathEdit);
    outLay->addWidget(browseOutBtn);
    form->addRow("Путь для сохранения:", outLay);

    m_deleteInputCheck = new QCheckBox("Удалять входные файлы");
    form->addRow(m_deleteInputCheck);

    m_collisionCombo = new QComboBox();
    m_collisionCombo->addItem("Перезапись", false);
    m_collisionCombo->addItem("Счетчик к имени", true);
    form->addRow("При совпадении имён:", m_collisionCombo);

    m_timerCheck = new QCheckBox("Работа по таймеру");
    m_periodSpin = new QSpinBox();
    m_periodSpin->setRange(1, 3600);
    m_periodSpin->setValue(5);
    m_periodSpin->setSuffix(" сек");
    m_periodSpin->setEnabled(false);
    QHBoxLayout *timerLay = new QHBoxLayout();
    timerLay->addWidget(m_timerCheck);
    timerLay->addWidget(new QLabel("Период:"));
    timerLay->addWidget(m_periodSpin);
    form->addRow("Режим таймера:", timerLay);

    m_hexValueEdit = new QLineEdit();
    m_hexValueEdit->setMaxLength(16);
    m_hexValueEdit->setPlaceholderText("16 шестнадцатеричных цифр");
    QRegularExpression hexRegex("^[0-9A-Fa-f]{16}$");
    m_hexValueEdit->setValidator(new QRegularExpressionValidator(hexRegex, this));
    form->addRow("8-байт XOR (hex):", m_hexValueEdit);

    mainLayout->addWidget(settingsGroup);

    // Кнопки управления
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("Старт");
    m_pauseBtn = new QPushButton("Пауза");
    m_resumeBtn = new QPushButton("Продолжить");
    m_searchBtn = new QPushButton("Поиск");   // кнопка поиска
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_pauseBtn);
    btnLayout->addWidget(m_resumeBtn);
    btnLayout->addWidget(m_searchBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // Таблица файлов
    m_filesTable = new QTableWidget(0, 4);
    m_filesTable->setHorizontalHeaderLabels({"Имя файла", "Размер", "Прогресс", "Статус"});
    m_filesTable->horizontalHeader()->setStretchLastSection(true);
    m_filesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainLayout->addWidget(m_filesTable);

    m_totalProgressBar = new QProgressBar();
    m_totalProgressBar->setRange(0, 100);
    m_totalProgressBar->setValue(0);
    mainLayout->addWidget(m_totalProgressBar);

    connect(browseInputBtn, &QPushButton::clicked, this, &MainWindow::onBrowseInputPath);
    connect(browseOutBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutputPath);
    connect(m_timerCheck, &QCheckBox::toggled, m_periodSpin, &QSpinBox::setEnabled);
}

void MainWindow::onBrowseInputPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку для поиска", m_inputPathEdit->text());
    if (!dir.isEmpty()) m_inputPathEdit->setText(dir);
}

void MainWindow::onBrowseOutputPath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку для сохранения", m_outputPathEdit->text());
    if (!dir.isEmpty()) m_outputPathEdit->setText(dir);
}

void MainWindow::onSearch()
{
    // Очищаем таблицу и список файлов
    m_filesTable->setRowCount(0);
    m_pendingFiles.clear();

    // Сканируем директорию и добавляем файлы
    QFileInfoList files = scanDirectory();
    addFilesToTable(files);

    // Обновляем кнопки (если есть файлы – разрешаем Старт, если не идёт обработка)
    updateButtonsState(m_isProcessing, m_isPaused);
}

QFileInfoList MainWindow::scanDirectory()
{
    QDir dir(m_inputPathEdit->text());
    QStringList masks = m_maskEdit->text().split(QRegularExpression("[; ]"), Qt::SkipEmptyParts);
    if (masks.isEmpty()) masks << "*";
    dir.setNameFilters(masks);
    return dir.entryInfoList(QDir::Files);
}

void MainWindow::addFilesToTable(const QFileInfoList &files)
{
    for (const QFileInfo &fi : files) {
        QString absPath = fi.absoluteFilePath();
        // Избегаем дубликатов (хотя после очистки их не будет)
        if (!m_pendingFiles.contains(absPath)) {
            m_pendingFiles << absPath;
            int row = m_filesTable->rowCount();
            m_filesTable->insertRow(row);
            m_filesTable->setItem(row, 0, new QTableWidgetItem(fi.fileName()));
            m_filesTable->setItem(row, 1, new QTableWidgetItem(QString::number(fi.size())));
            m_filesTable->setItem(row, 2, new QTableWidgetItem("0%"));
            m_filesTable->setItem(row, 3, new QTableWidgetItem("Ожидание"));
        }
    }
}

void MainWindow::onStart()
{
    if (m_timerCheck->isChecked()) {
        // Работа по таймеру: запуск/остановка опроса
        if (!m_pollingActive) {
            // Запускаем опрос
            m_pollTimer.start(m_periodSpin->value() * 1000);
            m_pollingActive = true;
            m_startBtn->setText("Остановить опрос");
            // Если сейчас нет обработки и есть файлы, можно сразу начать обработку
            if (!m_isProcessing && !m_pendingFiles.isEmpty()) {
                startProcessingFiles();
            }
        } else {
            // Останавливаем опрос
            m_pollTimer.stop();
            m_pollingActive = false;
            m_startBtn->setText("Старт");
        }
    } else {
        // Разовый запуск обработки найденных файлов
        if (m_pendingFiles.isEmpty()) {
            QMessageBox::information(this, "Информация", "Нет файлов для обработки. Сначала выполните поиск.");
            return;
        }
        startProcessingFiles();
    }
}

void MainWindow::onPause()
{
    m_processor->pauseProcessing();
    updateButtonsState(true, true);
}

void MainWindow::onResume()
{
    m_processor->resumeProcessing();
    updateButtonsState(true, false);
}

void MainWindow::onTimerTick()
{
    // Автоматический поиск и запуск обработки при активном таймере
    QFileInfoList files = scanDirectory();
    addFilesToTable(files);
    if (!m_isProcessing && !m_pendingFiles.isEmpty()) {
        startProcessingFiles();
    }
}

void MainWindow::startProcessingFiles()
{
    if (m_pendingFiles.isEmpty()) {
        updateButtonsState(false, false);
        return;
    }

    QString hexStr = m_hexValueEdit->text().trimmed();
    if (hexStr.size() != 16) {
        QMessageBox::warning(this, "Ошибка", "Введите ровно 16 шестнадцатеричных цифр (8 байт).");
        updateButtonsState(m_isProcessing, m_isPaused);
        return;
    }
    QByteArray xorKey = QByteArray::fromHex(hexStr.toLatin1());
    if (xorKey.size() != 8) {
        QMessageBox::warning(this, "Ошибка", "Некорректный hex-код.");
        updateButtonsState(m_isProcessing, m_isPaused);
        return;
    }

    bool deleteInput = m_deleteInputCheck->isChecked();
    QString outputDir = m_outputPathEdit->text();
    bool overwrite = (m_collisionCombo->currentData().toBool() == false);
    bool addCounter = m_collisionCombo->currentData().toBool();

    QDir().mkpath(outputDir);

    QStringList filesToProcess = m_pendingFiles;
    m_pendingFiles.clear();   // очищаем очередь, чтобы избежать повторов

    m_isProcessing = true;
    m_currentFileIndex = -1;
    updateButtonsState(true, false);

    QMetaObject::invokeMethod(m_processor, "processFiles",
                              Qt::QueuedConnection,
                              Q_ARG(QStringList, filesToProcess),
                              Q_ARG(QByteArray, xorKey),
                              Q_ARG(bool, deleteInput),
                              Q_ARG(QString, outputDir),
                              Q_ARG(bool, overwrite),
                              Q_ARG(bool, addCounter));
}

void MainWindow::onFileProgress(int fileIndex, qint64 processed, qint64 total)
{
    if (fileIndex < 0 || fileIndex >= m_filesTable->rowCount()) return;
    m_currentFileIndex = fileIndex;

    int percent = total ? static_cast<int>(processed * 100 / total) : 100;
    m_filesTable->item(fileIndex, 2)->setText(QString("%1%").arg(percent));
    m_filesTable->item(fileIndex, 3)->setText("Обработка...");

    m_totalProgressBar->setValue(percent);
}

void MainWindow::onFileCompleted(int fileIndex, bool success, const QString &message)
{
    if (fileIndex < 0 || fileIndex >= m_filesTable->rowCount()) return;
    m_filesTable->item(fileIndex, 3)->setText(success ? "Готово" : ("Ошибка: " + message));
    m_filesTable->item(fileIndex, 2)->setText(success ? "100%" : "Прервано");
}

void MainWindow::onAllFinished()
{
    m_isProcessing = false;
    m_isPaused = false;
    m_currentFileIndex = -1;
    updateButtonsState(false, false);
    m_totalProgressBar->setValue(0);

    // Если опрос активен и ещё есть файлы, запускаем следующую партию
    if (m_pollingActive && !m_pendingFiles.isEmpty()) {
        startProcessingFiles();
    }
    // Если таймер не активен, просто оставляем таблицу очищенной (файлы уже обработаны)
}

void MainWindow::onProcessingPaused()
{
    m_isPaused = true;
    if (m_currentFileIndex >= 0 && m_currentFileIndex < m_filesTable->rowCount())
        m_filesTable->item(m_currentFileIndex, 3)->setText("Пауза");
    updateButtonsState(true, true);
}

void MainWindow::onProcessingResumed()
{
    m_isPaused = false;
    if (m_currentFileIndex >= 0 && m_currentFileIndex < m_filesTable->rowCount())
        m_filesTable->item(m_currentFileIndex, 3)->setText("Обработка...");
    updateButtonsState(true, false);
}

void MainWindow::updateButtonsState(bool running, bool paused)
{
    bool polling = m_pollingActive;

    // Кнопка «Старт»: если активен опрос, она всегда доступна для остановки опроса;
    // иначе доступна, только если не идёт обработка.
    m_startBtn->setEnabled(polling || !running);

    m_pauseBtn->setEnabled(running && !paused);
    m_resumeBtn->setEnabled(running && paused);

    // Кнопка «Поиск» активна всегда
    m_searchBtn->setEnabled(true);
}

void MainWindow::stopWorker()
{
    if (m_isProcessing) {
        m_processor->stopProcessing();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Останавливаем таймер, если активен
    if (m_pollingActive) {
        m_pollTimer.stop();
        m_pollingActive = false;
    }
    stopWorker();
    m_workerThread.quit();
    m_workerThread.wait();
    event->accept();
}