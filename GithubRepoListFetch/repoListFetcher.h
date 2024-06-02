#ifndef REPO_FETCHER_H
#define REPO_FETCHER_H

#include <QtCore>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <utility>

class RepoListFetcher: public QObject {
    Q_OBJECT

    public:
        RepoListFetcher(int idStart, int idEnd, QString databaseFile, QString apiToken, bool showProgress, QObject *parent = nullptr);

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
        QFile databaseFile;

        QList<std::pair<int, int>> rangesToFetch;
        QString apiToken;
        bool showProgress;

        int idSince;
        int rangeIndex;
};

#endif
