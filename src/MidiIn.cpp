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

#include "MidiIn.h"

MidiIn::MidiIn()
  {
    init();
  }


MidiIn::~MidiIn()
  {

  }


void MidiIn::init()
  {
    sequencerHandle = 0L;
    sequencerEvent = 0L;
    if ( snd_seq_open( &sequencerHandle, "default", SND_SEQ_OPEN_INPUT, 0 ) < 0 )
      {
        emit errorMessage ( tr( "Cannot initialize the MIDI subsystem."), tr( "Please ensure ALSA is accessible." ) );
      }

    snd_seq_set_client_name(sequencerHandle, APPNAME );
    
    if ( snd_seq_create_simple_port( sequencerHandle, APPNAME, SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_APPLICATION) < 0 )
      {
        emit errorMessage ( tr( "Cannot create MIDI port."), tr( "Please ensure ALSA is set up properly." ) );
      }    
  }


void MidiIn::run()
  {
    recordMidi = false;
    int npfd = snd_seq_poll_descriptors_count(sequencerHandle, POLLIN);
    struct pollfd* pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
    snd_seq_poll_descriptors(sequencerHandle, pfd, npfd, POLLIN);
    while ( true )
      {
        // Avoid too much CPU load. According to the MIDI baud rate, 4 bytes need at least 1024 microseconds to pass the chord.
        usleep( 512 );
        if ( poll( pfd, npfd, 100000) > 0 && recordMidi == true )
          {
            processInput();
          }
      }
  }


void MidiIn::processInput()
  {
    do
      {
        snd_seq_event_input( sequencerHandle, &sequencerEvent );
        if ( sequencerEvent->type != SND_SEQ_EVENT_SYSEX )
          {
            snd_seq_free_event( sequencerEvent );
            continue;
          }
        // Stolen from aseqmm/alsaevent.cpp by Pedro Lopez-Cabanillas
        tempBuffer.append( QByteArray( (char *) sequencerEvent->data.ext.ptr, sequencerEvent->data.ext.len ) );

        // ALSA splits sysex messages into chunks, currently of 256 max bytes in size
        // Therefore the data needs collection before sending it to the master thread
        if ( tempBuffer.startsWith( 0xF0 ) && tempBuffer.endsWith( 0xF7 ) )
          {
            emit eventArrived( tempBuffer );
            tempBuffer.clear();
          }
        else
          {
            emit chunkArrived();
          }
        snd_seq_free_event( sequencerEvent );
      }
    while ( snd_seq_event_input_pending( sequencerHandle, 0 ) > 0 );
  }


void MidiIn::startReception()
  {
    recordMidi = true;
  }


void MidiIn::stopReception()
  {
    recordMidi = false;
  }


