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

#include "MyMainWindow.h"


MyMainWindow::MyMainWindow( QWidget* parent ) : QMainWindow( parent )
  {
    setupUi( this );
    setupMenus();
    setAcceptDrops( true );

    QCoreApplication::setOrganizationName( APPNAME );
    QCoreApplication::setOrganizationDomain( AUTHOR );
    QCoreApplication::setApplicationName( APPNAME );
    QIcon AppIcon( ":/icons/AppIcon.png" );
    setWindowIcon( AppIcon );

    emptyImageString = "empty.png";
    unknownDataString = "unknown";
    variousDataString = "various";

    setupMidi();
    midiStatus = idle;
    setupControls();
    initGuiData();
    setupConnections();
    createDatabase();
  }


void MyMainWindow::prepareOpenFile()
  {
    if ( confirmDataLoss() == false )
      return;

    discardData();

    QString filePath = "";
    QSettings appSettings;
    filePath = ( appSettings.value( "FilePathOpen", QDir::homePath() ) ).toString();
    filePath = QFileDialog::getOpenFileName( this, tr("Choose a file"), filePath, tr("Sysex (*.syx *.SYX);;MIDI (*.mid *.MID);;Any (*)"));

    // getOpenFileName returns a null string in case the user pressed channel
    if ( filePath == "" )
      return;

    loadFile( filePath );
    appSettings.setValue( "FilePathOpen", filePath );
  }


void MyMainWindow::prepareReceiveFile()
  {
    if ( confirmDataLoss() == false )
      return;

    discardData();
    midiStatus = receiving;
    setupControls();
    midiIn->startReception();
  }


void MyMainWindow::prepareDiscardData()
  {
    if ( confirmDataLoss() == false )
      return;

    discardData();
  }


void MyMainWindow::prepareTransmitFile()
  {
    if ( midiStatus != idle )
      {
        emit errorMessage( tr( "There was an attempt to send data while the internal status was not idle." ), tr( "Please restart the application and tell the author how to reproduce the problem." ) );
        return;
      }

    /*
    Commented, as sending huge files seems to work very well.
    for ( signed int i = 0; i < eventList.size(); i++ )
      {
        if ( eventList.at( i )->size() > 16355 )
        {
          emit errorMessage( tr( "The data contains huge SysEx events." ), tr( "Please check carefully if the device received the data. If not, send the file to the author for analysis." ) );
          break;
        }
      }
    */
    transmitFile();
  }


void MyMainWindow::loadFile( QString filepath )
  {
    if ( midiStatus != idle )
      {
        emit errorMessage( tr( "There was an attempt to load data while the internal status was not idle." ), tr( "Please restart the application and tell the author how to reproduce the problem." ) );
        return;
      }

    QFile fileHandle;
    fileHandle.setFileName( filepath );
    if ( !fileHandle.open( QIODevice::ReadOnly ) )
      {
        emit errorMessage( tr( "The file cannot be opened." ), tr( "Please choose another file." ) );
        return;
      }

    QByteArray inbytes = fileHandle.readAll();
    fileHandle.close();

    // About the MIDI file format, see http://faydoc.tripod.com/formats/mid.htm
    // The header always is 4D 54 68 64 00 00 00 06
    bool isMidiFile = false;
    if ( inbytes.size() > 6 )
      {
        if(        inbytes.at( 0 ) == 0x4D && inbytes.at( 1 ) == 0x54 && inbytes.at( 2 ) == 0x68 &&
                inbytes.at( 3 ) == 0x64 && inbytes.at( 4 ) == 0x00 && inbytes.at( 5 ) == 0x00 &&
                inbytes.at( 6 ) == 0x00 && inbytes.at( 7 ) == 0x06
          )
          {
            isMidiFile = true;
          }
      }

    vector<unsigned char> * buffer = new vector<unsigned char>;
    bool insideSysex = false;
    for( int i = 0; i < inbytes.size(); i++ )
      {
        // Sysex data starts with an 0xF0 byte (which is -16 in an signed char), and ends with an terminating 0xF7 (which is -9).
        // In case an 0xF0 occurs before a terminating 0xF7, cancel file loading.
        // This will probably exclude some users with some rare (Casio?) synths, but I have no test files so there's no alternative.
        // See http://www.somascape.org/midi/tech/mfile.html#sysex for details.
        if ( inbytes.at( i ) == -16 && insideSysex == true )
          {
            emit errorMessage( tr( "A System Exclusive start byte occured within a SysEx event." ), tr( "All data has been discarded. Either the file is damaged or neither a SysEx nor a MIDI file." ) );
            discardData();
            return;
          }

        if ( inbytes.at( i ) == -16 )
          {
            insideSysex = true;
          }
        if ( insideSysex == true )
          {
            buffer->push_back( inbytes.at( i ) );
          }
        if ( inbytes.at( i ) == -9 )
          {
            insideSysex = false;
          }

        // Checking if one complete event has been parsed
        if ( buffer->size() > 3 && buffer->at( 0 ) == 0xF0 && buffer->at( buffer->size() - 1 ) == 0xF7 )
          {
            if ( isMidiFile == true )
              {
                trimMidiEvent( buffer );
              }
            appendEvent( buffer );
            buffer = new vector<unsigned char>;
          }
      }
    emit statusMessage( tr( "File was loaded" ), 5000 );
    setupControls();
  }


void MyMainWindow::trimMidiEvent( vector<unsigned char> * buffer )
  {
    // About the variable length byte of SysEx events in MIDI files, see
    // http://www.somascape.org/midi/tech/mfile.html#sysex (also detailed info about sysex splitting) and
    // http://www.euclideanspace.com/art/music/midi/midifile/index
    // In short, the variable size is following the initial 0xF0 and gives the remaining size, including the tailing 0xF7
    // In case the leftmost bit is 1 (value > 127), further length bytes will follow. In case of 0, no further length bytes will follow
    // About bitwise operations, see http://www.imb-jena.de/~gmueller/kurse/c_c++/c_operbm.html (german language)
    unsigned int sizeByteAmount = 0;
    for ( unsigned int i = 1; i < buffer->size(); i++ )
      {
        sizeByteAmount++;
        if ( buffer->at( i ) > 127 )
          buffer->at( i ) = buffer->at( i ) & 127;
        else
          break;
      }
    
    for ( unsigned int i = 0; i < sizeByteAmount; i++ )
      {
        buffer->erase( buffer->begin()+1 );
      }
  }


void MyMainWindow::appendEvent( vector<unsigned char> * buffer )
  {
    eventList.append( buffer );
    prepareGuiData( buffer );
  }


bool MyMainWindow::prepareSaveFile()
  {
    QString filePath;
    QSettings appSettings;
  
    filePath = ( appSettings.value( "FilePathSave" ) ).toString();
    if ( filePath == "" )
      filePath = QDir::homePath();

    filePath = QFileDialog::getSaveFileName( this, tr("Choose a destination"), filePath, tr("Sysex (*.syx *.SYX);;Any (*)"));
    if ( filePath != "" )
      {
        if ( !filePath.endsWith( ".syx", Qt::CaseInsensitive ) )
          filePath.append( ".syx" );
        writeFile( filePath );
        appSettings.setValue( "FilePathSave", filePath );
        return true;
      }
    else
      return false;
  }


void MyMainWindow::writeFile( QString filepath )
  {
    if ( midiStatus != idle )
      {
        emit errorMessage( tr( "There was an attempt to save data while the internal status was not idle." ), tr( "Please restart the application and tell the author how to reproduce the problem." ) );
        return;
      }
    QFile fileHandle;
    fileHandle.setFileName( filepath );

    if ( !fileHandle.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
      emit errorMessage( tr( "It was not possible to open the specified file." ), tr( "Please check wether the chosen medium or the file is write protected." ) );

    for ( int i = 0; i < eventList.size(); i++ )
      {
        for ( unsigned int j = 0; j < eventList.at( i )->size(); j++ )
          {
            fileHandle.putChar( eventList.at( i )->at( j ) );
          }
      }

    fileHandle.close();
  }


void MyMainWindow::chunkArrived()
  {
    if ( midiStatus == receiving )
      emit statusMessage( tr( "Received data fragment" ), 500 );
  }


void MyMainWindow::eventArrived( QByteArray eventBytes )
  {
    vector<unsigned char> * buffer = new vector<unsigned char>;
    for ( int i = 0; i < eventBytes.size(); i++ )
      {
        buffer->push_back( eventBytes.at( i ) );
      }
    appendEvent( buffer );
    emit statusMessage( "", 2000 );
  }


void MyMainWindow::transmitFile()
  {
    midiOut->setEventList( eventList );
    midiStatus = transmitting;
    setupControls();
    midiOut->start();
  }


void MyMainWindow::cancelTransmission()
  {
    if ( midiStatus == transmitting )
      {
        midiOut->stop();
      }
    else if ( midiStatus == receiving )
      {
        midiIn->stopReception();
      }
    midiStatus = idle;
    setupControls();
    emit statusMessage( tr( "Transmission cancelled" ), 5000 );
  }


bool MyMainWindow::confirmDataLoss()
  {
    if ( eventList.size() == 0 )
      return true;
    QMessageBox messageBox;
    messageBox.setText( tr( "This will discard all currently loaded data." ) );
    messageBox.setInformativeText( tr( "Do you want to continue?" ) );
    messageBox.setStandardButtons( QMessageBox::Ok | QMessageBox::Cancel );
    messageBox.setDefaultButton( QMessageBox::Ok );
    if ( messageBox.exec() == QMessageBox::Ok )
      return true;
    else
      return false;
  }


void MyMainWindow::discardData()
  {
    eventList.clear();
    setupControls();
    initGuiData();
    emit statusMessage( tr( "Data discarded" ), 5000 );
  }


void MyMainWindow::displayAboutBox()
  {
    AboutBox = new QMessageBox(this);
    QString AboutText;
    AboutText.append( APPNAME ).append( " " ).append( (uint)VERSION ).append( tr( " is (c) " ) ).append( AUTHOR ).append( " ").append( YEARS ).append( ".\n" );
    AboutText.append( tr("It uses the Famous QT4 toolkit and the RtMidi-Classes of Gary P. Scavone.\n\n") );
    AboutText.append( tr("%1 has been released under terms and conditions of the GNU general public license version 2.\n").arg( APPNAME ) );
    AboutBox->setText( AboutText );
    AboutBox->show();
  }


void MyMainWindow::displayErrorMessage( QString message, QString advice )
  {
    QMessageBox messageBox;
    messageBox.setText( message );
    messageBox.setInformativeText( advice );
    messageBox.setStandardButtons( QMessageBox::Ok );
    messageBox.exec();
  }


void MyMainWindow::setStatusbarText( QString text, unsigned int time )
  {
    statusBar()->showMessage( text, time );
  }


void MyMainWindow::setupMidi()
  {
    midiIn = new MidiIn;
    midiOut = new MidiOut;
    // The MIDI in thread always is running to avoid RtMidi's buffer containing dated data
    midiIn->start();
  }


void MyMainWindow::setupConnections()
  {
    connect(this, SIGNAL(statusMessage(QString,unsigned int)), this, SLOT(setStatusbarText(QString,unsigned int)));
    connect(this, SIGNAL(errorMessage(QString,QString)), this, SLOT(displayErrorMessage(QString,QString)));

    connect(midiOut, SIGNAL(errorMessage(QString,QString)), this, SLOT(displayErrorMessage(QString,QString)));
    connect(midiOut, SIGNAL(eventSent(int, int)), this, SLOT(eventSent(int, int)));
    connect(midiOut, SIGNAL(finished()), this, SLOT(midiOutFinished()));

    connect(midiIn, SIGNAL(errorMessage(QString,QString)), this, SLOT(displayErrorMessage(QString,QString)));
    connect(midiIn, SIGNAL(chunkArrived()), this, SLOT(chunkArrived()));
    connect(midiIn, SIGNAL(eventArrived(QByteArray)), this, SLOT(eventArrived(QByteArray)));
  }


void MyMainWindow::setupControls()
  {
    if ( midiStatus == idle && eventList.size() == 0 )
      {
        ActionFileReceive->setEnabled(true);
        ActionFileTransmit->setEnabled(false);
        ActionFileCancelTransmission->setEnabled(false);
        ActionFileOpen->setEnabled(true);
        ActionFileSaveas->setEnabled(false);
        ActionFileDiscard->setEnabled(false);
        ActionFileQuit->setEnabled(true);
        progressBar->setVisible( false );
        progressBar->setMinimum( 0 );
        progressBar->setMaximum( 100 );
        progressBar->setValue( 0 );
        progressBar->reset();
      }
    else if ( midiStatus == idle && eventList.size() > 0 )
      {
        ActionFileReceive->setEnabled(true);
        ActionFileTransmit->setEnabled(true);
        ActionFileCancelTransmission->setEnabled(false);
        ActionFileOpen->setEnabled(true);
        ActionFileSaveas->setEnabled(true);
        ActionFileDiscard->setEnabled(true);
        ActionFileQuit->setEnabled(true);
        progressBar->setVisible( false );
        progressBar->setMinimum( 0 );
        progressBar->setMaximum( 100 );
        progressBar->setValue( 0 );
        progressBar->reset();
      }
    else if ( midiStatus == transmitting )
      {
        ActionFileReceive->setEnabled(false);
        ActionFileTransmit->setEnabled(false);
        ActionFileCancelTransmission->setEnabled(true);
        ActionFileOpen->setEnabled(false);
        ActionFileSaveas->setEnabled(false);
        ActionFileDiscard->setEnabled(false);
        ActionFileQuit->setEnabled(false);
        progressBar->setVisible( true );
      }
    else if ( midiStatus == receiving )
      {
        ActionFileReceive->setEnabled(false);
        ActionFileTransmit->setEnabled(false);
        ActionFileCancelTransmission->setEnabled(true);
        ActionFileOpen->setEnabled(false);
        ActionFileSaveas->setEnabled(false);
        ActionFileDiscard->setEnabled(false);
        ActionFileQuit->setEnabled(false);
        progressBar->setMinimum( 0 );
        progressBar->setMaximum( 0 );
        progressBar->setValue( 0 );
        progressBar->setVisible( true );
      }
  }


void MyMainWindow::initGuiData()
  {
    manufacturerName = "";
    modelName = "";
    modelImageFilename = emptyImageString;
    deviceNumber = "";
    datatypeName = "";
    displayGuiData();
  }


void MyMainWindow::prepareGuiData(  vector<unsigned char> * buffer )
  {
    signed int modelByte = 0;
    signed int deviceByte = 0;
    signed int datatypeByte = 0;

    QString tmpManufacturerName;
    QString tmpModelName;
    QString tmpModelImageFilename;
    QString tmpDeviceNumber;
    QString tmpDatatypeName;

    QByteArray someBytes;
    someBytes.append( buffer->at( 1 ) );
    if ( buffer->at( 1 ) == 0x00 )
      {
        someBytes.append( buffer->at( 2 ) );
        someBytes.append( buffer->at( 3 ) );
      }

    QSettings modelDatabase( "ModelDatabase" );
    modelDatabase.beginGroup( someBytes.toHex() );
      someBytes.clear();
      tmpManufacturerName = modelDatabase.value( "ManufacturerName", "" ).toString();
      modelByte = modelDatabase.value( "ModelByteStart", 0 ).toInt() -1;
      if ( modelByte > -1 )
        someBytes.append(buffer->at( modelByte ) );
      modelDatabase.beginGroup( someBytes.toHex() );
        someBytes.clear();
        tmpModelName = modelDatabase.value( "ModelName", "" ).toString();
        tmpModelImageFilename = modelDatabase.value( "ModelImage", "" ).toString();
        deviceByte = modelDatabase.value( "DeviceByte", 0 ).toInt() -1;
        if ( deviceByte > -1 )
          someBytes.append( buffer->at( deviceByte ) );
        tmpDeviceNumber = someBytes.toHex();
        someBytes.clear();

        datatypeByte = modelDatabase.value( "DatatypeByte", 0 ).toInt() -1;
        if ( datatypeByte > -1 )
          someBytes.append( buffer->at( datatypeByte ) );
        modelDatabase.beginGroup( someBytes.toHex() );
          someBytes.clear();
          tmpDatatypeName = modelDatabase.value( "DatatypeName", "" ).toString();
        modelDatabase.endGroup();
      modelDatabase.endGroup();
    modelDatabase.endGroup();

    if( tmpManufacturerName == "" )
      tmpManufacturerName = unknownDataString;
    if( tmpModelName == ""  )
      tmpModelName = unknownDataString;
    if( tmpDeviceNumber == ""  )
      tmpDeviceNumber = unknownDataString;
    if( tmpDatatypeName == ""  )
      tmpDatatypeName = unknownDataString;
    if( tmpModelImageFilename == ""  )
      {
        tmpModelImageFilename = unknownDataString;
        tmpModelImageFilename.append( ".png" );
      }

    if( manufacturerName == "" )
      manufacturerName = tmpManufacturerName;
    if( modelName == "" )
      modelName = tmpModelName;
    if( deviceNumber == "" )
      deviceNumber = tmpDeviceNumber;
    if( datatypeName == "" )
      datatypeName = tmpDatatypeName;
    if( modelImageFilename == emptyImageString )
      modelImageFilename = tmpModelImageFilename;

    if( manufacturerName != tmpManufacturerName )
      manufacturerName = variousDataString;
    if( modelName != tmpModelName  )
      modelName = variousDataString;
    if( deviceNumber != tmpDeviceNumber  )
      deviceNumber = variousDataString;
    if( datatypeName != tmpDatatypeName  )
      datatypeName = variousDataString;
    if( modelImageFilename != tmpModelImageFilename  )
      {
        modelImageFilename = variousDataString;
        modelImageFilename.append( ".png" );
      }

    displayGuiData();
  }


void MyMainWindow::displayGuiData()
  {
    unsigned int events = eventList.size();
    unsigned int size = 0;
    for ( unsigned int i = 0; i < events; i++ )
      {
        size += eventList.at( i )->size();
      }
    if ( events == 0 )
      {
        labelAmountDisplay->setText( "" );
        labelSizeDisplay->setText( "" );
        labelAmount->setText( "" );
        labelSize->setText( "" );
      }
    else
      {
        labelAmount->setText( tr( "Amount of Packages:" ) );
        labelSize->setText( tr( "File size (bytes):" ) );
        labelAmountDisplay->setNum( (int)events );
        labelSizeDisplay->setNum( (int)size );
      }

    if ( manufacturerName == unknownDataString || manufacturerName == "" || manufacturerName == variousDataString )
      {
        labelManufacturer->setText( "" );
        labelManufacturerDisplay->setText( "" );
      }
    else
      {
        labelManufacturer->setText( tr( "Manufacturer:" ) );
        labelManufacturerDisplay->setText( manufacturerName );
      }
    if ( modelName == unknownDataString || modelName == "" || modelName == variousDataString )
      {
        labelModel->setText( "" );
        labelModelDisplay->setText( "" );
      }
    else
      {
        labelModel->setText( tr( "Model:" ) );
        labelModelDisplay->setText( modelName );
      }
    if ( deviceNumber == unknownDataString || deviceNumber == "" || deviceNumber == variousDataString )
      {
        labelDevice->setText( "" );
        labelDeviceDisplay->setText( "" );
      }
    else
      {
        labelDevice->setText( tr( "Device No.:" ) );
        labelDeviceDisplay->setText( deviceNumber );
      }
    if ( datatypeName == unknownDataString || datatypeName == "" || datatypeName == variousDataString )
      {
        labelDatatype->setText( "" );
        labelDatatypeDisplay->setText( "" );
      }
    else
      {
        labelDatatype->setText( tr( "Data type:" ) );
        labelDatatypeDisplay->setText( datatypeName );
      }
    if ( modelImageFilename == "" )
      modelImageFilename = emptyImageString;

    QPixmap modelImage( QString( ":/models/model_images/" ).append( modelImageFilename) );
    labelModelImage->setPixmap ( modelImage );
  }


void MyMainWindow::createDatabase()
  {
    // Dirty. QSettings is not capable using the embedded config file.
    // Thus writing one to disk if it doesn't exist yet.
    QSettings modelDatabase( "ModelDatabase" );
    QFileInfo fileInfo( modelDatabase.fileName() );
    if ( !fileInfo.exists() )
      {
        QFile dbIn( ":/models/ManufacturerDatabase.conf" );
        QFile dbOut( modelDatabase.fileName() );
        dbIn.open( QIODevice::ReadOnly );
        dbOut.open( QIODevice::WriteOnly | QIODevice::Truncate );
        QByteArray inbytes = dbIn.readAll();
        QDataStream outstream(&dbOut);
        outstream << inbytes;
        dbIn.close();
        dbOut.close();      
      }
  }


void MyMainWindow::midiOutFinished()
  {
    midiStatus = idle;
    setupControls();
  }


void MyMainWindow::eventSent(int current, int total)
  {
    progressBar->setMinimum( 0 );
    progressBar->setMaximum( total );
    progressBar->setValue( current );
  }


void MyMainWindow::dragEnterEvent( QDragEnterEvent* DragEvent )
  {
    if ( DragEvent->mimeData()->hasFormat( "text/uri-list" ) )
      DragEvent->acceptProposedAction();
  }


void MyMainWindow::dropEvent( QDropEvent* DropEvent )
  {
    QList<QUrl> urls = DropEvent->mimeData()->urls();
    if ( urls.isEmpty() )
      return;
    if ( urls.size() > 1 )
      {
        emit errorMessage( tr( "This application can only open one file at a time." ), tr( "Please drop one file only." ) );
        return;
      }

    QString filePath = urls.first().toLocalFile();
    if ( filePath.isEmpty() )
      return;
    if ( confirmDataLoss() == false )
      return;

    discardData();
    loadFile( filePath );
  }


void MyMainWindow::setupMenus()
  {
    MenuFile = menuBar()->addMenu(tr("&File"));
    MenuHelp = menuBar()->addMenu(tr("&Help"));

    MainToolBar = addToolBar( tr("Main Toolbar") );
    MainToolBar->setIconSize( QSize( 24, 24 ) );
    MainToolBar->setAllowedAreas( Qt::TopToolBarArea );
    MainToolBar->setMovable( false );

    ActionFileOpen = new QAction(QIcon(":/icons/FileOpen.png"), tr("&Open..."), this);
    ActionFileOpen->setShortcut(tr("Ctrl+O"));
    ActionFileOpen->setStatusTip(tr("Open file from storage medium"));
    ActionFileOpen->setWhatsThis( tr("Open a file. All currently loaded data will be discarded") );
    ActionFileOpen->setEnabled(true);
    connect(ActionFileOpen, SIGNAL(triggered()), this, SLOT(prepareOpenFile()));

    ActionFileDiscard = new QAction(QIcon(":/icons/FileClear.png"), tr("&Discard..."), this);
    ActionFileDiscard->setShortcut(tr("Ctrl+D"));
    ActionFileDiscard->setStatusTip(tr("Discard all currently loaded data"));
    ActionFileDiscard->setWhatsThis( tr("Discard all currently loaded data") );
    ActionFileDiscard->setEnabled(true);
    connect(ActionFileDiscard, SIGNAL(triggered()), this, SLOT(prepareDiscardData()));

    ActionFileSaveas = new QAction(QIcon(":/icons/FileSaveas.png"), tr("&Save as..."), this);
    ActionFileSaveas->setShortcut(tr("Ctrl+S"));
    ActionFileSaveas->setStatusTip(tr("Save file to storage medium"));
    ActionFileSaveas->setWhatsThis( tr("Save file to storage medium") );
    ActionFileSaveas->setEnabled(true);
    connect(ActionFileSaveas, SIGNAL(triggered()), this, SLOT(prepareSaveFile()));

    ActionFileReceive = new QAction(QIcon(":/icons/FileReceive.png"), tr("&Receive"), this);
    ActionFileReceive->setShortcut(tr("Ctrl+R"));
    ActionFileReceive->setStatusTip(tr("Receive data from MIDI device"));
    ActionFileReceive->setWhatsThis( tr("Receive data from MIDI device") );
    ActionFileReceive->setEnabled(true);
    connect(ActionFileReceive, SIGNAL(triggered()), this, SLOT(prepareReceiveFile()));

    ActionFileCancelTransmission = new QAction(QIcon(":/icons/FileCancelTransmission.png"), tr("&Cancel Transmission"), this);
    ActionFileCancelTransmission->setShortcut(tr("ESC"));
    ActionFileCancelTransmission->setStatusTip(tr("Cancel data transmission"));
    ActionFileCancelTransmission->setWhatsThis( tr("Cancel data transmission") );
    ActionFileCancelTransmission->setEnabled(true);
    connect(ActionFileCancelTransmission, SIGNAL(triggered()), this, SLOT(cancelTransmission()));

    ActionFileTransmit = new QAction(QIcon(":/icons/FileTransmit.png"), tr("&Transmit"), this);
    ActionFileTransmit->setShortcut(tr("Ctrl+T"));
    ActionFileTransmit->setStatusTip(tr("Send data to MIDI device"));
    ActionFileTransmit->setWhatsThis( tr("Send data to MIDI device") );
    ActionFileTransmit->setEnabled(true);
    connect(ActionFileTransmit, SIGNAL(triggered()), this, SLOT(prepareTransmitFile()));

    ActionFileQuit = new QAction(QIcon(":/icons/FileQuit.png"), tr("&Quit"), this);
    ActionFileQuit->setShortcut(tr("Ctrl+Q"));
    ActionFileQuit->setStatusTip(tr("Quit this application"));
    ActionFileQuit->setWhatsThis( tr("Quits this application without further confirmation. All currently loaded data will be lost.") );
    ActionFileQuit->setEnabled(true);
    connect(ActionFileQuit, SIGNAL(triggered()), this, SLOT(close()));

    ActionHelpAbout = new QAction(QIcon(":/icons/AppIcon.png"), tr("&About"), this);
    ActionHelpAbout->setShortcut(tr("Ctrl+A"));
    ActionHelpAbout->setStatusTip(tr("Display information about the application") );
    ActionHelpAbout->setWhatsThis( tr("Display information about the application") );
    ActionHelpAbout->setEnabled(true);
    connect(ActionHelpAbout, SIGNAL(triggered() ), this, SLOT( displayAboutBox() ) );

    ActionHelpWhatsThis = QWhatsThis::createAction( this );

    MenuFile->addAction( ActionFileOpen );
    MenuFile->addAction( ActionFileSaveas );
    MenuFile->addSeparator();
    MenuFile->addAction( ActionFileReceive );
    MenuFile->addAction( ActionFileCancelTransmission );
    MenuFile->addAction( ActionFileTransmit );
    MenuFile->addSeparator();
    
    MenuFile->addSeparator();
    MenuFile->addAction( ActionFileDiscard );
    MenuFile->addAction( ActionFileQuit );

    MenuHelp->addAction(ActionHelpWhatsThis);
    MenuHelp->addAction(ActionHelpAbout);

    MainToolBar->addAction( ActionFileDiscard );
    MainToolBar->addAction( ActionFileOpen );
    MainToolBar->addAction( ActionFileSaveas );
    MainToolBar->addSeparator();
    MainToolBar->addAction( ActionFileReceive );
    MainToolBar->addAction( ActionFileCancelTransmission );
    MainToolBar->addAction( ActionFileTransmit );

    progressBar->setVisible( false );
    statusBar()->showMessage( tr("Idle"), 10000 );
  }
