/*
 * Copyright (c) 2007 Henrique Pinto <henrique.pinto@kdemail.net>
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

#include "libarchivehandler.h"
#include "settings.h"

#include <archive.h>
#include <archive_entry.h>

#include <kdebug.h>
#include <ThreadWeaver/Job>
#include <ThreadWeaver/Weaver>

#include <QFile>
#include <QDir>
#include <QList>
#include <QStringList>

class ArchiveListingJob: public ThreadWeaver::Job
{
	public:
		ArchiveListingJob( const QString& fileName, QObject *parent = 0 )
			: Job( parent ), m_fileName( fileName ), m_success( false )
		{
		}

		QList<ArchiveEntry> entries() const { return m_entries; }

		virtual bool success() const { return m_success; }

	protected:
		void run()
		{
			m_success = listEntries();
			kDebug( 1601 ) << k_funcinfo << "listEntries() returned " << m_success << endl;
		}

		bool listEntries()
		{
			struct archive *arch;
			struct archive_entry *entry;
			int result;

			arch = archive_read_new();
			if ( !arch )
				return false;

			result = archive_read_support_compression_all( arch );
			if ( result != ARCHIVE_OK ) return false;

			result = archive_read_support_format_all( arch );
			if ( result != ARCHIVE_OK ) return false;

			result = archive_read_open_filename( arch, QFile::encodeName( m_fileName ), 4096 );

			if ( result != ARCHIVE_OK )
			{
				kDebug( 1601 ) << "Couldn't open the file '" << m_fileName << "', libarchive can't handle it." << endl;
				return false;
			}

			while ( ( result = archive_read_next_header( arch, &entry ) ) == ARCHIVE_OK )
			{
				ArchiveEntry e;
				e[ FileName ] = QString( archive_entry_pathname( entry ) );
				e[ Size ] = ( qlonglong ) archive_entry_size( entry );
				if ( archive_entry_symlink( entry ) )
				{
					e[ Link ] = archive_entry_symlink( entry );
				}
				m_entries.append( e );
				archive_read_data_skip( arch );
			}

			if ( result != ARCHIVE_EOF )
			{
				return false;
			}

			return archive_read_finish( arch ) == ARCHIVE_OK;
		}

	private:
		QString m_fileName;
		QList<ArchiveEntry> m_entries;
		bool m_success;
};

class ExtractionJob: public ThreadWeaver::Job
{
	public:
		ExtractionJob( const QString & fileName, const QStringList & entries, const QString & baseDirectory, QObject *parent = 0 )
			: ThreadWeaver::Job( parent ), m_fileName( fileName ), m_entries( entries ),
			  m_directory( baseDirectory ), m_success( false )
		{
		}

		bool success() const { return m_success; }
	protected:
		void run()
		{
			kDebug( 1601 ) << "ExtractionJob::run() will try to extract " << m_entries << endl;
			m_success = extractFiles();
		}

	private:
		int flags()
		{
			int result = ARCHIVE_EXTRACT_TIME;

			if ( ArkSettings::preservePerms() )
			{
				result &= ARCHIVE_EXTRACT_PERM;
			}

			if ( !ArkSettings::extractOverwrite() )
			{
				result &= ARCHIVE_EXTRACT_NO_OVERWRITE;
			}

			return result;
		}

		bool extractFiles()
		{
			QDir::setCurrent( m_directory );

			const bool extractAll = m_entries.isEmpty();
			struct archive *arch, *writer;
			struct archive_entry *entry;

			arch = archive_read_new();
			if ( !arch )
			{
				return false;
			}

			writer = archive_write_disk_new();
			archive_write_disk_set_options( writer, flags() );

			archive_read_support_compression_all( arch );
			archive_read_support_format_all( arch );
			int res = archive_read_open_filename( arch, QFile::encodeName( m_fileName ), 10240 );

			if ( res != ARCHIVE_OK )
			{
				kDebug( 1601 ) << "Couldn't open the file '" << m_fileName << "', libarchive can't handle it." << endl;
				return false;
			}

			while ( archive_read_next_header( arch, &entry ) == ARCHIVE_OK )
			{
				QString entryName = QFile::decodeName( archive_entry_pathname( entry ) );
				if ( m_entries.contains( entryName ) || extractAll )
				{
					if ( archive_write_header( writer, entry ) == ARCHIVE_OK )
						copyData( arch, writer );
					m_entries.removeAll( entryName );
				}
				else
				{
					kDebug( 1601 ) << entryName << " matches no requested file. " << endl;
					archive_read_data_skip( arch );
				}
			}
			if ( m_entries.size() > 0 ) return false;
			return archive_read_finish( arch ) == ARCHIVE_OK;
		}

		void copyData( struct archive *source, struct archive *dest )
		{
			const void *buff = 0;
			size_t size;
			off_t  offset;

			while ( archive_read_data_block( source, &buff, &size, &offset ) == ARCHIVE_OK )
			{
				kDebug( 1601 )  << "	copyData: copying " << size << " from offset " << offset << endl;
				archive_write_data_block( dest, buff, size, offset );
			}
		}

		QString     m_fileName;
		QStringList m_entries;
		QString     m_directory;
		bool        m_success;
};

LibArchiveHandler::LibArchiveHandler( const QString &filename )
	: Arch( filename )
{
	kDebug( 1601 ) << "libarchive api version = " << archive_api_version() << endl;
}

LibArchiveHandler::~LibArchiveHandler()
{
}

void LibArchiveHandler::open()
{
	ArchiveListingJob *job = new ArchiveListingJob( fileName(), this );
	connect( job, SIGNAL( done( ThreadWeaver::Job* ) ),
	         this, SLOT( listingDone( ThreadWeaver::Job * ) ) );
	ThreadWeaver::Weaver::instance()->enqueue( job );
}

void LibArchiveHandler::listingDone( ThreadWeaver::Job *job )
{
	foreach( ArchiveEntry entry, static_cast<ArchiveListingJob*>( job )->entries() )
	{
		emit newEntry( entry );
	}

	emit opened( job->success() );
	delete job;
}

void LibArchiveHandler::create()
{
}

void LibArchiveHandler::addFile( const QStringList & )
{
}

void LibArchiveHandler::addDir( const QString & )
{
}

void LibArchiveHandler::remove( const QStringList & )
{
}

void LibArchiveHandler::extractFiles( const QStringList & files, const QString& destinationDir )
{
	ExtractionJob *job = new ExtractionJob( fileName(), files, destinationDir, this );
	connect( job, SIGNAL( done( ThreadWeaver::Job* ) ),
	         this, SLOT( extractionDone( ThreadWeaver::Job * ) ) );
	ThreadWeaver::Weaver::instance()->enqueue( job );
}

void LibArchiveHandler::extractionDone( ThreadWeaver::Job *job )
{
	emit sigExtract( job->success() );
	delete job;
}

#include "libarchivehandler.moc"
