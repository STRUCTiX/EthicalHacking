#include <QDateTime>
#include <QRandomGenerator>
#include <QTextStream>
#include <algorithm>
#include "apiTokenDispatcher.h"

#define UNAUTHENTICATED_LIMIT 60
#define AUTHENTICATED_LIMIT 5000

ApiTokenDispatcher::ApiTokenDispatcher(QStringList apiTokens, UnauthenticatedMode unauthenticatedMode, DispatchMethod dispatchMethod)
    : unauthenticatedMode(unauthenticatedMode), dispatchMethod(dispatchMethod) {
    unauthenticatedToken.token = "";
    unauthenticatedToken.remainingRequests = UNAUTHENTICATED_LIMIT;
    unauthenticatedToken.resetTime = 0;

    for (const QString &token: apiTokens) {
        TokenInfo tokenInfo;
        tokenInfo.token = token;
        tokenInfo.remainingRequests = AUTHENTICATED_LIMIT;
        tokenInfo.resetTime = 0;

        ApiTokenDispatcher::apiTokens.append(tokenInfo);
    }
}


void ApiTokenDispatcher::processResets() {
    long long currentTime = QDateTime::currentSecsSinceEpoch();
    if (currentTime >= unauthenticatedToken.resetTime + 2) {
        unauthenticatedToken.remainingRequests = UNAUTHENTICATED_LIMIT;
    }

    for (TokenInfo &token: apiTokens) {
        if (currentTime >= token.resetTime + 2) {
            token.remainingRequests = AUTHENTICATED_LIMIT;
        }
    }
}


QString ApiTokenDispatcher::getApiToken() const {
    if (unauthenticatedMode == UnauthenticatedMode::First && unauthenticatedToken.remainingRequests > 0) {
        return unauthenticatedToken.token;
    }

    QList<TokenInfo> availableTokens;
    if (unauthenticatedMode == UnauthenticatedMode::Normal && unauthenticatedToken.remainingRequests > 0) {
        availableTokens.append(unauthenticatedToken);
    }

    for (const TokenInfo &token: apiTokens) {
        if (token.remainingRequests > 0) {
            availableTokens.append(token);
        }
    }

    if (!availableTokens.isEmpty()) {
        int selectedTokenIndex;

        switch (dispatchMethod) {
            case DispatchMethod::Balance:
                selectedTokenIndex = 0;
                for (int i = 1; i < availableTokens.size(); i++) {
                    if (availableTokens[i].remainingRequests > availableTokens[selectedTokenIndex].remainingRequests) {
                        selectedTokenIndex = i;
                    }
                }
                break;
            case DispatchMethod::FirstAvailable:
                selectedTokenIndex = 0;
                break;
            case DispatchMethod::Random:
                selectedTokenIndex = QRandomGenerator::global()->bounded(availableTokens.size());
                break;
        }

        return availableTokens[selectedTokenIndex].token;
    }

    if (unauthenticatedMode == UnauthenticatedMode::Last && unauthenticatedToken.remainingRequests > 0) {
        return unauthenticatedToken.token;
    }

    return QString();
}

bool ApiTokenDispatcher::processRateLimitInfo(const QNetworkReply &reply, const QString &usedApiToken) {
    if (!reply.hasRawHeader("x-ratelimit-remaining") || !reply.hasRawHeader("x-ratelimit-reset")) {
        QTextStream(stderr) << "Rate limit header(s) missing\n";
        return false;
    }

    bool ok1, ok2;
    int rateLimitRemaining = reply.rawHeader("x-ratelimit-remaining").toInt(&ok1);
    long long rateLimitReset = reply.rawHeader("x-ratelimit-reset").toLongLong(&ok2);
    if (!ok1 || !ok2) {
        QTextStream(stderr) << "Invalid rate limit header(s)\n";
        return false;
    }

    if (usedApiToken.isEmpty()) {
        unauthenticatedToken.remainingRequests = rateLimitRemaining;
        unauthenticatedToken.resetTime = rateLimitReset;

        return true;
    }

    for (TokenInfo &token: apiTokens) {
        if (token.token == usedApiToken) {
            token.remainingRequests = rateLimitRemaining;
            token.resetTime = rateLimitReset;

            return true;
        }
    }

    QTextStream(stderr) << "Unknown API token\n";
    return false;
}

long long ApiTokenDispatcher::secUntilTokenAvailable() {
    if (unauthenticatedMode != UnauthenticatedMode::Off && unauthenticatedToken.remainingRequests > 0) {
        return 0;
    }

    for (const TokenInfo &token: apiTokens) {
        if (token.remainingRequests > 0) {
            return 0;
        }
    }

    long long minResetTime = unauthenticatedToken.resetTime;
    for (const TokenInfo &token: apiTokens) {
        if (token.resetTime < minResetTime) {
            minResetTime = token.resetTime;
        }
    }

    return std::max(minResetTime - QDateTime::currentSecsSinceEpoch() + 5, 0ll);
}
