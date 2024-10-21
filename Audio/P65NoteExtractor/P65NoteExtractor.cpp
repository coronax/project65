// P65NoteExtractor.cpp
// Copyright(c) 2023 Christopher Just
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met :
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and /or other materials
//    provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED.IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// This program reads a MIDI file and tries to output a data structure that
// can be used by the Project:65 audio test program.
//
// Resources:
//
// http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html
//
// Regarding Note On events with 0 velocity working as Note Offs:
// https://stackoverflow.com/questions/43321950/synthesia-plays-well-midi-file-without-any-note-off-event
//

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <set>
#include <vector>
#include <filesystem>

using std::byte;
using std::format;
using std::print;
using std::println;
using std::string;
using std::vector;
using std::cout, std::endl;

struct MidiFreq
{
	int Number = -1;
	std::string Name1;
	std::string Name2;
	float Frequency = 0.f;  // Hz
	float Wavelength = 0.f; // Seconds
	int Count = 0;   // p65 counter value
};



enum class EventType { NOTE_ON, NOTE_OFF };



struct MidiEvent
{
	int mTime;  // absolute, not delta
	EventType mEventType;
	int mChannel;
	int mTrack;
	uint16_t mKey;
	uint16_t mVelocity;
};

struct MidiTrack
{
	string mName;
	vector<MidiEvent> mEvents;
};

struct MidiSong
{
	string mFilename;
	vector<MidiTrack> mTracks;
	int mTrackCount = 0;
	vector<std::string> mErrors;

	MidiSong(string fname) : mFilename(fname)
	{
		;
	}
};



// Raw MIDI note data. Wavelength & Count values are filled in by InitializeFreqsTable.
std::vector<MidiFreq> Freqs = {
	{0,"","",8.18f, 0.f, 0 },
	{1, "","",8.66f, 0.f, 0 },
	{2, "","",9.18f, 0.f, 0 },
	{3, "","",9.72f, 0.f, 0 },
	{4, "","",10.3f, 0.f, 0 },
	{5, "","",10.91f, 0.f, 0 },
	{6, "","",11.56f, 0.f, 0 },
	{7, "","",12.25f, 0.f, 0 },
	{8, "","",12.98f, 0.f, 0 },
	{9, "","",13.75f, 0.f, 0 },
	{10, "","",14.57f, 0.f, 0 },
	{11, "","",15.43f, 0.f, 0 },
	{12, "","",16.35f, 0.f, 0 },
	{13, "","",17.32f, 0.f, 0 },
	{14, "","",18.35f, 0.f, 0 },
	{15, "","",19.45f, 0.f, 0 },
	{16, "","",20.6f, 0.f, 0 },
	{17, "","",21.83f, 0.f, 0 },
	{18, "","",23.12f, 0.f, 0 },
	{19, "","",24.5f, 0.f, 0 },
	{20, "","",25.96f, 0.f, 0 },
	{21, "A0","", 27.5f, 0.f, 0 },
	{22, "A#0","Bb0", 29.14f, 0.f, 0 },
	{23, "B0","", 30.87f, 0.f, 0 },
	{24, "C1","", 32.7f, 0.f, 0 },
	{25, "C#1","Db1", 34.65f, 0.f, 0 },
	{26, "D1","", 36.71f, 0.f, 0 },
	{27, "D#1","Eb1", 38.89f, 0.f, 0 },
	{28, "E1","", 41.2f, 0.f, 0 },
	{29, "F1","", 43.65f, 0.f, 0 },
	{30, "F#1","Gb1", 46.25f, 0.f, 0 },
	{31, "G1","", 49.f, 0.f, 0 },
	{32, "G#1","Ab1", 51.91f, 0.f, 0 },
	{33, "A1","", 55.f, 0.f, 0 },
	{34, "A#1","Bb1", 58.27f, 0.f, 0 },
	{35, "B1","", 61.74f, 0.f, 0 },
	{36, "C2","", 65.41f, 0.f, 0 },
	{37, "C#2","Db2", 69.3f, 0.f, 0 },
	{38, "D2","", 73.42f, 0.f, 0 },
	{39, "D#2","Eb2", 77.78f, 0.f, 0 },
	{40, "E2","", 82.41f, 0.f, 0 },
	{41, "F2","", 87.31f, 0.f, 0 },
	{42, "F#2","Gb2", 92.5f, 0.f, 0 },
	{43, "G2","", 98.f, 0.f, 0 },
	{44, "G#2","Ab2", 103.83f, 0.f, 0 },
	{45, "A2","", 110.f, 0.f, 0 },
	{46, "A#2","Bb2", 116.54f, 0.f, 0 },
	{47, "B2","", 123.47f, 0.f, 0 },
	{48, "C3","", 130.81f, 0.f, 0 },
	{49, "C#3","Db3", 138.59f, 0.f, 0 },
	{50, "D3","", 146.83f, 0.f, 0 },
	{51, "D#3","Eb3", 155.56f, 0.f, 0 },
	{52, "E3","", 164.81f, 0.f, 0 },
	{53, "F3","", 174.61f, 0.f, 0 },
	{54, "F#3","Gb3", 185.f, 0.f, 0 },
	{55, "G3","", 196.f, 0.f, 0 },
	{56, "G#3","Ab3", 207.65f, 0.f, 0 },
	{57, "A3","", 220.f, 0.f, 0 },
	{58, "A#3","Bb3", 233.08f, 0.f, 0 },
	{59, "B3","", 246.94f, 0.f, 0 },
	{60, "C4","", 261.63f, 0.f, 0 },		// Middle C
	{61, "C#4","Db4", 277.18f, 0.f, 0 },
	{62, "D4","", 293.66f, 0.f, 0 },
	{63, "D#4","Eb4", 311.13f, 0.f, 0 },
	{64, "E4","", 329.63f, 0.f, 0 },
	{65, "F4","", 349.23f, 0.f, 0 },
	{66, "F#4","Gb4", 369.99f, 0.f, 0 },
	{67, "G4","", 392.f, 0.f, 0 },
	{68, "G#4","Ab4", 415.3f, 0.f, 0 },
	{69, "A4","", 440.f, 0.f, 0 },  // A4 concert pitch
	{70, "A#4","Bb4", 466.16f, 0.f, 0 },
	{71, "B4","", 493.88f, 0.f, 0 },
	{72, "C5","", 523.25f, 0.f, 0 },
	{73, "C#5","Db5", 554.37f, 0.f, 0 },
	{74, "D5","", 587.33f, 0.f, 0 },
	{75, "D#5","Eb5", 622.25f, 0.f, 0 },
	{76, "E5","", 659.26f, 0.f, 0 },
	{77, "F5","", 698.46f, 0.f, 0 },
	{78, "F#5","Gb5", 739.99f, 0.f, 0 },
	{79, "G5","", 783.99f, 0.f, 0 },
	{80, "G#5","Ab5", 830.61f, 0.f, 0 },
	{81, "A5","", 880.f, 0.f, 0 },
	{82, "A#5","Bb5", 932.33f, 0.f, 0 },
	{83, "B5","", 987.77f, 0.f, 0 },
	{84, "C6","", 1046.5f, 0.f, 0 },
	{85, "C#6","Db6", 1108.73f, 0.f, 0 },
	{86, "D6","", 1174.66f, 0.f, 0 },
	{87, "D#6","Eb6", 1244.51f, 0.f, 0 },
	{88, "E6","", 1318.51f, 0.f, 0 },
	{89, "F6","", 1396.91f, 0.f, 0 },
	{90, "F#6","Gb6", 1479.98f, 0.f, 0 },
	{91, "G6","", 1567.98f, 0.f, 0 },
	{92, "G#6","Ab6", 1661.22f, 0.f, 0 },
	{93, "A6","", 1760.f, 0.f, 0 },
	{94, "A#6","Bb6", 1864.66f, 0.f, 0 },
	{95, "B6","", 1975.53f, 0.f, 0 },
	{96, "C7","", 2093.f, 0.f, 0 },
	{97, "C#7","Db7", 2217.46f, 0.f, 0 },
	{98, "D7","", 2349.32f, 0.f, 0 },
	{99, "D#7","Eb7", 2489.02f, 0.f, 0 },
	{ 100, "E7","", 2637.02f, 0.f, 0 },
	{ 101, "F7","", 2793.83f, 0.f, 0 },
	{ 102, "F#7","Gb7", 2959.96f, 0.f, 0 },
	{ 103, "G7","", 3135.96f, 0.f, 0 },
	{ 104, "G#7","Ab7", 3322.44f, 0.f, 0 },
	{ 105, "A7","", 3520.f, 0.f, 0 },
	{ 106, "A#7","Bb7", 3729.31f, 0.f, 0 },
	{ 107, "B7","", 3951.07f, 0.f, 0 },
	{ 108, "C8","", 4186.01f, 0.f, 0 },
	{ 109, "C#8","Db8", 4434.92f, 0.f, 0 },
	{ 110, "D8","", 4698.64f, 0.f, 0 },
	{ 111, "D#8","Eb8", 4978.03f, 0.f, 0 },
	{ 112, "E8","", 5274.04f, 0.f, 0 },
	{ 113, "F8","", 5587.65f, 0.f, 0 },
	{ 114, "F#8","Gb8", 5919.91f, 0.f, 0 },
	{ 115, "G8","", 6271.93f, 0.f, 0 },
	{ 116, "G#8","Ab8", 6644.88f, 0.f, 0 },
	{ 117, "A8","", 7040.f, 0.f, 0 },
	{ 118, "A#8","Bb8", 7458.62f, 0.f, 0 },
	{ 119, "B8","", 7902.13f, 0.f, 0 },
	{ 120, "C9","", 8372.02f, 0.f, 0 },
	{ 121, "C#9","Db9", 8869.84f, 0.f, 0 },
	{ 122, "D9","", 9397.27f, 0.f, 0 },
	{ 123, "D#9","Eb9", 9956.06f, 0.f, 0 },
	{ 124, "E9","", 10548.08f, 0.f, 0 },
	{ 125, "F9","", 11175.3f, 0.f, 0 },
	{ 126, "F#9","Gb9", 11839.82f, 0.f, 0 },
	{ 127, "G9","", 12543.85f, 0.f, 0 },
};


void InitializeFreqsTable()
{
	float p65scale = .00003201f; // time in seconds of a single iteration of the counter. 32*1MHz oscillator.
	for (auto& f : Freqs)
	{
		f.Wavelength = 1.f / f.Frequency;
		f.Count = std::max(0, 256 - (int)(f.Wavelength / p65scale));
	}
}

void PrintP65NoteTable()
{
	for (auto& f : Freqs)
	{
		std::cout << std::format("{0}\t{1}\t{2}\r\n", f.Number, f.Count, f.Name1);
	}
}

//std::string jingle {
//"e2e2e4e2e2e4e2g2c2d2e8"
//"f2f2f2f2f2e2e2e1e1g2g2f2d2c8"
//};

MidiFreq FindNote(std::string name)
{
	auto it = std::find_if(Freqs.begin(), Freqs.end(), [name](const MidiFreq& f/*, const std::string& name*/) { return (f.Name1 == name) || (f.Name2 == name); });
	if (it != Freqs.end())
		return *it;
	else
		return {};
}


MidiFreq FindNote(int number)
{
	auto it = std::find_if(Freqs.begin(), Freqs.end(), [number](const MidiFreq& f/*, const std::string& name*/) { return (f.Number == number); });
	if (it != Freqs.end())
		return *it;
	else
		return {};
}

#if 0
void PrintP65SongFn(std::string notes)
{
	// for this first test I'ma just assume everything is octave 4
	int i = 0;
	while (i < notes.length())
	{
		// for this 1st draft, i'm going to treat each note as being the 4th octave version. 
		// I think that'll get me as far as jingle bells.
		auto c = notes[i];
		std::string name{ (char)toupper(c), '3' };
		MidiFreq f = FindNote(name);
		++i;
		c = notes[i++];
		int len = c - '1' + 1;
		if (f.Number > 0)
		{
			std::cout << "  *r = " << f.Count << ";\r\n";
			std::cout << "  sleep(";
			switch (len)
			{
			case 1:
				std::cout << 300 - 50;
				break;
			case 2:
				std::cout << 600 - 50;
				break;
			case 3:
				std::cout << 750 - 50;
				break;
			case 4:
				std::cout << 1200 - 50;
				break;
			case 8:
				std::cout << 2400 - 50;
				break;
			default:
				std::cout << "Error: Invalid note length " << c << std::endl;
			}
			std::cout << ");\r\n";
		}
		else
			std::cout << "Error: Unknown note name " << name << std::endl;
		std::cout << "  *r = 0xff;\r\n";
		std::cout << "  sleep(50);\r\n";
	}

	std::cout << "  *r = 0xff;" << std::endl;
}
#endif



// Interpret the bytes beginning at buffer[index] as a big-endian unsigned
// 32-bit value.
uint32_t u32(vector<char>& buffer, uint64_t index)
{
	unsigned char* b = (unsigned char*)buffer.data();
	uint32_t v = ((unsigned int)b[index] << 24)
		| ((unsigned int)b[index + 1] << 16)
		| ((unsigned int)b[index + 2] << 8)
		| ((unsigned int)b[index + 3]);
	return v;
}



// Interpret the bytes beginning at buffer[index] as a big-endian unsigned
// 16-bit value.
uint16_t u16(vector<char>& buffer, uint64_t index)
{
	unsigned char* b = (unsigned char*)buffer.data();
	int v = ((unsigned int)b[index] << 8)
		| ((unsigned int)b[index + 1]);
	return v;
}



// Interpret the bytes beginning at buffer[index] as a MIDI VLQ. This is a
// multibyte integer. For each byte, the msb=1 indicates that there will be
// an additional byte following. The other 7 bits are actual data.
int vlq(vector<char>& buffer, uint64_t& index)
{
	int v = 0;
	for (;;)
	{
		char b = buffer[index++];
		v |= (b & 0x7f);
		if (b & 0x80)
		{
			v = v << 7;
		}
		else
		{
			break;
		}
	}
	return v;
}



uint64_t ReadChunk(MidiSong& song, vector<char>& buffer, uint64_t index)
{
	std::string chunk_type(buffer.begin() + index, buffer.begin() + index + 4);
	uint32_t len = u32(buffer, index + 4);
	uint64_t result = index + len + 8;
	
	//std::cout << "Reading chunk " << chunk_type << " size " << len << " at " << index << std::endl;
	println("Reading chunk {} size {} at {}", chunk_type, len, index);

	index += 8;

	if (chunk_type == "MThd")
	{
		std::cout << "Reading header chunk" << std::endl;
		int format = u16(buffer, index);
		int ntracks = u16(buffer, index + 2);
		int division = u16(buffer, index + 4);
		std::cout << "format=" << format << ", #tracks=" << ntracks << std::endl;
		if (division & 0x8000)
		{
			std::cout << "SMPTE time coding" << std::endl;
		}
		else
		{
			std::cout << "quarter note = " << division << " ticks" << std::endl;
		}
	}
	else if (chunk_type == "MTrk")
	{
		std::cout << "Reading Mtrk chunk" << std::endl;
		MidiTrack track;

		uint64_t track_end = index + len;
		int track_num = song.mTrackCount++;

		std::set<int> on_notes;
		int absolute_time = 0;
		
		uint8_t previous_event = 0;

		while (index < track_end)
		{
			// read midi events! woo!
			int delta = vlq(buffer, index);
			absolute_time += delta;
			uint8_t event_type = (uint8_t)buffer[index];
			if (event_type & 0x80)
			{
				// actually an event - eat it
				++index;
				previous_event = event_type;
			}
			else
			{
				// the actual event status was omitted, substitute previous
				event_type = previous_event;
			}

			bool handled = true;
			switch (event_type)
			{
			case 0xf0:
			{
				std::print("System Exclusive at index {}-", index);
				do {
					;
				} while ((uint8_t)buffer[index++] != 0xf7);
				println("{}", index);
				break;
			}
			case 0xf1:
			case 0xf4:
			case 0xf5:
			case 0xf9:
			case 0xfd:
				cout << "Undefined" << endl;
				break;
			case 0xf2:
				cout << "Song position pointer" << endl;
				index += 2;
				break;
			case 0xf3:
				cout << "Song select" << endl;
				index++;
				break;
			case 0xf6:
				cout << "Tune request" << endl;
				break;
			case 0xf7:
				cout << "End of exclusive" << endl;
				break;
			case 0xf8:
				cout << "Timing clock" << endl;
				break;
			case 0xfa:
				cout << "Start" << endl;
				break;
			case 0xfb:
				cout << "Continue" << endl;
				break;
			case 0xfc:
				cout << "Stop" << endl;
				break;
			case 0xfe:
				cout << "Active sensing" << endl;
				break;
			case 0xff:
			{
				// meta event
				int meta_event_type = buffer[index++];
				int meta_len = vlq(buffer, index);
				switch (meta_event_type)
				{
				case 0x03:
				{
					std::string name(&buffer[index], meta_len);
					track.mName = name;
					println("meta event Track Name '{}'", name);
				}
				break;
				case 0x06:
				{
					std::string value(&buffer[index], meta_len);
					println("meta event Marker '{}'", value);
				}
				break;
				case 0x20:
					// prefix channel. assigns a channel number to a following(?) event, 
					// which doesn't ordinarily encode one. Apparently useful in editing?
					break;
				case 0x21:
					// prefix port. Identifies the sound card this track should be sent
					// to. Typically 0, but I suppose this is a way to have more than 
					// 16 channels?
					break;
				case 0x2f:
					println("meta event track end. index = {}; expected {}", index + meta_len, track_end);
					break;
				default:
					println("meta event {:#4x} of length {}", meta_event_type, meta_len);
				}
				index += meta_len;
			}
			break;

			default:
				handled = false;
				break;
			}
			if (!handled)
			{
				handled = true;
				switch (event_type & 0xf0)
				{
				case 0x80:
				{
					// Note off
					int channel = event_type & 0x0f;
					uint16_t key = (uint16_t)(buffer[index++]);
					uint16_t velocity = (uint16_t)(buffer[index++]);
					track.mEvents.emplace_back(absolute_time, EventType::NOTE_OFF, channel, track_num, key, velocity);
					//cout << delta << " : Note off ch " << (event_type & 0xf) << " key " << key << "  " << velocity << endl;


					//if (delta > 0)
					//{
					//	cout << "  { " << delta;
					//	
					//	for (int i : on_notes)
					//	{
					//		MidiFreq f = FindNote(i);
					//		cout << ", " << f.Count;
					//		//if (f.Count == 0) cout << "[[[note " << f.Number << " has count 0]]]";
					//	}
					//	for (auto i = on_notes.size(); i < 4; ++i)
					//		cout << ", 0";
					//	cout << "},\r\n";
					//}

					//on_notes.erase(key);
				}
				break;
				case 0x90:
				{
					// note on
					int channel = event_type & 0x0f;
					uint16_t key = (uint16_t)(buffer[index++]);
					uint16_t velocity = (uint16_t)(buffer[index++]);
					track.mEvents.emplace_back(absolute_time, EventType::NOTE_ON, channel, track_num, key, velocity);
					//cout << delta << " : Note on ch " << (event_type & 0xf) << " key " << key << "  " << velocity << endl;
					//on_notes.insert(key);

					//if (delta > 0)
					//{
					//	cout << "  { " << delta;

					//	for (int i : on_notes)
					//	{
					//		MidiFreq f = FindNote(i);
					//		cout << ", " << f.Count;
					//		//if (f.Count == 0) cout << "[[[note " << f.Number << " has count 0]]]";
					//	}
					//	for (auto i = on_notes.size(); i < 4; ++i)
					//		cout << ", 0";
					//	cout << "},\r\n";
					//}
				}
				break;
				case 0xa0:
				{
					uint16_t key = (uint16_t)(buffer[index++]);
					uint16_t pressure = (uint16_t)(buffer[index++]);
					cout << "Polyphonic key pressure " << key << "  " << pressure << endl;
				}
				break;
				case 0xb0:
				{
					// control change
					uint16_t controller = (uint16_t)(buffer[index++]);
					uint16_t value = (uint16_t)(buffer[index++]);
					cout << "Control change " << controller << " " << value << endl;
				}
				break;
				case 0xc0:
				{
					uint16_t program = (uint16_t)(buffer[index++]);
					cout << "Program change " << program << endl;
				}
				break;
				case 0xd0:
				{
					uint16_t pressure = (uint16_t)(buffer[index++]);
					cout << "Channel pressure " << pressure << endl;
				}
				break;
				case 0xe0:
				{
					uint16_t lsb = (uint16_t)(buffer[index++]);
					uint16_t msb = (uint16_t)(buffer[index++]);
					uint16_t pitchwheel = msb << 7 | lsb;
					cout << "Pitch Wheel Change " << pitchwheel  << endl;
				}
				break;
				default:
					handled = false;

				}

			}
			if (!handled)
			{
				string error = format("unknown event type {:#x} at {} - halting", (int)event_type, index-1);
				song.mErrors.push_back(error);
				std::cout << error << endl;
				break;
			}
		}
		song.mTracks.push_back(std::move(track));
	}
	else
	{
		string error = format("No handler for chunk {} - skipping", chunk_type);
		song.mErrors.push_back(error);
		std::cout << error << std::endl;
	}

	return result;
}



void OutputNotes(MidiSong& song)
{
	int last_time = 0;
	int delta = 0;
	std::set<int> on_notes;
	int commands_issued_count = 0;
	float time_scale = 0.5f;

	println("------------------ Outputing song ---------------");
	for (string& s : song.mErrors)
		println("Error: {}", s);

	std::vector<MidiEvent> all_events;
	for (auto& track : song.mTracks)
	{
		println("track {}: {} events", track.mName, track.mEvents.size());
		all_events.insert(all_events.begin(), track.mEvents.begin(), track.mEvents.end());
	}

	std::sort(all_events.begin(), all_events.end(), 
		[](const MidiEvent& e1, const MidiEvent& e2) 
		{ 
			return (e1.mTime < e2.mTime) || ((e1.mTime == e2.mTime) && (e1.mTrack < e2.mTrack)); 
		});

	println("all_events size = {}", all_events.size());
	int note_on_count = 0, note_off_count = 0;

	for (MidiEvent& event : all_events)
	{
		//if (event.mTrack < 3 || event.mTrack > 4)
		//if (event.mTrack != 3)
		//	continue;

		switch (event.mEventType)
		{
		case EventType::NOTE_ON:
			note_on_count++;
			if (event.mVelocity == 0)
				println("note on with zero velocity!");
			if (on_notes.size() < 4)
			{
				on_notes.insert(event.mKey);
				delta = event.mTime - last_time;
				last_time = event.mTime;
				if (delta > 0)
				{
					cout << "  { " << (int)(time_scale * delta);

					for (int i : on_notes)
					{
						MidiFreq f = FindNote(i);
						cout << ", " << f.Count;
						//if (f.Count == 0) cout << "[[[note " << f.Number << " has count 0]]]";
					}
					for (auto i = on_notes.size(); i < 4; ++i)
						cout << ", 0";
					cout << "},\r\n";
					++commands_issued_count;
				}
			}
			break;
		case EventType::NOTE_OFF:
			note_off_count++;
			delta = event.mTime - last_time;
			last_time = event.mTime;
			if (delta > 0)
			{
				cout << "  { " << (int)(time_scale * delta);

				for (int i : on_notes)
				{
					MidiFreq f = FindNote(i);
					cout << ", " << f.Count;
					//if (f.Count == 0) cout << "[[[note " << f.Number << " has count 0]]]";
				}
				for (auto i = on_notes.size(); i < 4; ++i)
					cout << ", 0";
				cout << "},\r\n";
				++commands_issued_count;
			}

			on_notes.erase(event.mKey);
			break;
		default:
			// uh... do nothing?
			break;
		}

	}

	cout << "};\r\nint songlen = " << commands_issued_count << ";\r\n";

	println("note on count:  {}", note_on_count);
	println("note off count: {}", note_off_count);
}



MidiSong ReadMidi(std::string fname)
{
	MidiSong song (fname);

	// first, read the entire file into a buffer.
	if (std::filesystem::exists(fname))
	{
		uint64_t sz = std::filesystem::file_size(fname);
		vector<char> buffer(sz);
		std::ifstream f(fname, std::ios::binary);
		f.read(buffer.data(), sz);
		if (f)
		{
			std::cout << "Read " << sz << " bytes of " << fname << std::endl;

			uint64_t index = 0;
			do
			{
				index = ReadChunk(song, buffer, index);
			} while (index < sz);
		}
		else
			song.mErrors.push_back(format("Failed to read: \"{}\"", fname));
	}
	else
		song.mErrors.push_back(format("File not found: \"{}\"", fname));

	return song;
}



int main(int argc, char** argv)
{
	std::cout << "Hello World!\n";

	std::string filename;
	if (argc == 2)
		filename = argv[1];
	else
	{
		//filename = "jingle-bells-keyboard.mid"; 
		//filename = "R:\\Transfer\\psychedelia\\Music\\Sounds and Midis\\Ultima IV\\u4wander.mid";
		filename = "R:\\Transfer\\psychedelia\\Music\\Sounds and Midis\\King Crimson\\Frame by Frame.mid";
	}

	InitializeFreqsTable();
	//PrintP65NoteTable();
	println();

	MidiSong song = ReadMidi(filename);

	OutputNotes(song);
}
