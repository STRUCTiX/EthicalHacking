#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <limits>
#include "repoListFetcher.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    app.setOrganizationName("deisele");
    app.setApplicationName("GithubRepoListFetcher");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOptions({
        {{"r", "range"}, "Inclusive range of IDs to fetch. Leave either side empty to specify min/max.", "from..to", ".."},
        {{"t", "token"}, "Github API access token", "token string", QString()},
        {{"p", "progress"}, "Print one rate limit information line per request."}
    });

    parser.process(app);

    QStringList rangeParts = parser.value("range").split("..");
    if (rangeParts.size() != 2) {
        QTextStream(stderr) << app.applicationName() << ": Invalid range.\n";
        return 1;
    }

    int idStart, idEnd;
    if (rangeParts[0].isEmpty()) {
        idStart = 1;
    } else {
        bool ok;
        idStart = rangeParts[0].toInt(&ok);
        if (!ok || idStart < 1) {
            QTextStream(stderr) << app.applicationName() << ": Invalid start ID.\n";
            return 1;
        }
    }

    if (rangeParts[1].isEmpty()) {
        idEnd = std::numeric_limits<int>::max();
    } else {
        bool ok;
        idEnd = rangeParts[1].toInt(&ok);
        if (!ok || idEnd < idStart) {
            QTextStream(stderr) << app.applicationName() << ": Invalid end ID.\n";
            return 1;
        }
    }

    RepoListFetcher *task = new RepoListFetcher(idStart, idEnd, "githubRepoList.db", parser.value("token"), parser.isSet("progress"), &app);

    QObject::connect(task, &RepoListFetcher::finished, &app, &QCoreApplication::quit);

    // run the task from the application event loop
    QTimer::singleShot(0, task, &RepoListFetcher::run);

    return app.exec();
}
