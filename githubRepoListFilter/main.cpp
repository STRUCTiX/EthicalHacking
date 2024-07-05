#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QByteArray>

int filter(QTextStream &inputStream, QTextStream &outputStream);

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    app.setOrganizationName("deisele");
    app.setApplicationName("githubRepoListFilter");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOptions({
        {{"i", "input"}, "Input database file.", "file", "githubRepoList.db"},
        {{"o", "output"}, "Output file.", "file", "filteredList.db"}
    });

    QStringList args = app.arguments();
    QString envExtraArgsStr = qEnvironmentVariable("GITHUB_REPO_FILTER_ARGS");
    if (!envExtraArgsStr.isNull()) {
        QTextStream(stdout) << "Extra arguments from env: " << envExtraArgsStr << '\n';
    }
    QStringList envExtraArgs = envExtraArgsStr.split(' ', Qt::SkipEmptyParts);
    args.append(envExtraArgs);
    parser.process(args);

    QFile inputFile(parser.value("input"));
    if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open input database file\n";
        return 1;
    }

    QFile outputFile(parser.value("output"));
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Could not open output file for writing\n";
        return 1;
    }

    QTextStream inputStream(&inputFile);
    QTextStream outputStream(&outputFile);

    return filter(inputStream, outputStream);
}


int filter(QTextStream &inputStream, QTextStream &outputStream) {
    while (!inputStream.atEnd()) {
        QString line = inputStream.readLine();
        QStringList parts = line.split(",");
        if (parts.size() != 11) {
            QTextStream(stderr) << "Invalid line in file: " << line << "\n";
            return 1;
        }

        bool ok;
        int idStart = parts[1].toInt(&ok);
        if (!ok || idStart < 1) {
            QTextStream(stderr) << "Invalid start ID: " << parts[0] << "\n";
            return 1;
        }

        int idEnd = parts[0].toInt(&ok);
        if (!ok || idEnd < idStart) {
            QTextStream(stderr) << "Invalid end ID: " << parts[1] << "\n";
            return 1;
        }

        QString fullName = parts[2];
        if (fullName.count('/') != 1) {
            QTextStream(stderr) << "Slash count in full name not one: " << fullName << "\n";
            return 1;
        }
        if (fullName.split("/")[0].isEmpty() || fullName.split("/")[1].isEmpty()) {
            QTextStream(stderr) << "Empty user or repo name: " << fullName << "\n";
            return 1;
        }

        QByteArray::FromBase64Result b64res = QByteArray::fromBase64Encoding(parts[3].toUtf8(), QByteArray::AbortOnBase64DecodingErrors);
        if (!b64res) {
            QTextStream(stderr) << "Invalid base64 decription: " << parts[3] << "\n";
            return 1;
        }
        QByteArray description = *b64res;

        bool fork = (parts[4] == "1");

        QString nodeId = parts[5];

        QString ownerLogin = parts[6];
        /*if (ownerLogin != fullName.split("/")[0]) {
            QTextStream(stderr) << "Mismatch between full name and owner login: " << fullName << "\n";
            return 1;
        }*/

        QString ownerId = parts[7];
        QString ownerNodeId = parts[8];

        QString ownerType = parts[9];
        if (ownerType != "User" && ownerType != "Organization") {
            QTextStream(stderr) << "Strange owner type found: " << ownerType << "\n";
            return 1;
        }

        bool ownerSiteAdmin = (parts[10] == "1");

        // specify filter condition and output format here
        if (ownerLogin == fullName.split("/")[0] && !fork) {
            //outputStream << parts[0] << "," << fullName << "," << parts[3] << "," << ownerId << "," << (ownerType == "Organization") << "\n";
            outputStream << parts[0] << "\n";
        }
    }

    return 0;
}
