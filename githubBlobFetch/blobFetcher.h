#ifndef REPO_FETCHER_H
#define REPO_FETCHER_H

#include <QtCore>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QQueue>
#include "apiTokenDispatcher.h"

struct BlobIdentifier {
    BlobIdentifier() = default;
    BlobIdentifier(const QString &repo, const QString &sha)
        : repo(repo), sha(sha) {};

    QString repo;
    QString sha;

    friend std::strong_ordering operator<=>(const BlobIdentifier &, const BlobIdentifier &) = default;
};

inline size_t qHash(const BlobIdentifier &key, size_t seed){
    return qHashMulti(seed, key.repo, key.sha);
}


class BlobFetcher: public QObject {
    Q_OBJECT

    public:
        BlobFetcher(QString treeListFile, QString treeBaseDirectory, QString outputDirectory, QString unavailableBlobsFile, QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod, bool showProgress, int errorStreakLimit, QObject *parent = nullptr);

    public slots:
        void run();

    signals:
        void finished();

    private:
        bool loadPreviouslyUnavailableReposList();
        bool loadBlobsToFetch();
        QString blobFilePath();
        void logAnomaly(QString message);

    private slots:
        void fetchNextBlob();
        void fetchBlob();
        void onRequestFinished(QNetworkReply *reply);

    private:
        QNetworkAccessManager *networkAccessManager;
        ApiTokenDispatcher apiTokenDispatcher;
        QDir outputDirectory;
        QDir treeBaseDirectory;
        QFile treeListFile;
        QFile unavailableBlobsFile;
        QTextStream unavailableBlobsStream;

        bool showProgress;
        QSet<BlobIdentifier> previouslyUnavailableBlobs;
        QQueue<BlobIdentifier> blobsToFetch;

        BlobIdentifier currentBlob;
        QString lastApiToken;

        int errorStreakLimit;
        int errorStreak;
};

#endif
