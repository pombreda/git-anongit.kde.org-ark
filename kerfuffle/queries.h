/*
 * ark -- archiver for the KDE project
 *
 * Copyright (C) 2008 Harald Hvaal <haraldhv@stud.ntnu.no>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES ( INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION ) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * ( INCLUDING NEGLIGENCE OR OTHERWISE ) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef QUERIES_H
#define QUERIES_H

#include "kerfuffle_export.h"

#include <QString>
#include <QHash>
#include <QWaitCondition>
#include <QMutex>
#include <QVariant>

namespace Kerfuffle
{

typedef QHash<QString, QVariant> QueryData;

class KERFUFFLE_EXPORT Query
{
public:
    /**
     * Execute the response. Will happen in the GUI thread, so it's
     * safe to use widgets/gui elements here. Must call setResponse
     * when done.
     */
    virtual void execute() = 0;

    /**
     * Will block until the response have been set
     */
    void waitForResponse();

    QVariant response();

protected:
    /**
     * Protected constructor
     */
    Query();
    virtual ~Query() {}

    void setResponse(QVariant response);

    QueryData m_data;

private:
    QWaitCondition m_responseCondition;
    QMutex m_responseMutex;
};

class KERFUFFLE_EXPORT OverwriteQuery : public Query
{
public:
    explicit OverwriteQuery(const QString& filename);
    void execute();
    bool responseCancelled();
    bool responseOverwriteAll();
    bool responseOverwrite();
    bool responseRename();
    bool responseAutoRenameAll();
    bool responseSkip();
    bool responseAutoSkip();
    bool responseUpdateExisting();
    QString newFilename();

    void setNoRenameMode(bool enableNoRenameMode);
    bool noRenameMode();
    void setAutoRenameMode(bool enableAutoRenameMode);
    bool autoRenameMode();
    void setMultiMode(bool enableMultiMode);
    bool multiMode();
    void setUpdateExistingMode(bool updateExisting);
    bool updateExistingMode();

private:
    bool m_noRenameMode;
    bool m_autoRenameMode;
    bool m_multiMode;
    bool m_updateExistingMode;
};

class KERFUFFLE_EXPORT PasswordNeededQuery : public Query
{
public:
    enum PasswordFlag { NoFlags = 0x0, IncorrectTryAgain = 0x1, AskNewPassword = 0x2 };
    Q_DECLARE_FLAGS(PasswordFlags, PasswordFlag);

    explicit PasswordNeededQuery(const QString& archiveFilename, const PasswordFlags flags = NoFlags);
    void execute();

    bool responseCancelled();
    QString password();
};
Q_DECLARE_OPERATORS_FOR_FLAGS(PasswordNeededQuery::PasswordFlags)

class KERFUFFLE_EXPORT UseCurrentPasswordQuery : public Query
{
public:
    explicit UseCurrentPasswordQuery(const QString& archiveFilename);
    void execute();

    bool responseYes();
    bool responseNo();
    bool responseYesAll();
};

}

#endif /* ifndef QUERIES_H */
