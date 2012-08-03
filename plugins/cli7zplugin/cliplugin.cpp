/*
 * ark -- archiver for the KDE project
 *
 * Copyright (C) 2009 Harald Hvaal <haraldhv@stud.ntnu.no>
 * Copyright (C) 2009-2011 Raphael Kubo da Costa <rakuco@FreeBSD.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "cliplugin.h"
#include "kerfuffle/cliinterface.h"
#include "kerfuffle/kerfuffle_export.h"

#include <QDateTime>
#include <QDir>
#include <QLatin1String>
#include <QString>

#include <KDebug>

using namespace Kerfuffle;

CliPlugin::CliPlugin(QObject *parent, const QVariantList & args)
    : CliInterface(parent, args)
    , m_archiveType(ArchiveType7z)
    , m_state(ReadStateHeader)
{
}

CliPlugin::~CliPlugin()
{
}

bool CliPlugin::supportsOption(const Kerfuffle::SupportedOptions option, const QString & mimeType)
{
    bool ret = CliInterface::supportsOption(option, mimeType);

    // 7z only supports EncryptHeader for 7z archive type, not for zip, tar, etc.
    if (option == EncryptHeader && mimeType != QLatin1String("application/x-7z-compressed")) {
        return false;
    }

    return ret;
}

ParameterList CliPlugin::parameterList() const
{
    static ParameterList p;

    if (p.isEmpty()) {
        //p[CaptureProgress] = true;
        p[ListProgram] = p[ExtractProgram] = p[DeleteProgram] = p[AddProgram] = QStringList() << QLatin1String( "7zr" ) << QLatin1String( "7za" ) << QLatin1String( "7z" );

        p[ListArgs] = QStringList() << QLatin1String("l") << QLatin1String("-slt") << QLatin1String("$Archive");
        p[ExtractArgs] = QStringList() << QLatin1String("$MultiThreadingSwitch") << QLatin1String("$PreservePathSwitch") << QLatin1String("$PasswordSwitch") << QLatin1String("$Archive") << QLatin1String("$Files");
        p[PreservePathSwitch] = QStringList() << QLatin1String("x") << QLatin1String("e");
        p[PasswordSwitch] = QStringList() << QLatin1String("-p$Password");
        p[FileExistsExpression] = QLatin1String("already exists. Overwrite with");
        p[WrongPasswordPatterns] = QStringList() << QLatin1String("Wrong password");
        p[AddArgs] = QStringList() << QLatin1String("a") << QLatin1String("$TemporaryDirectorySwitch") << QLatin1String("$CompressionLevelSwitch") << QLatin1String("$MultiThreadingSwitch") << QLatin1String("$PasswordSwitch") << QLatin1String("$EncryptHeaderSwitch")  << QLatin1String("$MultiPartSwitch") << QLatin1String("$Archive") << QLatin1String("$Files");
        p[DeleteArgs] = QStringList() << QLatin1String("d") << QLatin1String("$Archive") << QLatin1String("$Files");

        p[FileExistsInput] = QStringList()
                             << QLatin1String("Y")   //overwrite
                             << QLatin1String("N")   //skip
                             << QLatin1String("A")   //overwrite all
                             << QLatin1String("S")   //autoskip
                             << QLatin1String("Q")   //cancel
                             << QLatin1String("U")   //autorename
                             ;
        p[SupportsRename] = true; // just for GUI feedback

        p[CompressionLevelSwitches] = QStringList() << QLatin1String("-mx=0") << QLatin1String("-mx=2") << QLatin1String("-mx=5") << QLatin1String("-mx=7") << QLatin1String("-mx=9");
        p[MultiThreadingSwitch] = QLatin1String("-mmt");
        p[MultiPartSwitch] = QLatin1String("-v$MultiPartSizek");

        p[PasswordPromptPattern] = QLatin1String("Enter password \\(will not be echoed\\) :");

        p[EncryptHeaderSwitch] = QLatin1String("-mhe");

        p[TestProgram] = QLatin1String("7z");
        p[TestArgs] = QStringList() << QLatin1String("t") << QLatin1String("$PasswordSwitch") << QLatin1String("$Archive") << QLatin1String("$Files");
        p[TestFailedPatterns] = QStringList() << QLatin1String("Data Error") << QLatin1String("CRC Failed") << QLatin1String("Can not open file as archive");
        p[TemporaryDirectorySwitch] = QLatin1String("-w$DirectoryPath");
    }

    return p;
}

bool CliPlugin::readListLine(const QString& line)
{
    static const QLatin1String archiveInfoDelimiter1("--"); // 7z 9.13+
    static const QLatin1String archiveInfoDelimiter2("----"); // 7z 9.04
    static const QLatin1String entryInfoDelimiter("----------");
    static int volumes = -1;
    static QString fileName;

    switch (m_state) {
    case ReadStateHeader:
        if (line.startsWith(QLatin1String("Listing archive:"))) {
            kDebug(1601) << "Archive name: "
                         << line.right(line.size() - 16).trimmed();
            volumes = 1;
            fileName.clear();
        } else if ((line == archiveInfoDelimiter1) ||
                   (line == archiveInfoDelimiter2)) {
            m_state = ReadStateArchiveInformation;
        } else if (line.contains(QLatin1String("Error:"))) {
            kDebug(1601) << line.mid(6);
        }
        break;

    case ReadStateArchiveInformation:
        if (line == entryInfoDelimiter) {
            m_state = ReadStateEntryInformation;
        } else if (line.startsWith(QLatin1String("Type ="))) {
            const QString type = line.mid(7).trimmed().toLower();
            kDebug(1601) << "Archive type: " << type;

            if (type == QLatin1String("7z")) {
                m_archiveType = ArchiveType7z;
            } else if (type == QLatin1String("bzip2")) {
                m_archiveType = ArchiveTypeBZip2;
            } else if (type == QLatin1String("gzip")) {
                m_archiveType = ArchiveTypeGZip;
            } else if (type == QLatin1String("tar")) {
                m_archiveType = ArchiveTypeTar;
            } else if (type == QLatin1String("zip")) {
                m_archiveType = ArchiveTypeZip;
            } else if (type == QLatin1String("split")) {
                // the real type will be detected later
            } else {
                // Should not happen
                kWarning() << "Unsupported archive type";
                return false;
            }
        } else if (line.startsWith(QLatin1String("Volumes ="))) {
            bool ok;
            volumes = line.mid(9).toInt(&ok);
        } else if (volumes > 1 && line.startsWith(QLatin1String("Path ="))) {
            fileName = line.mid(6).trimmed();
            fileName = fileName.left(fileName.size() - 4); // remove .bz2 extension
        }

        break;

    case ReadStateEntryInformation:
        // '7z l -slt' does not print the line 'Path =' for tar.bz2.001 archives in ReadStateEntryInformation state.
        if (line.startsWith(QLatin1String("Path =")) || (m_archiveType == ArchiveTypeBZip2 && !fileName.isEmpty())) {
            QString entryFilename;
            if (m_archiveType == ArchiveTypeBZip2 && !fileName.isEmpty()) {
                entryFilename = fileName;
                fileName.clear();
            } else {
                entryFilename = line.mid(6).trimmed();
            }
            entryFilename = QDir::fromNativeSeparators(entryFilename);
            m_currentArchiveEntry.clear();
            m_currentArchiveEntry[FileName] = autoConvertEncoding(entryFilename);
            m_currentArchiveEntry[InternalID] = entryFilename;
        } else if (line.startsWith(QLatin1String("Size = "))) {
            m_currentArchiveEntry[ Size ] = line.mid(7).trimmed();
        } else if (line.startsWith(QLatin1String("Packed Size = "))) {
            // #236696: 7z files only show a single Packed Size value
            //          corresponding to the whole archive.
            if (m_archiveType != ArchiveType7z) {
                m_currentArchiveEntry[CompressedSize] = line.mid(14).trimmed();
            }
        } else if (line.startsWith(QLatin1String("Modified = "))) {
            m_currentArchiveEntry[ Timestamp ] =
                QDateTime::fromString(line.mid(11).trimmed(),
                                      QLatin1String("yyyy-MM-dd hh:mm:ss"));
        } else if (line.startsWith(QLatin1String("Attributes = "))) {
            const QString attributes = line.mid(13).trimmed();

            const bool isDirectory = attributes.startsWith(QLatin1Char('D'));
            m_currentArchiveEntry[ IsDirectory ] = isDirectory;
            if (isDirectory) {
                const QString directoryName =
                    m_currentArchiveEntry[FileName].toString();
                if (!directoryName.endsWith(QLatin1Char('/'))) {
                    const bool isPasswordProtected = (line.at(12) == QLatin1Char('+'));
                    m_currentArchiveEntry[FileName] =
                        m_currentArchiveEntry[InternalID] = QString(directoryName + QLatin1Char('/'));
                    m_currentArchiveEntry[ IsPasswordProtected ] =
                        isPasswordProtected;
                }
            }

            m_currentArchiveEntry[ Permissions ] = attributes.mid(1);
        } else if (line.startsWith(QLatin1String("CRC = "))) {
            m_currentArchiveEntry[ CRC ] = line.mid(6).trimmed();
        } else if (line.startsWith(QLatin1String("Method = "))) {
            m_currentArchiveEntry[ Method ] = line.mid(9).trimmed();
        } else if (line.startsWith(QLatin1String("Encrypted = ")) &&
                   line.size() >= 13) {
            m_currentArchiveEntry[ IsPasswordProtected ] = (line.at(12) == QLatin1Char('+'));
        } else if (line.isEmpty()) {
            if (m_currentArchiveEntry.contains(FileName)) {
                emit entry(m_currentArchiveEntry);
            }
        }
        break;
    }

    return true;
}

void CliPlugin::resetReadState()
{
    m_state = ReadStateHeader;
}

void CliPlugin::saveLastLine(const QString & line)
{
    m_lastLine = line;
}

// for simplicity checks only the last line, otherwise we would have to parse
// every entry passed to saveLastLine.
QString CliPlugin::fileExistsName()
{
    QRegExp existsPattern(QLatin1String("^file (.+)"));

    if (existsPattern.indexIn(m_lastLine) != -1) {
        return existsPattern.cap(1);
    }

    return QString();
}

KERFUFFLE_EXPORT_PLUGIN(CliPlugin)

#include "cliplugin.moc"
