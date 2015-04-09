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

#include "MidiOut.h"
#include "Versioning.h"


MidiOut::MidiOut()
  {
    init();
  }


MidiOut::~MidiOut()
  {
    snd_seq_close ( sequencerHandle );
  }


void MidiOut::determineChunkAmount()
  {
    chunkAmount = 0;
    for ( int i = 0; i < eventList.size(); i++ )
      {
        chunkAmount += ceil((double)eventList.at(i)->size() / (double)CHUNKMAXSIZE);
      }
  }


void MidiOut::run()
  {
    chunkCount = 0;
    determineChunkAmount();
    sendMidi = true;
    for ( int i = 0; i < eventList.size(); i++ )
      {
        if ( sendMidi == false )
          {
            eventList.clear();
            return;
          }
        eventCount = i + 1;
        send( eventList.at( i ) );

        // Some synths cannot cope with too much sysex events in a given period of time.
        // Via a config option, some additional delay between sysex events can be added.
        QSettings appSettings;
        unsigned int delay = (appSettings.value( "SendingDelay", 250 ).toInt() );
        // Create the settings entry in case it didn't exist yet
        appSettings.setValue( "SendingDelay", delay );
        msleep( delay );
      }
    eventCount = 0;
  }


void MidiOut::send( vector<unsigned char>* data )
  {
      snd_seq_event_t event;
      snd_seq_ev_clear(&event);
      snd_seq_ev_set_source(&event, portNumber);
      snd_seq_ev_set_subs(&event);
      snd_seq_ev_set_direct(&event);

      // Also see
      // http://www.mail-archive.com/mixxx-devel@lists.sourceforge.net/msg01503.html
      // http://doxygen.scummvm.org/de/d2a/alsa_8cpp-source.html
      // In case of SysEx data, ALSA takes a data pointer (even an array) and the data size.
      // ALSA's buffer is limited to 16356 bytes anyway.

      vector<unsigned char> chunk;
      for ( unsigned int i = 0; i < data->size(); i++ )
        {
          chunk.push_back( data->at(i) );
          if ( chunk.size() == CHUNKMAXSIZE || chunk.back() == 0xF7 )
            {
	      chunkCount ++;
              snd_seq_ev_set_sysex(&event, chunk.size(), &chunk.front());
              if( snd_seq_event_output_direct(sequencerHandle, &event) < 0 )
                {
                  emit errorMessage( tr("Event %1 was not sent successfully.").arg( eventCount, 10 ), tr("Please send the file used to the author for analysis.") );
                  // TODO: I'm not sure if the next line is thread save - see MidiOut::stop() which gets invoked by the GUI thread
                  sendMidi = false;
                  return;
                }
              emit eventSent( chunkCount, chunkAmount );
              // The baud rate of MIDI is 31250 bits/s = 32 microseconds per bit.
	      // Per byte, an additional start and one or two stop bits are used.
              // The following delay should avoid data loss in case more data gets sent as fits into the ALSA buffers.
              usleep( chunk.size() * 352 );
              chunk.clear();
            }
        }
  }


void MidiOut::stop()
  {
    sendMidi = false;
  }


void MidiOut::init()
  {
    // Try open a new client...
    sequencerHandle = NULL;
    if ( snd_seq_open( &sequencerHandle, "default", SND_SEQ_OPEN_OUTPUT, 0 ) < 0 )
      {
        emit errorMessage( tr( "Could not access ALSA." ), tr( "Please ensure ALSA is up and running." ) );
        return;
      }

    if ( sequencerHandle == NULL )
      {
        emit errorMessage( tr( "Could not create MIDI output port." ), tr( "Please check your ALSA installation." ) );
        return;
      }

    // Set client name.
    snd_seq_set_client_name( sequencerHandle, APPNAME );
    portNumber = snd_seq_create_simple_port( sequencerHandle, "Out", SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ, SND_SEQ_PORT_TYPE_APPLICATION );
    if ( portNumber < 0 )
    {
      emit errorMessage( tr( "Could not create MIDI output port." ), tr( "Please check your ALSA installation." ) );
      return;
    }

  }


void MidiOut::setEventList( QList<vector<unsigned char>*> events )
  {
    eventList = events;
  }

