/*

 $Id$

 ark -- archiver for the KDE project

 Copyright (C)

 1997-1999: Rob Palmbos palm9744@kettering.edu
 1999: Francois-Xavier Duranceau duranceau@kde.org

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// Qt includes
#include <qlayout.h>

#include <klocale.h>
#include <kpushbutton.h>
#include <kstdguiitem.h>

// ark includes
#include "shellOutputDlg.h"
#include "shellOutputDlg.moc"
#include "arksettings.h"

ShellOutputDlg::ShellOutputDlg( ArkSettings *_data, QWidget *_parent,
				const char *_name )
	: QDialog( _parent, _name, true )
{
  setCaption( i18n("Shell Output") );
  QGridLayout *grid1 = new QGridLayout(this,10,5,15,7);  
  QMultiLineEdit *l1 = new QMultiLineEdit( this );
  l1->setReadOnly( true );
  grid1->addMultiCellWidget(l1,0,8,0,4);

  l1->setText( *(_data->getLastShellOutput()) );
  l1->setCursorPosition(l1->numLines(), 0);

  QPushButton *close = new KPushButton( KStdGuiItem::close(), this );
  grid1->addWidget(close,9,4);
  connect( close, SIGNAL( clicked() ), SLOT( accept() ) );
  close->setFocus();
  resize(520, 380);
}
