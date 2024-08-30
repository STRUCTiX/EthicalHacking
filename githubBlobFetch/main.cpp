#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include "blobFetcher.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    app.setOrganizationName("deisele");
    app.setApplicationName("githubBlobFetch");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOptions({
        {{"i", "input-tree-list"}, "Input file containing a newline-separated list of tree file names, for which the workflow blobs should be downloaded.", "file", "trees.txt"},
        {{"b", "tree-base"}, "Base directory of tree files.", "directory", "."},
        {{"p", "progress"}, "Print one dot per request."},
        {{"o", "output"}, "Output directory.", "directory", "blobs"},
        {{"l", "unavailable-list"}, "Text file used to record all unavailable blobs.", "file", "unavailable_blobs.csv"},
        {{"t", "token"}, "Github API access token(s). To specify multiple tokens, use multiple -t options.", "token string"},
        {{"u", "unauthenticated-mode"}, "How/if to use unauthenticated requests.", "off|first|normal|last", "first"},
        {{"d", "dispatch-method"}, "Method for selecting the next API token.", "balance|first-available|random", "balance"},
        {{"e", "max-error-streak"}, "Quit if the number of consecutive network errors exceeds this limit.", "limit", "5"}
    });

    QStringList args = app.arguments();
    QString envExtraArgsStr = qEnvironmentVariable("GITHUB_BLOB_FETCH_ARGS");
    if (!envExtraArgsStr.isNull()) {
        QTextStream(stdout) << "Extra arguments from env: " << envExtraArgsStr << '\n';
    }
    QStringList envExtraArgs = envExtraArgsStr.split(' ', Qt::SkipEmptyParts);
    args.append(envExtraArgs);
    parser.process(args);

    bool ok;
    int errorStreakLimit = parser.value("max-error-streak").toInt(&ok);
    if (!ok || errorStreakLimit < 0) {
        QTextStream(stderr) << app.applicationName() << ": Invalid error streak limit.\n";
        return 1;
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

    BlobFetcher *task = new BlobFetcher(parser.value("input-tree-list"), parser.value("tree-base"), parser.value("output"), parser.value("unavailable-list"), parser.values("token"), unauthenticatedMode, dispatchMethod, parser.isSet("progress"), errorStreakLimit, &app);

    QObject::connect(task, &BlobFetcher::finished, &app, &QCoreApplication::quit);

    // run the task from the application event loop
    QTimer::singleShot(0, task, &BlobFetcher::run);

    return app.exec();
}
