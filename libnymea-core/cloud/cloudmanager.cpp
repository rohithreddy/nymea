/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2017 Michael Zanetti <michael.zanetti@guh.io>            *
 *                                                                         *
 *  This file is part of nymea.                                            *
 *                                                                         *
 *  nymea is free software: you can redistribute it and/or modify          *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, version 2 of the License.                *
 *                                                                         *
 *  nymea is distributed in the hope that it will be useful,               *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with nymea. If not, see <http://www.gnu.org/licenses/>.          *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "cloudmanager.h"
#include "awsconnector.h"
#include "loggingcategories.h"
#include "cloudnotifications.h"
#include "nymeaconfiguration.h"
#include "cloudtransport.h"
#include "nymeaconfiguration.h"
#include "nymeasettings.h"

#include "nymea-remoteproxyclient/remoteproxyconnection.h"
#include <QDir>

using namespace remoteproxyclient;

CloudManager::CloudManager(NymeaConfiguration *configuration, NetworkManager *networkManager, QObject *parent):
    QObject(parent),
    m_configuration(configuration),
    m_networkManager(networkManager)
{
    m_awsConnector = new AWSConnector(this);
    connect(m_awsConnector, &AWSConnector::devicePaired, this, &CloudManager::onPairingFinished);
    connect(m_awsConnector, &AWSConnector::connected, this, &CloudManager::awsConnected);
    connect(m_awsConnector, &AWSConnector::disconnected, this, &CloudManager::awsDisconnected);

    connect(m_networkManager, &NetworkManager::stateChanged, this, &CloudManager::onlineStateChanged);

    ServerConfiguration config;
    config.id = "remote";
    config.authenticationEnabled = false;
    config.sslEnabled = true;
    m_transport = new CloudTransport(config);
    connect(m_awsConnector, &AWSConnector::proxyConnectionRequestReceived, m_transport, &CloudTransport::connectToCloud);

    m_deviceId = m_configuration->serverUuid();
    m_deviceName = m_configuration->serverName();
    m_serverUrl = m_configuration->cloudServerUrl();
    m_caCertificate = m_configuration->cloudCertificateCA();
    m_clientCertificate = m_configuration->cloudCertificate();
    m_clientCertificateKey = m_configuration->cloudCertificateKey();

    setEnabled(m_configuration->cloudEnabled());
    connect(m_configuration, &NymeaConfiguration::cloudEnabledChanged, this, &CloudManager::setEnabled);
    connect(m_configuration, &NymeaConfiguration::serverNameChanged, this, &CloudManager::setDeviceName);

}

CloudManager::~CloudManager()
{
}

void CloudManager::setDeviceName(const QString &name)
{
    m_deviceName = name;
    m_awsConnector->setDeviceName(name);
}

bool CloudManager::enabled() const
{
    return m_enabled;
}

void CloudManager::setEnabled(bool enabled)
{
    if (enabled) {
        m_enabled = true;
        emit connectionStateChanged();

        bool missingConfig = false;
        if (m_deviceId.isNull()) {
            qCWarning(dcCloud()) << "Don't have a unique device ID.";
            missingConfig = true;
        }
        if (m_deviceName.isEmpty()) {
            qCWarning(dcCloud()) << "Don't have a device name set";
            missingConfig = true;
        }
        if (m_serverUrl.isEmpty()) {
            qCWarning(dcCloud()) << "Cloud server URL not set.";
            missingConfig = true;
        }
        if (m_clientCertificate.isEmpty()) {
            qCWarning(dcCloud()) << "Cloud certificate not set.";
            missingConfig = true;
        }
        if (m_clientCertificateKey.isEmpty()) {
            qCWarning(dcCloud()) << "Cloud certificate key not set.";
            missingConfig = true;
        }
        if (m_caCertificate.isEmpty()) {
            qCWarning(dcCloud()) << "Cloud certificate CA not set.";
            missingConfig = true;
        }
        if (missingConfig) {
            qCWarning(dcCloud()) << "Cloud configuration incomplete. Not enabling cloud connection.";
            return;
        }

        qCDebug(dcCloud()) << "Enabling cloud connection.";
        if (!m_awsConnector->isConnected() && m_networkManager->state() == NetworkManager::NetworkManagerStateConnectedGlobal) {
            connect2aws();
        }
    } else {
        qCDebug(dcCloud()) << "Disabling cloud connection.";
        m_enabled = false;
        m_awsConnector->disconnectAWS();
        emit connectionStateChanged();
    }
}

bool CloudManager::installClientCertificates(const QByteArray &rootCA, const QByteArray &certificatePEM, const QByteArray &publicKey, const QByteArray &privateKey, const QString &endpoint)
{
    QString baseDir = NymeaSettings::storagePath() + "/certs/cloud/";
    QDir dir;
    // We never delete old certs, cycle until we find an unused path
    int i = 0;
    do {
        dir = QDir(baseDir + QString::number(i++) + '/');
    } while (dir.exists());

    if (!dir.mkpath(dir.absolutePath())) {
        qCWarning(dcCloud) << "Cannot install cloud certificates. Unable to create path.";
        return false;
    }
    QFile ca(dir.absoluteFilePath("aws-certification-authority.crt"));
    if (!ca.open(QFile::WriteOnly) || ca.write(rootCA) != rootCA.length()) {
        qCWarning(dcCloud()) << "Cannot install cloud certificates. Unable to write CA file" << dir.absoluteFilePath(ca.fileName());
        ca.close();
        return false;
    }
    ca.close();
    QFile pem(dir.absoluteFilePath("guh-cloud.pem"));
    if (!pem.open(QFile::WriteOnly) || pem.write(certificatePEM) != certificatePEM.length()) {
        qCWarning(dcCloud()) << "Cannot install cloud certificates. Unable to write certificate file" << dir.absoluteFilePath(pem.fileName());
        pem.close();
        return false;
    }
    pem.close();
    QFile pub(dir.absoluteFilePath("guh-cloud.pub"));
    if (!pub.open(QFile::WriteOnly) || pub.write(publicKey) != publicKey.length()) {
        qCWarning(dcCloud()) << "Cannot install cloud certificates. Unable to write public key file" << dir.absoluteFilePath(pub.fileName());
        pub.close();
        return false;
    }
    pub.close();
    QFile key(dir.absoluteFilePath("guh-cloud.key"));
    if (!key.open(QFile::WriteOnly) || key.write(privateKey) != privateKey.length()) {
        qCWarning(dcCloud()) << "Cannot install cloud certificates. Unable to write private key file" << dir.absoluteFilePath(key.fileName());
        key.close();
        return false;
    }
    key.close();
    qCDebug(dcCloud) << "Installed cloud certificates to" << dir.absolutePath();
    m_caCertificate = dir.absoluteFilePath("aws-certification-authority.crt");
    m_clientCertificate = dir.absoluteFilePath("guh-cloud.pem");
    m_clientCertificateKey = dir.absoluteFilePath("guh-cloud.key");
    m_serverUrl = endpoint;

    if (m_enabled) {
        m_awsConnector->disconnectAWS();
        connect2aws();
    }
    m_configuration->setCloudCertificateCA(m_caCertificate);
    m_configuration->setCloudCertificate(m_clientCertificate);
    m_configuration->setCloudCertificateKey(m_clientCertificateKey);
    m_configuration->setCloudServerUrl(m_serverUrl);

    emit connectionStateChanged();
    return true;
}

CloudManager::CloudConnectionState CloudManager::connectionState() const
{
    if (m_awsConnector->isConnected()) {
        return CloudConnectionStateConnected;
    }
    if (!m_enabled) {
        return CloudConnectionStateDisabled;
    }
    if (m_deviceId.isNull() || m_deviceName.isEmpty() || m_serverUrl.isEmpty() || m_clientCertificate.isEmpty() || m_clientCertificateKey.isEmpty() || m_caCertificate.isEmpty()) {
        return CloudConnectionStateUnconfigured;
    }
    return CloudConnectionStateConnecting;
}

void CloudManager::pairDevice(const QString &idToken, const QString &userId)
{
    m_awsConnector->pairDevice(idToken, userId);
}

CloudNotifications *CloudManager::createNotificationsPlugin() const
{
    CloudNotifications* notifications = new CloudNotifications(m_awsConnector);
    return notifications;
}

CloudTransport *CloudManager::createTransportInterface() const
{
    return m_transport;
}

void CloudManager::connect2aws()
{
    m_awsConnector->connect2AWS(m_serverUrl,
                                m_deviceId.toString().remove(QRegExp("[{}]*")),
                                m_deviceName,
                                m_caCertificate,
                                m_clientCertificate,
                                m_clientCertificateKey
                                );
}

void CloudManager::onlineStateChanged()
{
    if (m_networkManager->state() == NetworkManager::NetworkManagerStateConnectedGlobal) {
        if (m_enabled && !m_awsConnector->isConnected()) {
            connect2aws();
        }
    }
}

void CloudManager::onPairingFinished(const QString &cognitoUserId, int errorCode, const QString &message)
{
    emit pairingReply(cognitoUserId, errorCode, message);
}

void CloudManager::awsConnected()
{
    emit connectionStateChanged();
}

void CloudManager::awsDisconnected()
{
    emit connectionStateChanged();
}
