#ifndef MUSICXMLCONVERTER_H
#define MUSICXMLCONVERTER_H

#include <QString>
#include <QList>
#include <QCoreApplication>
#include <QHash>

class QIODevice;

// Reads a (partwise) MusicXML file as exported by Guitar Pro into a data model
// and generates the LaTeX snippet from it.
// Parsing and LaTeX generation are separated: generateLatex() is the ONLY
// place that knows the target LaTeX format - everything else is format-neutral.
class MusicXmlConverter
{
    Q_DECLARE_TR_FUNCTIONS(MusicXmlConverter)

public:
    /* ------------------------------------------------------------------
     *  Data model
     * ------------------------------------------------------------------ */

    // Guitar techniques from <notations><technical> / <articulations> / <ornaments>
    struct Technique {
        bool hammerOn  = false;     // <hammer-on type="start">
        bool pullOff   = false;     // <pull-off type="start">
        bool slide     = false;     // <slide> or <glissando> start
        bool harmonic  = false;     // <harmonic>
        double bend    = 0.0;       // <bend><bend-alter> in semitones (0 = none)
        bool vibrato   = false;     // <wavy-line> / <vibrato>
        bool staccato  = false;
        bool accent    = false;     // <accent/>  ">"
        bool marcato   = false;     // <strong-accent/>  "^" (Guitar Pro: "accentuated note")
        bool palmMute  = false;     // <other-technical>palm mute</other-technical> (text as exported)
        QString pluck;              // <pluck>: right-hand finger p / i / m / a (PIMA)
        QString fingering;          // <fingering>: left-hand finger 1-4 (0 = open, T = thumb)
        bool arpeggiate = false;    // <arpeggiate/>: rolled chord
    };

    // One syllable of a lyric line (<lyric number="n">)
    struct Lyric {
        int     verse = 1;          // number attribute (verse / line)
        QString syllabic;           // single, begin, middle, end
        QString text;
        bool    extend = false;     // <extend/>: melisma continues
    };

    struct Note {
        bool   rest   = false;
        bool   chord  = false;      // <chord/>: sounds together with the previous note
        bool   grace  = false;      // <grace/>: no duration
        QString step;               // C D E F G A B
        int    alter  = 0;          // -1 = flat, +1 = sharp
        int    octave = 0;
        int    duration = 0;        // in <divisions> units (0 for grace notes)
        int    position = 0;        // onset inside the measure, in <divisions> units
        QString type;               // whole, half, quarter, eighth, 16th, ...
        int    dots   = 0;
        int    voice  = 1;          // <voice>
        int    staff  = 1;          // <staff> (Guitar Pro may export notation + TAB staff)
        int    string = 0;          // guitar string (1 = high E), 0 = not set
        int    fret   = -1;         // -1 = not set
        bool   tieStart = false;    // <tie type="start">
        bool   tieStop  = false;    // <tie type="stop">
        bool   slurStart = false;   // <slur type="start">
        bool   slurStop  = false;   // <slur type="stop">
        int    tupletActual = 0;    // <time-modification><actual-notes> (e.g. 3 for triplets)
        int    tupletNormal = 0;    // <time-modification><normal-notes>
        bool   tupletStart = false; // <notations><tuplet type="start">
        bool   tupletStop  = false; // <notations><tuplet type="stop">
        bool   tupletBracket = false;   // bracket="yes"
        QString tupletPlacement;    // "above" / "below" / empty
        QString beam1;              // <beam number="1">: begin / continue / end (empty = not beamed)
        QString beam2;              // <beam number="2">: begin / continue / end / forward hook / backward hook
        Technique tech;
        QList<Lyric> lyrics;
    };

    // Chord symbol from <harmony> (with optional chord diagram <frame>)
    struct Harmony {
        int     position = 0;       // onset inside the measure, in <divisions> units
        QString root;               // C D E F G A B
        int     rootAlter = 0;
        QString kind;               // MusicXML kind value: major, minor, dominant, ...
        QString bass;               // bass note step (from <bass> or derived from the diagram), may be empty
        int     bassAlter = 0;
        int     firstFret = 1;      // diagram: <first-fret>
        QList<int> frets;           // diagram: index 0 = string 1 (high e) ... -1 = not played; empty = no diagram
        QString name() const;       // "Am", "C/G", "G7", ...
    };

    // Text or dynamics from <direction>
    struct DirectionText {
        int     position = 0;
        QString words;              // e.g. "Auftakt"
        QString dynamics;           // e.g. "mf"
    };

    struct Tuning {                 // one entry per <staff-tuning line="n">
        int line = 0;               // 1 = lowest line
        QString step;
        int alter = 0;
        int octave = 0;
    };

    struct Measure {
        QString number;
        // Attribute changes that START in this measure (0/empty = unchanged)
        int divisions  = 0;         // <divisions>
        int keyFifths  = 0;         // <key><fifths>
        bool hasKey    = false;
        QString keyMode;            // major / minor
        int beats      = 0;         // <time><beats>
        int beatType   = 0;         // <time><beat-type>
        double tempo   = 0.0;       // <sound tempo="...">
        bool repeatStart = false;   // <barline><repeat direction="forward">
        bool repeatEnd   = false;   // <barline><repeat direction="backward">
        int  repeatTimes = 0;       // <repeat times="n">
        QString ending;             // <ending number="1" type="start"> (volta), empty = none
        bool endingStop = false;    // <ending type="stop|discontinue">: the volta ends with this measure
        bool pickup = false;        // number "0" or implicit="yes" (Auftakt)
        bool newSystem = false;     // <print new-system="yes">: line break before this measure
        QList<Harmony> harmonies;   // chord symbols (duplicates of the 2nd staff removed)
        QList<DirectionText> texts; // words / dynamics (duplicates removed)
        struct LyricAt { int position = 0; Lyric lyric; };
        QList<LyricAt> lyrics;      // lyrics of ALL staves by position (Guitar Pro puts them on the notation staff)
        QList<Note> notes;          // ALL staves - use Part::primaryStaff() to pick one
    };

    struct Part {
        QString id;
        QString name;
        int staffLines = 0;         // <staff-details><staff-lines> (6 = guitar TAB)
        int capo       = 0;         // <staff-details><capo>
        QList<Tuning> tuning;       // lowest line first
        QHash<int, QString> clefs;  // staff number -> clef sign ("G", "TAB", ...)
        QHash<int, int> octaveChange; // staff number -> <transpose><octave-change> (notation is written 8va)
        QList<Measure> measures;

        int tabStaff() const;       // staff with TAB clef, 0 = none
        int primaryStaff() const;   // TAB staff if present (sounding pitch + string/fret), else 0 = all
        int staffCount() const { return qMax(1, int(clefs.size())); }
    };

    struct Score {
        QString title;
        QString composer;
        QString sourceFile;         // file name only
        QList<Part> parts;
        int noteCount() const;      // sounding notes (no rests) of each part's primary staff
        int measureCount() const;   // measures of the longest part
    };

    struct Result {
        bool    ok = false;
        QString latex;              // generated snippet (valid if ok)
        QString error;              // translated error message (valid if !ok)
        Score   score;
    };

    static Result convertFile(const QString &path);
    static Result convertDevice(QIODevice *device, const QString &sourceName);

    // MusiXTeX snippet (notation + TAB), see musixtexgenerator.cpp
    static QString generateLatex(const Score &score);
    // Commented overview of all parsed data (debugging the parser)
    static QString generateDataDump(const Score &score);

    // Helpers useful for any generator
    static QString pitchName(const Note &n);            // "F#3", "Bb2"
    static QString keyName(int fifths, const QString &mode);   // "G major", "E minor"
    static QString tuningName(const QList<Tuning> &t);  // "E2 A2 D3 G3 B3 E4"
    static QString stepName(const QString &step, int alter);   // "F#", "Bb"

    // Guitar Pro exports pull-offs as <hammer-on>H</hammer-on> too. A legato
    // start whose target note (next note on the same string, staff and voice)
    // has a LOWER fret is a pull-off. Called automatically after parsing.
    static void resolveLegato(Part &part);
    // Bass note of a chord diagram (lowest played string), used for "C/G"
    static void deriveBassFromFrame(Harmony &h, const QList<Tuning> &tuning);

private:
    static QString noteToString(const Note &n);
};

#endif // MUSICXMLCONVERTER_H
