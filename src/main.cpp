 /*************************************************************************
*   Copyright (C) 2009 by Christoph Eckert                                *
*   ce@christeck.de                                                       *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "Versioning.h"

#include <QtGui/QtGui>
#include "MyMainWindow.h"

int main(int argc, char *argv[])
  {
    QApplication app(argc, argv);

    QString languageFile;
    QLocale DefaultLocale;
    languageFile = DefaultLocale.name();
    languageFile.append( ".qm" );
    languageFile.prepend( ":/translations/" );
    QTranslator translator( &app );
    QTranslator qtTranslator;
    translator.load( languageFile );
    qtTranslator.load( QString("qt_") + DefaultLocale.name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator( &translator );
    app.installTranslator( &qtTranslator );

    MyMainWindow * MainForm = new MyMainWindow;
    MainForm->show();
    
    if ( argc > 1 )
      {
	QString FileName ( argv[1] );
	QFileInfo FileInfo( FileName );
	if ( FileInfo.isReadable() )
	  {
	    MainForm->loadFile( FileName );
	  }
      }

    return app.exec();
  }


