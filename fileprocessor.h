#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QByteArray>
#include <QStringList>

class FileProcessor : public QObject
{
    Q_OBJECT
public:
    explicit FileProcessor(QObject *parent = nullptr);
    ~FileProcessor();

public slots:
    // Запуск обработки списка файлов
    void processFiles(const QStringList &files,
                      const QByteArray &xorValue,
                      bool deleteInput,
                      const QString &outputDir,
                      bool overwrite,
                      bool addCounter);
    // Приостановить обработку
    void pauseProcessing();
    // Возобновить обработку
    void resumeProcessing();
    // Остановить обработку
    void stopProcessing();

signals:
    // Прогресс по текущему файлу (индекс, обработано байт, всего)
    void fileProgress(int fileIndex, qint64 processed, qint64 total);
    // Завершение обработки одного файла
    void fileCompleted(int fileIndex, bool success, const QString &message);
    // Все файлы обработаны (или остановлено)
    void allFinished();
    // Поставлено на паузу
    void processingPaused();
    // Возобновлено
    void processingResumed();

private:
    // Генерация уникального имени при коллизии
    QString generateUniqueName(const QString &basePath, const QString &baseName,
                               const QString &suffix, bool addCounter);

    // Флаги управления
    bool m_paused = false;
    bool m_stopped = false;
    QMutex m_controlMutex;
    QWaitCondition m_pauseCondition;
};

#endif // FILEPROCESSOR_H