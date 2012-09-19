/*
 * Copyright (C) 2012 Andres Pagliano, Gabriel Miretti, Gonzalo Buteler,
 * Nestor Bustamante, Pablo Perez de Angelis
 *
 * This file is part of LVK Chatbot.
 *
 * LVK Chatbot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LVK Chatbot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LVK Chatbot.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stats/statsmanager.h"
#include "stats/securestatsfile.h"
#include "common/settings.h"
#include "common/settingskeys.h"

#include <QMutex>
#include <QMutexLocker>
#include <QMetaType>
#include <QtDebug>

#define STATS_FILE_PREFIX ""
#define STATS_FILE_EXT    ".stat"

#define SCORE_INTERVAL_DUR                (5*3600) // In seconds


//--------------------------------------------------------------------------------------------------
// StatsManager
//--------------------------------------------------------------------------------------------------

namespace
{

inline QString getStatsFilename(const QString &id)
{
    Lvk::Cmn::Settings settings;
    QString statsPath = settings.value(SETTING_STATS_PATH).toString();

    QString filename = STATS_FILE_PREFIX + id + STATS_FILE_EXT;

    return statsPath + "/" + filename;
}

} // namespace


//--------------------------------------------------------------------------------------------------
// StatsManager
//--------------------------------------------------------------------------------------------------

Lvk::Stats::StatsManager * Lvk::Stats::StatsManager::m_manager = 0;
QMutex *                   Lvk::Stats::StatsManager::m_mgrMutex = new QMutex();

//--------------------------------------------------------------------------------------------------

Lvk::Stats::StatsManager::StatsManager()
    : m_scoreMutex(new QMutex(QMutex::Recursive)),
      m_statsFile(new SecureStatsFile()),
      m_elapsedTime(0)
{
    connect(&m_scoreTimer, SIGNAL(timeout()), SLOT(onScoreTick()));

    qRegisterMetaType<Lvk::Stats::Score>("Lvk::Stats::Score");
    qRegisterMetaTypeStreamOperators<Lvk::Stats::Score>("Lvk::Stats::Score");
}

//--------------------------------------------------------------------------------------------------

Lvk::Stats::StatsManager * Lvk::Stats::StatsManager::manager()
{
    if (!m_manager) {
        QMutexLocker locker(m_mgrMutex);
        if (!m_manager) {
            static StatsManager manager;
            m_manager = &manager;
        }
    }

    return m_manager;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::setChatbotId(const QString &chatbotId)
{
    QMutexLocker locker(m_scoreMutex);

    m_scoreTimer.stop();

    if (!m_chatbotId.isEmpty()) {
        if (!m_statsFile->isEmpty()) {
            m_statsFile->save();
        }
        m_statsFile->close();
        m_chatbotId.clear();
    }

    if (!chatbotId.isEmpty()) {
        m_chatbotId = chatbotId;
        m_statsFile->load(getStatsFilename(chatbotId));
        m_elapsedTime = m_statsFile->scoreElapsedTime();

        Cmn::Conversation h;
        m_statsFile->chatHistory(h);

        m_histStats = Stats::HistoryStatsHelper(h);
        m_ruleStats.clear(); // FIXME init
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::metric(Stats::Metric m, QVariant &value)
{
    return m_statsFile->metric(m, value);
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::clear()
{
    QMutexLocker locker(m_scoreMutex);

    m_statsFile->clear();
}

//--------------------------------------------------------------------------------------------------

Lvk::Stats::Score Lvk::Stats::StatsManager::currentScore()
{
    QMutexLocker locker(m_scoreMutex);

    unsigned hic  = m_statsFile->contacts().size();
    unsigned trp  = m_ruleStats.points();
    unsigned hcls = m_histStats.chatbotLexiconSize();
    unsigned hcl  = m_histStats.chatbotLines();

    const unsigned SCORE_CONTACTS_POINTS = 1000;

    Stats::Score score;
    score.conversations = hcls + hcl;
    score.contacts      = hic*SCORE_CONTACTS_POINTS;
    score.rules         = trp;
    score.total         = score.conversations + score.contacts + score.rules;

    m_statsFile->setCurrentScore(score);

    return score;
}

//--------------------------------------------------------------------------------------------------

Lvk::Stats::Score Lvk::Stats::StatsManager::bestScore()
{
    QMutexLocker locker(m_scoreMutex);

    updateBestScore();

    return m_statsFile->bestScore();
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::updateBestScore()
{
    Score best = m_statsFile->bestScore();
    Score current = currentScore();

    if (current.total > best.total) {
        best = current;

        m_statsFile->setBestScore(best);
        m_statsFile->save();
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::startTicking()
{
//    m_connStartTime = QDateTime::currentDateTime().toTime_t();

    const int TICK_MSEC = 1000;

    if (!m_scoreTimer.isActive()) {
        m_scoreTimer.start(TICK_MSEC);
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::stopTicking()
{
//    uint duration = QDateTime::currentDateTime().toTime_t() - m_connStartTime;
//    Stats::StatsManager::manager()->setMetric(Stats::ConnectionTime, duration);

    m_scoreTimer.stop();
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::onScoreTick()
{
    m_elapsedTime = (m_elapsedTime + 1) % SCORE_INTERVAL_DUR;

    m_statsFile->setScoreElapsedTime(m_elapsedTime);

    // If score timeout
    if (m_elapsedTime == 0) {
        qDebug() << "Score timeout!";

        QMutexLocker locker(m_scoreMutex);

        updateBestScore();
        m_statsFile->newInterval();
        m_histStats.clear();

        Score s = m_statsFile->currentScore();
        s.conversations = 0; // reset conversation points
        m_statsFile->setCurrentScore(s);
    }

    // Save every minute
    if (m_elapsedTime % 60 == 0) {
        m_statsFile->save();
    }

    emit scoreRemainingTime(scoreRemainingTime());
}

//--------------------------------------------------------------------------------------------------

int Lvk::Stats::StatsManager::scoreRemainingTime() const
{
    return SCORE_INTERVAL_DUR - m_elapsedTime - 1;
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::updateScoreWith(const BE::Rule *root)
{
    QMutexLocker locker(m_scoreMutex);

    m_ruleStats.reset(root);
}

//--------------------------------------------------------------------------------------------------

void Lvk::Stats::StatsManager::updateScoreWith(const Cmn::Conversation::Entry &entry)
{
    QMutexLocker locker(m_scoreMutex);

    int size = m_histStats.scoreContacts().size();

    m_histStats.update(entry);
    m_statsFile->appendChatEntry(entry);

    // If there is a new contact
    if (size < m_histStats.scoreContacts().size()) {
        foreach (const QString &contact, m_histStats.scoreContacts()) {
            m_statsFile->addContact(contact);
        }
    }

}
