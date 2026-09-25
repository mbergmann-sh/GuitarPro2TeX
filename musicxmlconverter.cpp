#include "musicxmlconverter.h"

#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QStringList>
#include <QHash>
#include <algorithm>
#include <QRegularExpression>

/* =====================================================================
 *  Score helpers
 * ===================================================================== */

int MusicXmlConverter::Score::noteCount() const
{
    int n = 0;
    for (const Part &p : parts) {
        const int staff = p.primaryStaff();         // don't count Guitar Pro's duplicated staff twice
        for (const Measure &m : p.measures)
            for (const Note &note : m.notes)
                if (!note.rest && (staff == 0 || note.staff == staff))
                    ++n;
    }
    return n;
}

int MusicXmlConverter::Part::tabStaff() const
{
    for (auto it = clefs.cbegin(); it != clefs.cend(); ++it)
        if (it.value() == QLatin1String("TAB"))
            return it.key();
    return 0;
}

int MusicXmlConverter::Part::primaryStaff() const
{
    // TAB staff: sounding pitch and string/fret as real elements
    return tabStaff();
}

QString MusicXmlConverter::Harmony::name() const
{
    static const QHash<QString, QString> suffix = {
        { QStringLiteral("major"), QString() },
        { QStringLiteral("minor"), QStringLiteral("m") },
        { QStringLiteral("augmented"), QStringLiteral("aug") },
        { QStringLiteral("diminished"), QStringLiteral("dim") },
        { QStringLiteral("dominant"), QStringLiteral("7") },
        { QStringLiteral("major-seventh"), QStringLiteral("maj7") },
        { QStringLiteral("minor-seventh"), QStringLiteral("m7") },
        { QStringLiteral("diminished-seventh"), QStringLiteral("dim7") },
        { QStringLiteral("augmented-seventh"), QStringLiteral("aug7") },
        { QStringLiteral("half-diminished"), QStringLiteral("m7b5") },
        { QStringLiteral("major-minor"), QStringLiteral("m(maj7)") },
        { QStringLiteral("major-sixth"), QStringLiteral("6") },
        { QStringLiteral("minor-sixth"), QStringLiteral("m6") },
        { QStringLiteral("dominant-ninth"), QStringLiteral("9") },
        { QStringLiteral("major-ninth"), QStringLiteral("maj9") },
        { QStringLiteral("minor-ninth"), QStringLiteral("m9") },
        { QStringLiteral("suspended-second"), QStringLiteral("sus2") },
        { QStringLiteral("suspended-fourth"), QStringLiteral("sus4") },
        { QStringLiteral("power"), QStringLiteral("5") },
    };
    QString s = stepName(root, rootAlter) + suffix.value(kind, kind.isEmpty() ? QString() : QStringLiteral("(%1)").arg(kind));
    if (!bass.isEmpty())
        s += QLatin1Char('/') + stepName(bass, bassAlter);
    return s;
}

int MusicXmlConverter::Score::measureCount() const
{
    int n = 0;
    for (const Part &p : parts)
        n = qMax(n, int(p.measures.size()));
    return n;
}

/* =====================================================================
 *  Parsing
 * ===================================================================== */

MusicXmlConverter::Result MusicXmlConverter::convertFile(const QString &path)
{
    Result r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        r.error = tr("Cannot open file \"%1\":\n%2").arg(path, f.errorString());
        return r;
    }
    return convertDevice(&f, QFileInfo(path).fileName());
}

namespace {

inline bool is(const QXmlStreamReader &xml, const char *name)
{
    return xml.name() == QLatin1String(name);
}

inline QString attr(const QXmlStreamReader &xml, const char *name)
{
    return xml.attributes().value(QLatin1String(name)).toString();
}

// Text of the current element as int (reader must be on its StartElement)
int readInt(QXmlStreamReader &xml, int fallback = 0)
{
    bool ok = false;
    const int v = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed().toInt(&ok);
    return ok ? v : fallback;
}

double readDouble(QXmlStreamReader &xml, double fallback = 0.0)
{
    bool ok = false;
    const double v = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed().toDouble(&ok);
    return ok ? v : fallback;
}

QString readText(QXmlStreamReader &xml)
{
    return xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
}

using Note      = MusicXmlConverter::Note;
using Measure   = MusicXmlConverter::Measure;
using Part      = MusicXmlConverter::Part;
using Tuning    = MusicXmlConverter::Tuning;

// <technical> children.
// Guitar Pro 8 writes string/fret of the NOTATION staff not as elements but inside a
// processing instruction:  <?GP <root><string>2</string><fret>1</fret></root> ?>
void readTechnical(QXmlStreamReader &xml, Note &n)
{
    static const QRegularExpression reString(QStringLiteral("<string>\\s*(\\d+)\\s*</string>"));
    static const QRegularExpression reFret(QStringLiteral("<fret>\\s*(\\d+)\\s*</fret>"));

    while (!xml.atEnd()) {
        const QXmlStreamReader::TokenType tt = xml.readNext();
        if (tt == QXmlStreamReader::EndElement)
            break;                                  // </technical> (children are consumed below)
        if (tt == QXmlStreamReader::ProcessingInstruction) {
            if (xml.processingInstructionTarget() == QLatin1String("GP")) {
                const QString data = xml.processingInstructionData().toString();
                const auto ms = reString.match(data), mf = reFret.match(data);
                if (ms.hasMatch() && n.string == 0) n.string = ms.captured(1).toInt();
                if (mf.hasMatch() && n.fret < 0)    n.fret = mf.captured(1).toInt();
            }
            continue;
        }
        if (tt != QXmlStreamReader::StartElement)
            continue;

        if (is(xml, "string"))            n.string = readInt(xml);
        else if (is(xml, "fret"))         n.fret = readInt(xml, -1);
        else if (is(xml, "hammer-on"))  { if (attr(xml, "type") == QLatin1String("start")) n.tech.hammerOn = true; xml.skipCurrentElement(); }
        else if (is(xml, "pull-off"))   { if (attr(xml, "type") == QLatin1String("start")) n.tech.pullOff = true;  xml.skipCurrentElement(); }
        else if (is(xml, "harmonic"))   { n.tech.harmonic = true; xml.skipCurrentElement(); }
        else if (is(xml, "pluck"))        n.tech.pluck = readText(xml);
        else if (is(xml, "fingering"))    n.tech.fingering = readText(xml);
        else if (is(xml, "bend")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "bend-alter")) n.tech.bend = readDouble(xml);
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "other-technical")) {
            if (readText(xml).contains(QLatin1String("palm"), Qt::CaseInsensitive))
                n.tech.palmMute = true;
        }
        else xml.skipCurrentElement();
    }
}

// <notations> children
void readNotations(QXmlStreamReader &xml, Note &n)
{
    while (xml.readNextStartElement()) {
        if (is(xml, "technical")) {
            readTechnical(xml, n);
        }
        else if (is(xml, "slur")) {
            const QString t = attr(xml, "type");
            if (t == QLatin1String("start")) n.slurStart = true;
            else if (t == QLatin1String("stop")) n.slurStop = true;
            xml.skipCurrentElement();
        }
        else if (is(xml, "tuplet")) {
            const QString t = attr(xml, "type");
            if (t == QLatin1String("start")) {
                n.tupletStart = true;
                n.tupletBracket = attr(xml, "bracket") == QLatin1String("yes");
                n.tupletPlacement = attr(xml, "placement");
            } else if (t == QLatin1String("stop")) {
                n.tupletStop = true;
            }
            xml.skipCurrentElement();
        }
        else if (is(xml, "arpeggiate")) {
            n.tech.arpeggiate = true;
            xml.skipCurrentElement();
        }
        else if (is(xml, "slide") || is(xml, "glissando")) {
            if (attr(xml, "type") == QLatin1String("start")) n.tech.slide = true;
            xml.skipCurrentElement();
        }
        else if (is(xml, "articulations")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "staccato")) n.tech.staccato = true;
                else if (is(xml, "accent")) n.tech.accent = true;
                else if (is(xml, "strong-accent")) n.tech.marcato = true;
                xml.skipCurrentElement();
            }
        }
        else if (is(xml, "ornaments")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "wavy-line") || is(xml, "vibrato")) n.tech.vibrato = true;
                xml.skipCurrentElement();
            }
        }
        else xml.skipCurrentElement();   // <tied> is covered by <tie> of the note itself
    }
}

void readNote(QXmlStreamReader &xml, Note &n)
{
    // reader is on <note>
    while (xml.readNextStartElement()) {
        if (is(xml, "rest"))           { n.rest = true;  xml.skipCurrentElement(); }
        else if (is(xml, "chord"))     { n.chord = true; xml.skipCurrentElement(); }
        else if (is(xml, "grace"))     { n.grace = true; xml.skipCurrentElement(); }
        else if (is(xml, "duration"))  n.duration = readInt(xml);
        else if (is(xml, "voice"))     n.voice = readInt(xml, 1);
        else if (is(xml, "staff"))     n.staff = readInt(xml, 1);
        else if (is(xml, "type"))      n.type = readText(xml);
        else if (is(xml, "dot"))       { ++n.dots; xml.skipCurrentElement(); }
        else if (is(xml, "beam")) {
            const QString num = attr(xml, "number");
            const QString val = readText(xml);
            if (num == QLatin1String("1") || num.isEmpty()) n.beam1 = val;
            else if (num == QLatin1String("2"))             n.beam2 = val;
        }
        else if (is(xml, "tie")) {
            const QString t = attr(xml, "type");
            if (t == QLatin1String("start")) n.tieStart = true;
            else if (t == QLatin1String("stop")) n.tieStop = true;
            xml.skipCurrentElement();
        }
        else if (is(xml, "time-modification")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "actual-notes"))      n.tupletActual = readInt(xml);
                else if (is(xml, "normal-notes")) n.tupletNormal = readInt(xml);
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "pitch")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "step"))        n.step = readText(xml);
                else if (is(xml, "alter"))  n.alter = qRound(readDouble(xml));
                else if (is(xml, "octave")) n.octave = readInt(xml);
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "notations")) readNotations(xml, n);
        else if (is(xml, "lyric")) {
            MusicXmlConverter::Lyric l;
            const QString num = attr(xml, "number");
            l.verse = num.isEmpty() ? 1 : qMax(1, num.toInt());
            while (xml.readNextStartElement()) {
                if (is(xml, "syllabic"))    l.syllabic = readText(xml);
                else if (is(xml, "text"))   l.text += readText(xml);      // several <text> with <elision>
                else if (is(xml, "extend")) { l.extend = true; xml.skipCurrentElement(); }
                else xml.skipCurrentElement();
            }
            if (!l.text.isEmpty() || l.extend)
                n.lyrics.append(l);
        }
        else xml.skipCurrentElement();
    }
    if (n.grace)
        n.duration = 0;     // grace notes take no time
}

void readAttributes(QXmlStreamReader &xml, Measure &m, Part &part)
{
    while (xml.readNextStartElement()) {
        if (is(xml, "divisions")) {
            m.divisions = readInt(xml);
        }
        else if (is(xml, "key")) {
            m.hasKey = true;
            while (xml.readNextStartElement()) {
                if (is(xml, "fifths"))    m.keyFifths = readInt(xml);
                else if (is(xml, "mode")) m.keyMode = readText(xml);
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "time")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "beats"))          m.beats = readInt(xml);
                else if (is(xml, "beat-type")) m.beatType = readInt(xml);
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "clef")) {
            const int staff = attr(xml, "number").isEmpty() ? 1 : attr(xml, "number").toInt();
            while (xml.readNextStartElement()) {
                if (is(xml, "sign")) part.clefs.insert(staff, readText(xml));
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "transpose")) {
            const int staff = attr(xml, "number").isEmpty() ? 1 : attr(xml, "number").toInt();
            while (xml.readNextStartElement()) {
                if (is(xml, "octave-change")) part.octaveChange.insert(staff, readInt(xml));
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "staff-details")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "staff-lines")) part.staffLines = readInt(xml);
                else if (is(xml, "capo"))   part.capo = readInt(xml);
                else if (is(xml, "staff-tuning")) {
                    Tuning t;
                    t.line = attr(xml, "line").toInt();
                    while (xml.readNextStartElement()) {
                        if (is(xml, "tuning-step"))        t.step = readText(xml);
                        else if (is(xml, "tuning-alter"))  t.alter = qRound(readDouble(xml));
                        else if (is(xml, "tuning-octave")) t.octave = readInt(xml);
                        else xml.skipCurrentElement();
                    }
                    // replace an earlier entry for the same line (TAB + notation staff)
                    bool replaced = false;
                    for (Tuning &e : part.tuning)
                        if (e.line == t.line) { e = t; replaced = true; }
                    if (!replaced)
                        part.tuning.append(t);
                }
                else xml.skipCurrentElement();
            }
        }
        else xml.skipCurrentElement();
    }
}

void readHarmony(QXmlStreamReader &xml, MusicXmlConverter::Harmony &h)
{
    int frameStrings = 0;
    QList<QPair<int, int>> frameNotes;      // (string, fret)
    while (xml.readNextStartElement()) {
        if (is(xml, "root")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "root-step"))       h.root = readText(xml);
                else if (is(xml, "root-alter")) h.rootAlter = qRound(readDouble(xml));
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "kind")) h.kind = readText(xml);
        else if (is(xml, "bass")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "bass-step"))       h.bass = readText(xml);
                else if (is(xml, "bass-alter")) h.bassAlter = qRound(readDouble(xml));
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "frame")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "frame-strings"))   frameStrings = readInt(xml);
                else if (is(xml, "first-fret")) h.firstFret = readInt(xml, 1);
                else if (is(xml, "frame-note")) {
                    int st = 0, fr = -1;
                    while (xml.readNextStartElement()) {
                        if (is(xml, "string"))    st = readInt(xml);
                        else if (is(xml, "fret")) fr = readInt(xml, -1);
                        else xml.skipCurrentElement();
                    }
                    if (st > 0) frameNotes.append({ st, fr });
                }
                else xml.skipCurrentElement();
            }
        }
        else xml.skipCurrentElement();
    }
    if (frameStrings > 0) {
        h.frets = QList<int>(frameStrings, -1);     // strings without <frame-note> are not played (x)
        for (const auto &fn : frameNotes)
            if (fn.first <= frameStrings)
                h.frets[fn.first - 1] = fn.second;
    }
}

void readBarline(QXmlStreamReader &xml, Measure &m)
{
    while (xml.readNextStartElement()) {
        if (is(xml, "repeat")) {
            const QString dir = attr(xml, "direction");
            if (dir == QLatin1String("forward"))  m.repeatStart = true;
            if (dir == QLatin1String("backward")) {
                m.repeatEnd = true;
                m.repeatTimes = attr(xml, "times").toInt();
            }
            xml.skipCurrentElement();
        }
        else if (is(xml, "ending")) {
            const QString type = attr(xml, "type");
            if (type == QLatin1String("start"))
                m.ending = attr(xml, "number");
            else if (type == QLatin1String("stop") || type == QLatin1String("discontinue"))
                m.endingStop = true;
            xml.skipCurrentElement();
        }
        else xml.skipCurrentElement();
    }
}

void readMeasure(QXmlStreamReader &xml, Measure &m, Part &part)
{
    int pos = 0;            // current time position in divisions
    int lastOnset = 0;      // onset of the previous non-chord note (for <chord/>)

    while (xml.readNextStartElement()) {
        if (is(xml, "note")) {
            Note n;
            readNote(xml, n);
            if (n.chord) {
                n.position = lastOnset;
            } else {
                n.position = pos;
                lastOnset = pos;
                pos += n.duration;
            }
            // lyrics by position, each verse once (notation and TAB staff may both carry them)
            if (!n.grace)
                for (const auto &l : n.lyrics) {
                    bool dup = false;
                    for (const auto &e : m.lyrics)
                        if (e.position == n.position && e.lyric.verse == l.verse) dup = true;
                    if (!dup && !l.text.isEmpty())
                        m.lyrics.append({ n.position, l });
                }
            m.notes.append(n);
        }
        else if (is(xml, "backup") || is(xml, "forward")) {
            const bool back = is(xml, "backup");
            int d = 0;
            while (xml.readNextStartElement()) {
                if (is(xml, "duration")) d = readInt(xml);
                else xml.skipCurrentElement();
            }
            pos = back ? qMax(0, pos - d) : pos + d;
        }
        else if (is(xml, "attributes")) {
            readAttributes(xml, m, part);
        }
        else if (is(xml, "harmony")) {
            MusicXmlConverter::Harmony h;
            h.position = pos;
            readHarmony(xml, h);
            // Guitar Pro repeats the chord symbol for the TAB staff: keep it once
            bool dup = false;
            for (const auto &e : m.harmonies)
                if (e.position == h.position && e.root == h.root && e.kind == h.kind && e.frets == h.frets)
                    dup = true;
            if (!dup)
                m.harmonies.append(h);
        }
        else if (is(xml, "direction")) {
            // <direction><direction-type>words | dynamics | metronome</direction-type><sound tempo="120"/></direction>
            MusicXmlConverter::DirectionText dt;
            dt.position = pos;
            while (xml.readNextStartElement()) {
                if (is(xml, "sound")) {
                    if (!attr(xml, "tempo").isEmpty())
                        m.tempo = attr(xml, "tempo").toDouble();
                    xml.skipCurrentElement();
                }
                else if (is(xml, "direction-type")) {
                    while (xml.readNextStartElement()) {
                        if (is(xml, "words")) {
                            dt.words = readText(xml);
                        } else if (is(xml, "dynamics")) {
                            while (xml.readNextStartElement()) {  // <mf/>, <p/>, ...
                                dt.dynamics = xml.name().toString();
                                xml.skipCurrentElement();
                            }
                        } else {
                            xml.skipCurrentElement();
                        }
                    }
                }
                else xml.skipCurrentElement();
            }
            if (!dt.words.isEmpty() || !dt.dynamics.isEmpty()) {
                bool dup = false;                   // repeated for the TAB staff
                for (const auto &e : m.texts)
                    if (e.position == dt.position && e.words == dt.words && e.dynamics == dt.dynamics)
                        dup = true;
                if (!dup)
                    m.texts.append(dt);
            }
        }
        else if (is(xml, "sound")) {
            if (!attr(xml, "tempo").isEmpty())
                m.tempo = attr(xml, "tempo").toDouble();
            xml.skipCurrentElement();
        }
        else if (is(xml, "barline")) {
            readBarline(xml, m);
        }
        else if (is(xml, "print")) {
            if (attr(xml, "new-system") == QLatin1String("yes"))
                m.newSystem = true;
            xml.skipCurrentElement();
        }
        else xml.skipCurrentElement();
    }
}

} // namespace

MusicXmlConverter::Result MusicXmlConverter::convertDevice(QIODevice *device, const QString &sourceName)
{
    Result r;
    r.score.sourceFile = sourceName;

    QXmlStreamReader xml(device);

    // Root element must be <score-partwise>
    if (!xml.readNextStartElement()) {
        r.error = xml.hasError()
            ? tr("XML error in line %1: %2").arg(xml.lineNumber()).arg(xml.errorString())
            : tr("The file is empty.");
        return r;
    }
    if (is(xml, "score-timewise")) {
        r.error = tr("Timewise MusicXML is not supported. Please export the file as partwise MusicXML.");
        return r;
    }
    if (!is(xml, "score-partwise")) {
        r.error = tr("This is not a MusicXML file (root element <%1>).").arg(xml.name().toString());
        return r;
    }

    QHash<QString, QString> partNames;      // id -> name from <part-list>
    QString workTitle, movementTitle;

    while (xml.readNextStartElement()) {
        if (is(xml, "work")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "work-title")) workTitle = readText(xml);
                else xml.skipCurrentElement();
            }
        }
        else if (is(xml, "movement-title")) {
            movementTitle = readText(xml);
        }
        else if (is(xml, "identification")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "creator") && attr(xml, "type") == QLatin1String("composer"))
                    r.score.composer = readText(xml);
                else
                    xml.skipCurrentElement();
            }
        }
        else if (is(xml, "part-list")) {
            while (xml.readNextStartElement()) {
                if (is(xml, "score-part")) {
                    const QString id = attr(xml, "id");
                    while (xml.readNextStartElement()) {
                        if (is(xml, "part-name")) partNames.insert(id, readText(xml));
                        else xml.skipCurrentElement();
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        }
        else if (is(xml, "part")) {
            Part part;
            part.id = attr(xml, "id");
            part.name = partNames.value(part.id, part.id);
            while (xml.readNextStartElement()) {
                if (is(xml, "measure")) {
                    Measure m;
                    m.number = attr(xml, "number");
                    m.pickup = (m.number == QLatin1String("0")) || attr(xml, "implicit") == QLatin1String("yes");
                    readMeasure(xml, m, part);
                    part.measures.append(m);
                } else {
                    xml.skipCurrentElement();
                }
            }
            std::sort(part.tuning.begin(), part.tuning.end(),
                      [](const Tuning &a, const Tuning &b) { return a.line < b.line; });
            for (Measure &m : part.measures)
                for (Harmony &h : m.harmonies)
                    deriveBassFromFrame(h, part.tuning);
            resolveLegato(part);
            r.score.parts.append(part);
        }
        else {
            xml.skipCurrentElement();
        }
    }

    if (xml.hasError()) {
        r.error = tr("XML error in line %1: %2").arg(xml.lineNumber()).arg(xml.errorString());
        return r;
    }

    r.score.title = !workTitle.isEmpty() ? workTitle : movementTitle;
    r.latex = generateLatex(r.score);
    r.ok = true;
    return r;
}

/* =====================================================================
 *  Helpers for generators
 * ===================================================================== */

QString MusicXmlConverter::pitchName(const Note &n)
{
    if (n.rest)
        return QString();
    QString s = n.step;
    if (n.alter > 0) s += QString(n.alter, QLatin1Char('#'));
    if (n.alter < 0) s += QString(-n.alter, QLatin1Char('b'));
    return s + QString::number(n.octave);
}

QString MusicXmlConverter::keyName(int fifths, const QString &mode)
{
    static const char *major[] = { "Cb","Gb","Db","Ab","Eb","Bb","F","C","G","D","A","E","B","F#","C#" };
    static const char *minor[] = { "Ab","Eb","Bb","F","C","G","D","A","E","B","F#","C#","G#","D#","A#" };
    const int i = qBound(-7, fifths, 7) + 7;
    const bool isMinor = (mode == QLatin1String("minor"));
    return QString::fromLatin1(isMinor ? minor[i] : major[i])
           + (isMinor ? QStringLiteral(" minor") : QStringLiteral(" major"));
}

QString MusicXmlConverter::stepName(const QString &step, int alter)
{
    QString s = step;
    if (alter > 0) s += QString(alter, QLatin1Char('#'));
    if (alter < 0) s += QString(-alter, QLatin1Char('b'));
    return s;
}

static int pitchClass(const QString &step, int alter)
{
    static const QHash<QString, int> base = {
        { QStringLiteral("C"), 0 }, { QStringLiteral("D"), 2 }, { QStringLiteral("E"), 4 },
        { QStringLiteral("F"), 5 }, { QStringLiteral("G"), 7 }, { QStringLiteral("A"), 9 },
        { QStringLiteral("B"), 11 } };
    return ((base.value(step, 0) + alter) % 12 + 12) % 12;
}

void MusicXmlConverter::deriveBassFromFrame(Harmony &h, const QList<Tuning> &tuning)
{
    if (!h.bass.isEmpty() || h.frets.isEmpty() || tuning.size() != h.frets.size())
        return;
    // Lowest played string = highest string number with a fret >= 0.
    // Tuning is sorted by line: line 1 = lowest string = string N.
    const int n = int(h.frets.size());
    for (int str = n; str >= 1; --str) {
        const int fret = h.frets.at(str - 1);
        if (fret < 0)
            continue;
        const Tuning &t = tuning.at(n - str);
        const int pc = (pitchClass(t.step, t.alter) + fret) % 12;
        if (pc == pitchClass(h.root, h.rootAlter))
            return;                                 // root in the bass: no slash chord
        static const char *sharps[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        static const char *flats[]  = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
        const QString name = QString::fromLatin1(h.rootAlter < 0 ? flats[pc] : sharps[pc]);
        h.bass = name.left(1);
        h.bassAlter = name.size() > 1 ? (name.at(1) == QLatin1Char('#') ? 1 : -1) : 0;
        return;
    }
}

void MusicXmlConverter::resolveLegato(Part &part)
{
    // Flat list of all sounding notes in score order
    struct Ref { int m; int i; };
    QList<Ref> refs;
    for (int m = 0; m < part.measures.size(); ++m)
        for (int i = 0; i < part.measures[m].notes.size(); ++i)
            if (!part.measures[m].notes[i].rest)
                refs.append({ m, i });

    for (int k = 0; k < refs.size(); ++k) {
        Note &n = part.measures[refs[k].m].notes[refs[k].i];
        if (!n.tech.hammerOn || n.string <= 0 || n.fret < 0)
            continue;
        for (int j = k + 1; j < refs.size(); ++j) {
            const Note &t = part.measures[refs[j].m].notes[refs[j].i];
            if (t.staff != n.staff || t.voice != n.voice || t.string != n.string)
                continue;
            if (t.fret >= 0 && t.fret < n.fret) {   // going down on the same string
                n.tech.hammerOn = false;
                n.tech.pullOff = true;
            }
            break;
        }
    }
}

QString MusicXmlConverter::tuningName(const QList<Tuning> &t)
{
    QStringList parts;
    for (const Tuning &e : t) {
        QString s = e.step;
        if (e.alter > 0) s += QString(e.alter, QLatin1Char('#'));
        if (e.alter < 0) s += QString(-e.alter, QLatin1Char('b'));
        parts << s + QString::number(e.octave);
    }
    return parts.join(QLatin1Char(' '));
}

QString MusicXmlConverter::noteToString(const Note &n)
{
    QString s = n.chord ? QStringLiteral("+") : QString();
    if (n.rest) {
        s += QStringLiteral("r");
    } else {
        s += pitchName(n);
        if (n.string > 0 && n.fret >= 0)
            s += QStringLiteral("(s%1f%2)").arg(n.string).arg(n.fret);
    }
    s += QLatin1Char(':') + (n.type.isEmpty() ? QStringLiteral("?") : n.type) + QString(n.dots, QLatin1Char('.'));
    if (n.tupletActual > 0)  s += QStringLiteral("/%1:%2").arg(n.tupletActual).arg(n.tupletNormal);
    if (n.voice != 1)        s += QStringLiteral("[v%1]").arg(n.voice);
    if (n.grace)             s += QStringLiteral("~grace");
    if (n.tieStart)          s += QStringLiteral("~tie>");
    if (n.tieStop)           s += QStringLiteral("~>tie");
    if (n.slurStart)         s += QStringLiteral("~slur(");
    if (n.slurStop)          s += QStringLiteral("~)slur");
    if (n.tech.hammerOn)     s += QStringLiteral("~H");
    if (n.tech.pullOff)      s += QStringLiteral("~P");
    if (n.tech.slide)        s += QStringLiteral("~sl");
    if (n.tech.bend != 0.0)  s += QStringLiteral("~b%1").arg(n.tech.bend);
    if (n.tech.harmonic)     s += QStringLiteral("~harm");
    if (n.tech.vibrato)      s += QStringLiteral("~vib");
    if (n.tech.palmMute)     s += QStringLiteral("~PM");
    if (n.tech.staccato)     s += QStringLiteral("~stacc");
    if (n.tech.accent)       s += QStringLiteral("~acc");
    if (n.tech.marcato)      s += QStringLiteral("~marc");
    if (n.tupletStart)       s += QStringLiteral("~tup(") + (n.tupletBracket ? QStringLiteral("[]") : QString());
    if (n.tupletStop)        s += QStringLiteral("~)tup");
    for (const auto &l : n.lyrics)
        s += QStringLiteral("~ly%1:%2%3").arg(l.verse).arg(l.text,
                 (l.syllabic == QLatin1String("begin") || l.syllabic == QLatin1String("middle")) ? QStringLiteral("-") : QString());
    if (!n.tech.pluck.isEmpty())     s += QStringLiteral("~") + n.tech.pluck;
    if (!n.tech.fingering.isEmpty()) s += QStringLiteral("~f") + n.tech.fingering;
    if (n.tech.arpeggiate)   s += QStringLiteral("~arp");
    return s;
}

/* =====================================================================
 *  LaTeX generation
 * ===================================================================== */

// Commented overview of ALL parsed data - handy to check the parser against
// new Guitar Pro exports. The real output is generateLatex() (musixtexgenerator.cpp).
QString MusicXmlConverter::generateDataDump(const Score &score)
{
    QStringList out;
    const QString rule = QStringLiteral("% ") + QString(68, QLatin1Char('-'));
    out << rule
        << QStringLiteral("% GuitarPROtoTeX-Convert - LaTeX snippet")
        << QStringLiteral("% Source:   %1").arg(score.sourceFile)
        << QStringLiteral("% Title:    %1").arg(score.title.isEmpty() ? QStringLiteral("-") : score.title)
        << QStringLiteral("% Composer: %1").arg(score.composer.isEmpty() ? QStringLiteral("-") : score.composer)
        << QStringLiteral("% Parts: %1, measures: %2, notes: %3")
               .arg(score.parts.size()).arg(score.measureCount()).arg(score.noteCount())
        << rule
        << QStringLiteral("% Notes: pitch(string/fret):type, +chord, /tuplet, [voice], ~ties/techniques")
        << QString();

    for (const Part &p : score.parts) {
        const int staff = p.primaryStaff();
        out << QStringLiteral("% Part %1: %2").arg(p.id, p.name);
        if (!p.clefs.isEmpty()) {
            QStringList st;
            for (auto it = p.clefs.cbegin(); it != p.clefs.cend(); ++it)
                st << QStringLiteral("%1=%2").arg(it.key()).arg(it.value());
            std::sort(st.begin(), st.end());
            out << QStringLiteral("%   Staves: %1%2").arg(st.join(QStringLiteral(", ")),
                       staff > 0 ? QStringLiteral(" (listing staff %1 only)").arg(staff) : QString());
        }
        if (!p.tuning.isEmpty())
            out << QStringLiteral("%   Tuning: %1 (%2 lines)%3")
                       .arg(tuningName(p.tuning)).arg(p.staffLines)
                       .arg(p.capo > 0 ? QStringLiteral(", capo %1").arg(p.capo) : QString());

        for (const Measure &m : p.measures) {
            QStringList info;
            if (m.beats > 0)        info << QStringLiteral("time %1/%2").arg(m.beats).arg(m.beatType);
            if (m.hasKey)           info << QStringLiteral("key %1").arg(keyName(m.keyFifths, m.keyMode));
            if (m.tempo > 0.0)      info << QStringLiteral("tempo %1").arg(m.tempo);
            if (m.divisions > 0)    info << QStringLiteral("div %1").arg(m.divisions);
            if (m.pickup)           info << QStringLiteral("pickup");
            for (const auto &t : m.texts)
                info << (t.words.isEmpty() ? QStringLiteral("dyn %1").arg(t.dynamics)
                                           : QStringLiteral("\"%1\"").arg(t.words));
            if (m.repeatStart)      info << QStringLiteral("|:");
            if (!m.ending.isEmpty())info << QStringLiteral("ending %1").arg(m.ending);
            if (m.repeatEnd)        info << (m.repeatTimes > 0 ? QStringLiteral(":| x%1").arg(m.repeatTimes)
                                                               : QStringLiteral(":|"));
            if (!info.isEmpty())
                out << QStringLiteral("%   [%1]").arg(info.join(QStringLiteral(", ")));

            if (!m.harmonies.isEmpty()) {
                QStringList ch;
                for (const auto &h : m.harmonies)
                    ch << QStringLiteral("%1@%2").arg(h.name()).arg(h.position);
                out << QStringLiteral("%   Chords: %1").arg(ch.join(QLatin1Char(' ')));
            }

            QStringList notes;
            for (const Note &n : m.notes)
                if (staff == 0 || n.staff == staff)
                    notes << noteToString(n);
            out << QStringLiteral("%   Measure %1: %2").arg(m.number, notes.join(QLatin1Char(' ')));
        }
        out << QString();
    }
    return out.join(QLatin1Char('\n'));
}
