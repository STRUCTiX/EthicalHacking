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
#include "apiTokenDispatcher.h"

class TreeFetcher: public QObject {
    Q_OBJECT

    public:
        TreeFetcher(QString repoListFile, QString outputDirectory, QString anomalyLogFile, QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod, bool showProgress, int errorStreakLimit, QObject *parent = nullptr);

    public slots:
        void run();

    signals:
        void finished();

    private:
        bool loadIDs();
        QString treeFilePath();

    private slots:
        void fetchNextTree();
        void fetchTree();
        void onRequestFinished(QNetworkReply *reply);

    private:
        QNetworkAccessManager *networkAccessManager;
        ApiTokenDispatcher apiTokenDispatcher;
        QFile repoListFile;
        QTextStream repoListStream;
        QDir outputDirectory;
        QFile anomalyLogFile;

        bool showProgress;

        QString currentRepoFullName;
        QString lastApiToken;

        int errorStreakLimit;
        int errorStreak;
};

#endif
