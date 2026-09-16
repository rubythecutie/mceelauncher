#include <QApplication>
#include <QCommandLineParser>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineView>
#if QT_VERSION >= 0x050C00
#include <QWebEngineUrlScheme>
#endif
#include <iostream>
#include "main.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument("startUrl", "The initial URL to open");
    parser.addPositionalArgument("endUrl", "The URL which will cause the webview to quit");
    parser.process(app);
    if (parser.positionalArguments().size() != 2) {
        return -1;
    }
    QUrl startUrl = QUrl::fromUserInput(parser.positionalArguments()[0]);
    QString endUrlStr = parser.positionalArguments()[1];
    QString endScheme = QUrl::fromUserInput(parser.positionalArguments()[1]).scheme();

    QWebEngineView view;
    QWebEngineProfile profile;
    profile.setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    profile.setHttpCacheType(QWebEngineProfile::NoCache);

    SchemeHandler* handler = nullptr;
    if(endScheme != "http" && endScheme != "https") {
#if QT_VERSION >= 0x050C00
        QWebEngineUrlScheme::registerScheme(QWebEngineUrlScheme(endScheme.toUtf8()));
#endif
        handler = new SchemeHandler(endUrlStr);
        profile.installUrlSchemeHandler(endScheme.toUtf8(), handler);
    }

    InterceptPage page(&profile, endUrlStr, &view);
    view.setPage(&page);
    QRect geometry = QGuiApplication::primaryScreen()->availableGeometry();
    const QSize size = geometry.size() * 4 / 5;
    const QSize offset = (geometry.size() - size) / 2;
    const QPoint pos = geometry.topLeft() + QPoint(offset.width(), offset.height());
    view.setGeometry(QRect(pos, size));

    QObject::connect(&view, &QWebEngineView::titleChanged, &view, &QWidget::setWindowTitle);
    QObject::connect(&view, &QWebEngineView::urlChanged, [&](const QUrl &url) {
        if(InterceptPage::matchesEndUrl(url, endUrlStr)) {
            std::cout << url.toString(QUrl::FullyEncoded).toStdString() << std::endl << std::flush;
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
        } else if(InterceptPage::isWrongPlace(url)) {
            std::cerr << "[webview] reached wrongplace without capturing code URL; "
                         "staying open (close window to cancel)" << std::endl << std::flush;
        }
    });

    view.setUrl(startUrl);
    view.show();
    int rc = app.exec();
    if(handler) {
        profile.removeUrlSchemeHandler(handler);
        delete handler;
    }
    return rc;
}

void SchemeHandler::requestStarted(QWebEngineUrlRequestJob *request) {
    QUrl url = request->requestUrl();
    if (InterceptPage::matchesEndUrl(url, endUrl)) {
        std::cout << url.toString(QUrl::FullyEncoded).toStdString() << std::endl << std::flush;
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    } else {
        request->fail(QWebEngineUrlRequestJob::UrlNotFound);
    }
}

static QString stripTrailingSlashes(QString s) {
    while (s.endsWith('/') && s.size() > 1)
        s.chop(1);
    return s;
}

InterceptPage::InterceptPage(QWebEngineProfile *profile, QString endUrlStr, QObject *parent)
    : QWebEnginePage(profile, parent), endUrlStr(std::move(endUrlStr)) {}

bool InterceptPage::isWrongPlace(const QUrl &url) {
    if (url.host().compare("login.microsoftonline.com", Qt::CaseInsensitive) != 0 &&
        url.host().compare("login.microsoft.com", Qt::CaseInsensitive) != 0 &&
        url.host().compare("login.live.com", Qt::CaseInsensitive) != 0)
        return false;
    return url.path(QUrl::FullyEncoded).contains("wrongplace", Qt::CaseSensitive);
}

bool InterceptPage::matchesEndUrl(const QUrl &url, const QString &endUrlStr) {
    if (endUrlStr.isEmpty() || !url.isValid())
        return false;
    QString full = url.toString(QUrl::FullyEncoded);
    if (full.startsWith(endUrlStr))
        return true;
    QString endTrim = stripTrailingSlashes(endUrlStr);
    if (full.startsWith(endTrim)) {
        if (full.size() == endTrim.size())
            return true;
        QChar c = full.at(endTrim.size());
        if (c == '?' || c == '#' || c == '/' || c == '&')
            return true;
    }
    QUrl endUrl = QUrl::fromUserInput(endTrim);
    if (!endUrl.isValid())
        return false;
    if (url.scheme().compare(endUrl.scheme(), Qt::CaseInsensitive) != 0)
        return false;
    if (url.host().compare(endUrl.host(), Qt::CaseInsensitive) != 0)
        return false;
    if (url.port() != endUrl.port())
        return false;
    QString p1 = stripTrailingSlashes(url.path(QUrl::FullyEncoded));
    QString p2 = stripTrailingSlashes(endUrl.path(QUrl::FullyEncoded));
    if (p1 != p2)
        return false;
    return true;
}

bool InterceptPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) {
    Q_UNUSED(type);
    if (!isMainFrame)
        return true;
    if (checkAndHandle(url))
        return false;
    if (isWrongPlace(url)) {
        std::cerr << "[webview] blocking navigation to wrongplace: "
                  << url.toString(QUrl::FullyEncoded).toStdString() << std::endl << std::flush;
        return false;
    }
    return true;
}

bool InterceptPage::checkAndHandle(const QUrl &url) {
    if (!matchesEndUrl(url, endUrlStr))
        return false;
    std::cout << url.toString(QUrl::FullyEncoded).toStdString() << std::endl << std::flush;
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    return true;
}
