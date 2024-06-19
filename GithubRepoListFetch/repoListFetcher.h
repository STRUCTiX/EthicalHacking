#ifndef REPO_FETCHER_H
#define REPO_FETCHER_H

#include <QtCore>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <utility>
#include "apiTokenDispatcher.h"

class RepoListFetcher: public QObject {
    Q_OBJECT

    public:
        RepoListFetcher(int idStart, int idEnd, QString databaseFile, QString anomalyLogFile, QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod, bool showProgress, int errorStreakLimit, QObject *parent = nullptr);

    public slots:
        void run();

    signals:
        void finished();

    private:
        bool readPreviouslyFetchedRanges(QList<std::pair<int, int>> &previouslyFetchedRanges);
        void fetchBatch();

    private slots:
        void onRequestFinished(QNetworkReply *reply);

    private:
        QNetworkAccessManager *networkAccessManager;
        ApiTokenDispatcher apiTokenDispatcher;
        QFile databaseFile;
        QFile anomalyLogFile;

        QList<std::pair<int, int>> rangesToFetch;
        bool showProgress;

        int idSince;
        int rangeIndex;
        QString lastApiToken;

        int errorStreakLimit;
        int errorStreak;
};

#endif
