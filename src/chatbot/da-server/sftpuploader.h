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

#ifndef LVK_DAS_SFTPUPLOADER_H
#define LVK_DAS_SFTPUPLOADER_H

#include <QObject>

#include "fileuploader.h"
#include "qssh/sftpchannel.h"
#include "qssh/sshconnection.h"

class QMutex;

namespace Lvk
{

/// \addtogroup Lvk
/// @{

namespace DAS
{

/// \ingroup Lvk
/// \addtogroup DAS
/// @{

/**
 * \brief The SftpUploader class provides an SFTP file uploader
 */
class SftpUploader : public FileUploader
{
    Q_OBJECT

public:

    /**
     * Constructs a SftpUploader object
     */
    SftpUploader();

    /**
     * Destroys the object and closes any connection
     */
    ~SftpUploader();

    /**
     * \copydoc FileUploader::upload
     */
    virtual void upload(const QString &filename, const QString &username);

signals:

private slots:
    void onConnected();
    void onConnectionError(QSsh::SshError);
    void onChannelInitialized();
    void onChannelError(const QString &err);
    void onOpfinished(QSsh::SftpJobId job, const QString & error = QString());

private:
    QMutex *m_mutex;
    bool m_inProgress;
    QString m_localFilename;
    QString m_remoteFilename;
    QSsh::SftpChannel::Ptr m_channel;
    QSsh::SshConnection *m_connection;

    void initFilenames(const QString &filename, const QString &username);
    void getConnectionParams(QSsh::SshConnectionParameters &params);
    void parseDestination(const QString &dest);
    void close();
};

/// @}

} // namespace DAS

/// @}

} // namespace Lvk


#endif // LVK_DAS_SFTPUPLOADER_H