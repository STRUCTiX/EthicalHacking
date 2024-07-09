#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include "treeFetcher.h"

TreeFetcher::TreeFetcher(QString repoListFile, QString outputDirectory, QString unavailableReposFile, QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod, bool showProgress, int errorStreakLimit, QObject *parent)
    : QObject(parent), networkAccessManager(new QNetworkAccessManager(this)), apiTokenDispatcher(apiTokens, unauthenticatedMode, dispatchMethod), outputDirectory(outputDirectory), repoListFile(repoListFile), unavailableReposFile(unavailableReposFile), showProgress(showProgress), errorStreakLimit(errorStreakLimit), errorStreak(0) {
    networkAccessManager->setTransferTimeout();
    connect(networkAccessManager, &QNetworkAccessManager::finished, this, &TreeFetcher::onRequestFinished);
}


void TreeFetcher::run() {
    if (!repoListFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open repository list\n";
        emit finished();
        return;
    }

    if (!loadPreviouslyUnavailableReposList()) {
        emit finished();
        return;
    }

    if (!unavailableReposFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open unavailable repos file for writing\n";
        emit finished();
        return;
    }

    repoListStream.setDevice(&repoListFile);
    unavailableReposStream.setDevice(&unavailableReposFile);

    fetchNextTree();
}


bool TreeFetcher::loadPreviouslyUnavailableReposList() {
    QFile unavailableReposFileIn(unavailableReposFile.fileName());
    if (!unavailableReposFileIn.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return true;
    }

    QTextStream in(&unavailableReposFileIn);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList parts = line.split(",");
        if (parts.size() != 2) {
            QTextStream(stderr) << "Invalid line in unavailable repos file: " << line << "\n";
            return false;
        }
        previouslyUnavailableRepos.insert(parts[0]);
    }

    return true;
}


QString TreeFetcher::treeFilePath() {
    return outputDirectory.filePath("tree_" + QString(currentRepoFullName).replace('/', '#') + ".json");
}


void TreeFetcher::logAnomaly(QString message) {
    QTextStream(stdout) << "Could not fetch " << currentRepoFullName << ", " << message << "\n";
    unavailableReposStream << currentRepoFullName << "," << message << Qt::endl;
}


void TreeFetcher::fetchNextTree() {
    while (!repoListStream.atEnd()) {
        currentRepoFullName = repoListStream.readLine();

        if (QFileInfo::exists(treeFilePath())) {
            QTextStream(stdout) << "Tree file already exists for " << currentRepoFullName << "\n";
        } else if (previouslyUnavailableRepos.contains(currentRepoFullName)) {
            QTextStream(stdout) << "Skipping previously unavailable repository " << currentRepoFullName << "\n";
        } else {
            fetchTree();
            return;
        }
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
    request.setDecompressedSafetyCheckThreshold(-1);

    request.setUrl("https://api.github.com/repos/" + currentRepoFullName + "/git/trees/HEAD?recursive=1");

    networkAccessManager->get(request);
}


void TreeFetcher::onRequestFinished(QNetworkReply *reply) {
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

        QTimer::singleShot(5000, this, &TreeFetcher::fetchTree);
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
            QTimer::singleShot(0, this, &TreeFetcher::fetchTree);
            return;
        } else if (message == "Repository access blocked") {
            if (!json["block"].isObject()) {
                QTextStream(stderr) << "No block object in " << httpStatusCode << " response\n";
                emit finished();
                return;
            }
            QJsonObject block = json["block"].toObject();

            QString blockReason = block["reason"].toString();
            if (blockReason != "unavailable" && blockReason != "tos" && blockReason != "sensitive_data" && blockReason != "dmca" && blockReason != "private_information" && blockReason != "size") {
                QTextStream(stderr) << "Unknown block reason " << block["reason"].toString() << " for repository " << currentRepoFullName << "\n";
                emit finished();
                return;
            }

            logAnomaly("access blocked (reason " + blockReason + ")");
            QTimer::singleShot(0, this, &TreeFetcher::fetchNextTree);
            return;
        } else {
            QTextStream(stderr) << "Unknown " << httpStatusCode << " response message: " << message << "\n";
            emit finished();
            return;
        }
    }

    if (reply->error() == QNetworkReply::ContentNotFoundError) {
        logAnomaly("tree not found");
        QTimer::singleShot(0, this, &TreeFetcher::fetchNextTree);
        return;
    }

    if (reply->error() == QNetworkReply::ContentConflictError) {
        QByteArray data = reply->readAll();

        QJsonDocument json = QJsonDocument::fromJson(data);
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
            QTimer::singleShot(0, this, &TreeFetcher::fetchNextTree);
            return;
        } else {
            QTextStream(stderr) << "Unknown 409 response message: " << message << "\n";
            emit finished();
            return;
        }
    }

    if (reply->error() == QNetworkReply::InternalServerError) {
        logAnomaly("internal server error");
        QTimer::singleShot(0, this, &TreeFetcher::fetchNextTree);
        return;
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

    /*if (!url.mid(urlFront.length()).startsWith(currentRepoFullName)) {
        QTextStream(stdout) << "Url from response does not match fetched repository: " << currentRepoFullName << "\n";
    }*/

    out << data;

    long long waitSeconds = apiTokenDispatcher.secUntilTokenAvailable();
    if (waitSeconds > 0) {
        QTextStream(stdout) << "Rate limit reached, waiting " << waitSeconds / 60 << "m " << waitSeconds % 60 << "s\n";
    }

    QTimer::singleShot(waitSeconds * 1000, this, &TreeFetcher::fetchNextTree);
}
