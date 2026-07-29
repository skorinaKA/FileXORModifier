#include "fileprocessor.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>

FileProcessor::FileProcessor(QObject *parent) : QObject(parent) {}

FileProcessor::~FileProcessor()
{
    stopProcessing();
}

void FileProcessor::processFiles(const QStringList &files,
                                 const QByteArray &xorValue,
                                 bool deleteInput,
                                 const QString &outputDir,
                                 bool overwrite,
                                 bool addCounter)
{
    m_stopped = false;
    m_paused = false;

    const int bufferSize = 65536; // 64 КБ
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    for (int i = 0; i < files.size(); ++i) {
        if (m_stopped) break;

        QString inputFilePath = files.at(i);
        QFileInfo fi(inputFilePath);
        QString baseName = fi.completeBaseName();
        QString suffix = fi.suffix();
        if (!suffix.isEmpty()) suffix.prepend('.');
        QString outputFilePath;

        if (overwrite) {
            outputFilePath = outputDir + "/" + fi.fileName();
        } else if (addCounter) {
            outputFilePath = generateUniqueName(outputDir, baseName, suffix, true);
        } else {
            outputFilePath = outputDir + "/" + fi.fileName();
        }

        QFile inputFile(inputFilePath);
        if (!inputFile.open(QIODevice::ReadOnly)) {
            emit fileCompleted(i, false, QString("Не удалось открыть входной файл: %1").arg(inputFile.errorString()));
            continue;
        }

        QFile outputFile(outputFilePath);
        if (!outputFile.open(QIODevice::WriteOnly)) {
            emit fileCompleted(i, false, QString("Не удалось создать выходной файл: %1").arg(outputFile.errorString()));
            inputFile.close();
            continue;
        }

        qint64 total = fi.size();
        qint64 processed = 0;
        int keySize = xorValue.size(); // всегда 8
        const char *keyData = xorValue.constData();

        while (!m_stopped && !inputFile.atEnd()) {
            {
                QMutexLocker locker(&m_controlMutex);
                while (m_paused && !m_stopped) {
                    emit processingPaused();
                    m_pauseCondition.wait(&m_controlMutex);
                }
                if (m_stopped) break;
            }

            qint64 bytesRead = inputFile.read(buffer.data(), bufferSize);
            if (bytesRead <= 0) break;

            char *data = buffer.data();
            for (qint64 j = 0; j < bytesRead; ++j) {
                data[j] ^= keyData[j % keySize];
            }

            outputFile.write(data, bytesRead);
            processed += bytesRead;
            emit fileProgress(i, processed, total);
        }

        inputFile.close();
        outputFile.close();

        bool success = !m_stopped && (processed == total);
        QString message;
        if (m_stopped) {
            message = "Обработка прервана";
        } else if (processed != total) {
            message = "Не все данные обработаны (возможно ошибка чтения/записи)";
        } else {
            message = "Успешно";
        }

        emit fileCompleted(i, success, message);

        if (deleteInput && success) {
            QFile::remove(inputFilePath);
        }

        if (m_stopped) break;
    }

    emit allFinished();
}

QString FileProcessor::generateUniqueName(const QString &basePath,
                                          const QString &baseName,
                                          const QString &suffix,
                                          bool addCounter)
{
    QString candidate = basePath + "/" + baseName + suffix;
    if (!QFile::exists(candidate))
        return candidate;

    if (!addCounter)
        return candidate;

    int counter = 1;
    while (true) {
        QString newName = QString("%1/%2_%3%4").arg(basePath, baseName).arg(counter).arg(suffix);
        if (!QFile::exists(newName))
            return newName;
        ++counter;
    }
}

void FileProcessor::pauseProcessing()
{
    QMutexLocker locker(&m_controlMutex);
    m_paused = true;
}

void FileProcessor::resumeProcessing()
{
    QMutexLocker locker(&m_controlMutex);
    m_paused = false;
    m_pauseCondition.wakeAll();
    emit processingResumed();
}

void FileProcessor::stopProcessing()
{
    QMutexLocker locker(&m_controlMutex);
    m_stopped = true;
    m_paused = false;
    m_pauseCondition.wakeAll();
}