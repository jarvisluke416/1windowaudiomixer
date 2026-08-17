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
HWND playAllButton = nullptr;
HWND loopButton = nullptr;
HWND boostButton = nullptr;

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
std::atomic_bool playingAll(false);

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
const UINT ID_PLAY_ALL = 1011;
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
            2.0 * PI * frequency * t
        ) *
        std::exp(-6.0 * t);

    double beater =
        std::sin(
            2.0 * PI * 95.0 * t
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
            2.0 * PI * 190.0 * t
        ) *
        std::exp(-19.0 * t);

    double ring =
        std::sin(
            2.0 * PI * 330.0 * t
        ) *
        std::exp(-27.0 * t);

    double attack =
        std::sin(
            2.0 * PI * 1700.0 * t
        ) *
        std::exp(-105.0 * t);

    return
        wire   * 0.68 +
        body   * 0.48 +
        ring   * 0.16 +
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
            2.0 * PI * pitch * t
        );

    double harmonic =
        std::sin(
            2.0 * PI * pitch * 1.98 * t
        ) * 0.16;

    double attack =
        std::sin(
            2.0 * PI * 700.0 * t
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
            2.0 * PI * 2800.0 * t
        ) * 0.22;

    double metallicTone =
        std::sin(
            2.0 * PI * 4700.0 * t
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
            2.0 * PI * 1250.0 * t
        ) *
        std::exp(-45.0 * t);

    double wood =
        std::sin(
            2.0 * PI * 420.0 * t
        ) *
        std::exp(-35.0 * t);

    double click =
        noise() *
        std::exp(-75.0 * t);

    return
        attack * 0.65 +
        wood   * 0.30 +
        click  * 0.20;
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
// GET EDITOR TEXT
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

// ============================================================
// SET EDITOR TEXT
// ============================================================

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

// ============================================================
// INSERT TEXT AT CARET
// ============================================================

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
// VOLUME MULTIPLIER
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
// PLAY ONE SONG
// ============================================================

void PlaySongText(
    const std::string& songText)
{
    if (playing)
        return;

    if (songText.empty())
        return;

    playing = true;
    playingAll = false;
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

    format.nSamplesPerSec =
        SAMPLE_RATE;

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

    std::vector<short> samples;

    double tempo =
        currentTempo.load();

    int pitch =
        currentPitch.load();

    double multiplier =
        GetVolumeMultiplier(
            volumeBoost.load()
        );

    if (!GenerateAudio(
        songText,
        tempo,
        pitch,
        multiplier,
        samples))
    {
        waveOutClose(audioDevice);
        playing = false;
        return;
    }

    WAVEHDR header = {};

    header.lpData =
        reinterpret_cast<LPSTR>(
            samples.data()
        );

    header.dwBufferLength =
        static_cast<DWORD>(
            samples.size() *
            sizeof(short)
        );

    if (
        waveOutPrepareHeader(
            audioDevice,
            &header,
            sizeof(WAVEHDR)
        ) != MMSYSERR_NOERROR)
    {
        waveOutClose(audioDevice);
        playing = false;
        return;
    }

    if (
        waveOutWrite(
            audioDevice,
            &header,
            sizeof(WAVEHDR)
        ) != MMSYSERR_NOERROR)
    {
        waveOutUnprepareHeader(
            audioDevice,
            &header,
            sizeof(WAVEHDR)
        );

        waveOutClose(audioDevice);
        playing = false;
        return;
    }

    while (!stopRequested)
    {
        if (header.dwFlags & WHDR_DONE)
            break;

        Sleep(1);
    }

    waveOutReset(audioDevice);

    waveOutUnprepareHeader(
        audioDevice,
        &header,
        sizeof(WAVEHDR)
    );

    waveOutClose(audioDevice);

    playing = false;
}

// ============================================================
// PLAY ALL SAVED TRACKS GAPLESS
// ============================================================

void PlayAllSavedTracks()
{
    if (playing)
        return;

    if (savedTracks.empty())
        return;

    playing = true;
    playingAll = true;
    stopRequested = false;

    // --------------------------------------------------------
    // Generate every track first.
    // --------------------------------------------------------

    std::vector<short> combinedSamples;

    double tempo =
        currentTempo.load();

    int pitch =
        currentPitch.load();

    double multiplier =
        GetVolumeMultiplier(
            volumeBoost.load()
        );

    for (
        size_t i = 0;
        i < savedTracks.size();
        ++i)
    {
        if (stopRequested)
            break;

        std::vector<short> trackSamples;

        if (!GenerateAudio(
            savedTracks[i],
            tempo,
            pitch,
            multiplier,
            trackSamples))
        {
            continue;
        }

        // ----------------------------------------------------
        // IMPORTANT:
        //
        // Track samples are appended directly.
        // There is NO silence inserted here.
        // ----------------------------------------------------

        combinedSamples.insert(
            combinedSamples.end(),
            trackSamples.begin(),
            trackSamples.end()
        );
    }

    if (
        combinedSamples.empty() ||
        stopRequested)
    {
        playing = false;
        playingAll = false;
        return;
    }

    // --------------------------------------------------------
    // OPEN ONE AUDIO DEVICE FOR THE ENTIRE SONG
    // --------------------------------------------------------

    WAVEFORMATEX format = {};

    format.wFormatTag =
        WAVE_FORMAT_PCM;

    format.nChannels = 1;

    format.nSamplesPerSec =
        SAMPLE_RATE;

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
        playingAll = false;
        return;
    }

    // --------------------------------------------------------
    // ONE CONTINUOUS BUFFER
    // --------------------------------------------------------

    WAVEHDR header = {};

    header.lpData =
        reinterpret_cast<LPSTR>(
            combinedSamples.data()
        );

    header.dwBufferLength =
        static_cast<DWORD>(
            combinedSamples.size() *
            sizeof(short)
        );

    if (
        waveOutPrepareHeader(
            audioDevice,
            &header,
            sizeof(WAVEHDR)
        ) != MMSYSERR_NOERROR)
    {
        waveOutClose(audioDevice);

        playing = false;
        playingAll = false;

        return;
    }

    if (
        waveOutWrite(
            audioDevice,
            &header,
            sizeof(WAVEHDR)
        ) != MMSYSERR_NOERROR)
    {
        waveOutUnprepareHeader(
            audioDevice,
            &header,
            sizeof(WAVEHDR)
        );

        waveOutClose(audioDevice);

        playing = false;
        playingAll = false;

        return;
    }

    // --------------------------------------------------------
    // WAIT UNTIL THE ENTIRE COMBINED SONG FINISHES
    // --------------------------------------------------------

    while (!stopRequested)
    {
        if (header.dwFlags & WHDR_DONE)
            break;

        Sleep(1);
    }

    // --------------------------------------------------------
    // STOP
    // --------------------------------------------------------

    waveOutReset(audioDevice);

    waveOutUnprepareHeader(
        audioDevice,
        &header,
        sizeof(WAVEHDR)
    );

    waveOutClose(audioDevice);

    playing = false;
    playingAll = false;
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
// PLAY ALL
// ============================================================

void StartPlayAll()
{
    if (savedTracks.empty())
    {
        MessageBoxA(
            mainWindow,
            "There are no saved tracks.",
            "PLAY ALL",
            MB_OK | MB_ICONINFORMATION
        );

        return;
    }

    if (playing)
    {
        stopRequested = true;
        return;
    }

    std::thread(
        PlayAllSavedTracks
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
// CHORD ROOTS
// ============================================================

std::vector<std::string> GetChordRoots()
{
    return
    {
        "C","C#","D","D#","E","F",
        "F#","G","G#","A","A#","B"
    };
}

// ============================================================
// CHORD TYPES
// ============================================================

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

// ============================================================
// NOTE FROM SEMITONE
// ============================================================

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

// ============================================================
// CHORD NOTES
// ============================================================

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
    {
        intervals = {0,4,7};
    }
    else if (type == "MINOR")
    {
        intervals = {0,3,7};
    }
    else if (type == "MINOR 7")
    {
        intervals = {0,3,7,10};
    }
    else if (type == "MAJOR 7")
    {
        intervals = {0,4,7,11};
    }

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

// ============================================================
// DRUM TYPE TO COMMAND
// ============================================================

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
// BUILD INSTRUMENT MENU
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
// BUILD CHORD MENU
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
// BUILD DRUM MENU
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
// SHOW INSTRUMENT SELECTOR
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

// ============================================================
// SHOW CHORD SELECTOR
// ============================================================

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

// ============================================================
// SHOW DRUM SELECTOR
// ============================================================

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
// CUSTOM DRAWING HELPERS
// ============================================================

void DrawButton(
    HDC dc,
    HWND hwnd,
    const char* text)
{
    RECT rect;

    GetClientRect(
        hwnd,
        &rect
    );

    // --------------------------------------------------------
    // Background
    // --------------------------------------------------------

    HBRUSH brush =
        CreateSolidBrush(
            COLUMN
        );

    FillRect(
        dc,
        &rect,
        brush
    );

    DeleteObject(
        brush
    );

    // --------------------------------------------------------
    // Border
    // --------------------------------------------------------

    HPEN pen =
        CreatePen(
            PS_SOLID,
            1,
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
        rect.left,
        rect.top,
        rect.right,
        rect.bottom
    );

    // Restore the original brush and pen.

    SelectObject(
        dc,
        oldBrush
    );

    SelectObject(
        dc,
        oldPen
    );

    DeleteObject(
        pen
    );

    // --------------------------------------------------------
    // Text
    // --------------------------------------------------------

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        BUTTON_TEXT
    );

    HFONT font =
        static_cast<HFONT>(
            GetStockObject(
                DEFAULT_GUI_FONT
            )
        );

    HFONT oldFont =
        static_cast<HFONT>(
            SelectObject(
                dc,
                font
            )
        );

    DrawTextA(
        dc,
        text,
        -1,
        &rect,
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE
    );

    SelectObject(
        dc,
        oldFont
    );
}

// ============================================================
// BUTTON WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK ButtonProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;

            HDC dc =
                BeginPaint(
                    hwnd,
                    &ps
                );

            char text[256];

            GetWindowTextA(
                hwnd,
                text,
                sizeof(text)
            );

            DrawButton(
                dc,
                hwnd,
                text
            );

            EndPaint(
                hwnd,
                &ps
            );

            return 0;
        }

        case WM_ERASEBKGND:
            return 1;
    }

    return DefWindowProcA(
        hwnd,
        message,
        wParam,
        lParam
    );
}

// ============================================================
// CREATE CUSTOM BUTTON
// ============================================================

HWND CreateCustomButton(
    const char* text,
    int x,
    int y,
    int width,
    int height,
    UINT id)
{
    return CreateWindowExA(
        0,
        "BUTTON",
        text,
        WS_CHILD |
        WS_VISIBLE |
        BS_OWNERDRAW,
        x,
        y,
        width,
        height,
        mainWindow,
        reinterpret_cast<HMENU>(
            static_cast<UINT_PTR>(id)
        ),
        GetModuleHandleA(nullptr),
        nullptr
    );
}

// ============================================================
// OWNER DRAW BUTTON
// ============================================================

void DrawOwnerButton(
    LPDRAWITEMSTRUCT dis)
{
    if (!dis)
        return;

    HDC dc =
        dis->hDC;

    RECT rect =
        dis->rcItem;

    bool pressed =
        (dis->itemState & ODS_SELECTED) != 0;

    COLORREF background =
        pressed
            ? RGB(45, 45, 55)
            : COLUMN;

    HBRUSH backgroundBrush =
        CreateSolidBrush(
            background
        );

    FillRect(
        dc,
        &rect,
        backgroundBrush
    );

    DeleteObject(
        backgroundBrush
    );

    // --------------------------------------------------------
    // Border
    // --------------------------------------------------------

    COLORREF borderColor =
        (dis->itemState & ODS_FOCUS)
            ? RGB(0, 255, 0)
            : COLUMN_BORDER;

    HPEN pen =
        CreatePen(
            PS_SOLID,
            1,
            borderColor
        );

    HPEN oldPen =
        static_cast<HPEN>(
            SelectObject(
                dc,
                pen
            )
        );

    // IMPORTANT:
    // NULL_BRUSH is selected separately.
    // Do NOT put the semicolon inside static_cast.

    HBRUSH nullBrush =
        static_cast<HBRUSH>(
            GetStockObject(
                NULL_BRUSH
            )
        );

    HBRUSH oldBrush =
        static_cast<HBRUSH>(
            SelectObject(
                dc,
                nullBrush
            )
        );

    Rectangle(
        dc,
        rect.left,
        rect.top,
        rect.right,
        rect.bottom
    );

    SelectObject(
        dc,
        oldBrush
    );

    SelectObject(
        dc,
        oldPen
    );

    DeleteObject(
        pen
    );

    // --------------------------------------------------------
    // Text
    // --------------------------------------------------------

    char text[256];

    GetWindowTextA(
        dis->hwndItem,
        text,
        sizeof(text)
    );

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        BUTTON_TEXT
    );

    HFONT font =
        static_cast<HFONT>(
            GetStockObject(
                DEFAULT_GUI_FONT
            )
        );

    HFONT oldFont =
        static_cast<HFONT>(
            SelectObject(
                dc,
                font
            )
        );

    if (pressed)
    {
        OffsetRect(
            &rect,
            1,
            1
        );
    }

    DrawTextA(
        dc,
        text,
        -1,
        &rect,
        DT_CENTER |
        DT_VCENTER |
        DT_SINGLELINE
    );

    SelectObject(
        dc,
        oldFont
    );
}

// ============================================================
// SAVE CURRENT TRACK
// ============================================================

void SaveCurrentTrack()
{
    std::string text =
        GetEditorText();

    if (text.empty())
    {
        MessageBoxA(
            mainWindow,
            "The editor is empty.",
            "SAVE TRACK",
            MB_OK | MB_ICONINFORMATION
        );

        return;
    }

    savedTracks.push_back(
        text
    );

    MessageBoxA(
        mainWindow,
        "Track saved.",
        "SAVE TRACK",
        MB_OK | MB_ICONINFORMATION
    );

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// LOAD TRACK
// ============================================================

void LoadTrack(
    size_t index)
{
    if (index >= savedTracks.size())
        return;

    SetEditorText(
        savedTracks[index]
    );

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// DELETE TRACK
// ============================================================

void DeleteTrack(
    size_t index)
{
    if (index >= savedTracks.size())
        return;

    savedTracks.erase(
        savedTracks.begin() +
        static_cast<std::ptrdiff_t>(
            index
        )
    );

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// TEMPO CONTROL
// ============================================================

void ChangeTempo(
    double amount)
{
    double tempo =
        currentTempo.load();

    tempo += amount;

    tempo =
        std::max(
            MIN_TEMPO,
            std::min(
                MAX_TEMPO,
                tempo
            )
        );

    currentTempo =
        tempo;

    UpdateTempoDisplay();
}

// ============================================================
// PITCH CONTROL
// ============================================================

void ChangePitch(
    int amount)
{
    int pitch =
        currentPitch.load();

    pitch += amount;

    pitch =
        std::max(
            MIN_PITCH,
            std::min(
                MAX_PITCH,
                pitch
            )
        );

    currentPitch =
        pitch;

    UpdatePitchDisplay();
}

// ============================================================
// BOOST CONTROL
// ============================================================

void ChangeBoost()
{
    int boost =
        volumeBoost.load();

    boost++;

    if (boost > 3)
        boost = 0;

    volumeBoost =
        boost;

    UpdateBoostDisplay();
}

// ============================================================
// TRACK BUTTON INFORMATION
// ============================================================

enum TrackButtonAction
{
    TRACK_LOAD = 0,
    TRACK_PLAY,
    TRACK_DELETE
};

UINT MakeTrackCommand(
    UINT base,
    size_t index)
{
    return
        base +
        static_cast<UINT>(
            index
        );
}

// ============================================================
// BUILD TRACK CONTROLS
// ============================================================

void RebuildTrackControls()
{
    // --------------------------------------------------------
    // Remove old track controls.
    //
    // IDs are in the track ranges, so scan child windows.
    // --------------------------------------------------------

    std::vector<HWND> children;

    HWND child =
        GetWindow(
            mainWindow,
            GW_CHILD
        );

    while (child)
    {
        HWND next =
            GetWindow(
                child,
                GW_HWNDNEXT
            );

        LONG_PTR id =
            GetWindowLongPtrA(
                child,
                GWLP_ID
            );

        if (
            (id >= ID_LOAD_TRACK_BASE &&
             id < ID_LOAD_TRACK_BASE + 1000)
            ||
            (id >= ID_PLAY_TRACK_BASE &&
             id < ID_PLAY_TRACK_BASE + 1000)
            ||
            (id >= ID_DELETE_TRACK_BASE &&
             id < ID_DELETE_TRACK_BASE + 1000)
        )
        {
            children.push_back(
                child
            );
        }

        child =
            next;
    }

    for (HWND hwnd : children)
    {
        DestroyWindow(
            hwnd
        );
    }

    // --------------------------------------------------------
    // Create controls for each saved track.
    // --------------------------------------------------------

    int y =
        TRACK_TOP -
        scrollY;

    for (
        size_t i = 0;
        i < savedTracks.size();
        ++i)
    {
        char label[64];

        std::snprintf(
            label,
            sizeof(label),
            "TRACK %d",
            static_cast<int>(
                i + 1
            )
        );

        HWND trackLabel =
            CreateWindowExA(
                0,
                "STATIC",
                label,
                WS_CHILD |
                WS_VISIBLE,
                LEFT_MARGIN,
                y,
                100,
                TRACK_BUTTON_HEIGHT,
                mainWindow,
                nullptr,
                GetModuleHandleA(nullptr),
                nullptr
            );

        SetTextColor(
            GetDC(trackLabel),
            TEXT_COLOR
        );

        CreateCustomButton(
            "LOAD",
            120,
            y,
            90,
            TRACK_BUTTON_HEIGHT,
            MakeTrackCommand(
                ID_LOAD_TRACK_BASE,
                i
            )
        );

        CreateCustomButton(
            "PLAY",
            218,
            y,
            90,
            TRACK_BUTTON_HEIGHT,
            MakeTrackCommand(
                ID_PLAY_TRACK_BASE,
                i
            )
        );

        CreateCustomButton(
            "DELETE",
            316,
            y,
            90,
            TRACK_BUTTON_HEIGHT,
            MakeTrackCommand(
                ID_DELETE_TRACK_BASE,
                i
            )
        );

        y +=
            TRACK_BUTTON_HEIGHT +
            TRACK_GAP;
    }

    contentHeight =
        std::max(
            900,
            y + 100
        );

    InvalidateRect(
        mainWindow,
        nullptr,
        TRUE
    );
}

// ============================================================
// CREATE MAIN CONTROLS
// ============================================================

void CreateMainControls()
{
    // --------------------------------------------------------
    // PLAY
    // --------------------------------------------------------

    playButton =
        CreateCustomButton(
            "PLAY",
            15,
            20,
            100,
            40,
            ID_PLAY
        );

    // --------------------------------------------------------
    // PLAY ALL
    // --------------------------------------------------------

    playAllButton =
        CreateCustomButton(
            "PLAY ALL",
            125,
            20,
            110,
            40,
            ID_PLAY_ALL
        );

    // --------------------------------------------------------
    // LOOP
    // --------------------------------------------------------

    loopButton =
        CreateCustomButton(
            "LOOP: OFF",
            245,
            20,
            110,
            40,
            ID_LOOP
        );

    // --------------------------------------------------------
    // BOOST
    // --------------------------------------------------------

    boostButton =
        CreateCustomButton(
            "BOOST: 1.0x",
            365,
            20,
            120,
            40,
            ID_BOOST
        );

    // --------------------------------------------------------
    // INSTRUMENTS
    // --------------------------------------------------------

    instrumentsButton =
        CreateCustomButton(
            "INSTRUMENTS",
            15,
            75,
            130,
            40,
            ID_INSTRUMENTS
        );

    // --------------------------------------------------------
    // CHORDS
    // --------------------------------------------------------

    chordsButton =
        CreateCustomButton(
            "CHORDS",
            155,
            75,
            110,
            40,
            ID_CHORDS
        );

    // --------------------------------------------------------
    // DRUMS
    // --------------------------------------------------------

    drumsButton =
        CreateCustomButton(
            "DRUMS",
            275,
            75,
            110,
            40,
            ID_DRUMS
        );

    // --------------------------------------------------------
    // TEMPO
    // --------------------------------------------------------

    tempoMinusButton =
        CreateCustomButton(
            "-",
            405,
            75,
            40,
            40,
            ID_TEMPO_MINUS
        );

    tempoPlusButton =
        CreateCustomButton(
            "+",
            495,
            75,
            40,
            40,
            ID_TEMPO_PLUS
        );

    tempoLabel =
        CreateWindowExA(
            0,
            "STATIC",
            "TEMPO: 120",
            WS_CHILD |
            WS_VISIBLE |
            SS_CENTER,
            445,
            75,
            50,
            40,
            mainWindow,
            nullptr,
            GetModuleHandleA(nullptr),
            nullptr
        );

    // --------------------------------------------------------
    // PITCH
    // --------------------------------------------------------

    pitchMinusButton =
        CreateCustomButton(
            "-",
            405,
            125,
            40,
            40,
            ID_PITCH_MINUS
        );

    pitchPlusButton =
        CreateCustomButton(
            "+",
            495,
            125,
            40,
            40,
            ID_PITCH_PLUS
        );

    pitchLabel =
        CreateWindowExA(
            0,
            "STATIC",
            "PITCH: 0",
            WS_CHILD |
            WS_VISIBLE |
            SS_CENTER,
            445,
            125,
            50,
            40,
            mainWindow,
            nullptr,
            GetModuleHandleA(nullptr),
            nullptr
        );

    // --------------------------------------------------------
    // SAVE
    // --------------------------------------------------------

    saveTrackButton =
        CreateCustomButton(
            "SAVE TRACK",
            560,
            20,
            130,
            40,
            ID_SAVE_TRACK
        );

    // --------------------------------------------------------
    // EDITOR
    // --------------------------------------------------------

    songEditor =
        CreateWindowExA(
            WS_EX_CLIENTEDGE,
            "EDIT",
            "",
            WS_CHILD |
            WS_VISIBLE |
            WS_VSCROLL |
            WS_HSCROLL |
            ES_MULTILINE |
            ES_AUTOVSCROLL |
            ES_AUTOHSCROLL |
            ES_WANTRETURN,
            LEFT_MARGIN,
            EDITOR_TOP,
            900,
            EDITOR_HEIGHT,
            mainWindow,
            nullptr,
            GetModuleHandleA(nullptr),
            nullptr
        );

    // --------------------------------------------------------
    // EDITOR FONT
    // --------------------------------------------------------

    HFONT editorFont =
        CreateFontA(
            16,
            0,
            0,
            0,
            FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            ANSI_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            FIXED_PITCH |
            FF_MODERN,
            "Consolas"
        );

    SendMessage(
        songEditor,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(
            editorFont
        ),
        TRUE
    );

    // --------------------------------------------------------
    // EDITOR BRUSH
    // --------------------------------------------------------

    editBrush =
        CreateSolidBrush(
            EDIT_BACKGROUND
        );

    UpdateTempoDisplay();
    UpdatePitchDisplay();
    UpdateBoostDisplay();

    RebuildTrackControls();
}

// ============================================================
// MAIN WINDOW PAINT
// ============================================================

void PaintMainWindow(
    HWND hwnd,
    HDC dc)
{
    RECT rect;

    GetClientRect(
        hwnd,
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

    // --------------------------------------------------------
    // Title
    // --------------------------------------------------------

    SetBkMode(
        dc,
        TRANSPARENT
    );

    SetTextColor(
        dc,
        RGB(0, 0, 0)
    );

    HFONT titleFont =
        CreateFontA(
            24,
            0,
            0,
            0,
            FW_BOLD,
            FALSE,
            FALSE,
            FALSE,
            ANSI_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH,
            "Arial"
        );

    HFONT oldFont =
        static_cast<HFONT>(
            SelectObject(
                dc,
                titleFont
            )
        );

    TextOutA(
        dc,
        15,
        145,
        "MUSIC TRACK EDITOR",
        -1
    );

    SelectObject(
        dc,
        oldFont
    );

    DeleteObject(
        titleFont
    );

    // --------------------------------------------------------
    // Saved tracks title
    // --------------------------------------------------------

    HFONT normalFont =
        static_cast<HFONT>(
            GetStockObject(
                DEFAULT_GUI_FONT
            )
        );

    oldFont =
        static_cast<HFONT>(
            SelectObject(
                dc,
                normalFont
            )
        );

    TextOutA(
        dc,
        LEFT_MARGIN,
        TRACK_TOP - 30 - scrollY,
        "SAVED TRACKS",
        -1
    );

    SelectObject(
        dc,
        oldFont
    );
}

// ============================================================
// MAIN WINDOW PROCEDURE
// ============================================================

LRESULT CALLBACK MainWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
        // ----------------------------------------------------
        // CREATE
        // ----------------------------------------------------

        case WM_CREATE:
        {
            mainWindow =
                hwnd;

            CreateMainControls();

            return 0;
        }

        // ----------------------------------------------------
        // PAINT
        // ----------------------------------------------------

        case WM_PAINT:
        {
            PAINTSTRUCT ps;

            HDC dc =
                BeginPaint(
                    hwnd,
                    &ps
                );

            PaintMainWindow(
                hwnd,
                dc
            );

            EndPaint(
                hwnd,
                &ps
            );

            return 0;
        }

        // ----------------------------------------------------
        // EDIT CONTROL COLORS
        // ----------------------------------------------------

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

            if (!editBrush)
            {
                editBrush =
                    CreateSolidBrush(
                        EDIT_BACKGROUND
                    );
            }

            return reinterpret_cast<LRESULT>(
                editBrush
            );
        }

        // ----------------------------------------------------
        // STATIC CONTROL COLORS
        // ----------------------------------------------------

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

            HBRUSH brush =
                CreateSolidBrush(
                    BACKGROUND
                );

            return reinterpret_cast<LRESULT>(
                brush
            );
        }

        // ----------------------------------------------------
        // OWNER DRAW
        // ----------------------------------------------------

        case WM_DRAWITEM:
        {
            DrawOwnerButton(
                reinterpret_cast<
                    LPDRAWITEMSTRUCT
                >(lParam)
            );

            return TRUE;
        }

        // ----------------------------------------------------
        // COMMANDS
        // ----------------------------------------------------

        case WM_COMMAND:
        {
            UINT id =
                LOWORD(wParam);

            // ------------------------------------------------
            // PLAY
            // ------------------------------------------------

            if (id == ID_PLAY)
            {
                PlayEditor();
                return 0;
            }

            // ------------------------------------------------
            // PLAY ALL
            // ------------------------------------------------

            if (id == ID_PLAY_ALL)
            {
                StartPlayAll();
                return 0;
            }

            // ------------------------------------------------
            // LOOP
            // ------------------------------------------------

            if (id == ID_LOOP)
            {
                bool newValue =
                    !looping.load();

                looping =
                    newValue;

                SetWindowTextA(
                    loopButton,
                    newValue
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
                ChangeBoost();
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
            // TEMPO
            // ------------------------------------------------

            if (id == ID_TEMPO_MINUS)
            {
                ChangeTempo(
                    -TEMPO_STEP
                );

                return 0;
            }

            if (id == ID_TEMPO_PLUS)
            {
                ChangeTempo(
                    TEMPO_STEP
                );

                return 0;
            }

            // ------------------------------------------------
            // PITCH
            // ------------------------------------------------

            if (id == ID_PITCH_MINUS)
            {
                ChangePitch(
                    -PITCH_STEP
                );

                return 0;
            }

            if (id == ID_PITCH_PLUS)
            {
                ChangePitch(
                    PITCH_STEP
                );

                return 0;
            }

            // ------------------------------------------------
            // SAVE
            // ------------------------------------------------

            if (id == ID_SAVE_TRACK)
            {
                SaveCurrentTrack();

                RebuildTrackControls();

                return 0;
            }

            // ------------------------------------------------
            // LOAD TRACK
            // ------------------------------------------------

            if (
                id >= ID_LOAD_TRACK_BASE &&
                id < ID_LOAD_TRACK_BASE + 1000)
            {
                size_t index =
                    static_cast<size_t>(
                        id -
                        ID_LOAD_TRACK_BASE
                    );

                LoadTrack(
                    index
                );

                return 0;
            }

            // ------------------------------------------------
            // PLAY TRACK
            // ------------------------------------------------

            if (
                id >= ID_PLAY_TRACK_BASE &&
                id < ID_PLAY_TRACK_BASE + 1000)
            {
                size_t index =
                    static_cast<size_t>(
                        id -
                        ID_PLAY_TRACK_BASE
                    );

                if (
                    index <
                    savedTracks.size()
                )
                {
                    if (!playing)
                    {
                        std::thread(
                            PlaySongText,
                            savedTracks[index]
                        ).detach();
                    }
                }

                return 0;
            }

            // ------------------------------------------------
            // DELETE TRACK
            // ------------------------------------------------

            if (
                id >= ID_DELETE_TRACK_BASE &&
                id < ID_DELETE_TRACK_BASE + 1000)
            {
                size_t index =
                    static_cast<size_t>(
                        id -
                        ID_DELETE_TRACK_BASE
                    );

                DeleteTrack(
                    index
                );

                RebuildTrackControls();

                return 0;
            }

            // ------------------------------------------------
            // INSERT NOTE
            // ------------------------------------------------

            auto noteIt =
                noteCommands.find(id);

            if (
                noteIt !=
                noteCommands.end())
            {
                InsertEditorText(
                    noteIt->second +
                    "\r\n"
                );

                return 0;
            }

            // ------------------------------------------------
            // INSERT CHORD
            // ------------------------------------------------

            auto chordIt =
                chordCommands.find(id);

            if (
                chordIt !=
                chordCommands.end())
            {
                InsertEditorText(
                    chordIt->second
                );

                return 0;
            }

            // ------------------------------------------------
            // INSERT DRUM
            // ------------------------------------------------

            auto drumIt =
                drumCommands.find(id);

            if (
                drumIt !=
                drumCommands.end())
            {
                InsertEditorText(
                    drumIt->second +
                    "\r\n"
                );

                return 0;
            }

            break;
        }

        // ----------------------------------------------------
        // KEYBOARD
        // ----------------------------------------------------

        case WM_KEYDOWN:
        {
            if (wParam == VK_ESCAPE)
            {
                stopRequested =
                    true;

                return 0;
            }

            break;
        }

        // ----------------------------------------------------
        // MOUSE WHEEL
        // ----------------------------------------------------

        case WM_MOUSEWHEEL:
        {
            int delta =
                GET_WHEEL_DELTA_WPARAM(
                    wParam
                );

            if (delta > 0)
            {
                scrollY -= 40;
            }
            else
            {
                scrollY += 40;
            }

            if (scrollY < 0)
                scrollY = 0;

            int maxScroll =
                std::max(
                    0,
                    contentHeight -
                    800
                );

            if (scrollY > maxScroll)
                scrollY = maxScroll;

            RebuildTrackControls();

            return 0;
        }

        // ----------------------------------------------------
        // DESTROY
        // ----------------------------------------------------

        case WM_DESTROY:
        {
            stopRequested =
                true;

            if (editBrush)
            {
                DeleteObject(
                    editBrush
                );

                editBrush =
                    nullptr;
            }

            PostQuitMessage(
                0
            );

            return 0;
        }
    }

    return DefWindowProcA(
        hwnd,
        message,
        wParam,
        lParam
    );
}

// ============================================================
// REGISTER WINDOW CLASS
// ============================================================

bool RegisterMainWindowClass(
    HINSTANCE instance)
{
    WNDCLASSEXA wc = {};

    wc.cbSize =
        sizeof(WNDCLASSEXA);

    wc.style =
        CS_HREDRAW |
        CS_VREDRAW;

    wc.lpfnWndProc =
        MainWindowProc;

    wc.hInstance =
        instance;

    wc.hCursor =
        LoadCursor(
            nullptr,
            IDC_ARROW
        );

    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            GetStockObject(
                NULL_BRUSH
            )
        );

    wc.lpszClassName =
        "MusicTrackEditorWindow";

    return
        RegisterClassExA(
            &wc
        ) != 0;
}

// ============================================================
// DEFAULT SONG
// ============================================================

const char* DEFAULT_SONG =
"TRACK 1 TEMPO 120\r\n"
"LENGTH 8\r\n"
"GUITAR C4 0 1\r\n"
"GUITAR E4 0 1\r\n"
"GUITAR G4 0 1\r\n"
"DRUM KICK 0\r\n"
"DRUM SNARE 2\r\n"
"GUITAR F4 4 1\r\n"
"GUITAR A4 4 1\r\n"
"GUITAR C5 4 1\r\n"
"DRUM KICK 4\r\n";

// ============================================================
// WINMAIN
// ============================================================

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE,
    LPSTR,
    int showCommand)
{
    if (!RegisterMainWindowClass(
            instance))
    {
        MessageBoxA(
            nullptr,
            "Could not register the window.",
            "Error",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    mainWindow =
        CreateWindowExA(
            0,
            "MusicTrackEditorWindow",
            "Music Track Editor",
            WS_OVERLAPPEDWINDOW |
            WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1000,
            850,
            nullptr,
            nullptr,
            instance,
            nullptr
        );

    if (!mainWindow)
    {
        MessageBoxA(
            nullptr,
            "Could not create the main window.",
            "Error",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    ShowWindow(
        mainWindow,
        showCommand
    );

    UpdateWindow(
        mainWindow
    );

    SetEditorText(
        DEFAULT_SONG
    );

    MSG message;

    while (
        GetMessageA(
            &message,
            nullptr,
            0,
            0
        ) > 0)
    {
        TranslateMessage(
            &message
        );

        DispatchMessageA(
            &message
        );
    }

    return static_cast<int>(
        message.wParam
    );
}
