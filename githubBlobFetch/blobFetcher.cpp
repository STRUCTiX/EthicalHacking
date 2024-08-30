#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include "blobFetcher.h"

BlobFetcher::BlobFetcher(QString treeListFile, QString treeBaseDirectory, QString outputDirectory, QString unavailableBlobsFile, QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod, bool showProgress, int errorStreakLimit, QObject *parent)
    : QObject(parent), networkAccessManager(new QNetworkAccessManager(this)), apiTokenDispatcher(apiTokens, unauthenticatedMode, dispatchMethod), outputDirectory(outputDirectory), treeBaseDirectory(treeBaseDirectory), treeListFile(treeListFile), unavailableBlobsFile(unavailableBlobsFile), showProgress(showProgress), errorStreakLimit(errorStreakLimit), errorStreak(0) {
    networkAccessManager->setTransferTimeout();
    connect(networkAccessManager, &QNetworkAccessManager::finished, this, &BlobFetcher::onRequestFinished);
}


void BlobFetcher::run() {
    if (!loadPreviouslyUnavailableReposList()) {
        emit finished();
        return;
    }

    if (!loadBlobsToFetch()) {
        emit finished();
        return;
    }

    if (!unavailableBlobsFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open unavailable repos file for writing\n";
        emit finished();
        return;
    }

    unavailableBlobsStream.setDevice(&unavailableBlobsFile);

    fetchNextBlob();
}


bool BlobFetcher::loadPreviouslyUnavailableReposList() {
    QFile unavailableReposFileIn(unavailableBlobsFile.fileName());
    if (!unavailableReposFileIn.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return true;
    }

    QTextStream in(&unavailableReposFileIn);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList parts = line.split(",");
        if (parts.size() != 3) {
            QTextStream(stderr) << "Invalid line in unavailable blobs file: " << line << "\n";
            return false;
        }
        previouslyUnavailableBlobs.insert(BlobIdentifier(parts[0], parts[1]));
    }

    return true;
}


bool BlobFetcher::loadBlobsToFetch() {
    if (!treeListFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open tree list\n";
        return false;
    }

    QTextStream treePathStream(&treeListFile);
    while (!treePathStream.atEnd()) {
        QString treePath = treePathStream.readLine();

        QFile treeFile(treeBaseDirectory.filePath(treePath));
        if (!treeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream(stderr) << "Could not open tree file " << treePath << "\n";
            return false;
        }

        QTextStream treeStream(&treeFile);
        QString treeJsonString = treeStream.readAll();

        QJsonDocument treeJson = QJsonDocument::fromJson(treeJsonString.toUtf8());
        if (treeJson.isNull()) {
            QTextStream(stderr) << "Could not parse tree file " << treePath << "\n";
            return false;
        }

        if (!treeJson.isObject()) {
            QTextStream(stderr) << "Tree file " << treePath << " is not a JSON object\n";
            return false;
        }

        QString treeUrl = treeJson["url"].toString();
        if (treeUrl.isNull()) {
            QTextStream(stderr) << "Tree file " << treePath << " does not contain a tree URL\n";
            return false;
        }

        QStringList parts = treeUrl.split('/');
        if (parts.size() != 9) {
            QTextStream(stderr) << "Invalid tree URL " << treeUrl << "\n";
            return false;
        }

        QString repoFullName = parts[4] + "/" + parts[5];

        if (!treeJson["tree"].isArray()) {
            QTextStream(stderr) << "Tree file " << treePath << " does not contain a tree array\n";
            return false;
        }

        QJsonArray treeArray = treeJson["tree"].toArray();
        for (const QJsonValue &treeItemValue: treeArray) {
            QJsonObject treeItem = treeItemValue.toObject();
            if (!treeItem["type"].isString() || !treeItem["path"].isString() || !treeItem["sha"].isString()) {
                QTextStream(stderr) << "Invalid tree item\n";
                return false;
            }

            QString type = treeItem["type"].toString();
            QString path = treeItem["path"].toString();
            QString sha = treeItem["sha"].toString();

            if (type == "blob" && path.startsWith(".github/workflows/") && path.endsWith(".yml")) {
                blobsToFetch.enqueue(BlobIdentifier(repoFullName, sha));
            }
        }
    }

    return true;
}


QString BlobFetcher::blobFilePath() {
    return outputDirectory.filePath(currentBlob.sha);
}


void BlobFetcher::logAnomaly(QString message) {
    QTextStream(stdout) << "Could not fetch " << currentBlob.repo << " (" << currentBlob.sha << "), " << message << "\n";
    unavailableBlobsStream << currentBlob.repo << "," << currentBlob.sha << "," << message << Qt::endl;
}


void BlobFetcher::fetchNextBlob() {
    while (!blobsToFetch.isEmpty()) {
        currentBlob = blobsToFetch.dequeue();

        if (QFileInfo::exists(blobFilePath())) {
            QTextStream(stdout) << "Blob file already exists for " << currentBlob.repo << " (" << currentBlob.sha << ")\n";
        } else if (previouslyUnavailableBlobs.contains(currentBlob)) {
            QTextStream(stdout) << "Skipping previously unavailable blob " << currentBlob.repo << " (" << currentBlob.sha << ")\n";
        } else {
            fetchBlob();
            return;
        }
    }

    emit finished();
}


void BlobFetcher::fetchBlob() {
    if (showProgress) {
        QTextStream(stdout) << "a";
    }

    apiTokenDispatcher.processResets();
    QString apiToken = apiTokenDispatcher.getApiToken();
    if (apiToken.isNull()) {
        long long waitSeconds = apiTokenDispatcher.secUntilTokenAvailable();
        QTextStream(stdout) << "No API token available\n"
                               "Rate limit reached, waiting " << waitSeconds / 60 << "m " << waitSeconds % 60 << "s\n";
        QTimer::singleShot(waitSeconds * 1000, this, &BlobFetcher::fetchBlob);
        return;
    }
    lastApiToken = apiToken;

    //qDebug() << "API token: " << apiToken;

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::UserAgentHeader, "BlobFetcher");
    if (!apiToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + apiToken).toUtf8());
    }
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setDecompressedSafetyCheckThreshold(-1);

    request.setUrl("https://api.github.com/repos/" + currentBlob.repo + "/git/blobs/" + currentBlob.sha);

    networkAccessManager->get(request);
}


void BlobFetcher::onRequestFinished(QNetworkReply *reply) {
    reply->deleteLater();

    if (showProgress) {
        QTextStream(stdout) << "b\n";
    }

    bool httpStatusAvailable = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid();
    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString httpReasonPhrase = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

    if (!((reply->error() == QNetworkReply::NoError && httpStatusCode == 200) ||
          (reply->error() == QNetworkReply::ContentAccessDenied && httpStatusCode == 403) ||
          (reply->error() == QNetworkReply::UnknownContentError && httpStatusCode == 451) ||
          (reply->error() == QNetworkReply::ContentNotFoundError && httpStatusCode == 404) ||
          (reply->error() == QNetworkReply::ContentConflictError && httpStatusCode == 409) ||
          (reply->error() == QNetworkReply::InternalServerError && httpStatusCode == 500))) {
        QTextStream(stderr) << "Request failed:\n"
                            << "Qt error: " << reply->error() << " " << reply->errorString() << "\n";
        if (httpStatusAvailable) {
            QTextStream(stderr) << "HTTP status code: " << httpStatusCode << " " << httpReasonPhrase << "\n";
        } else {
            QTextStream(stderr) << "HTTP status code not available\n";
        }

        errorStreak++;
        if (errorStreak > errorStreakLimit) {
            QTextStream(stderr) << "Error streak limit reached\n";
            emit finished();
            return;
        }

        QTimer::singleShot(5000, this, &BlobFetcher::fetchBlob);
        return;
    }

    if (!httpStatusAvailable) {
        QTextStream(stderr) << "HTTP status code not available\n";
        emit finished();
        return;
    }

    errorStreak = 0;

    if (!apiTokenDispatcher.processRateLimitInfo(*reply, lastApiToken)) {
        emit finished();
        return;
    }

    if (reply->error() == QNetworkReply::ContentAccessDenied || reply->error() == QNetworkReply::UnknownContentError) {
        QByteArray data = reply->readAll();

        QJsonDocument json = QJsonDocument::fromJson(data);
        if (json.isNull()) {
            QTextStream(stderr) << "Could not parse " << httpStatusCode << " response:\n"
                                << data << "\n";
            emit finished();
            return;
        }

        if (!json.isObject()) {
            QTextStream(stderr) << "Data from " << httpStatusCode << " response is not a json object\n";
            emit finished();
            return;
        }

        QString message = json["message"].toString();
        if (message.isNull()) {
            QTextStream(stderr) << "No message string in " << httpStatusCode << " response\n";
            emit finished();
            return;
        }

        if (message.startsWith("API rate limit exceeded")) {
            if (lastApiToken.isEmpty()) {
                QTextStream(stdout) << "Rate limit exceeded for unauthenticated requests\n";
            } else {
                QTextStream(stdout) << "Rate limit exceeded for token " << lastApiToken << "\n";
            }
            QTimer::singleShot(0, this, &BlobFetcher::fetchBlob);
            return;
        } else if (message == "Repository access blocked") {
            if (!json["block"].isObject()) {
                QTextStream(stderr) << "No block object in " << httpStatusCode << " response\n";
                emit finished();
                return;
            }
            QJsonObject block = json["block"].toObject();

            QString blockReason = block["reason"].toString();
            if (blockReason != "unavailable" && blockReason != "tos" && blockReason != "sensitive_data" && blockReason != "dmca" && blockReason != "private_information" && blockReason != "size" && blockReason != "trademark") {
                QTextStream(stderr) << "Unknown block reason " << block["reason"].toString() << " for repository " << currentBlob.repo << "(" << currentBlob.sha << ")\n";
                emit finished();
                return;
            }

            logAnomaly("access blocked (reason " + blockReason + ")");
            QTimer::singleShot(0, this, &BlobFetcher::fetchNextBlob);
            return;
        } else {
            QTextStream(stderr) << "Unknown " << httpStatusCode << " response message: " << message << "\n";
            emit finished();
            return;
        }
    }

    if (reply->error() == QNetworkReply::ContentNotFoundError) {
        logAnomaly("blob not found");
        QTimer::singleShot(0, this, &BlobFetcher::fetchNextBlob);
        return;
    }

    if (reply->error() == QNetworkReply::ContentConflictError) {
        QByteArray data = reply->readAll();

        QTextStream(stderr) << "409 response:\n" << data << "\n";
        emit finished();
        return;

        /*QJsonDocument json = QJsonDocument::fromJson(data);
        if (json.isNull()) {
            QTextStream(stderr) << "Could not parse 409 response:\n"
                                << data << "\n";
            emit finished();
            return;
        }

        if (!json.isObject()) {
            QTextStream(stderr) << "Data from 409 response is not a json object\n";
            emit finished();
            return;
        }

        QString message = json["message"].toString();
        if (message.isNull()) {
            QTextStream(stderr) << "No message string in 409 response\n";
            emit finished();
            return;
        }

        if (message == "Git Repository is empty.") {
            logAnomaly("repository empty");
            QTimer::singleShot(0, this, &BlobFetcher::fetchNextBlob);
            return;
        } else {
            QTextStream(stderr) << "Unknown 409 response message: " << message << "\n";
            emit finished();
            return;
        }*/
    }

    if (reply->error() == QNetworkReply::InternalServerError) {
        logAnomaly("internal server error");
        QTimer::singleShot(0, this, &BlobFetcher::fetchNextBlob);
        return;
    }

    QFile blobFile(blobFilePath());
    if (!blobFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODeviceBase::NewOnly)) {
        QTextStream(stderr) << "Could not open blob output file\n";
        emit finished();
        return;
    }
    QTextStream out(&blobFile);

    QByteArray data = reply->readAll();

    QJsonDocument json = QJsonDocument::fromJson(data);
    if (json.isNull()) {
        QTextStream(stderr) << "Could not parse response\n";
        emit finished();
        return;
    }

    if (!json.isObject()) {
        QTextStream(stderr) << "Data is not a json object\n";
        emit finished();
        return;
    }

    QString contentBase64 = json["content"].toString();
    if (contentBase64.isNull()) {
        QTextStream(stderr) << "No content string in response\n";
        emit finished();
        return;
    }

    QByteArray::FromBase64Result b64DecodeResult = QByteArray::fromBase64Encoding(contentBase64.toUtf8());
    if (!b64DecodeResult) {
        QTextStream(stderr) << "Could not decode base64 content:\n" << contentBase64 << "\n";
        emit finished();
        return;
    }

    out << *b64DecodeResult;

    long long waitSeconds = apiTokenDispatcher.secUntilTokenAvailable();
    if (waitSeconds > 0) {
        QTextStream(stdout) << "Rate limit reached, waiting " << waitSeconds / 60 << "m " << waitSeconds % 60 << "s\n";
    }

    QTimer::singleShot(waitSeconds * 1000, this, &BlobFetcher::fetchNextBlob);
}
