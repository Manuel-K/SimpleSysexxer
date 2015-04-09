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

#ifndef MIDIIN_H
#define MIDIIN_H

#include "Versioning.h"

#include <iostream>
#include <alsa/asoundlib.h>
#include <QThread>
#include <QtCore>

using namespace std;

class MidiIn : public QThread
  {
    // No signal and slot connections without this macro:
    Q_OBJECT 
    
    private:
      bool recordMidi;
      snd_seq_t* sequencerHandle;
      snd_seq_event_t* sequencerEvent;
      QByteArray tempBuffer;

    public:
      MidiIn();
      ~MidiIn();
      
    public slots:
      void init();
      void run();
      void processInput();
      void startReception();
      void stopReception();
    
    signals:
      void chunkArrived();
      void eventArrived( QByteArray );
      void errorMessage( QString, QString );
  };

#endif


