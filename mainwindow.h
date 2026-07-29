#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QFileInfoList>
#include <QTableWidget>
#include <QCloseEvent>
#include "fileprocessor.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QPushButton;
class QProgressBar;
class QTableWidget;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBrowseInputPath();
    void onBrowseOutputPath();
    void onStart();
    void onPause();
    void onResume();
    void onStop();
    void onTimerTick();

    void onFileProgress(int fileIndex, qint64 processed, qint64 total);
    void onFileCompleted(int fileIndex, bool success, const QString &message);
    void onAllFinished();
    void onProcessingPaused();
    void onProcessingResumed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void updateButtonsState(bool running, bool paused);
    QFileInfoList scanDirectory();
    void addFilesToTable(const QFileInfoList &files);
    void startProcessingFiles();
    void stopWorker();

    // UI элементы
    QLineEdit *m_maskEdit;
    QCheckBox *m_deleteInputCheck;
    QLineEdit *m_outputPathEdit;
    QLineEdit *m_inputPathEdit;
    QComboBox *m_collisionCombo;
    QCheckBox *m_timerCheck;
    QSpinBox *m_periodSpin;
    QLineEdit *m_hexValueEdit;

    QPushButton *m_startBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_resumeBtn;
    QPushButton *m_stopBtn;

    QTableWidget *m_filesTable;
    QProgressBar *m_totalProgressBar;

    QThread m_workerThread;
    FileProcessor *m_processor = nullptr;

    QTimer m_pollTimer;
    QStringList m_pendingFiles;

    bool m_isProcessing = false;
    bool m_isPaused = false;
    int m_currentFileIndex = -1;
};

#endif // MAINWINDOW_H