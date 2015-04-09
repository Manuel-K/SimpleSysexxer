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

#ifndef MIDIOUT_H
#define MIDIOUT_H

#include <iostream>
#include <math.h>

#include <QThread>
#include <QtCore>

#include <alsa/asoundlib.h> 

# define CHUNKMAXSIZE 256

using namespace std;

class MidiOut : public QThread
  {
    // No signal and slot connections without this macro:
    Q_OBJECT 
    
    private:
      snd_seq_t * sequencerHandle;
      int portNumber;
      unsigned int eventCount;
      unsigned int chunkAmount;
      unsigned int chunkCount;
      QList<vector<unsigned char>*> eventList;
      bool sendMidi;
      void determineChunkAmount();

    public:
      MidiOut();
      ~MidiOut();
      
    public slots:
      void init();
      void run();
      void send( vector<unsigned char>* );
      void stop();
      void setEventList( QList<vector<unsigned char>*> );
    
    signals:
      void eventSent( int, int );
      void errorMessage( QString, QString );
  };

#endif
