#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <map>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")

// ============================================================
// SETTINGS
// ============================================================

const int SAMPLE_RATE = 44100;
const double PI = 3.14159265358979323846;

const double MIN_TEMPO = 40.0;
const double MAX_TEMPO = 480.0;
const double TEMPO_STEP = 5.0;

const int MIN_PITCH = -12;
const int MAX_PITCH = 12;
const int PITCH_STEP = 1;

const int AUDIO_BUFFER_COUNT = 2;

const double MASTER_GAIN = 2.5;

// ============================================================
// COLORS
// ============================================================

const COLORREF BACKGROUND = RGB(0, 207, 74);
const COLORREF COLUMN = RGB(32, 32, 38);
const COLORREF COLUMN_BORDER = RGB(55, 55, 65);
const COLORREF TEXT_COLOR = RGB(255, 255, 255);
const COLORREF BUTTON_TEXT = RGB(0, 255, 0);
const COLORREF EDIT_BACKGROUND = RGB(15, 15, 20);
const COLORREF EDIT_TEXT = RGB(255, 255, 255);

// ============================================================
// MAIN WINDOW
// ============================================================

HWND mainWindow = nullptr;

HWND playButton = nullptr;
HWND loopButton = nullptr;
HWND boostButton = nullptr;
HWND playAllTracksButton = nullptr;

HWND instrumentsButton = nullptr;
HWND chordsButton = nullptr;
HWND drumsButton = nullptr;

HWND tempoMinusButton = nullptr;
HWND tempoPlusButton = nullptr;
HWND tempoLabel = nullptr;

HWND pitchMinusButton = nullptr;
HWND pitchPlusButton = nullptr;
HWND pitchLabel = nullptr;

HWND saveTrackButton = nullptr;
HWND songEditor = nullptr;

HBRUSH editBrush = nullptr;

// ============================================================
// PLAYER STATE
// ============================================================

std::atomic_bool playing(false);
std::atomic_bool looping(false);
std::atomic_bool stopRequested(false);

std::atomic<double> currentTempo(120.0);
std::atomic<int> currentPitch(0);
std::atomic<int> volumeBoost(0);

// ============================================================
// LAYOUT
// ============================================================

const int LEFT_MARGIN = 15;

const int EDITOR_TOP = 205;
const int EDITOR_HEIGHT = 350;

const int TRACK_TOP = 580;
const int TRACK_BUTTON_HEIGHT = 36;
const int TRACK_GAP = 8;
const int BOTTOM_MARGIN = 30;

int scrollY = 0;
int contentHeight = 900;

// ============================================================
// SAVED TRACKS
// ============================================================

std::vector<std::string> savedTracks;

// ============================================================
// MENU COMMANDS
// ============================================================

const UINT ID_PLAY = 1000;
const UINT ID_LOOP = 1001;
const UINT ID_BOOST = 1002;

const UINT ID_INSTRUMENTS = 1003;
const UINT ID_CHORDS = 1004;
const UINT ID_DRUMS = 1005;

const UINT ID_TEMPO_MINUS = 1006;
const UINT ID_TEMPO_PLUS = 1007;

const UINT ID_PITCH_MINUS = 1008;
const UINT ID_PITCH_PLUS = 1009;

const UINT ID_SAVE_TRACK = 1010;
const UINT ID_PLAY_ALL_TRACKS = 1011;

const UINT ID_LOAD_TRACK_BASE = 3000;
const UINT ID_PLAY_TRACK_BASE = 4000;
const UINT ID_DELETE_TRACK_BASE = 5000;

const UINT ID_NOTE_BASE = 10000;
const UINT ID_CHORD_BASE = 20000;
const UINT ID_DRUM_BASE = 30000;

std::map<UINT, std::string> noteCommands;
std::map<UINT, std::string> chordCommands;
std::map<UINT, std::string> drumCommands;

// ============================================================
// RANDOM NOISE
// ============================================================

double noise()
{
    thread_local std::mt19937 generator(
        std::random_device{}()
    );

    thread_local std::uniform_real_distribution<double>
        distribution(-1.0, 1.0);

    return distribution(generator);
}

// ============================================================
// SONG DATA
// ============================================================

struct NoteEvent
{
    double startBeat;
    double durationBeats;
    double frequency;
    std::string instrument;
};

struct DrumEvent
{
    double startBeat;
    std::string type;
};

// ============================================================
// NOTE FREQUENCY
// ============================================================

double noteFrequency(const std::string& note)
{
    if (note.size() < 2)
        return 0.0;

    char letter = note[0];

    int semitoneFromC = -1;

    switch (letter)
    {
        case 'C': semitoneFromC = 0;  break;
        case 'D': semitoneFromC = 2;  break;
        case 'E': semitoneFromC = 4;  break;
        case 'F': semitoneFromC = 5;  break;
        case 'G': semitoneFromC = 7;  break;
        case 'A': semitoneFromC = 9;  break;
        case 'B': semitoneFromC = 11; break;
        default:
            return 0.0;
    }

    size_t position = 1;

    if (
        position < note.size() &&
        note[position] == '#')
    {
        semitoneFromC++;
        position++;
    }

    if (position >= note.size())
        return 0.0;

    int octave = 0;

    try
    {
        octave =
            std::stoi(
                note.substr(position)
            );
    }
    catch (...)
    {
        return 0.0;
    }

    if (semitoneFromC >= 12)
    {
        semitoneFromC -= 12;
        octave++;
    }

    int midiNote =
        (octave + 1) * 12 +
        semitoneFromC;

    if (midiNote < 21 || midiNote > 108)
        return 0.0;

    return
        440.0 *
        std::pow(
            2.0,
            (midiNote - 69) / 12.0
        );
}

// ============================================================
// FULL 88-KEY PIANO
// ============================================================

std::vector<std::string> GetNotes()
{
    std::vector<std::string> notes;

    const char* noteNames[] =
    {
        "C","C#","D","D#","E","F",
        "F#","G","G#","A","A#","B"
    };

    for (int midi = 21; midi <= 108; ++midi)
    {
        int octave =
            (midi / 12) - 1;

        int noteIndex =
            midi % 12;

        notes.push_back(
            std::string(
                noteNames[noteIndex]
            ) +
            std::to_string(octave)
        );
    }

    return notes;
}

// ============================================================
// INSTRUMENT LIST
// ============================================================

std::vector<std::string> GetInstruments()
{
    std::vector<std::string> instruments =
    {
        "BASS",
        "BELL",
        "FLUTE",
        "GUITAR",
        "ORGAN",
        "PIANO",
        "SYNTH",
        "TRUMPET"
    };

    std::sort(
        instruments.begin(),
        instruments.end()
    );

    return instruments;
}

// ============================================================
// INSTRUMENT WAVEFORM
// ============================================================

double instrumentWave(
    const std::string& instrument,
    double frequency,
    double t)
{
    double phase =
        2.0 *
        PI *
        frequency *
        t;

    if (instrument == "PIANO")
    {
        double envelope =
            std::exp(-1.7 * t);

        return
            (
                std::sin(phase) +
                std::sin(phase * 2.0) * 0.35 +
                std::sin(phase * 3.0) * 0.15 +
                std::sin(phase * 4.0) * 0.06
            ) *
            envelope;
    }

    if (instrument == "BASS")
    {
        double envelope =
            std::exp(-1.0 * t);

        return
            (
                std::sin(phase) * 0.9 +
                std::sin(phase * 2.0) * 0.25 +
                std::sin(phase * 3.0) * 0.08
            ) *
            envelope;
    }

    if (instrument == "GUITAR")
    {
        double attack =
            1.0 -
            std::exp(-120.0 * t);

        double decay =
            std::exp(-3.8 * t);

        double pluck =
            std::exp(-35.0 * t);

        double sound =
            std::sin(phase) * 0.60 +
            std::sin(phase * 2.0) * 0.30 +
            std::sin(phase * 3.0) * 0.18 +
            std::sin(phase * 5.0) * 0.08;

        return
            sound *
            attack *
            decay
            +
            std::sin(phase * 2.0) *
            pluck *
            0.12;
    }

    if (instrument == "SYNTH")
    {
        double saw =
            2.0 *
            (
                frequency * t -
                std::floor(
                    frequency * t + 0.5
                )
            );

        return saw * 0.8;
    }

    if (instrument == "ORGAN")
    {
        return
            std::sin(phase) * 0.65 +
            std::sin(phase * 2.0) * 0.25 +
            std::sin(phase * 4.0) * 0.12 +
            std::sin(phase * 8.0) * 0.04;
    }

    if (instrument == "FLUTE")
    {
        return
            (
                std::sin(phase) * 0.85 +
                std::sin(phase * 2.0) * 0.10
            ) *
            std::exp(-0.8 * t);
    }

    if (instrument == "TRUMPET")
    {
        double attack =
            1.0 -
            std::exp(-45.0 * t);

        double envelope =
            0.85 +
            0.15 *
            std::exp(-1.0 * t);

        double brass =
            std::sin(phase) * 0.45 +
            std::sin(phase * 2.0) * 0.30 +
            std::sin(phase * 3.0) * 0.28 +
            std::sin(phase * 4.0) * 0.20 +
            std::sin(phase * 5.0) * 0.14 +
            std::sin(phase * 6.0) * 0.08;

        return
            brass *
            attack *
            envelope;
    }

    if (instrument == "BELL")
    {
        double envelope =
            std::exp(-2.2 * t);

        return
            (
                std::sin(phase) +
                std::sin(phase * 2.71) * 0.4 +
                std::sin(phase * 4.13) * 0.2
            ) *
            envelope;
    }

    return std::sin(phase);
}

// ============================================================
// DRUMS
// ============================================================

double kick(double t)
{
    if (t < 0.0 || t >= 0.8)
        return 0.0;

    double frequency =
        155.0 *
        std::exp(-22.0 * t)
        + 48.0;

    double body =
        std::sin(
            2.0 * PI *
            frequency *
            t
        ) *
        std::exp(-6.0 * t);

    double beater =
        std::sin(
            2.0 * PI *
            95.0 *
            t
        ) *
        std::exp(-55.0 * t);

    double click =
        noise() *
        std::exp(-140.0 * t);

    return
        body * 1.20 +
        beater * 0.28 +
        click * 0.025;
}

double snare(double t)
{
    if (t < 0.0 || t >= 0.5)
        return 0.0;

    double wire =
        noise() *
        std::exp(-17.0 * t);

    double body =
        std::sin(
            2.0 * PI *
            190.0 *
            t
        ) *
        std::exp(-19.0 * t);

    double ring =
        std::sin(
            2.0 * PI *
            330.0 *
            t
        ) *
        std::exp(-27.0 * t);

    double attack =
        std::sin(
            2.0 * PI *
            1700.0 *
            t
        ) *
        std::exp(-105.0 * t);

    return
        wire * 0.68 +
        body * 0.48 +
        ring * 0.16 +
        attack * 0.14;
}

double closedHiHat(double t)
{
    if (t < 0.0 || t >= 0.18)
        return 0.0;

    double metal =
        noise() * 0.72 +
        std::sin(2.0 * PI * 6800.0 * t) * 0.14 +
        std::sin(2.0 * PI * 8900.0 * t) * 0.10 +
        std::sin(2.0 * PI * 11200.0 * t) * 0.06;

    return
        metal *
        std::exp(-38.0 * t) *
        0.55;
}

double openHiHat(double t)
{
    if (t < 0.0 || t >= 0.8)
        return 0.0;

    double metal =
        noise() * 0.72 +
        std::sin(2.0 * PI * 6200.0 * t) * 0.14 +
        std::sin(2.0 * PI * 8400.0 * t) * 0.10 +
        std::sin(2.0 * PI * 10500.0 * t) * 0.06;

    double attack =
        1.0 -
        std::exp(-120.0 * t);

    double envelope =
        std::exp(-5.0 * t);

    return
        metal *
        attack *
        envelope *
        0.45;
}

double tom(
    double t,
    double frequency)
{
    if (t < 0.0 || t >= 0.9)
        return 0.0;

    double pitch =
        frequency *
        (
            1.0 +
            0.15 *
            std::exp(-8.0 * t)
        );

    double body =
        std::sin(
            2.0 * PI *
            pitch *
            t
        );

    double harmonic =
        std::sin(
            2.0 * PI *
            pitch *
            1.98 *
            t
        ) * 0.16;

    double attack =
        std::sin(
            2.0 * PI *
            700.0 *
            t
        ) *
        std::exp(-45.0 * t);

    double envelope =
        std::exp(-5.0 * t);

    return
        (
            body +
            harmonic +
            attack * 0.08
        ) *
        envelope *
        0.75;
}

double crash(double t)
{
    if (t < 0.0 || t >= 2.5)
        return 0.0;

    double metal =
        noise() * 0.72 +
        std::sin(2.0 * PI * 3900.0 * t) * 0.10 +
        std::sin(2.0 * PI * 5100.0 * t) * 0.08 +
        std::sin(2.0 * PI * 7300.0 * t) * 0.06;

    double attack =
        1.0 -
        std::exp(-180.0 * t);

    double envelope =
        std::exp(-1.7 * t);

    return
        metal *
        attack *
        envelope *
        0.65;
}

double ride(double t)
{
    if (t < 0.0 || t >= 1.5)
        return 0.0;

    double metallicNoise =
        noise() * 0.32;

    double bell =
        std::sin(
            2.0 * PI *
            2800.0 *
            t
        ) * 0.22;

    double metallicTone =
        std::sin(
            2.0 * PI *
            4700.0 *
            t
        ) * 0.15;

    double envelope =
        std::exp(-2.7 * t);

    return
        (
            metallicNoise +
            bell +
            metallicTone
        ) *
        envelope *
        0.45;
}

double rimshot(double t)
{
    if (t < 0.0 || t >= 0.18)
        return 0.0;

    double attack =
        std::sin(
            2.0 * PI *
            1250.0 *
            t
        ) *
        std::exp(-45.0 * t);

    double wood =
        std::sin(
            2.0 * PI *
            420.0 *
            t
        ) *
        std::exp(-35.0 * t);

    double click =
        noise() *
        std::exp(-75.0 * t);

    return
        attack * 0.65 +
        wood * 0.30 +
        click * 0.20;
}

double makeDrum(
    const std::string& type,
    double t)
{
    if (
        type == "KICK" ||
        type == "BASS_DRUM")
    {
        return kick(t);
    }

    if (type == "SNARE")
        return snare(t);

    if (
        type == "HIHAT" ||
        type == "CLOSED_HIHAT")
    {
        return closedHiHat(t);
    }

    if (
        type == "OPEN_HIHAT" ||
        type == "OPENHIHAT")
    {
        return openHiHat(t);
    }

    if (type == "LOW_TOM")
        return tom(t, 95.0);

    if (type == "MID_TOM")
        return tom(t, 145.0);

    if (type == "HIGH_TOM")
        return tom(t, 210.0);

    if (type == "CRASH")
        return crash(t);

    if (type == "RIDE")
        return ride(t);

    if (type == "RIMSHOT")
        return rimshot(t);

    return 0.0;
}

// ============================================================
// UPPERCASE
// ============================================================

std::string upper(std::string value)
{
    for (char& c : value)
    {
        if (c >= 'a' && c <= 'z')
            c =
                static_cast<char>(
                    c - 'a' + 'A'
                );
    }

    return value;
}

// ============================================================
// LOAD SONG TEXT
// ============================================================

bool LoadSongText(
    const std::string& text,
    std::vector<NoteEvent>& notes,
    std::vector<DrumEvent>& drums,
    double& tempo,
    double& loopLengthBeats)
{
    notes.clear();
    drums.clear();

    tempo = 120.0;
    loopLengthBeats = 4.0;

    std::istringstream input(text);

    std::string command;

    while (input >> command)
    {
        command = upper(command);

        if (command == "TEMPO")
        {
            input >> tempo;

            tempo =
                std::max(
                    MIN_TEMPO,
                    std::min(
                        MAX_TEMPO,
                        tempo
                    )
                );
        }
        else if (command == "LENGTH")
        {
            input >> loopLengthBeats;

            if (loopLengthBeats <= 0)
                loopLengthBeats = 4.0;
        }
        else if (
            command == "NOTE" ||
            command == "PIANO" ||
            command == "BASS" ||
            command == "GUITAR" ||
            command == "SYNTH" ||
            command == "ORGAN" ||
            command == "FLUTE" ||
            command == "TRUMPET" ||
            command == "BELL")
        {
            std::string noteName;

            double startBeat = 0.0;
            double durationBeats = 1.0;

            input >>
                noteName >>
                startBeat >>
                durationBeats;

            double frequency =
                noteFrequency(
                    upper(noteName)
                );

            if (frequency > 0.0)
            {
                std::string instrument =
                    command;

                if (command == "NOTE")
                    instrument = "PIANO";

                notes.push_back(
                    {
                        startBeat,
                        durationBeats,
                        frequency,
                        instrument
                    }
                );
            }
        }
        else if (command == "DRUM")
        {
            std::string type;
            double startBeat = 0.0;

            input >>
                type >>
                startBeat;

            drums.push_back(
                {
                    startBeat,
                    upper(type)
                }
            );
        }
    }

    return true;
}

// ============================================================
// EDITOR TEXT
// ============================================================

std::string GetEditorText()
{
    if (!songEditor)
        return "";

    int length =
        GetWindowTextLengthA(
            songEditor
        );

    if (length <= 0)
        return "";

    std::string text(
        length + 1,
        '\0'
    );

    int actualLength =
        GetWindowTextA(
            songEditor,
            &text[0],
            length + 1
        );

    text.resize(actualLength);

    return text;
}

void SetEditorText(
    const std::string& text)
{
    if (!songEditor)
        return;

    SetWindowTextA(
        songEditor,
        text.c_str()
    );
}

void InsertEditorText(
    const std::string& text)
{
    if (!songEditor)
        return;

    SendMessageA(
        songEditor,
        EM_REPLACESEL,
        TRUE,
        reinterpret_cast<LPARAM>(
            text.c_str()
        )
    );
}

// ============================================================
// VOLUME
// ============================================================

double GetVolumeMultiplier(int level)
{
    switch (level)
    {
        case 1:
            return 2.0;

        case 2:
            return 3.5;

        case 3:
            return 5.0;

        default:
            return 1.0;
    }
}

// ============================================================
// GENERATE AUDIO
// ============================================================

bool GenerateAudio(
    const std::string& text,
    double tempoOverride,
    int pitchSemitones,
    double volumeMultiplier,
    std::vector<short>& samples)
{
    std::vector<NoteEvent> notes;
    std::vector<DrumEvent> drums;

    double fileTempo;
    double loopLengthBeats;

    if (!LoadSongText(
        text,
        notes,
        drums,
        fileTempo,
        loopLengthBeats))
    {
        return false;
    }

    double tempo =
        std::max(
            MIN_TEMPO,
            std::min(
                MAX_TEMPO,
                tempoOverride
            )
        );

    int pitch =
        std::max(
            MIN_PITCH,
            std::min(
                MAX_PITCH,
                pitchSemitones
            )
        );

    double pitchMultiplier =
        std::pow(
            2.0,
            static_cast<double>(pitch) / 12.0
        );

    double secondsPerBeat =
        60.0 / tempo;

    double loopDuration =
        loopLengthBeats *
        secondsPerBeat;

    int totalSamples =
        static_cast<int>(
            std::round(
                loopDuration *
                SAMPLE_RATE
            )
        );

    if (totalSamples <= 0)
        return false;

    std::vector<double> audio(
        totalSamples,
        0.0
    );

    // ========================================================
    // NOTES
    // ========================================================

    for (const NoteEvent& note : notes)
    {
        double startSeconds =
            note.startBeat *
            secondsPerBeat;

        double durationSeconds =
            note.durationBeats *
            secondsPerBeat;

        int startSample =
            static_cast<int>(
                std::round(
                    startSeconds *
                    SAMPLE_RATE
                )
            );

        int noteSamples =
            static_cast<int>(
                std::round(
                    durationSeconds *
                    SAMPLE_RATE
                )
            );

        double shiftedFrequency =
            note.frequency *
            pitchMultiplier;

        for (int i = 0;
             i < noteSamples;
             ++i)
        {
            int index =
                startSample + i;

            if (index < 0)
                continue;

            if (index >= totalSamples)
                break;

            double time =
                static_cast<double>(i) /
                SAMPLE_RATE;

            double envelope = 1.0;

            if (time < 0.01)
            {
                envelope =
                    time / 0.01;
            }

            double remaining =
                durationSeconds -
                time;

            if (remaining < 0.05)
            {
                envelope =
                    std::min(
                        envelope,
                        std::max(
                            0.0,
                            remaining / 0.05
                        )
                    );
            }

            double wave =
                instrumentWave(
                    note.instrument,
                    shiftedFrequency,
                    time
                );

            audio[index] +=
                wave *
                envelope *
                0.55;
        }
    }

    // ========================================================
    // DRUMS
    // ========================================================

    for (const DrumEvent& drum : drums)
    {
        double startSeconds =
            drum.startBeat *
            secondsPerBeat;

        int startSample =
            static_cast<int>(
                std::round(
                    startSeconds *
                    SAMPLE_RATE
                )
            );

        int drumSamples =
            static_cast<int>(
                2.5 *
                SAMPLE_RATE
            );

        for (int i = 0;
             i < drumSamples;
             ++i)
        {
            int index =
                startSample + i;

            if (index < 0)
                continue;

            if (index >= totalSamples)
                break;

            double time =
                static_cast<double>(i) /
                SAMPLE_RATE;

            double drumVolume = 1.0;

            if (
                drum.type == "KICK" ||
                drum.type == "BASS_DRUM")
            {
                drumVolume = 2.25;
            }
            else if (drum.type == "SNARE")
            {
                drumVolume = 1.70;
            }
            else if (
                drum.type == "HIHAT" ||
                drum.type == "CLOSED_HIHAT")
            {
                drumVolume = 0.85;
            }
            else if (drum.type == "OPEN_HIHAT")
            {
                drumVolume = 0.95;
            }
            else if (
                drum.type == "LOW_TOM" ||
                drum.type == "MID_TOM" ||
                drum.type == "HIGH_TOM")
            {
                drumVolume = 1.15;
            }
            else if (drum.type == "CRASH")
            {
                drumVolume = 1.05;
            }
            else if (drum.type == "RIDE")
            {
                drumVolume = 0.85;
            }
            else if (drum.type == "RIMSHOT")
            {
                drumVolume = 0.90;
            }

            audio[index] +=
                makeDrum(
                    drum.type,
                    time
                ) *
                drumVolume;
        }
    }

    // ========================================================
    // MASTER
    // ========================================================

    if (volumeMultiplier < 1.0)
        volumeMultiplier = 1.0;

    samples.resize(totalSamples);

    for (int i = 0;
         i < totalSamples;
         ++i)
    {
        double value =
            audio[i] *
            MASTER_GAIN *
            volumeMultiplier;

        value =
            std::tanh(value);

        samples[i] =
            static_cast<short>(
                value *
                32767.0
            );
    }

    return true;
}

// ============================================================
// PLAY SONG TEXT
// ============================================================

void PlaySongText(
    const std::string& songText)
{
    if (playing)
        return;

    if (songText.empty())
        return;

    playing = true;
    stopRequested = false;

    std::vector<NoteEvent> notes;
    std::vector<DrumEvent> drums;

    double fileTempo = 120.0;
    double loopLengthBeats = 4.0;

    LoadSongText(
        songText,
        notes,
        drums,
        fileTempo,
        loopLengthBeats
    );

    if (
        notes.empty() &&
        drums.empty())
    {
        playing = false;
        return;
    }

    WAVEFORMATEX format = {};

    format.wFormatTag =
        WAVE_FORMAT_PCM;

    format.nChannels = 1;
    format.nSamplesPerSec = SAMPLE_RATE;
    format.wBitsPerSample = 16;

    format.nBlockAlign =
        format.nChannels *
        format.wBitsPerSample / 8;

    format.nAvgBytesPerSec =
        format.nSamplesPerSec *
        format.nBlockAlign;

    HWAVEOUT audioDevice = nullptr;

    MMRESULT result =
        waveOutOpen(
            &audioDevice,
            WAVE_MAPPER,
            &format,
            0,
            0,
            CALLBACK_NULL
        );

    if (
        result !=
        MMSYSERR_NOERROR)
    {
        playing = false;
        return;
    }

    std::vector<short> audioSamples[
        AUDIO_BUFFER_COUNT
    ];

    WAVEHDR headers[
        AUDIO_BUFFER_COUNT
    ] = {};

    bool prepared[
        AUDIO_BUFFER_COUNT
    ] = {};

    bool queued[
        AUDIO_BUFFER_COUNT
    ] = {};

    auto generateBuffer =
        [&](int index)
        {
            double tempo =
                currentTempo.load();

            int pitch =
                currentPitch.load();

            double multiplier =
                GetVolumeMultiplier(
                    volumeBoost.load()
                );

            return GenerateAudio(
                songText,
                tempo,
                pitch,
                multiplier,
                audioSamples[index]
            );
        };

    auto prepareAndWrite =
        [&](int index) -> bool
        {
            headers[index] = {};

            headers[index].lpData =
                reinterpret_cast<LPSTR>(
                    audioSamples[index].data()
                );

            headers[index].dwBufferLength =
                static_cast<DWORD>(
                    audioSamples[index].size() *
                    sizeof(short)
                );

            if (
                waveOutPrepareHeader(
                    audioDevice,
                    &headers[index],
                    sizeof(WAVEHDR)
                ) != MMSYSERR_NOERROR)
            {
                return false;
            }

            prepared[index] = true;

            if (
                waveOutWrite(
                    audioDevice,
                    &headers[index],
                    sizeof(WAVEHDR)
                ) != MMSYSERR_NOERROR)
            {
                waveOutUnprepareHeader(
                    audioDevice,
                    &headers[index],
                    sizeof(WAVEHDR)
                );

                prepared[index] = false;

                return false;
            }

            queued[index] = true;

            return true;
        };

    if (!generateBuffer(0))
    {
        waveOutClose(audioDevice);
        playing = false;
        return;
    }

    if (!prepareAndWrite(0))
    {
        waveOutClose(audioDevice);
        playing = false;
        return;
    }

    if (looping)
    {
        if (generateBuffer(1))
        {
            prepareAndWrite(1);
        }
    }

    while (!stopRequested)
    {
        bool didSomething = false;

        for (int i = 0;
             i < AUDIO_BUFFER_COUNT;
             ++i)
        {
            if (!queued[i])
                continue;

            if (!(headers[i].dwFlags & WHDR_DONE))
                continue;

            didSomething = true;

            waveOutUnprepareHeader(
                audioDevice,
                &headers[i],
                sizeof(WAVEHDR)
            );

            prepared[i] = false;
            queued[i] = false;

            if (!looping)
                continue;

            if (stopRequested)
                continue;

            if (!generateBuffer(i))
            {
                stopRequested = true;
                continue;
            }

            if (!prepareAndWrite(i))
            {
                stopRequested = true;
                continue;
            }
        }

        if (!looping)
        {
            bool anythingQueued = false;

            for (int i = 0;
                 i < AUDIO_BUFFER_COUNT;
                 ++i)
            {
                if (queued[i])
                {
                    anythingQueued = true;
                    break;
                }
            }

            if (!anythingQueued)
                break;
        }

        if (!didSomething)
            Sleep(1);
    }

    waveOutReset(audioDevice);

    for (int i = 0;
         i < AUDIO_BUFFER_COUNT;
         ++i)
    {
        if (prepared[i])
        {
            waveOutUnprepareHeader(
                audioDevice,
                &headers[i],
                sizeof(WAVEHDR)
            );

            prepared[i] = false;
        }

        queued[i] = false;
    }

    waveOutClose(audioDevice);

    playing = false;
}

// ============================================================
// PLAY EDITOR
// ============================================================

void PlayEditor()
{
    std::string text =
        GetEditorText();

    if (text.empty())
        return;

    stopRequested = false;

    std::thread(
        PlaySongText,
        text
    ).detach();
}

// ============================================================
// UPDATE DISPLAYS
// ============================================================

void UpdateTempoDisplay()
{
    if (!tempoLabel)
        return;

    char text[64];

    std::snprintf(
        text,
        sizeof(text),
        "TEMPO: %d",
        static_cast<int>(
            std::round(
                currentTempo.load()
            )
        )
    );

    SetWindowTextA(
        tempoLabel,
        text
    );
}

void UpdatePitchDisplay()
{
    if (!pitchLabel)
        return;

    char text[64];

    int pitch =
        currentPitch.load();

    if (pitch > 0)
    {
        std::snprintf(
            text,
            sizeof(text),
            "PITCH: +%d",
            pitch
        );
    }
    else
    {
        std::snprintf(
            text,
            sizeof(text),
            "PITCH: %d",
            pitch
        );
    }

    SetWindowTextA(
        pitchLabel,
        text
    );
}

void UpdateBoostDisplay()
{
    if (!boostButton)
        return;

    char text[64];

    std::snprintf(
        text,
        sizeof(text),
        "BOOST: %.1fx",
        GetVolumeMultiplier(
            volumeBoost.load()
        )
    );

    SetWindowTextA(
        boostButton,
        text
    );
}

// ============================================================
// CHORDS
// ============================================================

std::vector<std::string> GetChordRoots()
{
    return
    {
        "C","C#","D","D#","E","F",
        "F#","G","G#","A","A#","B"
    };
}

std::vector<std::string> GetChordTypes()
{
    return
    {
        "MAJOR",
        "MINOR",
        "MINOR 7",
        "MAJOR 7"
    };
}

std::string NoteFromSemitone(
    int semitone)
{
    static const char* names[] =
    {
        "C","C#","D","D#","E","F",
        "F#","G","G#","A","A#","B"
    };

    int octave =
        4 +
        semitone / 12;

    int index =
        semitone % 12;

    if (index < 0)
    {
        index += 12;
        octave--;
    }

    return
        std::string(names[index]) +
        std::to_string(octave);
}

std::vector<std::string> GetChordNotes(
    const std::string& root,
    const std::string& type)
{
    std::vector<std::string> roots =
        GetChordRoots();

    int rootIndex = 0;

    for (int i = 0;
         i < static_cast<int>(roots.size());
         ++i)
    {
        if (roots[i] == root)
        {
            rootIndex = i;
            break;
        }
    }

    std::vector<int> intervals;

    if (type == "MAJOR")
        intervals = {0,4,7};
    else if (type == "MINOR")
        intervals = {0,3,7};
    else if (type == "MINOR 7")
        intervals = {0,3,7,10};
    else if (type == "MAJOR 7")
        intervals = {0,4,7,11};

    std::vector<std::string> result;

    for (int interval : intervals)
    {
        int semitone =
            rootIndex +
            interval;

        result.push_back(
            NoteFromSemitone(
                semitone
            )
        );
    }

    return result;
}

// ============================================================
// DRUM LIST
// ============================================================

std::vector<std::string> GetDrums()
{
    return
    {
        "KICK",
        "SNARE",
        "HIHAT",
        "OPEN HIHAT",
        "LOW TOM",
        "MID TOM",
        "HIGH TOM",
        "CRASH",
        "RIDE",
        "RIMSHOT"
    };
}

std::string DrumCommandName(
    const std::string& displayName)
{
    if (displayName == "OPEN HIHAT")
        return "OPEN_HIHAT";

    if (displayName == "LOW TOM")
        return "LOW_TOM";

    if (displayName == "MID TOM")
        return "MID_TOM";

    if (displayName == "HIGH TOM")
        return "HIGH_TOM";

    return displayName;
}

// ============================================================
// INSTRUMENT MENU
// ============================================================

HMENU BuildInstrumentMenu()
{
    HMENU menu =
        CreatePopupMenu();

    std::vector<std::string> instruments =
        GetInstruments();

    std::vector<std::string> notes =
        GetNotes();

    for (const std::string& instrument :
         instruments)
    {
        HMENU noteMenu =
            CreatePopupMenu();

        for (const std::string& note :
             notes)
        {
            UINT id =
                ID_NOTE_BASE +
                static_cast<UINT>(
                    noteCommands.size()
                );

            std::string command =
                instrument +
                " " +
                note +
                " 0 1";

            noteCommands[id] =
                command;

            AppendMenuA(
                noteMenu,
                MF_STRING,
                id,
                note.c_str()
            );
        }

        AppendMenuA(
            menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(
                noteMenu
            ),
            instrument.c_str()
        );
    }

    return menu;
}

// ============================================================
// CHORD MENU
// ============================================================

HMENU BuildChordMenu()
{
    HMENU menu =
        CreatePopupMenu();

    std::vector<std::string> instruments =
        GetInstruments();

    std::vector<std::string> roots =
        GetChordRoots();

    std::vector<std::string> types =
        GetChordTypes();

    for (const std::string& instrument :
         instruments)
    {
        HMENU instrumentMenu =
            CreatePopupMenu();

        for (const std::string& root :
             roots)
        {
            HMENU rootMenu =
                CreatePopupMenu();

            for (const std::string& type :
                 types)
            {
                UINT id =
                    ID_CHORD_BASE +
                    static_cast<UINT>(
                        chordCommands.size()
                    );

                std::vector<std::string> chordNotes =
                    GetChordNotes(
                        root,
                        type
                    );

                std::string command;

                for (const std::string& note :
                     chordNotes)
                {
                    command +=
                        instrument +
                        " " +
                        note +
                        " 0 1\r\n";
                }

                chordCommands[id] =
                    command;

                std::string label =
                    root +
                    " " +
                    type;

                AppendMenuA(
                    rootMenu,
                    MF_STRING,
                    id,
                    label.c_str()
                );
            }

            AppendMenuA(
                instrumentMenu,
                MF_POPUP,
                reinterpret_cast<UINT_PTR>(
                    rootMenu
                ),
                root.c_str()
            );
        }

        AppendMenuA(
            menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(
                instrumentMenu
            ),
            instrument.c_str()
        );
    }

    return menu;
}

// ============================================================
// DRUM MENU
// ============================================================

HMENU BuildDrumMenu()
{
    HMENU menu =
        CreatePopupMenu();

    std::vector<std::string> drums =
        GetDrums();

    for (const std::string& drum :
         drums)
    {
        HMENU beatMenu =
            CreatePopupMenu();

        std::string drumType =
            DrumCommandName(drum);

        for (int i = 0; i < 16; ++i)
        {
            double beat =
                static_cast<double>(i) * 0.5;

            UINT id =
                ID_DRUM_BASE +
                static_cast<UINT>(
                    drumCommands.size()
                );

            char beatText[32];

            if (
                std::fabs(
                    beat -
                    std::round(beat)
                ) < 0.001)
            {
                std::snprintf(
                    beatText,
                    sizeof(beatText),
                    "BEAT %d",
                    static_cast<int>(beat)
                );
            }
            else
            {
                std::snprintf(
                    beatText,
                    sizeof(beatText),
                    "BEAT %.1f",
                    beat
                );
            }

            char commandText[128];

            std::snprintf(
                commandText,
                sizeof(commandText),
                "DRUM %s %.1f",
                drumType.c_str(),
                beat
            );

            drumCommands[id] =
                commandText;

            AppendMenuA(
                beatMenu,
                MF_STRING,
                id,
                beatText
            );
        }

        AppendMenuA(
            menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(
                beatMenu
            ),
            drum.c_str()
        );
    }

    return menu;
}

// ============================================================
// SHOW MENUS
// ============================================================

void ShowInstrumentSelector()
{
    if (!mainWindow)
        return;

    POINT point;
    GetCursorPos(&point);

    HMENU menu =
        BuildInstrumentMenu();

    TrackPopupMenu(
        menu,
        TPM_LEFTALIGN |
        TPM_TOPALIGN |
        TPM_RIGHTBUTTON,
        point.x,
        point.y,
        0,
        mainWindow,
        nullptr
    );

    DestroyMenu(menu);
}

void ShowChordSelector()
{
    if (!mainWindow)
        return;

    POINT point;
    GetCursorPos(&point);

    HMENU menu =
        BuildChordMenu();

    TrackPopupMenu(
        menu,
        TPM_LEFTALIGN |
        TPM_TOPALIGN |
        TPM_RIGHTBUTTON,
        point.x,
        point.y,
        0,
        mainWindow,
        nullptr
    );

    DestroyMenu(menu);
}

void ShowDrumSelector()
{
    if (!mainWindow)
        return;

    POINT point;
    GetCursorPos(&point);

    HMENU menu =
        BuildDrumMenu();

    TrackPopupMenu(
        menu,
        TPM_LEFTALIGN |
        TPM_TOPALIGN |
        TPM_RIGHTBUTTON,
        point.x,
        point.y,
        0,
        mainWindow,
        nullptr
    );

    DestroyMenu(menu);
}

// ============================================================
// TRACK BUTTONS
// ============================================================

std::vector<HWND> trackPlayButtons;
std::vector<HWND> trackLoadButtons;
std::vector<HWND> trackDeleteButtons;

void DestroyTrackButtons()
{
    for (HWND button :
         trackPlayButtons)
    {
        if (button)
            DestroyWindow(button);
    }

    for (HWND button :
         trackLoadButtons)
    {
        if (button)
            DestroyWindow(button);
    }

    for (HWND button :
         trackDeleteButtons)
    {
        if (button)
            DestroyWindow(button);
    }

    trackPlayButtons.clear();
    trackLoadButtons.clear();
    trackDeleteButtons.clear();
}

// ============================================================
// CREATE SAVED TRACK BUTTONS
// ============================================================

void ResizeTrackButtons()
{
    DestroyTrackButtons();

    if (!mainWindow)
        return;

    RECT rect;

    GetClientRect(
        mainWindow,
        &rect
    );

    int width =
        rect.right -
        LEFT_MARGIN * 2;

    if (width < 400)
        width = 400;

    int y =
        TRACK_TOP -
        scrollY;

    const int deleteWidth = 90;
    const int loadWidth = 80;
    const int gap = 5;

    int playWidth =
        width -
        deleteWidth -
        loadWidth -
        gap * 2;

    for (int i = 0;
         i < static_cast<int>(
                savedTracks.size());
         ++i)
    {
        std::string trackName =
            "TRACK " +
            std::to_string(i + 1);

        // PLAY TRACK
        HWND play =
            CreateWindowA(
                "BUTTON",
                trackName.c_str(),
                WS_VISIBLE |
                WS_CHILD |
                BS_PUSHBUTTON,
                LEFT_MARGIN,
                y,
                playWidth,
                TRACK_BUTTON_HEIGHT,
                mainWindow,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        ID_PLAY_TRACK_BASE +
                        i
                    )
                ),
                GetModuleHandleA(nullptr),
                nullptr
            );

        // LOAD
        HWND load =
            CreateWindowA(
                "BUTTON",
                "LOAD",
                WS_VISIBLE |
                WS_CHILD |
                BS_PUSHBUTTON,
                LEFT_MARGIN +
                playWidth +
                gap,
                y,
                loadWidth,
                TRACK_BUTTON_HEIGHT,
                mainWindow,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        ID_LOAD_TRACK_BASE +
                        i
                    )
                ),
                GetModuleHandleA(nullptr),
                nullptr
            );

        // DELETE
        HWND deleteButton =
            CreateWindowA(
                "BUTTON",
                "DELETE",
                WS_VISIBLE |
                WS_CHILD |
                BS_PUSHBUTTON,
                LEFT_MARGIN +
                playWidth +
                gap +
                loadWidth +
                gap,
                y,
                deleteWidth,
                TRACK_BUTTON_HEIGHT,
                mainWindow,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(
                        ID_DELETE_TRACK_BASE +
                        i
                    )
                ),
                GetModuleHandleA(nullptr),
                nullptr
            );

        trackPlayButtons.push_back(play);
        trackLoadButtons.push_back(load);
        trackDeleteButtons.push_back(deleteButton);

        y +=
            TRACK_BUTTON_HEIGHT +
            TRACK_GAP;
    }
}

// ============================================================
// CONTENT HEIGHT
// ============================================================

int CalculateContentHeight(
    RECT rect)
{
    int trackCount =
        static_cast<int>(
            savedTracks.size()
        );

    int trackRows =
        std::max(
            1,
            trackCount
        );

    int tracksBottom =
        TRACK_TOP +
        trackRows *
        (
            TRACK_BUTTON_HEIGHT +
            TRACK_GAP
        ) +
        BOTTOM_MARGIN;

    if (tracksBottom > rect.bottom)
        return tracksBottom;

    return rect.bottom;
}

// ============================================================
// SCROLL BAR
// ============================================================

void UpdateScrollBar(HWND window)
{
    RECT rect;

    GetClientRect(
        window,
        &rect
    );

    contentHeight =
        CalculateContentHeight(
            rect
        );

    int visibleHeight =
        rect.bottom;

    if (visibleHeight < 1)
        visibleHeight = 1;

    int maximum =
        contentHeight -
        visibleHeight;

    if (maximum < 0)
        maximum = 0;

    if (scrollY > maximum)
        scrollY = maximum;

    SCROLLINFO si = {};

    si.cbSize =
        sizeof(SCROLLINFO);

    si.fMask =
        SIF_RANGE |
        SIF_PAGE |
        SIF_POS;

    si.nMin = 0;
    si.nMax = contentHeight;

    si.nPage =
        static_cast<UINT>(
            visibleHeight
        );

    si.nPos =
        scrollY;

    SetScrollInfo(
        window,
        SB_VERT,
        &si,
        TRUE
    );
}

// ============================================================
// RESIZE ALL CONTROLS
// ============================================================

void ResizeControls()
{
    if (!mainWindow)
        return;

    RECT rect;

    GetClientRect(
        mainWindow,
        &rect
    );

    int width =
        rect.right -
        LEFT_MARGIN * 2;

    if (width < 300)
        width = 300;

    // TOP ROW

    MoveWindow(
        playButton,
        LEFT_MARGIN,
        12 - scrollY,
        80,
        30,
        TRUE
    );

    MoveWindow(
        loopButton,
        LEFT_MARGIN + 85,
        12 - scrollY,
        90,
        30,
        TRUE
    );

    MoveWindow(
        boostButton,
        LEFT_MARGIN + 180,
        12 - scrollY,
        95,
        30,
        TRUE
    );

    MoveWindow(
        instrumentsButton,
        LEFT_MARGIN + 280,
        12 - scrollY,
        110,
        30,
        TRUE
    );

    MoveWindow(
        chordsButton,
        LEFT_MARGIN + 395,
        12 - scrollY,
        90,
        30,
        TRUE
    );

    MoveWindow(
        drumsButton,
        LEFT_MARGIN + 490,
        12 - scrollY,
        80,
        30,
        TRUE
    );

    // TEMPO

    MoveWindow(
        tempoMinusButton,
        LEFT_MARGIN,
        55 - scrollY,
        35,
        28,
        TRUE
    );

    MoveWindow(
        tempoLabel,
        LEFT_MARGIN + 40,
        55 - scrollY,
        120,
        28,
        TRUE
    );

    MoveWindow(
        tempoPlusButton,
        LEFT_MARGIN + 165,
        55 - scrollY,
        35,
        28,
        TRUE
    );

    // PITCH

    MoveWindow(
        pitchMinusButton,
        LEFT_MARGIN,
        88 - scrollY,
        35,
        28,
        TRUE
    );

    MoveWindow(
        pitchLabel,
        LEFT_MARGIN + 40,
        88 - scrollY,
        120,
        28,
        TRUE
    );

    MoveWindow(
        pitchPlusButton,
        LEFT_MARGIN + 165,
        88 - scrollY,
        35,
        28,
        TRUE
    );

    // SAVE

    MoveWindow(
        saveTrackButton,
        LEFT_MARGIN,
        125 - scrollY,
        200,
        32,
        TRUE
    );

    // PLAY ALL

    MoveWindow(
        playAllTracksButton,
        LEFT_MARGIN + 210,
        125 - scrollY,
        150,
        32,
        TRUE
    );

    // EDITOR

    MoveWindow(
        songEditor,
        LEFT_MARGIN,
        EDITOR_TOP - scrollY,
        width,
        EDITOR_HEIGHT,
        TRUE
    );

    ResizeTrackButtons();

    UpdateScrollBar(
        mainWindow
    );
}

// ============================================================
// SAVE TRACK
// ============================================================

void SaveTrack()
{
    std::string text =
        GetEditorText();

    if (text.empty())
    {
        MessageBoxA(
            mainWindow,
            "There is nothing in the editor to save.",
            "Save Track",
            MB_OK | MB_ICONINFORMATION
        );

        return;
    }

    savedTracks.push_back(text);

    SetEditorText("");

    ResizeControls();

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// LOAD TRACK
// ============================================================

void LoadTrack(int index)
{
    if (
        index < 0 ||
        index >= static_cast<int>(
            savedTracks.size()))
    {
        return;
    }

    stopRequested = true;

    SetEditorText(
        savedTracks[index]
    );

    SetFocus(
        songEditor
    );
}

// ============================================================
// DELETE TRACK
// ============================================================

void DeleteTrack(int index)
{
    if (
        index < 0 ||
        index >= static_cast<int>(
            savedTracks.size()))
    {
        return;
    }

    if (playing)
        stopRequested = true;

    std::string message =
        "Delete TRACK " +
        std::to_string(index + 1) +
        "?";

    int result =
        MessageBoxA(
            mainWindow,
            message.c_str(),
            "Delete Track",
            MB_YESNO |
            MB_ICONWARNING
        );

    if (result != IDYES)
        return;

    savedTracks.erase(
        savedTracks.begin() + index
    );

    ResizeControls();

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// PLAY SAVED TRACK
// ============================================================

void PlaySavedTrack(int index)
{
    if (
        index < 0 ||
        index >= static_cast<int>(
            savedTracks.size()))
    {
        return;
    }

    if (playing)
    {
        stopRequested = true;
        return;
    }

    std::string text =
        savedTracks[index];

    std::thread(
        PlaySongText,
        text
    ).detach();
}

// ============================================================
// PLAY ALL SAVED TRACKS
// ============================================================

void PlayAllSavedTracks()
{
    if (playing)
        return;

    if (savedTracks.empty())
    {
        MessageBoxA(
            mainWindow,
            "There are no saved tracks to play.",
            "Play All",
            MB_OK | MB_ICONINFORMATION
        );

        return;
    }

    // Make a copy so the playlist stays stable
    // while it is playing.
    std::vector<std::string> playlist =
        savedTracks;

    stopRequested = false;

    std::thread(
        [playlist]()
        {
            for (const std::string& track :
                 playlist)
            {
                if (stopRequested)
                    break;

                if (track.empty())
                    continue;

                PlaySongText(track);

                if (stopRequested)
                    break;
            }

            stopRequested = false;
        }
    ).detach();
}

// ============================================================
// WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
        // ====================================================
        // BUTTON COLOR
        // ====================================================

        case WM_CTLCOLORBTN:
        {
            HDC dc =
                reinterpret_cast<HDC>(
                    wParam
                );

            SetTextColor(
                dc,
                BUTTON_TEXT
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            return reinterpret_cast<LRESULT>(
                GetStockObject(
                    NULL_BRUSH
                )
            );
        }

        // ====================================================
        // EDIT COLOR
        // ====================================================

        case WM_CTLCOLOREDIT:
        {
            HDC dc =
                reinterpret_cast<HDC>(
                    wParam
                );

            SetTextColor(
                dc,
                EDIT_TEXT
            );

            SetBkColor(
                dc,
                EDIT_BACKGROUND
            );

            return reinterpret_cast<LRESULT>(
                editBrush
            );
        }

        // ====================================================
        // STATIC COLOR
        // ====================================================

        case WM_CTLCOLORSTATIC:
        {
            HDC dc =
                reinterpret_cast<HDC>(
                    wParam
                );

            SetTextColor(
                dc,
                TEXT_COLOR
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            return reinterpret_cast<LRESULT>(
                GetStockObject(
                    NULL_BRUSH
                )
            );
        }

        // ====================================================
        // COMMAND
        // ====================================================

        case WM_COMMAND:
        {
            UINT id =
                LOWORD(wParam);

            // ------------------------------------------------
            // PLAY
            // ------------------------------------------------

            if (id == ID_PLAY)
            {
                if (!playing)
                {
                    SetWindowTextA(
                        playButton,
                        "STOP"
                    );

                    PlayEditor();
                }
                else
                {
                    stopRequested = true;

                    SetWindowTextA(
                        playButton,
                        "PLAY"
                    );
                }

                return 0;
            }

            // ------------------------------------------------
            // LOOP
            // ------------------------------------------------

            if (id == ID_LOOP)
            {
                looping =
                    !looping.load();

                SetWindowTextA(
                    loopButton,
                    looping
                        ? "LOOP: ON"
                        : "LOOP: OFF"
                );

                return 0;
            }

            // ------------------------------------------------
            // BOOST
            // ------------------------------------------------

            if (id == ID_BOOST)
            {
                int level =
                    volumeBoost.load();

                level++;

                if (level > 3)
                    level = 0;

                volumeBoost =
                    level;

                UpdateBoostDisplay();

                return 0;
            }

            // ------------------------------------------------
            // INSTRUMENTS
            // ------------------------------------------------

            if (id == ID_INSTRUMENTS)
            {
                ShowInstrumentSelector();
                return 0;
            }

            // ------------------------------------------------
            // CHORDS
            // ------------------------------------------------

            if (id == ID_CHORDS)
            {
                ShowChordSelector();
                return 0;
            }

            // ------------------------------------------------
            // DRUMS
            // ------------------------------------------------

            if (id == ID_DRUMS)
            {
                ShowDrumSelector();
                return 0;
            }

            // ------------------------------------------------
            // TEMPO MINUS
            // ------------------------------------------------

            if (id == ID_TEMPO_MINUS)
            {
                double tempo =
                    currentTempo.load();

                tempo -= TEMPO_STEP;

                if (tempo < MIN_TEMPO)
                    tempo = MIN_TEMPO;

                currentTempo =
                    tempo;

                UpdateTempoDisplay();

                return 0;
            }

            // ------------------------------------------------
            // TEMPO PLUS
            // ------------------------------------------------

            if (id == ID_TEMPO_PLUS)
            {
                double tempo =
                    currentTempo.load();

                tempo += TEMPO_STEP;

                if (tempo > MAX_TEMPO)
                    tempo = MAX_TEMPO;

                currentTempo =
                    tempo;

                UpdateTempoDisplay();

                return 0;
            }

            // ------------------------------------------------
            // PITCH MINUS
            // ------------------------------------------------

            if (id == ID_PITCH_MINUS)
            {
                int pitch =
                    currentPitch.load();

                pitch -= PITCH_STEP;

                if (pitch < MIN_PITCH)
                    pitch = MIN_PITCH;

                currentPitch =
                    pitch;

                UpdatePitchDisplay();

                return 0;
            }

            // ------------------------------------------------
            // PITCH PLUS
            // ------------------------------------------------

            if (id == ID_PITCH_PLUS)
            {
                int pitch =
                    currentPitch.load();

                pitch += PITCH_STEP;

                if (pitch > MAX_PITCH)
                    pitch = MAX_PITCH;

                currentPitch =
                    pitch;

                UpdatePitchDisplay();

                return 0;
            }

            // ------------------------------------------------
            // SAVE
            // ------------------------------------------------

            if (id == ID_SAVE_TRACK)
            {
                SaveTrack();
                return 0;
            }

            // ------------------------------------------------
            // PLAY ALL
            // ------------------------------------------------

            if (id == ID_PLAY_ALL_TRACKS)
            {
                if (!playing)
                {
                    SetWindowTextA(
                        playAllTracksButton,
                        "PLAYING ALL..."
                    );

                    PlayAllSavedTracks();
                }
                else
                {
                    stopRequested = true;

                    SetWindowTextA(
                        playAllTracksButton,
                        "PLAY ALL"
                    );
                }

                return 0;
            }

            // ------------------------------------------------
            // NOTE SELECTOR
            // ------------------------------------------------

            auto noteFound =
                noteCommands.find(id);

            if (
                noteFound !=
                noteCommands.end())
            {
                InsertEditorText(
                    noteFound->second +
                    "\r\n"
                );

                return 0;
            }

            // ------------------------------------------------
            // CHORD SELECTOR
            // ------------------------------------------------

            auto chordFound =
                chordCommands.find(id);

            if (
                chordFound !=
                chordCommands.end())
            {
                InsertEditorText(
                    chordFound->second
                );

                return 0;
            }

            // ------------------------------------------------
            // DRUM SELECTOR
            // ------------------------------------------------

            auto drumFound =
                drumCommands.find(id);

            if (
                drumFound !=
                drumCommands.end())
            {
                InsertEditorText(
                    drumFound->second +
                    "\r\n"
                );

                return 0;
            }

            // ------------------------------------------------
            // DELETE SAVED TRACK
            // ------------------------------------------------

            if (
                id >= ID_DELETE_TRACK_BASE &&
                id <
                ID_DELETE_TRACK_BASE +
                savedTracks.size())
            {
                int index =
                    static_cast<int>(
                        id -
                        ID_DELETE_TRACK_BASE
                    );

                DeleteTrack(index);

                return 0;
            }

            // ------------------------------------------------
            // PLAY SAVED TRACK
            // ------------------------------------------------

            if (
                id >= ID_PLAY_TRACK_BASE &&
                id <
                ID_PLAY_TRACK_BASE +
                savedTracks.size())
            {
                int index =
                    static_cast<int>(
                        id -
                        ID_PLAY_TRACK_BASE
                    );

                PlaySavedTrack(index);

                return 0;
            }

            // ------------------------------------------------
            // LOAD SAVED TRACK
            // ------------------------------------------------

            if (
                id >= ID_LOAD_TRACK_BASE &&
                id <
                ID_LOAD_TRACK_BASE +
                savedTracks.size())
            {
                int index =
                    static_cast<int>(
                        id -
                        ID_LOAD_TRACK_BASE
                    );

                LoadTrack(index);

                return 0;
            }

            break;
        }

        // ====================================================
        // MOUSE WHEEL
        // ====================================================

        case WM_MOUSEWHEEL:
        {
            int delta =
                GET_WHEEL_DELTA_WPARAM(
                    wParam
                );

            if (delta > 0)
                scrollY -= 80;
            else
                scrollY += 80;

            RECT rect;

            GetClientRect(
                window,
                &rect
            );

            contentHeight =
                CalculateContentHeight(
                    rect
                );

            int maximum =
                contentHeight -
                rect.bottom;

            if (maximum < 0)
                maximum = 0;

            if (scrollY < 0)
                scrollY = 0;

            if (scrollY > maximum)
                scrollY = maximum;

            ResizeControls();

            InvalidateRect(
                window,
                nullptr,
                TRUE
            );

            return 0;
        }

        // ====================================================
        // VERTICAL SCROLL
        // ====================================================

        case WM_VSCROLL:
        {
            SCROLLINFO si = {};

            si.cbSize =
                sizeof(SCROLLINFO);

            si.fMask =
                SIF_ALL;

            GetScrollInfo(
                window,
                SB_VERT,
                &si
            );

            int newPos =
                si.nPos;

            switch (LOWORD(wParam))
            {
                case SB_LINEUP:
                    newPos -= 40;
                    break;

                case SB_LINEDOWN:
                    newPos += 40;
                    break;

                case SB_PAGEUP:
                    newPos -=
                        static_cast<int>(
                            si.nPage
                        );
                    break;

                case SB_PAGEDOWN:
                    newPos +=
                        static_cast<int>(
                            si.nPage
                        );
                    break;

                case SB_THUMBTRACK:
                    newPos =
                        si.nTrackPos;
                    break;

                case SB_TOP:
                    newPos =
                        si.nMin;
                    break;

                case SB_BOTTOM:
                    newPos =
                        si.nMax;
                    break;
            }

            int maximum =
                si.nMax -
                static_cast<int>(
                    si.nPage
                );

            if (maximum < 0)
                maximum = 0;

            if (newPos < 0)
                newPos = 0;

            if (newPos > maximum)
                newPos = maximum;

            scrollY =
                newPos;

            SetScrollPos(
                window,
                SB_VERT,
                scrollY,
                TRUE
            );

            ResizeControls();

            InvalidateRect(
                window,
                nullptr,
                TRUE
            );

            return 0;
        }

        // ====================================================
        // RESIZE
        // ====================================================

        case WM_SIZE:
        {
            ResizeControls();

            InvalidateRect(
                window,
                nullptr,
                TRUE
            );

            return 0;
        }

        // ====================================================
        // PAINT
        // ====================================================

        case WM_PAINT:
        {
            PAINTSTRUCT ps;

            HDC dc =
                BeginPaint(
                    window,
                    &ps
                );

            RECT rect;

            GetClientRect(
                window,
                &rect
            );

            HBRUSH backgroundBrush =
                CreateSolidBrush(
                    BACKGROUND
                );

            FillRect(
                dc,
                &rect,
                backgroundBrush
            );

            DeleteObject(
                backgroundBrush
            );

            // TOP BAR

            RECT topBar =
            {
                0,
                0,
                rect.right,
                175
            };

            HBRUSH topBrush =
                CreateSolidBrush(
                    RGB(10, 10, 14)
                );

            FillRect(
                dc,
                &topBar,
                topBrush
            );

            DeleteObject(
                topBrush
            );

            // EDITOR BORDER

            RECT editorArea =
            {
                LEFT_MARGIN - 3,
                EDITOR_TOP - scrollY - 3,
                rect.right -
                    LEFT_MARGIN +
                    3,
                EDITOR_TOP -
                    scrollY +
                    EDITOR_HEIGHT +
                    3
            };

            HPEN pen =
                CreatePen(
                    PS_SOLID,
                    2,
                    COLUMN_BORDER
                );

            HPEN oldPen =
                static_cast<HPEN>(
                    SelectObject(
                        dc,
                        pen
                    )
                );

            HBRUSH oldBrush =
            static_cast<HBRUSH>(
               SelectObject(
               dc,
               GetStockObject(
                NULL_BRUSH
               )
            )
            );

            Rectangle(
                dc,
                editorArea.left,
                editorArea.top,
                editorArea.right,
                editorArea.bottom
            );

            SelectObject(
                dc,
                oldBrush
            );

            SelectObject(
                dc,
                oldPen
            );

            DeleteObject(pen);

            // SAVED TRACKS LABEL

            SetTextColor(
                dc,
                TEXT_COLOR
            );

            SetBkMode(
                dc,
                TRANSPARENT
            );

            HFONT oldFont =
                static_cast<HFONT>(
                    SelectObject(
                        dc,
                        GetStockObject(
                            DEFAULT_GUI_FONT
                        )
                    )
                );

            TextOutA(
                dc,
                LEFT_MARGIN,
                TRACK_TOP -
                scrollY -
                28,
                "SAVED TRACKS",
                12
            );

            SelectObject(
                dc,
                oldFont
            );

            EndPaint(
                window,
                &ps
            );

            return 0;
        }

        // ====================================================
        // DESTROY
        // ====================================================

        case WM_DESTROY:
        {
            stopRequested = true;

            DestroyTrackButtons();

            if (editBrush)
            {
                DeleteObject(
                    editBrush
                );

                editBrush = nullptr;
            }

            PostQuitMessage(0);

            return 0;
        }
    }

    return DefWindowProcA(
        window,
        message,
        wParam,
        lParam
    );
}

// ============================================================
// CREATE BUTTON
// ============================================================

HWND CreateButton(
    const char* text,
    UINT id,
    HINSTANCE instance)
{
    return CreateWindowA(
        "BUTTON",
        text,
        WS_VISIBLE |
        WS_CHILD |
        BS_PUSHBUTTON,
        0,
        0,
        100,
        30,
        mainWindow,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(
                id
            )
        ),
        instance,
        nullptr
    );
}

// ============================================================
// MAIN
// ============================================================

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE,
    LPSTR,
    int)
{
    const char CLASS_NAME[] =
        "CppSongMakerOnePlayer";

    // ========================================================
    // EDIT BRUSH
    // ========================================================

    editBrush =
        CreateSolidBrush(
            EDIT_BACKGROUND
        );

    // ========================================================
    // WINDOW CLASS
    // ========================================================

    WNDCLASSA windowClass = {};

    windowClass.lpfnWndProc =
        WindowProcedure;

    windowClass.hInstance =
        instance;

    windowClass.lpszClassName =
        CLASS_NAME;

    windowClass.hbrBackground =
        CreateSolidBrush(
            BACKGROUND
        );

    windowClass.hCursor =
        LoadCursorA(
            nullptr,
            IDC_ARROW
        );

    RegisterClassA(
        &windowClass
    );

    // ========================================================
    // WINDOW
    // ========================================================

    mainWindow =
        CreateWindowExA(
            WS_EX_COMPOSITED,
            CLASS_NAME,
            "C++ Song Maker - One Player",
            WS_OVERLAPPEDWINDOW |
            WS_VSCROLL,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1100,
            850,
            nullptr,
            nullptr,
            instance,
            nullptr
        );

    if (!mainWindow)
        return 0;

    // ========================================================
    // TOP BUTTONS
    // ========================================================

    playButton =
        CreateButton(
            "PLAY",
            ID_PLAY,
            instance
        );

    loopButton =
        CreateButton(
            "LOOP: OFF",
            ID_LOOP,
            instance
        );

    boostButton =
        CreateButton(
            "BOOST: 1.0x",
            ID_BOOST,
            instance
        );

    instrumentsButton =
        CreateButton(
            "INSTRUMENTS",
            ID_INSTRUMENTS,
            instance
        );

    chordsButton =
        CreateButton(
            "CHORDS",
            ID_CHORDS,
            instance
        );

    drumsButton =
        CreateButton(
            "DRUMS",
            ID_DRUMS,
            instance
        );

    // ========================================================
    // TEMPO
    // ========================================================

    tempoMinusButton =
        CreateButton(
            "-",
            ID_TEMPO_MINUS,
            instance
        );

    tempoLabel =
        CreateWindowA(
            "STATIC",
            "TEMPO: 120",
            WS_VISIBLE |
            WS_CHILD |
            SS_CENTER |
            SS_CENTERIMAGE,
            0,
            0,
            120,
            28,
            mainWindow,
            nullptr,
            instance,
            nullptr
        );

    tempoPlusButton =
        CreateButton(
            "+",
            ID_TEMPO_PLUS,
            instance
        );

    // ========================================================
    // PITCH
    // ========================================================

    pitchMinusButton =
        CreateButton(
            "-",
            ID_PITCH_MINUS,
            instance
        );

    pitchLabel =
        CreateWindowA(
            "STATIC",
            "PITCH: 0",
            WS_VISIBLE |
            WS_CHILD |
            SS_CENTER |
            SS_CENTERIMAGE,
            0,
            0,
            120,
            28,
            mainWindow,
            nullptr,
            instance,
            nullptr
        );

    pitchPlusButton =
        CreateButton(
            "+",
            ID_PITCH_PLUS,
            instance
        );

    // ========================================================
    // SAVE
    // ========================================================

    saveTrackButton =
        CreateButton(
            "SAVE TRACK",
            ID_SAVE_TRACK,
            instance
        );

    // ========================================================
    // PLAY ALL
    // ========================================================

    playAllTracksButton =
        CreateButton(
            "PLAY ALL",
            ID_PLAY_ALL_TRACKS,
            instance
        );

    // ========================================================
    // EDITOR
    // ========================================================

    songEditor =
        CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            "",
            WS_VISIBLE |
            WS_CHILD |
            WS_VSCROLL |
            ES_MULTILINE |
            ES_AUTOVSCROLL |
            ES_WANTRETURN |
            ES_NOHIDESEL,
            0,
            0,
            500,
            EDITOR_HEIGHT,
            mainWindow,
            nullptr,
            instance,
            nullptr
        );

    SendMessageA(
        songEditor,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(
            GetStockObject(
                DEFAULT_GUI_FONT
            )
        ),
        TRUE
    );

    // ========================================================
    // INITIAL LAYOUT
    // ========================================================

    ResizeControls();

    // ========================================================
    // SHOW
    // ========================================================

    ShowWindow(
        mainWindow,
        SW_SHOW
    );

    UpdateWindow(
        mainWindow
    );

    // ========================================================
    // MESSAGE LOOP
    // ========================================================

    MSG message = {};

    while (
        GetMessageA(
            &message,
            nullptr,
            0,
            0
        ))
    {
        TranslateMessage(
            &message
        );

        DispatchMessageA(
            &message
        );

        if (
            !playing &&
            playButton)
        {
            SetWindowTextA(
                playButton,
                "PLAY"
            );

            if (playAllTracksButton)
            {
                SetWindowTextA(
                    playAllTracksButton,
                    "PLAY ALL"
                );
            }
        }
    }

    return 0;
}
