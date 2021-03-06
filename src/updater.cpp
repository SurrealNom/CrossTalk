#include "updater.h"

#include <QApplication>
#include <QDesktopServices>
#include <QMessageBox>

#include "ts_helpers_qt.h"
#include "ts_logging_qt.h"
#include "version_qt.h"

const QUrl STABLE(QStringLiteral("http://addons.teamspeak.com/directory/plugins/miscellaneous/CrossTalk.html"));
const QUrl BETA_DOWNLOAD(QStringLiteral("https://github.com/thorwe/CrossTalk/releases"));
const QString GITHUBAPI_URL = QStringLiteral("https://api.github.com/repos/");
const QString GITHUBAPI_LATEST = QStringLiteral("/releases/latest");   //GET /repos/:owner/:repo/releases/latest

//! Constructor
/*!
 * \brief Config::Config create an instance of this class
 */
Updater::Updater(QObject *parent) :
    QObject(parent)
{
    this->setObjectName(QStringLiteral("Updater"));
}

//! Retrieves update url
/*!
 * \brief Config::onNetwManagerFinished Qt slot to retrieve the content
 * \param reply a QNetworkReply containing the update data to parse
 */
void Updater::onNetwManagerFinished(QNetworkReply *reply)
{
    if (!m_URLs.remove(reply->url()))
        return;

    if (reply->error() != QNetworkReply::NoError)
    {
        TSLogging::Log(reply->errorString(),LogLevel_WARNING);
    }
    else
    {
        auto possibleRedirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

        /* We'll deduct if the redirection is valid in the redirectUrl function */
        _urlRedirectedTo = this->redirectUrl(possibleRedirectUrl.toUrl(), _urlRedirectedTo);

        if(!_urlRedirectedTo.isEmpty()) /* If the URL is not empty, we're being redirected. */
        {
            TSLogging::Log(QString("%1: Update check forwarding to %2").arg(this->objectName()).arg(_urlRedirectedTo.toString()),LogLevel_INFO);
            CheckUpdate(_urlRedirectedTo);
            return;
        }

        //https://api.github.com/repos/thorwe/crosstalk/releases/latest
        auto replyUrl = reply->url().toString();
        if (replyUrl.startsWith(QLatin1String("https://api.github.com/repos/")) && replyUrl.endsWith(QLatin1String("/releases/latest")))
            parseGithubResponse(reply);
        else
            parseResponse(reply);

    }

    reply->deleteLater();
    _urlRedirectedTo.clear();
    if (m_URLs.isEmpty())
    {
        m_netwManager->deleteLater();
        m_netwManager=NULL;
        CheckTriggerUpdateDialog();
    }
}

void Updater::CheckUpdate(bool isBetaChannelEnabled)
{
    if (m_netwManager==NULL)
        m_netwManager = new QNetworkAccessManager(this);

    m_isBetaChannel = isBetaChannelEnabled;
    if (m_isBetaChannel)
        CheckUpdate(QString(GITHUBAPI_URL + "thorwe" + "/" + "crosstalk" + GITHUBAPI_LATEST));

    CheckUpdate(STABLE);
}

void Updater::CheckUpdate(QUrl url)
{
    m_URLs.insert(url);
    connect(m_netwManager, &QNetworkAccessManager::finished, this, &Updater::onNetwManagerFinished, Qt::UniqueConnection);
    QNetworkRequest request(url);
    m_netwManager->get(request);
}

void Updater::ShowUpdateDialog(QString remoteVersion, QUrl downloadUrl)
{
    m_resultDownloadUrl = downloadUrl;

    auto mainWindow = TSHelpers::GetMainWindow();
    // Create Dialog
    QMessageBox updateMsgBox(mainWindow);
//    updateMsgBox = new QMessageBox();
    updateMsgBox.moveToThread(mainWindow->thread());
//    updateMsgBox->setAttribute( Qt::WA_DeleteOnClose, true ); //makes sure the msgbox is deleted automatically when closed
    updateMsgBox.setIcon(QMessageBox::Question);
    updateMsgBox.setText(QString("An update for %1 is available!").arg(ts3plugin_name()));
    updateMsgBox.setInformativeText(QString("Local Version: %1\nRemote Version: %2\n\nContinue to download site?\n\nNote that due to client limitations, Teamspeak will be closed to enable plugin installation.").arg(ts3plugin_version()).arg(remoteVersion));
    //updateMsgBox.setDetailedText(QString("Local Version: %1\nRemote Version: %2").arg(ts3plugin_version()).arg(remoteVersion));
    updateMsgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    updateMsgBox.setDefaultButton(QMessageBox::Ok);
    updateMsgBox.setWindowTitle(ts3plugin_name());
    int ret = updateMsgBox.exec();    // blocking variant; might work to workaround api check disaster
//    updateMsgBox->open(this,SLOT(onButtonClicked(QAbstractButton*)));

    // I must be missing something for the non-blocking variant to work -.-
    if (ret == QMessageBox::Ok || ret == QMessageBox::Yes)
    {
        //QDesktopServices::openUrl((remoteVersion==m_VersionStable)?STABLE:BETA_DOWNLOAD);
        QDesktopServices::openUrl(m_resultDownloadUrl);
        qApp->quit();
    }
    else if (ret == QMessageBox::Cancel || ret == QMessageBox::No)
        TSLogging::Log("Update Rejected",LogLevel_INFO);
    else
        TSLogging::Error("Could not find button role");
}

QUrl Updater::redirectUrl(const QUrl& possibleRedirectUrl, const QUrl& oldRedirectUrl) const
{
    QUrl redirectUrl;
    /*
     * Check if the URL is empty and
     * that we aren't being fooled into a infinite redirect loop.
     * We could also keep track of how many redirects we have been to
     * and set a limit to it, but we'll leave that to you.
     */
    if(!possibleRedirectUrl.isEmpty() && possibleRedirectUrl != oldRedirectUrl)
        redirectUrl = possibleRedirectUrl;

    return redirectUrl;
}

void Updater::parseResponse(QNetworkReply *reply)
{
    auto isReplyStable = reply->url().toString().contains("addons.teamspeak.com");

    auto arr = reply->readAll();
    auto start = arr.indexOf((isReplyStable)?QString(ts3plugin_name()).append("_"):"Version",0);
    if (start == -1)
    {
        TSLogging::Log((this->objectName() + ": Did not find Version."),LogLevel_WARNING);
        return;
    }

    QString endStr = isReplyStable ? QStringLiteral(".ts3_plugin") : QStringLiteral("Platforms");
    auto end = arr.indexOf(endStr,start);
    if (end == -1)
    {
        TSLogging::Log((this->objectName() + ": Did not find %1.").arg(endStr),LogLevel_WARNING);
        return;
    }

    QString parse(arr.mid(start,end-start));
    QRegExp rx;
    if (isReplyStable)
        rx.setPattern("\\d+(?:\\_\\d+)+");
    else
        rx.setPattern("\\d+(?:\\.\\d+)+");

    auto pos = rx.indexIn(parse);
    if (pos > -1)
        parse = rx.cap(0);

    if (isReplyStable)
    {
        parse.replace("_",".");
        m_VersionStable = parse;
    }
    else
        m_VersionBeta = parse;
}

void Updater::parseGithubResponse(QNetworkReply *reply)
{
    QJsonParseError jsonError;
    auto jdoc = QJsonDocument::fromJson(reply->readAll(),&jsonError);
    if (jsonError.error != QJsonParseError::ParseError::NoError)
    {
        TSLogging::Error(QString("%1: Json error: %2").arg(this->objectName()).arg(jsonError.errorString()),true);
        return;
    }
    if (!jdoc.isObject())
    {
        TSLogging::Error(QString("%1: is not an object.").arg(this->objectName()),true);
        return;
    }

    auto jobj = jdoc.object();
    m_GithubBetaDownloadUrl = QUrl(jobj.value("html_url").toString());

    m_VersionGithubBeta = jobj.value("tag_name").toString();

    if (m_VersionGithubBeta.endsWith("-beta"))
        m_VersionGithubBeta.chop(5);

    if (m_VersionGithubBeta.startsWith("v"))
        m_VersionGithubBeta.remove(0,1);
}

void Updater::CheckTriggerUpdateDialog()
{
    TSLogging::Log(QString("Versions found: Stable: %1 Github Beta: %2").arg(m_VersionStable, m_VersionGithubBeta));

    VersionQt current(this,ts3plugin_version());
    if (!current.valid())
    {
        TSLogging::Error(QString("Could not parse current version: %1").arg(ts3plugin_version()));
        return;
    }

    VersionQt stable(this,m_VersionStable);
    if (!stable.valid()) {
        TSLogging::Log(QString("Could not parse stable version: %1").arg(ts3plugin_version()),LogLevel_WARNING);
        //stable = VersionQt(this);
    }

    TSLogging::Log(QString("Current Version: %1").arg(current.toString()));
    if (!m_isBetaChannel)
    {
        if (current < stable)
            ShowUpdateDialog(m_VersionStable, STABLE);
    }
    else
    {
        bool isUseStable = true;
        bool isUseBetaGithub = true;

        VersionQt betaGithub(this,m_VersionGithubBeta);
        if (!betaGithub.valid())
        {
            TSLogging::Log(QString("Could not parse github beta version: %1").arg(m_VersionGithubBeta));
            //betaGithub = VersionQt(this);
        }

        if (stable == betaGithub)
            isUseBetaGithub = false;
        else if (stable < betaGithub)
            isUseStable = false;
        else
            isUseBetaGithub = false;

        if (isUseStable == isUseBetaGithub)
        {
            TSLogging::Error("Logic error in version comparison. Please report.");
            return;
        }

        if (isUseStable)
        {
            if (current < stable)
                ShowUpdateDialog(m_VersionStable, STABLE);
        }
        else if (isUseBetaGithub)
        {
            if (current < betaGithub)
                ShowUpdateDialog(m_VersionGithubBeta, m_GithubBetaDownloadUrl);
        }
        else
            TSLogging::Error("Logic error #2 in version comparison. Please report.");
    }
}
