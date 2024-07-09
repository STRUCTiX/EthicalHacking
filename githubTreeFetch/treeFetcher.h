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
#include "apiTokenDispatcher.h"

class TreeFetcher: public QObject {
    Q_OBJECT

    public:
        TreeFetcher(QString repoListFile, QString outputDirectory, QString unavailableReposFile, QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod, bool showProgress, int errorStreakLimit, QObject *parent = nullptr);

    public slots:
        void run();

    signals:
        void finished();

    private:
        bool loadPreviouslyUnavailableReposList();
        QString treeFilePath();
        void logAnomaly(QString message);

    private slots:
        void fetchNextTree();
        void fetchTree();
        void onRequestFinished(QNetworkReply *reply);

    private:
        QNetworkAccessManager *networkAccessManager;
        ApiTokenDispatcher apiTokenDispatcher;
        QDir outputDirectory;
        QFile repoListFile;
        QFile unavailableReposFile;
        QTextStream repoListStream;
        QTextStream unavailableReposStream;

        bool showProgress;
        QSet<QString> previouslyUnavailableRepos;

        QString currentRepoFullName;
        QString lastApiToken;

        int errorStreakLimit;
        int errorStreak;
};

#endif
