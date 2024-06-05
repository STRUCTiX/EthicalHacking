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
        {{"p", "progress"}, "Print one dot per request."},
        {{"o", "output"}, "Output database file.", "file", "githubRepoList.db"},
        {{"t", "token"}, "Github API access token", "token string"},
        {{"u", "unauthenticated-mode"}, "How/if to use unauthenticated requests.", "off|first|normal|last", "first"},
        {{"d", "dispatch-method"}, "Method for selecting the next API token.", "balance|first-available|random", "balance"},
    });

    QStringList args = app.arguments();
    QString envExtraArgsStr = qEnvironmentVariable("GITHUB_REPO_FETCHER_ARGS");
    if (!envExtraArgsStr.isNull()) {
        QTextStream(stdout) << "Extra arguments from env: " << envExtraArgsStr << '\n';
    }
    QStringList envExtraArgs = envExtraArgsStr.split(' ', Qt::SkipEmptyParts);
    args.append(envExtraArgs);
    parser.process(args);

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

    UnauthenticatedMode unauthenticatedMode;
    if (parser.value("unauthenticated-mode") == "off") {
        unauthenticatedMode = UnauthenticatedMode::Off;
    } else if (parser.value("unauthenticated-mode") == "first") {
        unauthenticatedMode = UnauthenticatedMode::First;
    } else if (parser.value("unauthenticated-mode") == "normal") {
        unauthenticatedMode = UnauthenticatedMode::Normal;
    } else if (parser.value("unauthenticated-mode") == "last") {
        unauthenticatedMode = UnauthenticatedMode::Last;
    } else {
        QTextStream(stderr) << app.applicationName() << ": Invalid unauthenticated mode.\n";
        return 1;
    }

    DispatchMethod dispatchMethod;
    if (parser.value("dispatch-method") == "balance") {
        dispatchMethod = DispatchMethod::Balance;
    } else if (parser.value("dispatch-method") == "first-available") {
        dispatchMethod = DispatchMethod::FirstAvailable;
    } else if (parser.value("dispatch-method") == "random") {
        dispatchMethod = DispatchMethod::Random;
    } else {
        QTextStream(stderr) << app.applicationName() << ": Invalid dispatch method.\n";
        return 1;
    }

    //qDebug() << parser.values("token");

    RepoListFetcher *task = new RepoListFetcher(idStart, idEnd, parser.value("output"), parser.values("token"), unauthenticatedMode, dispatchMethod, parser.isSet("progress"), &app);

    QObject::connect(task, &RepoListFetcher::finished, &app, &QCoreApplication::quit);

    // run the task from the application event loop
    QTimer::singleShot(0, task, &RepoListFetcher::run);

    return app.exec();
}
