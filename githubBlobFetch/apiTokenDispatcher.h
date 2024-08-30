#ifndef API_TOKEN_DISPATCHER_H
#define API_TOKEN_DISPATCHER_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QNetworkReply>

enum class UnauthenticatedMode {
    Off,
    First,
    Normal,
    Last
};

enum class DispatchMethod {
    Balance,
    FirstAvailable,
    Random
};

class ApiTokenDispatcher {
    public:
        ApiTokenDispatcher(QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod);

        void processResets();
        QString getApiToken() const;
        bool processRateLimitInfo(const QNetworkReply &reply, const QString &usedApiToken);
        long long secUntilTokenAvailable();

    private:
        struct TokenInfo {
            QString token;
            int remainingRequests;
            long long resetTime;
        };
        QList<TokenInfo> apiTokens;
        TokenInfo unauthenticatedToken;

        UnauthenticatedMode unauthenticatedMode;
        DispatchMethod dispatchMethod;
};

#endif
