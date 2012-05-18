/*
 * Copyright (C) 2012 Andres Pagliano, Gabriel Miretti, Gonzalo Buteler,
 * Nestor Bustamante, Pablo Perez de Angelis
 *
 * This file is part of LVK Botmaster.
 *
 * LVK Botmaster is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LVK Botmaster is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LVK Botmaster.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "xmmpchatbot.h"
#include "chatvirtualuser.h"

#include "QXmppClient.h"
#include "QXmppMessage.h"
#include "QXmppVCardManager.h"
#include "QXmppRosterManager.h"
#include "QXmppVCardIq.h"
#include "QXmppMucManager.h"


#include <QMutex>
#include <QMutexLocker>
#include <iostream>

//--------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------

namespace
{

QString getBareJid(const QString &from)
{
    return from.split("/").at(0);
}

QXmppMucManager *mucManager;

} // names


//--------------------------------------------------------------------------------------------------
// XmppChatbot
//--------------------------------------------------------------------------------------------------

Lvk::CA::XmppChatbot::XmppChatbot(QObject *parent)
    : m_xmppClient(new QXmppClient(parent)),
      m_virtualUser(0),
      m_contactInfoMutex(new QMutex()),
      m_rosterMutex(new QMutex()),
      m_rosterHasChanged(false)
{
    // Signals

    connect(m_xmppClient, SIGNAL(messageReceived(const QXmppMessage&)),
            this, SLOT(onMessageReceived(const QXmppMessage&)));

    connect(m_xmppClient, SIGNAL(connected()),    SLOT(onConnected()));

    connect(&m_xmppClient->rosterManager(), SIGNAL(rosterReceived()), SLOT(onRosterReceived()));
    connect(&m_xmppClient->rosterManager(), SIGNAL(rosterChanged(QString)),
            SLOT(onRosterChanged(QString)));

    connect(&m_xmppClient->vCardManager(), SIGNAL(vCardReceived(const QXmppVCardIq&)),
            this, SLOT(onVCardReceived(const QXmppVCardIq&)));

    connect(m_xmppClient, SIGNAL(disconnected()), SIGNAL(disconnected()));

    connect(m_xmppClient, SIGNAL(error(QXmppClient::Error)),
            SLOT(emitLocalError(QXmppClient::Error)));

    connect(m_xmppClient, SIGNAL(iqReceived(QXmppIq)), SLOT(onIqReceived(QXmppIq)));

    connect(m_xmppClient, SIGNAL(presenceReceived(QXmppPresence)),
            SLOT(onPresenceReceived(QXmppPresence)));

    mucManager = new QXmppMucManager;
    m_xmppClient->addExtension(mucManager);

    connect(mucManager, SIGNAL(invitationReceived(QString,QString,QString)),
            SLOT(onInvitationReceived(QString,QString,QString)));

    // Xmpp Logger

    QXmppLogger *xmppLogger = new QXmppLogger(this);
    xmppLogger->setLoggingType(QXmppLogger::FileLogging);
    xmppLogger->setLogFilePath("./xmpp.log");
    xmppLogger->setMessageTypes(QXmppLogger::AnyMessage);
    m_xmppClient->setLogger(xmppLogger);
}

//--------------------------------------------------------------------------------------------------

Lvk::CA::XmppChatbot::~XmppChatbot()
{
    delete m_rosterMutex;
    delete m_contactInfoMutex;
    delete m_virtualUser;
    delete m_xmppClient;
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::connectToServer(const QString &user, const QString &passwd,
                                          const QString &domain)
{
//    QXmppConfiguration conf;
//    conf.setDomain(domain);
//    conf.setUser(user);
//    conf.setPassword(passwd);
//    conf.setResource("");
//    conf.setAutoAcceptSubscriptions(true);

//    m_xmppClient->connectToServer(conf);
    m_xmppClient->connectToServer(user + "@" + domain, passwd);
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::disconnectFromServer()
{
    m_xmppClient->disconnectFromServer();
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::setVirtualUser(Lvk::CA::VirtualUser *virtualUser)
{
    if (m_virtualUser != virtualUser) {
        delete m_virtualUser;
        m_virtualUser = virtualUser;
    }
}

//--------------------------------------------------------------------------------------------------

Lvk::CA::VirtualUser * Lvk::CA::XmppChatbot::virtualUser()
{
    return m_virtualUser;
}

//--------------------------------------------------------------------------------------------------

Lvk::CA::ContactInfoList Lvk::CA::XmppChatbot::roster() const
{
    QMutexLocker locker(m_rosterMutex);

    if (m_rosterHasChanged) {
        m_roster.clear();

        foreach (const QString &jid, m_xmppClient->rosterManager().getRosterBareJids()) {
            m_roster.append(getContactInfo(jid));
        }
    }

    return m_roster;
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::setBlackListRoster(const Lvk::CA::ContactInfoList &blackList)
{
    QMutexLocker locker(m_rosterMutex);

    m_blackListRoster = blackList;

    // Set for fast look-up

    m_blackListSet.clear();
    foreach (const ContactInfo &info, blackList) {
        m_blackListSet.insert(info.username);
    }
}

//--------------------------------------------------------------------------------------------------

Lvk::CA::ContactInfoList Lvk::CA::XmppChatbot::blackListRoster() const
{
    QMutexLocker locker(m_rosterMutex);

    return m_blackListRoster;
}

//--------------------------------------------------------------------------------------------------


bool Lvk::CA::XmppChatbot::isInBlackList(const QString &jid)
{
    QMutexLocker locker(m_rosterMutex);

    return m_blackListSet.contains(jid);
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::emitLocalError(QXmppClient::Error err)
{
    emit error(convertToLocalError(err));
}

//--------------------------------------------------------------------------------------------------

Lvk::CA::XmppChatbot::Error Lvk::CA::XmppChatbot::convertToLocalError(QXmppClient::Error err)
{
    switch (err) {
    case QXmppClient::SocketError:
        return XmppChatbot::SocketError;
    case QXmppClient::KeepAliveError:
        return XmppChatbot::KeepAliveError;
    case QXmppClient::XmppStreamError:
        return XmppChatbot::XmppStreamError;
    default:
        return XmppChatbot::InternalError;
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::onMessageReceived(const QXmppMessage& msg)
{
    std::cout << "onMessageReceived from " << m_xmppClient->rosterManager().
                 getRosterEntry(msg.from()).name().toStdString() << std::endl;

    if (msg.type() != QXmppMessage::Chat) {
        return;
    }
    if (!m_virtualUser) {
        return;
    }
    if (msg.body().isEmpty()) {
        return;
    }

    QString bareJid = getBareJid(msg.from());

    if (!isInBlackList(bareJid)) {
        ContactInfo info = getContactInfo(bareJid);

        QString response = m_virtualUser->getResponse(msg.body(), info);

        if (!response.isEmpty()) {
            m_xmppClient->sendPacket(QXmppMessage("", msg.from(), response));
        }
    }
}


//--------------------------------------------------------------------------------------------------

Lvk::CA::ContactInfo Lvk::CA::XmppChatbot::getContactInfo(const QString &bareJid) const
{
    QXmppRosterIq::Item item = m_xmppClient->rosterManager().getRosterEntry(bareJid);

    return CA::ContactInfo(bareJid, item.name());
}

//--------------------------------------------------------------------------------------------------

QXmppVCardIq Lvk::CA::XmppChatbot::getVCard(const QString &bareJid)
{
    QMutexLocker locker(m_contactInfoMutex);

    QXmppVCardIq vCard;

    if (m_vCards.contains(bareJid)) {
        vCard = m_vCards[bareJid];
    }

    return vCard;
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::requestVCard(const QString &bareJid)
{
    m_xmppClient->vCardManager().requestVCard(bareJid);
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::onVCardReceived(const QXmppVCardIq &vCard)
{
    QString bareJid = getBareJid(vCard.from());

    QMutexLocker locker(m_contactInfoMutex);
    m_vCards[bareJid] = vCard;
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::onConnected()
{
    // Nothing to do
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::onRosterReceived()
{
    m_rosterHasChanged = true;

    ////////////////////////////////////////////////////////////////

    //    <iq xmlns='jabber:client'
    //        type='set'
    //        id='enable1'>
    //      <enable xmlns='urn:xmpp:carbons:1'/>
    //    </iq>

    QXmppElement enableNode;
    enableNode.setTagName("enable");
    enableNode.setAttribute("xmlns", "urn:xmpp:carbons:1");

    QXmppIq iq;
    iq.setType(QXmppIq::Set);
    iq.setExtensions(QXmppElementList() << enableNode);

    QString xml;
    QXmlStreamWriter writer(&xml);
    iq.toXml(&writer);

    std::cout << "=== iqSent ===" << std::endl;
    std::cout <<  xml.toStdString() << std::endl;

    //     <iq id="qxmpp9" type="set">
    //          <enable xmlns="urn:xmpp:carbons:1"/>
    //     </iq>

    m_xmppClient->sendPacket(iq);
    ////////////////////////////////////////////////////////////////

    emit connected();
}

//--------------------------------------------------------------------------------------------------

void Lvk::CA::XmppChatbot::onRosterChanged(const QString &/*bareJid*/)
{
    m_rosterHasChanged = true;
}




void Lvk::CA::XmppChatbot::onIqReceived(QXmppIq iq)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    iq.toXml(&writer);

    std::cout << "=== onIqReceived ===" << std::endl;
    std::cout <<  xml.toStdString() << std::endl;
}

void Lvk::CA::XmppChatbot::onPresenceReceived(QXmppPresence /*p*/)
{
//    QString xml;
//    QXmlStreamWriter writer(&xml);
//    p.toXml(&writer);

//    std::cout << "=== onPresenceReceived ===" << std::endl;
//    std::cout <<  xml.toStdString() << std::endl;

//    std::cout << " *** " << m_xmppClient->rosterManager().getRosterEntry(p.from()).getName().toStdString() << std::endl;
}

void Lvk::CA::XmppChatbot::onInvitationReceived(QString, QString, QString)
{
    std::cout << "=== onInvitationReceived ===" << std::endl;
}


