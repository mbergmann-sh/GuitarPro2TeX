// musixtexgenerator.cpp - MusicXmlConverter::generateLatex()
//
// Generates a MusiXTeX snippet (standard notation + TAB) from the parsed Score.
// The snippet is a complete \begin{music}...\end{music} block; the document
// needs \usepackage{xcolor} and \usepackage{musixtex} (pdflatex, one pass).
//
// Layout (like Guitar Pro):   instrument 2 = notation, treble clef with 8 below
//                             instrument 1 = 6-line TAB staff (MusiXTeX counts bottom-up)

#include "musicxmlconverter.h"

#include <QStringList>
#include <QMap>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <deque>
#include <algorithm>
#include <cmath>

namespace {

using Note    = MusicXmlConverter::Note;
using Measure = MusicXmlConverter::Measure;
using Part    = MusicXmlConverter::Part;
using Harmony = MusicXmlConverter::Harmony;

constexpr int MIDDLE_LINE = 34;     // B4 = diatonic index of the middle staff line (treble)

// Distance of the TAB lines relative to a normal staff. MusiXTeX's largest predefined
// size is \Largevalue (1.44); \setsize accepts any factor. 1.75 keeps even five stacked,
// parenthesised fret numbers (held chord) clearly apart. Candidate for a later preset.
constexpr const char *TAB_SIZE = "1.75";

// Rhythm below the TAB (stems, flags, beams, dots, tuplets - as in Guitar Pro).
// Candidate for a later preset.
constexpr bool TAB_RHYTHM = true;
constexpr int  RHYTHM_TOP = -2;             // stems start here (TAB internotes, 0 = bottom line)
constexpr int  RHYTHM_DOT = -8;             // augmentation dot near the stem end
constexpr int  RHYTHM_TUPLET = -10;         // tuplet bracket below the rhythm beams
constexpr int  RHYTHM_PIMA = -14;           // first PIMA row when the rhythm is shown

int stepIndex(const QString &step)
{
    static const QString order = QStringLiteral("CDEFGAB");
    const int i = order.indexOf(step);
    return i < 0 ? 0 : i;
}

// Diatonic index: C4 = 28, each step +1, each octave +7
int diatonicIndex(const QString &step, int octave)
{
    return octave * 7 + stepIndex(step);
}

// MusiXTeX pitch letter: A1..G3 = 'A'..'N' (12..25), A3.. = 'a'..'z' (26..51)
QString pitchLetter(int d)
{
    d = qBound(12, d, 51);
    return d >= 26 ? QString(QChar('a' + (d - 26))) : QString(QChar('A' + (d - 12)));
}

// Alteration of a step in a key signature
int keyAlter(int fifths, int step)
{
    static const int sharpOrder[7] = { 3, 0, 4, 1, 5, 2, 6 };   // F C G D A E B
    static const int flatOrder[7]  = { 6, 2, 5, 1, 4, 0, 3 };   // B E A D G C F
    for (int i = 0; i < qMin(7, fifths); ++i)
        if (sharpOrder[i] == step) return 1;
    for (int i = 0; i < qMin(7, -fifths); ++i)
        if (flatOrder[i] == step) return -1;
    return 0;
}

QString accidentalPrefix(int alter)
{
    switch (alter) {
    case  2: return QStringLiteral(">");
    case  1: return QStringLiteral("^");
    case  0: return QStringLiteral("=");
    case -1: return QStringLiteral("_");
    case -2: return QStringLiteral("<");
    }
    return QString();
}

// Guitar Pro does not export "triplet feel" to MusicXML. A text (<words>) that
// consists of one of these keywords is typeset as the swing symbol instead.
bool isSwingText(const QString &words)
{
    static const QStringList keys = {
        QStringLiteral("swing"), QStringLiteral("swing feel"), QStringLiteral("shuffle"),
        QStringLiteral("triplet feel"), QStringLiteral("triolenfeeling"), QStringLiteral("triolen-feeling"),
        QStringLiteral("triolen feeling")
    };
    return keys.contains(words.trimmed().toLower());
}

// LaTeX-safe text (chord names, words)
QString texEscape(QString s)
{
    s.replace(QLatin1Char('\\'), QStringLiteral("\\textbackslash{}"));
    s.replace(QLatin1Char('#'), QStringLiteral("\\#"));
    s.replace(QLatin1Char('&'), QStringLiteral("\\&"));
    s.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    s.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    s.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    s.replace(QLatin1Char('{'), QStringLiteral("\\{"));
    s.replace(QLatin1Char('}'), QStringLiteral("\\}"));
    return s;
}

// Chord name for text: "C#m" -> "C$\sharp$m", "Bb7" -> "B$\flat$7"
QString chordText(const Harmony &h)
{
    QString s = texEscape(h.name());
    // accidentals directly after a note letter (root or bass)
    static const QRegularExpression sharpRe(QStringLiteral("([A-G])\\\\#"));
    static const QRegularExpression flatRe(QStringLiteral("([A-G])b"));
    s.replace(sharpRe, QStringLiteral("\\1$\\sharp$"));
    s.replace(flatRe, QStringLiteral("\\1$\\flat$"));
    return s;
}

// gchords diagram:  \chord{<modifier>}{<low E>,...,<high e>}{<name>}
//   o = open string, x = not played, pN = dot on fret N (relative to the diagram top)
// Harmony::frets has index 0 = string 1 (high e), so the order is reversed here.
QString gchordsDiagram(const Harmony &h)
{
    int lowest = 99, highest = 0;
    for (int f : h.frets)
        if (f > 0) { lowest = qMin(lowest, f); highest = qMax(highest, f); }
    // Diagram starts at <first-fret>; if the chord would not fit into 5 frets, start at its lowest fret
    int top = qMax(1, h.firstFret);
    if (highest > 0 && highest - top + 1 > 5)
        top = lowest;
    QStringList pos;
    for (int i = h.frets.size() - 1; i >= 0; --i) {
        const int f = h.frets.at(i);
        if (f < 0)       pos << QStringLiteral("x");
        else if (f == 0) pos << QStringLiteral("o");
        else             pos << QStringLiteral("p%1").arg(f - top + 1);
    }
    // modifier: "t" = thick nut line (open position), otherwise the fret number at the side
    const QString mod = (top <= 1) ? QStringLiteral("t") : QString::number(top);
    return QStringLiteral("\\chord{%1}{%2}{%3}").arg(mod, pos.join(QLatin1Char(',')), chordText(h));
}

// All notes of ONE voice that start at the same time (a chord, a single note or a rest)
struct VEvent {
    int voice = 1;
    int pos = 0;                    // onset in divisions
    int dur = 0;                    // duration of the main note in divisions
    QList<const Note *> notes;      // file order (chord members after their main note)
    bool rest = true;               // only rests
    bool grace = false;             // grace note(s), rendered in a small block before the onset
    QString beam1, beam2;           // beam state of this event
    const Note *main = nullptr;     // first note (carries type/dots)
};

struct Written {                    // a note as it is written in the notation staff
    const Note *note = nullptr;
    int index = 0;                  // diatonic index of the written pitch
};

QString spacingMacro(double quarters)
{
    if (quarters >= 4.0) return QStringLiteral("\\NOTEs");
    if (quarters >= 2.0) return QStringLiteral("\\NOTes");
    if (quarters >= 1.0) return QStringLiteral("\\NOtes");
    if (quarters >= 0.5) return QStringLiteral("\\Notes");
    return QStringLiteral("\\notes");
}

QString restMacro(const QString &type, int dots)
{
    QString m;
    if (type == QLatin1String("whole") || type.isEmpty()) m = QStringLiteral("\\pause");
    else if (type == QLatin1String("half"))    m = QStringLiteral("\\hpause");
    else if (type == QLatin1String("quarter")) m = QStringLiteral("\\qp");
    else if (type == QLatin1String("eighth"))  m = QStringLiteral("\\ds");
    else if (type == QLatin1String("16th"))    m = QStringLiteral("\\qs");
    else                                        m = QStringLiteral("\\hs");  // 32nd and shorter
    if (dots > 0) m += QLatin1Char('p');
    return m;
}

// Unbeamed note with stem (stem note of a chord or single note)
QString noteMacro(const QString &type, int dots, bool up)
{
    QString m;
    if (type == QLatin1String("whole"))        m = QStringLiteral("\\wh");
    else if (type == QLatin1String("half"))    m = up ? QStringLiteral("\\hu") : QStringLiteral("\\hl");
    else if (type == QLatin1String("quarter")) m = up ? QStringLiteral("\\qu") : QStringLiteral("\\ql");
    else if (type == QLatin1String("eighth"))  m = up ? QStringLiteral("\\cu") : QStringLiteral("\\cl");
    else if (type == QLatin1String("16th"))    m = up ? QStringLiteral("\\ccu") : QStringLiteral("\\ccl");
    else                                        m = up ? QStringLiteral("\\cccu") : QStringLiteral("\\cccl");
    if (dots > 0) m += QLatin1Char('p');
    return m;
}

// Note with stem that does NOT advance (for all voices but the last one at an onset)
QString zNoteMacro(const QString &type, int dots, bool up)
{
    QString m;
    if (type == QLatin1String("whole"))        m = QStringLiteral("\\zw");
    else if (type == QLatin1String("half"))    m = up ? QStringLiteral("\\zhu") : QStringLiteral("\\zhl");
    else if (type == QLatin1String("quarter")) m = up ? QStringLiteral("\\zqu") : QStringLiteral("\\zql");
    else if (type == QLatin1String("eighth"))  m = up ? QStringLiteral("\\zcu") : QStringLiteral("\\zcl");
    else if (type == QLatin1String("16th"))    m = up ? QStringLiteral("\\zccu") : QStringLiteral("\\zccl");
    else                                        m = up ? QStringLiteral("\\zcccu") : QStringLiteral("\\zcccl");
    if (dots > 0) m += QLatin1Char('p');
    return m;
}

// Chord member without stem
QString headMacro(const QString &type, int dots)
{
    QString m;
    if (type == QLatin1String("whole"))     m = QStringLiteral("\\zw");
    else if (type == QLatin1String("half")) m = QStringLiteral("\\zh");
    else                                     m = QStringLiteral("\\zq");
    if (dots > 0) m += QLatin1Char('p');
    return m;
}

// Small id pool for MusiXTeX slurs/ties (default \maxslurs = 6)
struct IdPool {
    QList<bool> used = QList<bool>(6, false);
    int take() { for (int i = 0; i < used.size(); ++i) if (!used[i]) { used[i] = true; return i; } return -1; }
    void give(int id) { if (id >= 0 && id < used.size()) used[id] = false; }
};

} // namespace


QString MusicXmlConverter::generateLatex(const Score &score)
{
    QStringList out;

    // Chord diagrams used in the piece (first part), in order of appearance, each voicing once
    QStringList diagrams;
    if (!score.parts.isEmpty())
        for (const Measure &m : score.parts.first().measures)
            for (const Harmony &h : m.harmonies)
                if (!h.frets.isEmpty()) {
                    const QString d = gchordsDiagram(h);
                    if (!diagrams.contains(d))
                        diagrams << d;
                }

    // ---------------------------------------------------------------- header
    const QString rule = QStringLiteral("% ") + QString(68, QLatin1Char('-'));
    out << rule
        << QStringLiteral("% %1").arg(score.title.isEmpty() ? score.sourceFile : score.title)
        << QStringLiteral("% Generated by GuitarPROtoTeX-Convert from %1").arg(score.sourceFile)
        << QStringLiteral("%")
        << QStringLiteral("% Required packages (preamble):")
        << QStringLiteral("%   \\usepackage{xcolor}     % white background behind the TAB fret numbers")
        << QStringLiteral("%   \\usepackage{musixtex}   % notation + TAB");
    if (!diagrams.isEmpty())
        out << QStringLiteral("%   \\usepackage{gchords}    % chord diagrams (\\chord)");
    out << QStringLiteral("%")
        << QStringLiteral("% Typeset:  musixtex -l -p doc.tex")
        << QStringLiteral("%   (= pdflatex doc; musixflx doc; pdflatex doc - justified lines;")
        << QStringLiteral("%    a single pdflatex run works too, but without justification)")
        << QStringLiteral("%   In a book with several snippets: run musixflx on the MAIN file (one .mx1 for all).")
        << rule;

    if (!diagrams.isEmpty()) {
        // \leavevmode: otherwise the first \chord (a \vbox) sits alone in vertical mode.
        // \par afterwards: MusiXTeX must start in a fresh paragraph, or
        // musixflx gets the width of the first music line wrong.
        out << QStringLiteral("% Chord diagrams")
            << QStringLiteral("\\begin{center}\\leavevmode")
            << diagrams.join(QStringLiteral("\\hspace{1.5em}\n"))
            << QStringLiteral("\\end{center}")
            << QStringLiteral("\\par");
    }

    if (score.parts.isEmpty()) {
        out << QStringLiteral("% (no parts found)");
        return out.join(QLatin1Char('\n'));
    }
    if (score.parts.size() > 1)
        out << QStringLiteral("% NOTE: %1 parts found, only the first one is typeset.").arg(score.parts.size());

    const Part &part = score.parts.first();

    // Which staff to render: TAB staff (sounding pitch + string/fret) if present
    const int staff = part.primaryStaff();
    // Guitar notation is written one octave higher than it sounds. Guitar Pro's
    // notation staff says octave-change -1; the TAB staff holds sounding pitches.
    const bool hasTab = (staff > 0) || [&] {
        for (const Measure &m : part.measures)
            for (const Note &n : m.notes)
                if (n.string > 0 && n.fret >= 0) return true;
        return false;
    }();
    // Written pitch of the notation staff:
    //  - the file has a notation staff (clef != TAB): follow its <transpose><octave-change>
    //    (Guitar Pro guitar tracks: -1 -> written one octave higher, treble clef with 8 below;
    //     no transposition, e.g. a "Singer" track: written as is, plain treble clef)
    //  - TAB only / no clef information: guitar convention, one octave higher
    int notationStaff = 0;
    for (auto it = part.clefs.cbegin(); it != part.clefs.cend(); ++it)
        if (it.value() != QLatin1String("TAB") && (notationStaff == 0 || it.key() < notationStaff))
            notationStaff = it.key();
    int writtenShift = hasTab ? 1 : 0;
    if (notationStaff > 0 && staff > 0)
        writtenShift = -part.octaveChange.value(notationStaff, 0);
    else
        for (auto it = part.octaveChange.cbegin(); it != part.octaveChange.cend(); ++it)
            if (it.key() != staff)
                writtenShift = -it.value();
    const bool octaveClef = (writtenShift == 1);

    // ------------------------------------------------------------ first attributes
    int divisions = 1, fifths = 0, beats = 4, beatType = 4;
    for (const Measure &m : part.measures) {
        if (m.divisions > 0) { divisions = m.divisions; break; }
    }
    if (!part.measures.isEmpty()) {
        const Measure &m0 = part.measures.first();
        if (m0.hasKey)    fifths = m0.keyFifths;
        if (m0.beats > 0) { beats = m0.beats; beatType = m0.beatType; }
    }
    const bool anySystemBreak = std::any_of(part.measures.cbegin(), part.measures.cend(),
                                            [](const Measure &m) { return m.newSystem; });

    // --------------------------------------------------------------- set-up
    out << QStringLiteral("\\begin{music}")
        << QStringLiteral("% MusiXTeX uses the old font commands (\\it for bar numbers ...), which KOMA-Script")
        << QStringLiteral("% classes (scrbook, scrartcl, ...) forbid. Map them to NFSS locally (no effect in book):")
        << QStringLiteral("\\def\\it{\\itshape}\\def\\bf{\\bfseries}\\def\\rm{\\rmfamily}\\def\\sl{\\slshape}\\def\\sf{\\sffamily}\\def\\tt{\\ttfamily}\\def\\sc{\\scshape}")
        << QStringLiteral("\\makeatletter")
        << QStringLiteral("% pdflatex ignores the PostScript \"setgray\" specials in MusiXTeX's \\tabbox:")
        << QStringLiteral("% draw the white background behind the fret numbers with \\color instead")
        << QStringLiteral("\\def\\tabbox#1#2{\\setbox0=\\hbox{\\tabfnt #2}\\stringraise\\nblines")
        << QStringLiteral("  \\advance\\stringraise by -#1\\multiply\\stringraise by 2")
        << QStringLiteral("  \\advancefalse\\def\\q@u{}\\loffset{0.45}{\\@nq{\\the\\stringraise}}%")
        << QStringLiteral("  \\iftabstylespace\\else\\advance\\stringraise-1\\fi")
        << QStringLiteral("  \\ccharnote{\\the\\stringraise}{\\color{white}\\vrule height \\ht0 width \\wd0 depth \\dp0}%")
        << QStringLiteral("  \\ccharnote{\\the\\stringraise}{\\box0}}")
        << QStringLiteral("% notes without head and without ledger lines: rhythm stems/beams below the TAB")
        << QStringLiteral("\\let\\gpt@hline\\h@linei")
        << QStringLiteral("% (the real head glyph in white keeps MusiXTeX's metrics exact - an empty box does not)")
        << QStringLiteral("\\def\\gptHeadless{\\def\\q@symbol{\\def\\q@u{{\\color{white}\\musixfont\\s@v@n}}}\\def\\h@symbol{\\def\\q@u{{\\color{white}\\musixfont\\@ight}}}\\let\\h@linei\\relax}")
        << QStringLiteral("\\def\\gptRestoreHeads{\\def\\q@symbol{\\def\\q@u{\\musixfont\\s@v@n}}\\def\\h@symbol{\\def\\q@u{\\musixfont\\@ight}}\\let\\h@linei\\gpt@hline}")
        << QStringLiteral("\\makeatother");

    if (hasTab) {
        out << QStringLiteral("\\instrumentnumber{2}")
            << QStringLiteral("\\setlines{1}{%1}").arg(part.tuning.isEmpty() ? 6 : part.tuning.size())
            << QStringLiteral("\\setclefsymbol{1}{\\tabclef}")
            << QStringLiteral("\\setsize{1}{%1}   % TAB lines %1x apart: fret numbers on neighbouring strings must not touch").arg(QLatin1String(TAB_SIZE))
            << QStringLiteral("% fret numbers: fixed 8pt Helvetica bold, independent of the TAB size (MusiXTeX's own")
            << QStringLiteral("% font for large TAB sizes refers to the misspelled \\tabfntwentynine and fails)")
            << QStringLiteral("\\font\\tabfnteight=phvb8t at 8pt\\def\\tabfnt{\\tabfnteight}")
            << (octaveClef ? QStringLiteral("\\setclefsymbol{2}{\\treblelowoct}") : QStringLiteral("% notation at sounding pitch: plain treble clef"))
            << QStringLiteral("\\setclef{2}{\\treble}");
    } else {
        out << QStringLiteral("\\instrumentnumber{1}")
            << (octaveClef ? QStringLiteral("\\setclefsymbol{1}{\\treblelowoct}") : QStringLiteral("% plain treble clef"))
            << QStringLiteral("\\setclef{1}{\\treble}");
    }
    out << QStringLiteral("\\generalmeter{\\meterfrac{%1}{%2}}").arg(beats).arg(beatType)
        << QStringLiteral("\\generalsignature{%1}").arg(fifths) + (hasTab ? QStringLiteral("\\setsign{1}{0}") : QString());
    if (!part.name.isEmpty())
        out << QStringLiteral("% Instrument: %1   (for a name left of the staff: \\setname{%2}{...})")
                   .arg(part.name).arg(hasTab ? 2 : 1);

    // PIMA letters below the TAB need room before the next system
    const bool anyPluck = std::any_of(part.measures.cbegin(), part.measures.cend(), [](const Measure &m) {
        return std::any_of(m.notes.cbegin(), m.notes.cend(), [](const Note &n) { return !n.tech.pluck.isEmpty(); });
    });
    const bool rhythm = TAB_RHYTHM && hasTab;
    if (rhythm || anyPluck)
        out << QStringLiteral("\\staffbotmarg=%1\\Interligne   % room below the TAB (rhythm stems / PIMA)")
                   .arg(rhythm ? (anyPluck ? 17 : 10) : 11);

    // Lyrics: one line per verse below the notation staff, clear of its lowest note.
    int maxVerse = 0, lowestRel = 0;       // lowest written note, internotes above the bottom line (E4)
    bool anyMulti = false;
    for (const Measure &m : part.measures) {
        for (const auto &l : m.lyrics) maxVerse = qMax(maxVerse, l.lyric.verse);
        QSet<int> vs;
        for (const Note &n : m.notes) {
            if ((staff > 0 && n.staff != staff) || n.rest || n.grace) continue;
            vs.insert(n.voice);
            lowestRel = qMin(lowestRel, diatonicIndex(n.step, n.octave + writtenShift) - 30);
        }
        anyMulti = anyMulti || vs.size() > 1;
    }
    // below the lowest head (and below down-stems of a second voice)
    const int lyricTop = qMin(-5, lowestRel - 3) - (anyMulti ? 6 : 0);
    if (maxVerse > 0 && hasTab) {
        // room between TAB (instrument 1) and notation (instrument 2) for the lyric lines
        // (added to MusiXTeX's normal distance, which already covers ~6 interlines)
        const int need = qMax(1, (-lyricTop + 3 * maxVerse) / 2 - 3);
        out << QStringLiteral("\\setinterinstrument{1}{%1\\Interligne}   % room for %2 lyric line(s)").arg(need).arg(maxVerse);
    }

    bool numOk = false;
    const int firstNumber = part.measures.isEmpty() ? 1 : part.measures.first().number.toInt(&numOk);
    // Swing symbol "(eighth eighth = quarter 3 eighth)", built from MusiXTeX's text notes
    // (\lcu/\lqu need a pitch argument; \lcu has zero net width -> explicit kerns)
    const bool anySwing = std::any_of(part.measures.cbegin(), part.measures.cend(), [](const Measure &m) {
        return std::any_of(m.texts.cbegin(), m.texts.cend(), [](const auto &t) { return isSwingText(t.words); });
    });
    if (anySwing)
        out << QStringLiteral("% swing / triplet feel symbol")
            << QStringLiteral("\\def\\gptSwing{\\hbox{\\footnotesize(\\kern1.1em\\lcu{0}\\kern1.0em\\lcu{0}\\kern.9em=\\kern1.0em"
                              "\\lqu{0}\\kern.2em\\raise3.4ex\\hbox{\\tiny 3}\\kern.75em\\lcu{0}\\kern.8em)}}");

    // Thinner beams. Only HORIZONTAL beams are drawn as rules with adjustable thickness
    // (sloped beams come from a font with fixed thickness), so all beams are horizontal.
    // MusiXTeX default: half thickness .24 \Interligne, distance of 16th beams .75.
    out << QStringLiteral("% thinner beams (MusiXTeX default: \\b@amthick .24, \\interbeam .75)")
        << QStringLiteral("\\makeatletter\\def\\set@normalnotesize{\\let\\musixfont\\musicnorfont\\let\\xgregfont\\xgregnorfont")
        << QStringLiteral("  \\let\\fetafont\\fetanorfont\\b@amthick.19\\Interligne \\interbeam.6\\Interligne}\\normalnotesize\\makeatother");

    // A \leftrepeat at the very beginning counts as a bar line in MusiXTeX
    const bool startsWithRepeat = !part.measures.isEmpty() && part.measures.first().repeatStart;
    out << QStringLiteral("\\startbarno=%1").arg((numOk ? firstNumber : 1) - (startsWithRepeat ? 1 : 0))
        << QStringLiteral("\\startpiece");

    // --------------------------------------------------------------- measures
    IdPool slurIds;                         // ties and slurs share MusiXTeX's slur numbers
    QHash<int, int> openTies;               // written pitch key -> id
    QList<int> openSlurs;                   // stack of open slur ids
    QHash<int, int> beamLevel;              // voice -> current beam multiplicity
    QHash<int, int> rhythmBeamLevel;        // voice -> beam multiplicity of the TAB rhythm
    int voltaEnd = -1;                      // measure index where an open \Setvolta ends

    // NO spaces around "&": inside \Notes they become real glue that musixflx
    // does not account for -> every line ends up overfull
    const QString sep = hasTab ? QStringLiteral("&") : QString();

    for (int mi = 0; mi < part.measures.size(); ++mi) {
        const Measure &m = part.measures.at(mi);

        // ---- bar line / line break / context changes BEFORE this measure
        if (mi > 0) {
            const Measure &prev = part.measures.at(mi - 1);
            QString bar;
            if (m.hasKey && m.keyFifths != fifths) {
                fifths = m.keyFifths;
                bar += QStringLiteral("\\generalsignature{%1}").arg(fifths) + (hasTab ? QStringLiteral("\\setsign{1}{0}") : QString());
            }
            if (m.beats > 0 && (m.beats != beats || m.beatType != beatType)) {
                beats = m.beats; beatType = m.beatType;
                bar += QStringLiteral("\\generalmeter{\\meterfrac{%1}{%2}}").arg(beats).arg(beatType);
            }
            // volta brackets are settings that take effect at the NEXT bar line
            if (voltaEnd == mi - 1) {
                bar += QStringLiteral("\\setendvolta");
                voltaEnd = -1;
            }
            if (!m.ending.isEmpty()) {
                int end = mi;
                while (end < part.measures.size() - 1 && !part.measures.at(end).endingStop)
                    ++end;
                if (end == mi || !part.measures.at(end).endingStop) {
                    bar += QStringLiteral("\\setvolta{%1}").arg(texEscape(m.ending));   // one measure
                } else {
                    bar += QStringLiteral("\\Setvolta{%1}").arg(texEscape(m.ending));   // several measures
                    voltaEnd = end;
                }
            }
            if (prev.repeatEnd && m.repeatStart) bar += QStringLiteral("\\setleftrightrepeat");
            else if (prev.repeatEnd)             bar += QStringLiteral("\\setrightrepeat");
            else if (m.repeatStart)              bar += QStringLiteral("\\setleftrepeat");

            const bool lineBreak = anySystemBreak ? m.newSystem : (mi % 4 == 0);
            if (lineBreak)
                bar += QStringLiteral("\\alaligne");
            else if (bar.contains(QLatin1String("\\general")))
                bar += QStringLiteral("\\changecontext");
            else
                bar += QStringLiteral("\\bar");
            out << bar;
        } else if (m.repeatStart) {
            out << QStringLiteral("\\leftrepeat");
        }
        if (m.divisions > 0)
            divisions = m.divisions;

        out << QStringLiteral("% Measure %1").arg(m.number);

        std::deque<Note> mergedNotes;       // merged copies of duplicate notes (stable addresses)

        // ---- collect: position -> voice -> event, grace notes separately
        QMap<int, QMap<int, VEvent>> byPos;
        QMap<int, QList<VEvent>> graces;    // position -> grace events (file order)
        QList<int> voices;
        for (const Note &n : m.notes) {
            if (staff > 0 && n.staff != staff)
                continue;
            if (n.grace) {
                QList<VEvent> &gl = graces[n.position];
                if (n.chord && !gl.isEmpty() && gl.last().voice == n.voice) {
                    gl.last().notes.append(&n);
                } else {
                    VEvent g; g.voice = n.voice; g.pos = n.position; g.grace = true;
                    g.rest = false; g.main = &n; g.notes.append(&n);
                    gl.append(g);
                }
                continue;
            }
            if (!voices.contains(n.voice))
                voices.append(n.voice);
            VEvent &e = byPos[n.position][n.voice];
            e.voice = n.voice;
            e.pos = n.position;
            if (!e.main) { e.main = &n; e.dur = n.duration; }
            if (n.rest) {
                if (e.notes.isEmpty() && e.rest) e.main = &n;
                continue;
            }
            if (e.rest) { e.rest = false; e.main = &n; e.dur = n.duration; }
            e.notes.append(&n);
            if (e.beam1.isEmpty() && !n.beam1.isEmpty()) { e.beam1 = n.beam1; e.beam2 = n.beam2; }
        }
        std::sort(voices.begin(), voices.end());
        const bool multi = voices.size() > 1;          // several voices: voice 1 up, others down
        auto rankOf = [&](int v) { const int r = voices.indexOf(v); return r < 0 ? 0 : r; };

        const QList<int> onsets = byPos.keys();
        if (onsets.isEmpty()) {
            out << QStringLiteral("\\NOTEs %1\\pause\\en").arg(sep);
            continue;
        }

        auto writtenOf = [&](const VEvent &e) {
            QList<Written> w;
            for (const Note *n : e.notes)
                w.append({ n, diatonicIndex(n->step, n->octave + writtenShift) });
            std::stable_sort(w.begin(), w.end(), [](const Written &a, const Written &b) { return a.index < b.index; });
            return w;
        };

        // ---- accidental state of this measure (written pitches, shared by all voices)
        QHash<int, int> alterState;
        auto currentAlter = [&](int idx) {
            return alterState.contains(idx) ? alterState.value(idx) : keyAlter(fifths, idx % 7);
        };

        // ---- beam groups per voice (key: voice * 1000 + onset index)
        struct Group { int first = -1, last = -1; bool up = true; };
        QHash<int, Group> groupOf;
        for (int v : voices) {
            int start = -1;
            for (int i = 0; i < onsets.size(); ++i) {
                const auto &vm = byPos[onsets[i]];
                if (!vm.contains(v)) continue;
                const QString &b = vm[v].beam1;
                if (b == QLatin1String("begin")) start = i;
                if (b == QLatin1String("end") && start >= 0) {
                    Group g; g.first = start; g.last = i;
                    if (multi) {
                        g.up = (rankOf(v) == 0);
                    } else {
                        int sum = 0, cnt = 0;
                        for (int k = start; k <= i; ++k)
                            if (byPos[onsets[k]].contains(v))
                                for (const Note *n : byPos[onsets[k]][v].notes) {
                                    sum += diatonicIndex(n->step, n->octave + writtenShift); ++cnt;
                                }
                        g.up = cnt == 0 || (double(sum) / cnt) <= MIDDLE_LINE;
                    }
                    for (int k = start; k <= i; ++k)
                        if (byPos[onsets[k]].contains(v))
                            groupOf.insert(v * 1000 + k, g);
                    start = -1;
                }
            }
        }

        // ---- chord symbols, words, tempo: attach to the first onset at or after their position
        auto onsetFor = [&](int pos) {
            for (int i = 0; i < onsets.size(); ++i)
                if (onsets[i] >= pos) return i;
            return int(onsets.size()) - 1;
        };
        QHash<int, QStringList> aboveText;
        {
            // A chord symbol less than a half note after the previous one would overlap
            // it (e.g. "Cmaj7/G" followed by "D7" one beat later): set it one level higher.
            int prevPos = -1000000, prevHeight = 18;
            for (const Harmony &h : m.harmonies) {
                const bool close = divisions > 0 && (h.position - prevPos) < 2 * divisions;
                const int height = (close && prevHeight == 15) ? 18 : 15;
                aboveText[onsetFor(h.position)] << QStringLiteral("\\zcharnote{%1}{\\bfseries\\small %2}").arg(height).arg(chordText(h));
                prevPos = h.position;
                prevHeight = height;
            }
        }
        for (const auto &t : m.texts)
            if (isSwingText(t.words)) {
                aboveText[onsetFor(t.position)] << QStringLiteral("\\zcharnote{24}{\\gptSwing}");   // above the text line (19)
            } else
            if (!t.words.isEmpty())
                aboveText[onsetFor(t.position)] << QStringLiteral("\\zcharnote{19}{\\itshape\\footnotesize %1}").arg(texEscape(t.words));
        if (m.tempo > 0.0)
            aboveText[0] << QStringLiteral("\\zcharnote{22}{\\metron{\\lqu}{%1}}").arg(qRound(m.tempo));
        // lyrics: centered below the note, one line per verse; hyphen after begin/middle syllables
        for (const auto &la : m.lyrics) {
            // Guitar Pro writes "__" for notes that continue the previous syllable
            // (melisma); typeset it as a short line, as Guitar Pro shows it
            if (QRegularExpression(QStringLiteral("^_+$")).match(la.lyric.text.trimmed()).hasMatch()) {
                aboveText[onsetFor(la.position)] << QStringLiteral("\\ccharnote{%1}{\\rule[-.2ex]{1.1em}{.4pt}}")
                                                        .arg(lyricTop - 3 * (la.lyric.verse - 1));
                continue;
            }
            QString txt = texEscape(la.lyric.text);
            if (la.lyric.syllabic == QLatin1String("begin") || la.lyric.syllabic == QLatin1String("middle"))
                txt += QStringLiteral("-");
            aboveText[onsetFor(la.position)] << QStringLiteral("\\ccharnote{%1}{\\small %2}")
                                                    .arg(lyricTop - 3 * (la.lyric.verse - 1)).arg(txt);
        }

        // ---- tuplets per voice: from <tuplet type="start|stop" bracket placement> (Guitar Pro),
        //      otherwise (older files) every <actual-notes> notes form a group
        struct Tuplet { int last = -1; int actual = 3; bool bracket = false; QString placement; };
        QHash<int, Tuplet> tupletAt;        // voice * 1000 + first onset index -> tuplet
        for (int v : voices) {
            QList<int> idx;                 // onset indices where this voice has a (non-rest) event
            for (int i = 0; i < onsets.size(); ++i)
                if (byPos[onsets[i]].contains(v)) idx << i;
            for (int k = 0; k < idx.size(); ++k) {
                const VEvent &e = byPos[onsets[idx[k]]][v];
                if (!e.main) continue;
                const int act = e.main->tupletActual;
                bool xmlStart = false;
                for (const Note *n : e.notes) xmlStart = xmlStart || n->tupletStart;
                if (e.main->tupletStart) xmlStart = true;
                if (xmlStart) {
                    Tuplet t; t.actual = act > 1 ? act : 3;
                    t.bracket = e.main->tupletBracket; t.placement = e.main->tupletPlacement;
                    for (const Note *n : e.notes) if (n->tupletStart) { t.bracket = n->tupletBracket; t.placement = n->tupletPlacement; }
                    int j = k;
                    for (; j < idx.size(); ++j) {
                        bool stop = false;
                        const VEvent &f = byPos[onsets[idx[j]]][v];
                        for (const Note *n : f.notes) stop = stop || n->tupletStop;
                        if (f.main && f.main->tupletStop) stop = true;
                        if (stop) break;
                    }
                    t.last = idx[qMin(j, int(idx.size()) - 1)];
                    tupletAt.insert(v * 1000 + idx[k], t);
                    k = qMin(j, int(idx.size()) - 1);
                } else if (act > 1) {
                    // legacy: no <tuplet> elements -> group of <actual> consecutive tuplet notes
                    Tuplet t; t.actual = act;
                    const int j = qMin(k + act - 1, int(idx.size()) - 1);
                    t.last = idx[j];
                    tupletAt.insert(v * 1000 + idx[k], t);
                    k = j;
                }
            }
        }

        // ---- render one voice event of the notation staff
        //      advance = this is the element that moves on to the next onset
        auto renderVoice = [&](const VEvent &e, int onsetIdx, bool advance) -> QString {
            QString nota;
            const int rank = rankOf(e.voice);

            if (e.rest) {
                const QString macro = restMacro(e.main ? e.main->type : QString(), e.main ? e.main->dots : 0);
                if (!multi)
                    return macro;
                // several voices: the rest glyph is placed at the note position with
                // \zcharnote (no advance) - upper voice 2 steps higher, lower voice 2 lower.
                // Glyphs/heights as in MusiXTeX's own rest macros (measured):
                //   whole 61 @6, half 60 @4, quarter 62 / eighth 63 / 16th 64 / 32nd 65 @0
                const QString type = e.main ? e.main->type : QString();
                int glyph = 65, height = 0;
                if (type == QLatin1String("whole") || type.isEmpty()) { glyph = 61; height = 6; }
                else if (type == QLatin1String("half"))    { glyph = 60; height = 4; }
                else if (type == QLatin1String("quarter")) glyph = 62;
                else if (type == QLatin1String("eighth"))  glyph = 63;
                else if (type == QLatin1String("16th"))    glyph = 64;
                height += (rank == 0) ? 2 : -2;
                const QString rest = QStringLiteral("\\zcharnote{%1}{\\musixchar%2}").arg(height).arg(glyph);
                return advance ? rest + QStringLiteral("\\sk") : rest;
            }

            const QList<Written> w = writtenOf(e);
            const int gkey = e.voice * 1000 + onsetIdx;
            const bool inBeam = !e.grace && groupOf.contains(gkey);
            const Group grp = inBeam ? groupOf.value(gkey) : Group();
            const bool up = e.grace ? (rank == 0)
                          : inBeam  ? grp.up
                          : multi   ? (rank == 0)
                                    : (double(w.first().index + w.last().index) / 2.0) <= MIDDLE_LINE;
            const Written &stemNote = up ? w.first() : w.last();
            const QString ud = up ? QStringLiteral("u") : QStringLiteral("l");
            const int beamNo = rank;

            // accidentals (in pitch order)
            QHash<const Note *, QString> acc;
            for (const Written &x : w) {
                if (x.note->alter != currentAlter(x.index)) {
                    acc.insert(x.note, accidentalPrefix(x.note->alter));
                    alterState.insert(x.index, x.note->alter);
                }
            }

            // ties (stop first, then start)
            for (const Written &x : w) {
                const int key = x.index * 10 + x.note->alter;
                if (x.note->tieStop && openTies.contains(key)) {
                    const int id = openTies.take(key);
                    nota += QStringLiteral("\\ttie{%1}").arg(id);
                    slurIds.give(id);
                }
            }
            for (const Written &x : w) {
                const int key = x.index * 10 + x.note->alter;
                if (x.note->tieStart && !openTies.contains(key)) {
                    const int id = slurIds.take();
                    if (id < 0) continue;
                    openTies.insert(key, id);
                    const bool above = (w.size() > 1) ? (x.index == w.last().index) : !up;
                    nota += QStringLiteral("\\itie%1{%2}{%3}").arg(above ? QStringLiteral("u") : QStringLiteral("d"))
                                .arg(id).arg(pitchLetter(x.index));
                }
            }

            // slurs (H/P legato). Guitar Pro also puts stray slur start/stop marks on
            // chord members, so per event at most ONE slur ends and ONE starts -
            // preferably at the note that carries the hammer-on/pull-off.
            for (const Note *n : e.notes) {
                if (n->slurStop && !openSlurs.isEmpty()) {
                    const int id = openSlurs.takeLast();
                    const int idx = diatonicIndex(n->step, n->octave + writtenShift);
                    nota += QStringLiteral("\\tslur{%1}{%2}").arg(id).arg(pitchLetter(idx));
                    slurIds.give(id);
                    break;
                }
            }
            {
                const Note *startNote = nullptr;
                for (const Note *n : e.notes)
                    if (n->slurStart && (n->tech.hammerOn || n->tech.pullOff)) { startNote = n; break; }
                if (!startNote)
                    for (auto it = w.crbegin(); it != w.crend(); ++it)
                        if (it->note->slurStart) { startNote = it->note; break; }
                if (startNote) {
                    const int id = slurIds.take();
                    if (id >= 0) {
                        openSlurs.append(id);
                        const int idx = diatonicIndex(startNote->step, startNote->octave + writtenShift);
                        const bool above = multi ? up : ((w.size() > 1) ? (idx == w.last().index) : !up);
                        nota += QStringLiteral("\\islur%1{%2}{%3}").arg(above ? QStringLiteral("u") : QStringLiteral("d"))
                                    .arg(id).arg(pitchLetter(idx));
                    }
                }
            }

            // tuplet: bracket (\uptuplet/\downtuplet) or number only (\xtuplet).
            // Default side = stem/beam side; the beam lies one stem length beyond the
            // group's extreme note, so the mark goes 9 steps beyond it on that side.
            if (tupletAt.contains(gkey)) {
                const Tuplet t = tupletAt.value(gkey);
                int hi = w.last().index, lo = w.first().index;
                for (int k = onsetIdx; k <= t.last && k < onsets.size(); ++k) {
                    if (!byPos[onsets[k]].contains(e.voice)) continue;
                    const QList<Written> wk = writtenOf(byPos[onsets[k]][e.voice]);
                    if (wk.isEmpty()) continue;
                    hi = qMax(hi, wk.last().index); lo = qMin(lo, wk.first().index);
                }
                const bool above = t.placement.isEmpty() ? up : (t.placement == QLatin1String("above"));
                const int height = above ? (up ? hi + 9 : hi + 4) : (up ? lo - 4 : lo - 9);
                const int width = qMax(1, t.last - onsetIdx);      // in note spaces, first to last note
                if (t.bracket)
                    nota += QStringLiteral("\\def\\tuplettxt{%1\\/\\/}\\%2tuplet{%3}{%4}{0}")
                                .arg(t.actual).arg(above ? QStringLiteral("up") : QStringLiteral("down"))
                                .arg(pitchLetter(height)).arg(width);
                else
                    nota += QStringLiteral("\\xtuplet{%1}{%2}").arg(t.actual).arg(pitchLetter(height));
            }

            // arpeggio (rolled chord)
            bool arp = false;
            for (const Note *n : e.notes) arp = arp || n->tech.arpeggiate;
            if (arp && w.size() > 1)
                nota += QStringLiteral("\\larpeggio{%1}{%2}").arg(pitchLetter(w.first().index))
                            .arg((w.last().index - w.first().index) / 2 + 1);

            // beams
            if (inBeam) {
                if (onsetIdx == grp.first) {
                    // Horizontal beam (slope 0, see "thinner beams"). It must clear every note of
                    // the group: start pitch = highest note (stems up) / lowest note (stems down).
                    int extreme = stemNote.index;
                    for (int k = grp.first; k <= grp.last; ++k) {
                        if (!byPos[onsets[k]].contains(e.voice)) continue;
                        const QList<Written> wk = writtenOf(byPos[onsets[k]][e.voice]);
                        if (wk.isEmpty()) continue;
                        extreme = up ? qMax(extreme, wk.last().index) : qMin(extreme, wk.first().index);
                    }
                    const bool dbl = (e.beam2 == QLatin1String("begin"));
                    beamLevel[e.voice] = dbl ? 2 : 1;
                    nota += QStringLiteral("\\ib%1%2{%3}{%4}{0}").arg(dbl ? QStringLiteral("b") : QString(), ud)
                                .arg(beamNo).arg(pitchLetter(extreme));
                } else {
                    if (e.beam2 == QLatin1String("begin") && beamLevel.value(e.voice) < 2) {
                        nota += QStringLiteral("\\nbb%1{%2}").arg(ud).arg(beamNo);
                        beamLevel[e.voice] = 2;
                    } else if ((e.beam2 == QLatin1String("backward hook"))
                               || (e.beam2 == QLatin1String("end") && onsetIdx != grp.last)) {
                        nota += QStringLiteral("\\tbb%1{%2}").arg(ud).arg(beamNo);
                        beamLevel[e.voice] = 1;
                    }
                    if (onsetIdx == grp.last)
                        nota += QStringLiteral("\\tb%1{%2}").arg(ud).arg(beamNo);
                }
            }

            // articulations on the note-head side (opposite the stem):
            // accent ">" (\usf/\lsf), staccato dot (\upz/\lpz)
            {
                bool accent = false, staccato = false;
                bool marcato = false;
                for (const Note *n : e.notes) { accent = accent || n->tech.accent; staccato = staccato || n->tech.staccato; marcato = marcato || n->tech.marcato; }
                const QString at = pitchLetter(up ? w.first().index : w.last().index);
                if (staccato) nota += (up ? QStringLiteral("\\lpz{%1}") : QStringLiteral("\\upz{%1}")).arg(at);
                if (accent)   nota += (up ? QStringLiteral("\\lsf{%1}") : QStringLiteral("\\usf{%1}")).arg(at);
                if (marcato)  nota += (up ? QStringLiteral("\\lsfz{%1}") : QStringLiteral("\\usfz{%1}")).arg(at);
            }

            // note heads: chord members first, stem note last
            const QString type = e.main->type;
            const int dots = e.main->dots;
            for (const Written &x : w) {
                if (&x == &stemNote) continue;
                nota += headMacro(type, dots) + QStringLiteral("{%1%2}").arg(acc.value(x.note), pitchLetter(x.index));
            }
            const QString pitch = acc.value(stemNote.note) + pitchLetter(stemNote.index);
            if (e.grace)
                nota += (up ? QStringLiteral("\\grcu") : QStringLiteral("\\grcl")) + QStringLiteral("{%1}").arg(pitch);
            else if (inBeam)
                nota += QStringLiteral("\\%1qb%2{%3}{%4}").arg(advance ? QString() : QStringLiteral("z"),
                                                            dots > 0 ? QStringLiteral("p") : QString())
                            .arg(beamNo).arg(pitch);
            else
                nota += (advance ? noteMacro(type, dots, up) : zNoteMacro(type, dots, up)) + QStringLiteral("{%1}").arg(pitch);
            return nota;
        };

        // ---- TAB for a set of notes: legato letter, PIMA letters below, fret numbers
        auto renderTab = [&](const QList<const Note *> &all, bool small, const QString &rhythmStr) -> QString {
            QString tab = rhythmStr;            // non-advancing: must come before the \tab macros
            QList<const Note *> tn;
            for (const Note *n : all) {
                if (n->string <= 0 || n->fret < 0) continue;
                bool dup = false;               // the same string/fret in two voices: print once
                for (const Note *o : tn) if (o->string == n->string && o->fret == n->fret) dup = true;
                if (!dup) tn.append(n);
            }
            std::stable_sort(tn.begin(), tn.end(), [](const Note *a, const Note *b) { return a->string > b->string; });
            QString legato;
            for (const Note *n : tn) {
                if (n->tech.hammerOn) legato = QStringLiteral("H");
                if (n->tech.pullOff)  legato = QStringLiteral("P");
            }
            if (!legato.isEmpty())
                tab += QStringLiteral("\\zcharnote{11}{\\kern.5\\noteskip\\tiny %1}").arg(legato);   // just above the top TAB line
            // other techniques as small labels above the TAB (as in tab books)
            QStringList labels;
            for (const Note *n : tn) {
                const double b = n->tech.bend;
                if (b != 0.0) {
                    // bend in semitones: 1 = 1/2, 2 = full, 3 = 1 1/2, 4 = 2 ...
                    QString amount;
                    if (qFuzzyCompare(b, 2.0))      amount = QStringLiteral("full");
                    else if (qFuzzyCompare(b, 1.0)) amount = QStringLiteral("$\\frac{1}{2}$");
                    else {
                        const int halves = qRound(b);           // semitones = halves of a whole tone
                        amount = QString::number(halves / 2);
                        if (halves % 2) amount += QStringLiteral("$\\frac{1}{2}$");
                    }
                    labels << QStringLiteral("$\\nearrow$%1").arg(amount);
                }
                if (n->tech.vibrato)  labels << QStringLiteral("$\\sim\\!\\sim$");
                if (n->tech.palmMute) labels << QStringLiteral("P.M.");
                if (n->tech.slide)    labels << QStringLiteral("sl.");
            }
            labels.removeDuplicates();
            if (!labels.isEmpty())
                tab += QStringLiteral("\\zcharnote{12}{\\kern.25\\noteskip\\tiny %1}").arg(labels.join(QStringLiteral("\\,")));
            // PIMA below the TAB, highest string on top
            QList<QPair<int, QString>> picks;
            for (const Note *n : all)
                if (!n->tech.pluck.isEmpty()) {
                    bool dup = false;
                    for (const auto &pk : picks) if (pk.first == n->string) dup = true;
                    if (!dup) picks.append({ n->string, n->tech.pluck });
                }
            std::sort(picks.begin(), picks.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
            for (int k = 0; k < picks.size(); ++k)
                tab += QStringLiteral("\\zcharnote{%1}{\\itshape\\small %2}").arg((rhythm ? RHYTHM_PIMA : -4) - 3 * k).arg(texEscape(picks[k].second));
            // fret numbers (tied continuation in parentheses); \tab (without z) advances
            for (int k = 0; k < tn.size(); ++k) {
                const Note *n = tn[k];
                QString fret = n->tieStop       ? QStringLiteral("(%1)").arg(n->fret)
                             : n->tech.harmonic ? QStringLiteral("<%1>").arg(n->fret)   // natural harmonic
                                                : QString::number(n->fret);
                if (small) fret = QStringLiteral("\\scriptsize ") + fret;
                tab += QStringLiteral("\\%1tab{%2}{%3}").arg(k + 1 < tn.size() ? QStringLiteral("z") : QString())
                           .arg(n->string).arg(fret);
            }
            if (tn.isEmpty())
                tab += QStringLiteral("\\sk");  // keep the TAB in step (e.g. only rests)
            return tab;
        };

        for (int i = 0; i < onsets.size(); ++i) {
            const int pos = onsets[i];
            const int nextPos = (i + 1 < onsets.size()) ? onsets[i + 1] : [&] {
                int end = pos;
                for (const VEvent &e : byPos[pos]) end = qMax(end, pos + e.dur);
                return end;
            }();
            const double quarters = divisions > 0 ? double(nextPos - pos) / divisions : 1.0;

            // grace notes: own small block right before the onset
            if (graces.contains(pos)) {
                QString gn, gt;
                QList<const Note *> gnotes;
                const QList<VEvent> &gl = graces[pos];
                for (int g = 0; g < gl.size(); ++g) {
                    gn += renderVoice(gl[g], -1, g + 1 == gl.size());
                    gnotes += gl[g].notes;
                }
                gt = hasTab ? renderTab(gnotes, true, QString()) : QString();
                out << QStringLiteral("\\notes ") + (hasTab ? gt + sep + gn : gn) + QStringLiteral("\\en");
            }

            // voice events at this onset: upper voice first; the advancing element comes last
            QList<VEvent> evs = byPos[pos].values();
            std::sort(evs.begin(), evs.end(), [](const VEvent &a, const VEvent &b) { return a.voice < b.voice; });

            // TAB and PIMA use ALL notes (the TAB prints each string/fret once anyway)
            QList<const Note *> all;
            for (const VEvent &e : evs) all += e.notes;

            // TAB rhythm follows the first (upper) voice that plays at this onset
            QString rhythmStr;
            if (rhythm) {
                const VEvent *rs = nullptr;
                for (const VEvent &e : evs) if (!e.rest && e.main) { rs = &e; break; }
                if (rs) {
                    const QString type = rs->main->type;
                    const int dots = rs->main->dots;
                    const int gk = rs->voice * 1000 + i;
                    const QString top = QString::number(RHYTHM_TOP);
                    QString r = QStringLiteral("\\gptHeadless");
                    if (dots > 0 && type != QLatin1String("whole"))
                        r += QStringLiteral("\\pt{%1}").arg(RHYTHM_DOT);
                    if (groupOf.contains(gk)) {
                        const Group g = groupOf.value(gk);
                        if (i == g.first) {
                            const bool dbl = (rs->beam2 == QLatin1String("begin"));
                            rhythmBeamLevel[rs->voice] = dbl ? 2 : 1;
                            r += QStringLiteral("\\ib%1l{3}{%2}{0}").arg(dbl ? QStringLiteral("b") : QString(), top);
                        } else {
                            if (rs->beam2 == QLatin1String("begin") && rhythmBeamLevel.value(rs->voice) < 2) {
                                r += QStringLiteral("\\nbbl{3}");
                                rhythmBeamLevel[rs->voice] = 2;
                            } else if (rs->beam2 == QLatin1String("backward hook")
                                       || (rs->beam2 == QLatin1String("end") && i != g.last)) {
                                r += QStringLiteral("\\tbbl{3}");
                                rhythmBeamLevel[rs->voice] = 1;
                            }
                            if (i == g.last)
                                r += QStringLiteral("\\tbl{3}");
                        }
                        r += QStringLiteral("\\zqb{3}{%1}").arg(top);
                    } else if (type == QLatin1String("half")) {
                        // half note: shorter stem, as in Guitar Pro
                        r += QStringLiteral("\\stemlength{2.5}\\zhl{%1}\\stemlength{\\DefaultStemlength}").arg(top);
                    } else if (type == QLatin1String("quarter")) {
                        r += QStringLiteral("\\zql{%1}").arg(top);
                    } else if (type == QLatin1String("eighth")) {
                        r += QStringLiteral("\\zcl{%1}").arg(top);
                    } else if (type == QLatin1String("16th")) {
                        r += QStringLiteral("\\zccl{%1}").arg(top);
                    } else if (type == QLatin1String("32nd") || type == QLatin1String("64th")) {
                        r += QStringLiteral("\\zcccl{%1}").arg(top);
                    }                                   // whole note: no stem
                    r += QStringLiteral("\\gptRestoreHeads");
                    if (tupletAt.contains(gk)) {
                        const Tuplet t = tupletAt.value(gk);
                        if (t.bracket)
                            r += QStringLiteral("\\def\\tuplettxt{%1\\/\\/}\\downtuplet{%2}{%3}{0}")
                                     .arg(t.actual).arg(RHYTHM_TUPLET).arg(qMax(1, t.last - i));
                        else
                            r += QStringLiteral("\\xtuplet{%1}{%2}").arg(t.actual).arg(RHYTHM_TUPLET);
                    }
                    rhythmStr = r;
                }
            }

            // The same pitch in two voices at the same time (e.g. a bass note entered in
            // voice 1 AND voice 2): print it only once in the notation, as the note of the
            // later voice (voice 2, stem down, its own duration). A voice-1 event that
            // becomes empty is dropped - unless it belongs to a beam or tuplet group,
            // whose start/end commands must not get lost.
            if (multi && evs.size() > 1) {
                // pitch key -> (event index, note index) of the later voice's note
                QHash<int, QPair<int, int>> later;
                for (int k = 1; k < evs.size(); ++k)
                    for (int j = 0; j < evs[k].notes.size(); ++j) {
                        const Note *n = evs[k].notes[j];
                        later.insert(diatonicIndex(n->step, n->octave + writtenShift) * 10 + n->alter, { k, j });
                    }
                VEvent &first = evs[0];
                if (!first.rest) {
                    QList<const Note *> keep;
                    for (const Note *n : first.notes) {
                        const int key = diatonicIndex(n->step, n->octave + writtenShift) * 10 + n->alter;
                        if (!later.contains(key)) { keep << n; continue; }
                        // merge the removed note's ties/slurs/marks into (a copy of) the kept one,
                        // so e.g. a hammer-on slur ending on the duplicate is not lost
                        const auto ref = later.value(key);
                        Note c = *evs[ref.first].notes[ref.second];
                        c.tieStart  = c.tieStart  || n->tieStart;
                        c.tieStop   = c.tieStop   || n->tieStop;
                        c.slurStart = c.slurStart || n->slurStart;
                        c.slurStop  = c.slurStop  || n->slurStop;
                        c.tech.hammerOn = c.tech.hammerOn || n->tech.hammerOn;
                        c.tech.pullOff  = c.tech.pullOff  || n->tech.pullOff;
                        c.tech.accent   = c.tech.accent   || n->tech.accent;
                        c.tech.marcato  = c.tech.marcato  || n->tech.marcato;
                        c.tech.staccato = c.tech.staccato || n->tech.staccato;
                        c.tech.arpeggiate = c.tech.arpeggiate || n->tech.arpeggiate;
                        mergedNotes.push_back(c);
                        evs[ref.first].notes[ref.second] = &mergedNotes.back();
                        if (evs[ref.first].main == evs[ref.first].notes[ref.second] || ref.second == 0)
                            evs[ref.first].main = evs[ref.first].notes[0];
                    }
                    const int gk = first.voice * 1000 + i;
                    const bool grouped = groupOf.contains(gk) || tupletAt.contains(gk);
                    if (keep.isEmpty() && !grouped) {
                        evs.removeFirst();
                    } else if (!keep.isEmpty() && keep.size() < first.notes.size()) {
                        if (!keep.contains(first.main)) first.main = keep.first();
                        first.notes = keep;
                    }
                }
            }
            int adv = 0;
            for (int k = 0; k < evs.size(); ++k)
                if (!evs[k].rest) { adv = k; break; }

            QString nota;
            for (const QString &t : aboveText.value(i))
                nota += t;
            for (int k = 0; k < evs.size(); ++k) {
                if (k == adv) continue;
                nota += renderVoice(evs[k], i, false);
            }
            nota += renderVoice(evs[adv], i, true);

            const QString tab = hasTab ? renderTab(all, false, rhythmStr) : QString();
            out << spacingMacro(quarters) + QLatin1Char(' ') + (hasTab ? tab + sep + nota : nota) + QStringLiteral("\\en");
        }
    }

    // --------------------------------------------------------------- end
    const bool finalRepeat = !part.measures.isEmpty() && part.measures.last().repeatEnd;
    const QString finalVolta = (voltaEnd >= 0) ? QStringLiteral("\\setendvolta") : QString();
    out << finalVolta + (finalRepeat ? QStringLiteral("\\setrightrepeat\\endpiece") : QStringLiteral("\\Endpiece"))
        << QStringLiteral("\\end{music}");


    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}
