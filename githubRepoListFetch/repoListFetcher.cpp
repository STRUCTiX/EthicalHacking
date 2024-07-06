#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <algorithm>
#include <limits>
#include "repoListFetcher.h"

RepoListFetcher::RepoListFetcher(int idStart, int idEnd, QString databaseFile, QString anomalyLogFile, QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod, bool showProgress, int errorStreakLimit, QObject *parent)
    : QObject(parent), networkAccessManager(new QNetworkAccessManager(this)), apiTokenDispatcher(apiTokens, unauthenticatedMode, dispatchMethod), databaseFile(databaseFile), anomalyLogFile(anomalyLogFile), rangesToFetch({{idStart, idEnd}}), showProgress(showProgress), errorStreakLimit(errorStreakLimit), errorStreak(0) {
    networkAccessManager->setTransferTimeout();
    connect(networkAccessManager, &QNetworkAccessManager::finished, this, &RepoListFetcher::onRequestFinished);
}


void RepoListFetcher::run() {
    QList<std::pair<int, int>> previouslyFetchedRanges;
    if (!readPreviouslyFetchedRanges(previouslyFetchedRanges)) {
        emit finished();
        return;
    }

    // subtract already fetched ranges from the requested range
    bool modified = false;
    for (const std::pair<int, int> &rangeExclude: previouslyFetchedRanges) {
        int firstAffectedRange = -1;
        for (int i = 0; i < rangesToFetch.size(); i++) {
            if (rangesToFetch[i].first <= rangeExclude.second && rangesToFetch[i].second >= rangeExclude.first) {
                firstAffectedRange = i;
                break;
            }
        }

        if (firstAffectedRange == -1) {
            continue;
        }

        modified = true;

        int lastAffectedRange = firstAffectedRange;
        for (int i = firstAffectedRange + 1; i < rangesToFetch.size(); i++) {
            if (rangesToFetch[i].first <= rangeExclude.second && rangesToFetch[i].second >= rangeExclude.first) {
                lastAffectedRange = i;
            }
        }

        std::pair<int, int> newRange1(rangesToFetch[firstAffectedRange].first, rangeExclude.first - 1);
        std::pair<int, int> newRange2(rangeExclude.second + 1, rangesToFetch[lastAffectedRange].second);

        rangesToFetch.remove(firstAffectedRange, lastAffectedRange - firstAffectedRange + 1);

        if (newRange2.first <= newRange2.second) {
            rangesToFetch.insert(firstAffectedRange, newRange2);
        }
        if (newRange1.first <= newRange1.second) {
            rangesToFetch.insert(firstAffectedRange, newRange1);
        }
    }

    if (rangesToFetch.isEmpty()) {
        QTextStream(stdout) << "Requested range has already been fetched.\n";
        emit finished();
        return;
    }

    if (modified) {
        QTextStream(stdout) << "Part(s) of the requested range have already been fetched. Remaining ranges:\n";

        QStringList rangeStrings;
        for (const std::pair<int, int> &range: rangesToFetch) {
            rangeStrings.append(QString::number(range.first) + ".." + (range.second == std::numeric_limits<int>::max()? "" : QString::number(range.second)));
        }
        QTextStream(stdout) << rangeStrings.join(", ") << '\n';
    }

    if (!databaseFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open database file for writing\n";
        emit finished();
        return;
    }

    if (!anomalyLogFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open anomaly log file for writing\n";
        emit finished();
        return;
    }

    idSince = rangesToFetch[0].first - 1;
    rangeIndex = 0;

    fetchBatch();
}

// read already downloaded previouslyFetchedRanges from csv file
bool RepoListFetcher::readPreviouslyFetchedRanges(QList<std::pair<int, int>> &previouslyFetchedRanges) {
    if (!databaseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return true;
    }

    QTextStream in(&databaseFile);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList parts = line.split(",");
        if (parts.size() != 11) {
            QTextStream(stderr) << "Invalid line in file: " << line;
            goto fail;
        }

        bool ok;
        int idStart = parts[1].toInt(&ok);
        if (!ok || idStart < 1) {
            QTextStream(stderr) << "Invalid start ID: " << parts[0];
            goto fail;
        }

        int idEnd = parts[0].toInt(&ok);
        if (!ok || idEnd < idStart) {
            QTextStream(stderr) << "Invalid end ID: " << parts[1];
            goto fail;
        }

        previouslyFetchedRanges.append({idStart, idEnd});
        std::ranges::sort(previouslyFetchedRanges);

        // compress overlapping/adjacent ranges
        int i = 0;
        while (i < previouslyFetchedRanges.size() - 1) {
            if (previouslyFetchedRanges[i].second >= previouslyFetchedRanges[i + 1].first - 1) {
                previouslyFetchedRanges[i].second = std::max(previouslyFetchedRanges[i].second, previouslyFetchedRanges[i + 1].second);
                previouslyFetchedRanges.removeAt(i + 1);
            } else {
                i++;
            }
        }
    }

    databaseFile.close();
    return true;

fail:
    databaseFile.close();
    return false;
}


void RepoListFetcher::fetchBatch() {
    if (showProgress) {
        QTextStream(stdout) << "a";
    }

    apiTokenDispatcher.processResets();
    QString apiToken = apiTokenDispatcher.getApiToken();
    if (apiToken.isNull()) {
        long long waitSeconds = apiTokenDispatcher.secUntilTokenAvailable();
        QTextStream(stdout) << "No API token available\n"
                               "Rate limit reached, waiting " << waitSeconds / 60 << "m " << waitSeconds % 60 << "s\n";
        QTimer::singleShot(waitSeconds * 1000, this, &RepoListFetcher::fetchBatch);
        return;
    }
    lastApiToken = apiToken;

    //qDebug() << "API token: " << apiToken;

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::UserAgentHeader, "RepoListFetcher");
    if (!apiToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + apiToken).toUtf8());
    }
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    request.setUrl("https://api.github.com/repositories?since=" + QString::number(idSince));

    networkAccessManager->get(request);
}


void RepoListFetcher::onRequestFinished(QNetworkReply *reply) {
    reply->deleteLater();

    QTextStream out(&databaseFile);

    if (showProgress) {
        QTextStream(stdout) << "b\n";
    }

    if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentAccessDenied) {
        QTextStream(stderr) << "Request failed:\n" << reply->error() << "\n" << reply->errorString();

        errorStreak++;
        if (errorStreak > errorStreakLimit) {
            QTextStream(stderr) << "Error streak limit reached\n";
            emit finished();
            return;
        }

        QTimer::singleShot(5000, this, &RepoListFetcher::fetchBatch);
        return;
    }

    errorStreak = 0;

    if (!apiTokenDispatcher.processRateLimitInfo(*reply, lastApiToken)) {
        emit finished();
        return;
    }

    if (reply->error() == QNetworkReply::ContentAccessDenied) {
        if (lastApiToken.isEmpty()) {
            QTextStream(stdout) << "Rate limit exceeded for unauthenticated requests\n";
        } else {
            QTextStream(stdout) << "Rate limit exceeded for token " << lastApiToken << "\n";
        }
        QTimer::singleShot(0, this, &RepoListFetcher::fetchBatch);
        return;
    }

    QByteArray data = reply->readAll();

    QJsonDocument json = QJsonDocument::fromJson(data);
    if (json.isNull()) {
        QTextStream(stderr) << "Could not parse response\n";
        emit finished();
        return;
    }

    if (!json.isArray()) {
        QTextStream(stderr) << "Data is not an array\n";
        emit finished();
        return;
    }

    const QJsonArray repoArray = json.array();
    if (50 <= repoArray.count() && repoArray.count() < 100) {
        QTextStream(stdout) << "Undersized response array, idSince: " << idSince << "\n";
        QTextStream(&anomalyLogFile) << "Undersized response array, idSince: " << idSince << "\n";
    }

    for (const QJsonValue &repo: repoArray) {
        if (!repo.isObject()) {
            if (!repo.isNull()) {
                QTextStream(stderr) << "Repo is not an object\n";
                emit finished();
                return;
            }

            QTextStream(stdout) << "Null repository, idSince: " << idSince << "\n";
            QTextStream(&anomalyLogFile) << "Null repository, idSince: " << idSince << "\n";
            continue;
        }

        if (repo["id"].toInt() <= 0) {
            QTextStream(stderr) << "Invalid id\n";
            emit finished();
            return;
        }

        if (!repo["full_name"].isString()) {
            QTextStream(stderr) << "full_name is not a string\n";
            emit finished();
            return;
        }

        int id = repo["id"].toInt();
        if (id <= idSince) {
            QTextStream(stderr) << "Decreasing/duplicate id\n";
            emit finished();
            return;
        }

        QString nodeId = repo["node_id"].toString();
        QString fullName = repo["full_name"].toString();
        QString description = repo["description"].toString();
        bool fork = repo["fork"].toBool();

        QJsonValue owner = repo["owner"];
        QString ownerLogin = owner["login"].toString();
        int ownerId = owner["id"].toInt();
        QString ownerNodeId = owner["node_id"].toString();
        QString ownerType = owner["type"].toString();
        bool ownerSiteAdmin = owner["site_admin"].toBool();

        if (id > rangesToFetch[rangeIndex].second) {
            if (idSince < rangesToFetch[rangeIndex].second) {
                out << rangesToFetch[rangeIndex].second << "," << idSince + 1 << ",endOfRangeDummy,,,,,,,,\n";
            }

            if (rangeIndex == rangesToFetch.size() - 1) {
                QTextStream(stdout) << "End of range reached\n";
                emit finished();
                return;
            }

            rangeIndex++;
            idSince = rangesToFetch[rangeIndex].first - 1;

            goto next;
        }

        //qDebug() << "ID:" << id;
        //qDebug() << "Full name:" << fullName;

        out << id << "," << idSince + 1 << "," << fullName << "," << description.toUtf8().toBase64() << "," << fork << "," << nodeId << "," << ownerLogin << "," << ownerId << "," << ownerNodeId << "," << ownerType << "," << ownerSiteAdmin << "\n";

        idSince = id;
    }

    if (repoArray.count() < 50) {
        QTextStream(stdout) << "End of list reached\n";
        if (rangesToFetch[rangeIndex].second != std::numeric_limits<int>::max()) {
            QTextStream(stderr) << "End of list reached before end of range\n";
        }

        emit finished();
        return;
    }

next:
    long long waitSeconds = apiTokenDispatcher.secUntilTokenAvailable();
    if (waitSeconds > 0) {
        QTextStream(stdout) << "Rate limit reached, waiting " << waitSeconds / 60 << "m " << waitSeconds % 60 << "s\n";
    }
    QTimer::singleShot(waitSeconds * 1000, this, &RepoListFetcher::fetchBatch);
}
