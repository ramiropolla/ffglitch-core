
#include <stdio.h>
#include <unistd.h>

#include "libavutil/rtmidi/RtMidi.h"

static RtMidiOut *midiout = NULL;
static int last_vals[0x80];

static void callback(double timeStamp, std::vector<unsigned char> *message, void *userData)
{
  unsigned int len = message->size();
  if ( len == 3 )
  {
    unsigned char v = message->at(0);
    if ( v == 0xB0 )
    {
      v = message->at(1);
      if ( v < 0x80 )
      {
        // printf("saving %d (%d)\n", v, message->at(2));
        last_vals[v] = message->at(2);
      }
    }
    else if ( v == 0xF0 && message->at(1) == 0x00 && message->at(2) == 0xF7 )
    {
      for ( size_t i = 0; i < sizeof(last_vals)/sizeof(last_vals[0]); i++ )
      {
        int last_val_i = last_vals[i];
        if ( last_val_i == -1 )
          continue;
        if ( midiout != NULL )
        {
          unsigned char _0xB0[3] = { 0xB0, (unsigned char) i, (unsigned char) last_val_i };
          // printf("sending %d (%d)\n", (int) i, last_val_i);
          midiout->sendMessage(_0xB0, 3);
        }
      }
      return;
    }
  }
  if ( midiout != NULL )
    midiout->sendMessage(message);
}

int main(int argc, char *argv[])
{
  RtMidiIn *midiin = NULL;
  int ret = EXIT_FAILURE;
  int count;
  std::string repeater_name = "RtMidi Repeater";
  // arguments
  std::string port_name;
  int port_num = -1;

  // Check arguments
  if ( argc > 1 )
  {
    const char *p = argv[1];
    while ( true )
    {
      char c = *p;
      if ( c == '\0' || c < '0' || c > '9' )
        break;
      p++;
    }
    if ( *p == '\0' )
      port_num = atoi(argv[1]);
    else
      port_name = argv[1];
  }
  if ( argc > 2 )
    repeater_name = argv[2];

  // Constructors
  try
  {
    midiin = new RtMidiIn();
    midiout = new RtMidiOut(RtMidi::UNSPECIFIED, repeater_name);
  }
  catch (RtMidiError &error)
  {
    error.printMessage();
    goto the_end;
  }

  // Check if any MIDI ports are available
  count = midiin->getPortCount();
  printf("RtMidi: %d port%s found\n", count, (count == 1) ? "" : "s");
  if ( count == 0 )
    goto the_end;

  // Print names and select port number
  for ( int i = 0; i < count; i++ )
  {
    std::string name = midiin->getPortName(i);
    printf("%d. %s\n", i, name.c_str());
    // Check if port name was specified
    if ( port_num == -1 && (name == port_name) )
      port_num = i;
  }

  // Sanity check for port name
  if ( !port_name.empty() && port_num == -1 )
  {
    printf("Selected port name (%s) is not available.\n", port_name.c_str());
    goto the_end;
  }

  // Sanity check for port number
  if ( port_num >= count )
  {
    printf("Selected port number (%d) greater than real number of ports (%d).\n", port_num, count);
    goto the_end;
  }

  // No port selected yet, use heuristics
  if ( port_num == -1 )
  {
    for ( int i = 0; i < count; i++ )
    {
      std::string name = midiin->getPortName(i);
      if ( name.find("Midi Through") != std::string::npos )
        continue;
      if ( name.find(repeater_name) != std::string::npos )
        continue;
      port_num = i;
      break;
    }
  }

  // Open input port
  printf("Opening input port number %d\n", port_num);
  try
  {
    midiin->openPort(port_num);
  }
  catch (RtMidiError &error)
  {
    error.printMessage();
    goto the_end;
  }

  // Prepare last_vals and setup callback
  for ( size_t i = 0; i < sizeof(last_vals)/sizeof(last_vals[0]); i++ )
    last_vals[i] = -1;
  midiin->setCallback(callback);
  midiin->ignoreTypes(false, false, false);

  // TODO client vs port name in windows
  // Setup repeater name
  port_name = midiin->getPortName(port_num);

  // Open output port
  printf("Opening repeater port '%s:%s'\n", repeater_name.c_str(), port_name.c_str());
  try
  {
    midiout->openVirtualPort(port_name);
  }
  catch (RtMidiError &error)
  {
    error.printMessage();
    goto the_end;
  }

  // Everything should be good to go. Just sleep it out.
  ret = EXIT_SUCCESS;
  while ( true )
  {
    usleep(1000);
  }

the_end:
  if ( midiin != NULL )
    delete midiin;
  if ( midiout != NULL )
    delete midiout;

  return ret;
}
