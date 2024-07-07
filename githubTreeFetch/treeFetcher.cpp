#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include "treeFetcher.h"

TreeFetcher::TreeFetcher(QString repoListFile, QString outputDirectory, QString anomalyLogFile, QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod, bool showProgress, int errorStreakLimit, QObject *parent)
    : QObject(parent), networkAccessManager(new QNetworkAccessManager(this)), apiTokenDispatcher(apiTokens, unauthenticatedMode, dispatchMethod), repoListFile(repoListFile), outputDirectory(outputDirectory), anomalyLogFile(anomalyLogFile), showProgress(showProgress), errorStreakLimit(errorStreakLimit), errorStreak(0) {
    networkAccessManager->setTransferTimeout();
    connect(networkAccessManager, &QNetworkAccessManager::finished, this, &TreeFetcher::onRequestFinished);
}


void TreeFetcher::run() {
    if (!repoListFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open repository list\n";
        emit finished();
        return;
    }

    if (!anomalyLogFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open anomaly log file for writing\n";
        emit finished();
        return;
    }

    repoListStream.setDevice(&repoListFile);

    fetchNextTree();
}


QString TreeFetcher::treeFilePath() {
    return outputDirectory.filePath("tree_" + QString(currentRepoFullName).replace('/', '#') + ".json");
}


void TreeFetcher::fetchNextTree() {
    while (!repoListStream.atEnd()) {
        currentRepoFullName = repoListStream.readLine();

        if (!QFileInfo::exists(treeFilePath())) {
            fetchTree();
            return;
        }

        QTextStream(stdout) << "Tree file already exists for " << currentRepoFullName << "\n";
    }

    emit finished();
}


void TreeFetcher::fetchTree() {
    if (showProgress) {
        QTextStream(stdout) << "a";
    }

    apiTokenDispatcher.processResets();
    QString apiToken = apiTokenDispatcher.getApiToken();
    if (apiToken.isNull()) {
        long long waitSeconds = apiTokenDispatcher.secUntilTokenAvailable();
        QTextStream(stdout) << "No API token available\n"
                               "Rate limit reached, waiting " << waitSeconds / 60 << "m " << waitSeconds % 60 << "s\n";
        QTimer::singleShot(waitSeconds * 1000, this, &TreeFetcher::fetchTree);
        return;
    }
    lastApiToken = apiToken;

    //qDebug() << "API token: " << apiToken;

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::UserAgentHeader, "TreeFetcher");
    if (!apiToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + apiToken).toUtf8());
    }
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    request.setUrl("https://api.github.com/repos/" + currentRepoFullName + "/git/trees/HEAD?recursive=1");

    networkAccessManager->get(request);
}


void TreeFetcher::onRequestFinished(QNetworkReply *reply) {
    reply->deleteLater();

    if (showProgress) {
        QTextStream(stdout) << "b\n";
    }

    if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentAccessDenied && reply->error() != QNetworkReply::UnknownContentError && reply->error() != QNetworkReply::ContentNotFoundError && reply->error() != QNetworkReply::ContentConflictError) {
        QTextStream(stderr) << "Request failed:\n" << reply->error() << "\n" << reply->errorString() << "\n";

        errorStreak++;
        if (errorStreak > errorStreakLimit) {
            QTextStream(stderr) << "Error streak limit reached\n";
            emit finished();
            return;
        }

        QTimer::singleShot(5000, this, &TreeFetcher::fetchTree);
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
            QTextStream(stderr) << "Could not parse 403 response\n";
            emit finished();
            return;
        }

        if (!json.isObject()) {
            QTextStream(stderr) << "Data from 403 response is not a json object\n";
            emit finished();
            return;
        }

        QString message = json["message"].toString();
        if (message.isNull()) {
            QTextStream(stderr) << "No message string in 403 response\n";
            emit finished();
            return;
        }

        if (message.startsWith("API rate limit exceeded")) {
            if (lastApiToken.isEmpty()) {
                QTextStream(stdout) << "Rate limit exceeded for unauthenticated requests\n";
            } else {
                QTextStream(stdout) << "Rate limit exceeded for token " << lastApiToken << "\n";
            }
            QTimer::singleShot(0, this, &TreeFetcher::fetchTree);
            return;
        } else if (message == "Repository access blocked") {
            if (!json["block"].isObject()) {
                QTextStream(stderr) << "No block object in 403 response\n";
                emit finished();
                return;
            }
            QJsonObject block = json["block"].toObject();

            QString blockReason = block["reason"].toString();
            if (blockReason != "unavailable" && blockReason != "tos" && blockReason != "sensitive_data" && blockReason != "dmca" && blockReason != "private_information") {
                QTextStream(stderr) << "Unknown block reason " << block["reason"].toString() << " for repository " << currentRepoFullName << "\n";
                emit finished();
                return;
            }

            QTextStream(stdout) << "Repository access blocked for " << currentRepoFullName << " (reason: " << blockReason << ")\n";
            QTextStream(&anomalyLogFile) << "Repository access blocked for " << currentRepoFullName << " (reason: " << blockReason << ")\n";
            QTimer::singleShot(0, this, &TreeFetcher::fetchNextTree);
            return;
        } else {
            QTextStream(stderr) << "Unknown 403 response message: " << message << "\n";
            emit finished();
            return;
        }
    }

    if (reply->error() == QNetworkReply::ContentNotFoundError) {
        QTextStream(stdout) << "No tree found for " << currentRepoFullName << "\n";
        QTextStream(&anomalyLogFile) << "No tree found for " << currentRepoFullName << "\n";
        QTimer::singleShot(0, this, &TreeFetcher::fetchNextTree);
        return;
    }

    if (reply->error() == QNetworkReply::ContentConflictError) {
        QByteArray data = reply->readAll();

        QJsonDocument json = QJsonDocument::fromJson(data);
        if (json.isNull()) {
            QTextStream(stderr) << "Could not parse 409 response\n";
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
            QTextStream(stdout) << "Empty repository: " << currentRepoFullName << "\n";
            QTextStream(&anomalyLogFile) << "Empty repository: " << currentRepoFullName << "\n";
            QTimer::singleShot(0, this, &TreeFetcher::fetchNextTree);
            return;
        } else {
            QTextStream(stderr) << "Unknown 409 response message: " << message << "\n";
            emit finished();
            return;
        }
    }

    QFile treeFile(treeFilePath());
    if (!treeFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODeviceBase::NewOnly)) {
        QTextStream(stderr) << "Could not open tree output file\n";
        emit finished();
        return;
    }
    QTextStream out(&treeFile);

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

    QString url = json["url"].toString();
    if (url.isNull()) {
        QTextStream(stderr) << "No url string in response\n";
        emit finished();
        return;
    }

    QString urlFront = "https://api.github.com/repos/";
    if (!url.startsWith(urlFront)) {
        QTextStream(stderr) << "Invalid url string in response\n";
        emit finished();
        return;
    }

    if (!url.mid(urlFront.length()).startsWith(currentRepoFullName)) {
        QTextStream(stdout) << "Url from response does not match fetched repository: " << currentRepoFullName << "\n";
        QTextStream(&anomalyLogFile) << "Url from response does not match fetched repository: " << currentRepoFullName << "\n";
    }

    out << data;

    long long waitSeconds = apiTokenDispatcher.secUntilTokenAvailable();
    if (waitSeconds > 0) {
        QTextStream(stdout) << "Rate limit reached, waiting " << waitSeconds / 60 << "m " << waitSeconds % 60 << "s\n";
    }

    QTimer::singleShot(waitSeconds * 1000, this, &TreeFetcher::fetchNextTree);
}
