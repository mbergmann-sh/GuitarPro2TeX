> Entwicklungsnotizen (Deutsch) aus der Entstehung des Programms, neueste Änderungen zuerst.
> Pfade und Dateinamen beziehen sich teils auf den damaligen Arbeitsstand.

# GuitarPROtoTeX-Convert – Entwicklungsnotizen

Dieses Archiv enthält den vollständigen Quellcode. Es ersetzt alle bisherigen Patches und Gesamtstände.

## Neu in diesem Stand

### Amazing Grace (echter Guitar-Pro-Export mit Liedtext)

- **Liedtext, wie Guitar Pro ihn exportiert:**
  - Jede Silbe ist `single`, der Trennstrich steht schon im Text („A-“, „ma-“).
  - Noten ohne neue Silbe tragen den Text `__`; er wird wie bei Guitar Pro als kurze Linie gesetzt.
  - Der Text hängt an Noten- und TAB-System und wird pro Position nur einmal gesetzt.
- **Notation mit oder ohne Oktavierung:** Der Generator folgt jetzt der Datei.
  - Gitarrenspuren mit `<octave-change>-1</octave-change>` werden eine Oktave höher mit oktaviertem Violinschlüssel notiert, wie bisher.
  - Spuren ohne Oktavierungsangabe, z. B. „Singer“ in Amazing Grace, werden wie in Guitar Pro im normalen Violinschlüssel notiert.
  - Reine TAB-Dateien ohne Notensystem folgen wie bisher der Gitarrenkonvention.
- **Neue Beispiele:** `samples/AmazingGrace.xml` mit `.tex` und `_MusiXTeX.pdf`.


### Rhythmus unter der TAB, Liedtext

- **Rhythmus unter der TAB** (wie bei Guitar Pro):
  - Hälse, Fähnchen, Balken (auch 16tel), Punkte und Triolenklammern stehen unter der TAB; Halbe haben einen kürzeren Hals, Ganze keinen.
  - Technik: MusiXTeX' normale Hals-/Balkenroutinen mit unsichtbarem Notenkopf (`\gptHeadless` / `\gptRestoreHeads`).
    - Der Kopf ist das echte Zeichen in Weiß; ein leerer Ersatzkopf hatte andere Maße und machte die Zeilen zu breit.
    - Hilfslinien werden dabei unterdrückt.
  - Der Rhythmus folgt der oberen Stimme, die am jeweiligen Anschlag spielt.
  - Abschaltbar über die Konstante `TAB_RHYTHM` in `musixtexgenerator.cpp` (Kandidat für ein Preset).
  - PIMA steht bei eingeschaltetem Rhythmus unter den Rhythmusbalken; der untere Rand ist entsprechend größer.
- **Liedtext:**
  - `<lyric>` wird gelesen: Silbe, `syllabic`, Strophennummer, `<extend/>`. Weil Guitar Pro den Text vermutlich nur an die Noten des Notensystems hängt, sammelt der Parser ihn pro Takt und Position aus allen Systemen (`Measure::lyrics`).
  - Der Text steht zentriert unter der Note, zwischen Notensystem und TAB; Trennstrich nach Silben am Wortanfang und in der Wortmitte; eine Zeile pro Strophe.
  - Die Höhe richtet sich nach der tiefsten Note des Stücks, bei zwei Stimmen tiefer. Der Abstand zur TAB wird passend vergrößert (`\setinterinstrument`).
  - Beispiel: `samples/Greensleeves_mit_Liedtext.xml` (Text von Hand eingefügt, Standard-MusicXML).


### Triolenklammern, Marcato, Swing-Symbol, doppelte Noten zweier Stimmen

- **Triolenklammern:** Guitar Pro exportiert Triolen mit `<tuplet type="start|stop" bracket="yes" placement="above">`.
  - Der Parser liest das jetzt (`Note::tupletStart/Stop/Bracket/Placement`); der Generator setzt eine Klammer mit Zahl (`\uptuplet`/`\downtuplet`), bei `bracket="no"` nur die Zahl.
  - Ältere Dateien ohne `<tuplet>` bekommen wie bisher nur die Zahl.
- **Marcato:** Die „betonten Noten“ aus Guitar Pro sind `<strong-accent/>`, also Marcato (^, unter der Note als v), nicht der Akzent „>“. Beides wird jetzt unterschieden (`\usfz`/`\lsfz` bzw. `\usf`/`\lsf`).
- **Swing-Angabe:** Guitar Pro 8 exportiert „Triplet Feel“ **nicht** nach MusicXML; in der Datei steht dazu nichts.
  - Abhilfe: In Guitar Pro an der Stelle einen Text einfügen, der nur aus einem Stichwort besteht: „Swing“, „Swing Feel“, „Shuffle“, „Triplet Feel“ oder „Triolenfeeling“ (Groß-/Kleinschreibung egal).
  - Der Converter setzt statt des Textes das Symbol (♪♪ = ♩³♪). Beispiel: `samples/Notenwerte_mit_Swing-Text.xml`.
- **Doppelte Noten zweier Stimmen:** Steht dieselbe Tonhöhe gleichzeitig in Stimme 1 und Stimme 2, erscheint im Notensystem nur noch die Note aus Stimme 2 (Hals nach unten, eigener Notenwert).
  - Ihre Bögen, Haltebögen, H/P und Artikulationen gehen auf die verbleibende Note über, zum Beispiel der Hammer-on-Bogen vom Vorschlag zur Hauptnote.
  - Ist die Note Teil einer Balken- oder Triolengruppe von Stimme 1, bleibt sie erhalten, damit die Gruppe intakt bleibt.
  - TAB und PIMA sind unverändert.


### Buch-Übersetzung: „Emergency stop“, Seitenfüllung; Akzente, Bendings, Techniken

- **„Emergency stop“ bei `\bar`:** Die Ursache war eine veraltete `.mx2` aus einem früheren Lauf. MusiXTeX liest sie schon im ersten pdflatex-Lauf; kam inzwischen ein Song dazu, liest TeX über ihr Ende hinaus. Auch die Überlänge-Warnungen von 1,5–2,6 pt kamen daher.
  - `samples/build_musixtex.bat` löscht jetzt zuerst `.mx1`/`.mx2`.
- **„Underfull \vbox“:** Die Klasse `book` streckt im zweiseitigen Satz jede Seite auf volle Höhe, Notensysteme sind aber unteilbar. Abhilfe: `\raggedbottom` in der Präambel (auch im `Rahmendokument.tex`).
- **Akzente und Staccato:** `>` bzw. `.` stehen auf der Notenkopfseite (`\usf`/`\lsf`, `\upz`/`\lpz`).
- **Bendings:** Pfeil mit Umfang über der TAB („↗full“, „↗½“, „↗1½“ …).
- **Weitere Techniken über der TAB:** Vibrato (∼∼), „P.M.“, „sl.“; Flageolett als `<12>` in der TAB.
  - H/P und diese Beschriftungen stehen jetzt direkt über der obersten TAB-Linie (vorher für die kleinere TAB bemessen und dadurch zu hoch).
- **Akkordnamen:** Folgt ein Akkord weniger als eine halbe Note nach dem vorigen, wird er eine Stufe höher gesetzt (Ring Of Fire, T. 14: „Cmaj7/G“ + „D7“).
- **Neues Beispiel:** `samples/Notenwerte.xml` mit `.tex` und `_MusiXTeX.pdf`.


### TAB-Saitenabstand weiter vergrößert

- **Linienabstand:** Die TAB-Linien liegen jetzt 1,75-fach auseinander (vorher 1,44). `\setsize` akzeptiert beliebige Faktoren; der Wert steht als Konstante `TAB_SIZE` in `musixtexgenerator.cpp` (Kandidat für ein späteres Preset).
- **Bundzahlen:** unverändert fest 8 pt Helvetica fett.
- **PIMA-Abstand:** Der Platz unter der TAB für PIMA ist entsprechend vergrößert (`\staffbotmarg=11\Interligne`).
- **Geprüft:** Faktoren 1,44 / 1,6 / 1,75 / 2,0 verglichen; 1,75 trennt auch fünf gestapelte, geklammerte Bundzahlen klar. Alle Beispiele und das Testbuch (`book`, `scrbook`) laufen mit 0 Fehlern.


### Lesbarkeit: größerer Saitenabstand in der TAB, dünnere Balken

- **TAB:**
  - Linienabstand 1,44-fach (`\setsize{1}{\Largevalue}`, vorher 1,2-fach) – inzwischen 1,75, siehe oben.
  - Die Bundzahlen haben eine feste Größe von 8 pt Helvetica fett (`\def\tabfnt{\tabfnteight}`) und wachsen nicht mehr mit.
  - Auch fünf geklammerte Zahlen übereinander (gehaltener Akkord) berühren sich nicht mehr.
  - Nebenbei umgangen: MusiXTeX verweist bei `\Largevalue` auf den falsch geschriebenen Font `\tabfntwentynine`. Mit der eigenen Ziffernschrift tritt das nicht mehr auf.
- **Balken:**
  - Nur waagerechte Balken zeichnet MusiXTeX als Linie mit einstellbarer Dicke; schräge Balken kommen aus einem Font mit fester Dicke. Alle Balken sind deshalb jetzt waagerecht und etwa 20 % dünner (`\b@amthick .19` statt `.24`, 16tel-Abstand `.6` statt `.75`).
  - Der Balken liegt über der höchsten (Hals oben) bzw. unter der tiefsten Note (Hals unten) der Gruppe und schneidet keine Notenköpfe.
  - Die Triolen-„3“ steht entsprechend oberhalb bzw. unterhalb des Balkens.
- **PIMA:** Etwas mehr Platz unter der TAB (`\staffbotmarg=8\Interligne`), weil die TAB höher geworden ist.

Geprüft: Greensleeves (16tel-Doppelbalken), Beispiel-Song, Technik-Test, Song-Test und das Testbuch (`book` und `scrbook`) mit 0 Fehlern und ohne überlange Notenzeilen.


### Buchprojekt: Zeilenausgleich, TAB-Abstände, scrbook, Save-Button

- **Leere Systeme bis zum Rand:** Das war kein Generatorfehler. Das Buch wurde nur mit pdflatex übersetzt; musixflx ist nie gelaufen (keine `.mx2`).
  - MusiXTeX gleicht Notenzeilen nur im Dreischritt pdflatex → musixflx → pdflatex auf Blockbreite aus.
  - Neu: `samples/build_musixtex.bat` (Aufruf: `build_musixtex.bat testsonbook_main`).
  - In TeXstudio lässt sich der Dreischritt sinngemäß als Benutzerbefehl eintragen: `txs:///pdflatex | musixflx % | txs:///pdflatex`.
  - Bei mehreren Snippets läuft musixflx auf der **Hauptdatei**; der Kopfkommentar der Snippets sagt das jetzt auch.
- **Überlappende Bundzahlen:** Die TAB wird jetzt 1,2-fach gesetzt (`\setsize{1}{\largevalue}`). Linienabstand und Ziffern wachsen gemeinsam; Ziffern auf benachbarten Saiten berühren sich nicht mehr. Das Notensystem bleibt unverändert.
- **scrbook:** Die Ursache war das veraltete `\it` von MusiXTeX (Taktnummern), das KOMA-Klassen verbieten.
  - Jedes Snippet leitet jetzt innerhalb der `music`-Umgebung die alten Schriftbefehle (`\it \bf \rm \sl \sf \tt \sc`) auf die modernen um.
  - Das Testbuch übersetzt mit `book` und `scrbook` fehlerfrei; ein Preset ist dafür nicht nötig.
- **Save-Button:** Er sitzt an der Stelle des früheren Convert-Buttons, mit demselben Symbol wie File > Save und dem Text „Save“ bzw. „Speichern“. Er verhält sich wie der Menüeintrag.
- **Behobener Fehler:** Bei jedem Sprachwechsel wurde das Feld „LaTeX exportieren“ geleert. Ursache war eine leere `text`-Eigenschaft im `.ui`, die `retranslateUi()` jedes Mal zurückschrieb.

**Für später notiert**
- **Preset „Takte pro Zeile“:** Bei schmalem Satzspiegel passen oft nur 2 dichte Takte in eine Zeile; MusiXTeX bricht dann selbst um.
- **Akkordnamen:** Stehen zwei Akkordsymbole dicht beieinander, überlappen sie (Ring Of Fire, Takt 14).


### Mehrstimmigkeit, PIMA-Fingersatz, Arpeggio, Vorschlagsnoten

Geprüft am zweistimmigen `samples/Beispiel-Song_zweistimmig.xml`. Ergebnis: `…_MusiXTeX.pdf`.

- **Mehrere Stimmen:**
  - Die erste Stimme hat Hälse nach oben, weitere Stimmen nach unten.
  - Jede Stimme behält ihre eigenen Notenwerte (halbe Bassnoten + Viertel-Melodie).
  - Nur die jeweils letzte Stimme eines Anschlags schiebt vor; die übrigen nutzen die nicht vorschiebenden MusiXTeX-Varianten (`\zhl`, `\zqu`, `\zqb` …).
  - Balken, Triolen und Balkennummern werden pro Stimme verwaltet.
  - Eine Note, die in beiden Stimmen steht, bekommt einen Kopf mit zwei Hälsen (notationsüblich).
- **Pausen in einer Stimme:** Sie werden an der Notenposition gesetzt (`\zcharnote` + Pausenzeichen), in der unteren Stimme 2 Stufen tiefer, in der oberen 2 Stufen höher.
- **PIMA-Fingersatz:** Guitar Pro exportiert ihn als `<pluck>`.
  - Er steht kursiv unter der TAB; bei Akkorden gestapelt, höchste Saite oben.
  - Die Greifhand-Finger (`<fingering>`) werden ebenfalls eingelesen, aber noch nicht gesetzt (im Beispiel nicht vorhanden).
- **Arpeggio:** `<arpeggiate/>` wird zur Arpeggio-Schlangenlinie vor dem Akkord (`\larpeggio`). Das gehaltene Arpeggio im Schlusstakt erscheint wie bei Guitar Pro als Akkord mit Haltebögen und geklammerten Bundzahlen.
- **Vorschlagsnoten:** Sie werden jetzt gesetzt (`\grcu`, eigener kleiner Block vor dem Anschlag, kleine Bundzahl in der TAB), inklusive Hammer-on-Bogen zur Hauptnote.
- **Taktnummern:** Ein Wiederholungszeichen ganz am Anfang zählt in MusiXTeX als Taktstrich. Die Nummerierung wird deshalb korrigiert (vorher um 1 zu hoch).

**Hinweis zum Beispiel, Takt 8:** Stimme 2 spielt F3 auf Saite 4 / Bund 3, gleichzeitig mit dem leeren D3 der Stimme 1 auf derselben Saite. Vermutlich ein Eingabeversehen; es wird so gesetzt, wie es in der Datei steht.


### „Convert“ entfernt, About-Text erweitert

- **Convert entfernt:** Button, Menüeintrag (File) und Toolbar-Symbol „Convert“ sind weg; das Laden konvertiert ja sofort. Die Slots `on_actionConvert_triggered()` und `on_pushButtonConvert_clicked()` sind ebenfalls entfernt.
- **Ersatz für den eingetippten Pfad:** Ein Pfad im Feld „MusicXML importieren“ wird mit **Enter** geladen und konvertiert. Ist das Feld leer, öffnet Enter den Dateidialog.
- **About-Dialog** um die Programmbeschreibung erweitert:
  - EN: „This program converts MusicXML files into LaTeX files (MusiXTeX) that can be included in existing LaTeX projects.“
  - DE: „Das Programm übersetzt MusicXML-Dateien in LaTeX-Dateien (MusiXTeX), die in bestehende LaTeX-Projekte eingebunden werden können.“
- **Übersetzungen:** 78/78; 4 veraltete Einträge entfernt, darunter die Convert-Texte und das Kürzel Strg+Alt+C.


### Griffbilder (Paket `gchords`)

- **Legende:** Alle im Stück verwendeten Griffbilder erscheinen wie bei Guitar Pro als Legende über den Noten.
  - Reihenfolge des ersten Auftretens; jede Griffvariante nur einmal.
  - Greensleeves: Am, C, G, Em, E, C mit G als höchstem Ton – dieselben 6 wie im Guitar-Pro-PDF.
- **Darstellung:** ○ = leere Saite, × = nicht gespielt, dicker Sattel in der ersten Lage.
- **Höhere Lagen:** Die Lagenzahl steht seitlich, zum Beispiel „5“.
- **Ohne Griffbilder:** Enthält die Datei keine Griffbilder, fehlen Legende und Paketzeile.

### Kopfkommentar mit allen benötigten Paketen

Der Kopf jeder erzeugten `.tex` listet die Pakete jetzt als kopierfertige Zeilen auf, jeweils mit Zweck:

```
% Required packages (preamble):
%   \usepackage{xcolor}     % white background behind the TAB fret numbers
%   \usepackage{musixtex}   % notation + TAB
%   \usepackage{gchords}    % chord diagrams (\chord)
```

`samples/Rahmendokument.tex` bindet jetzt auch `gchords` ein.


### MusiXTeX-Generator (`musixtexgenerator.cpp`) – vorheriger Stand

Die konvertierte Datei ist jetzt ein echtes **MusiXTeX-Snippet**: ein vollständiger Block `\begin{music} … \end{music}` mit Notensystem und TAB, wie im Guitar-Pro-PDF.

**Aufbau**
- Instrument 2: Notensystem mit Violinschlüssel und kleiner 8 darunter (Gitarre klingt eine Oktave tiefer als notiert).
- Instrument 1: 6-zeilige TAB mit TAB-Schlüssel, ohne Tonart-Vorzeichen.

**Inhalt**
- Noten, Akkorde, Pausen und Punktierungen.
- Vorzeichen, berechnet aus Tonart und bisherigen Vorzeichen im Takt.
- Balken, auch 16tel-Gruppen.
- Haltebögen; gehaltene Bundzahlen stehen wie bei Guitar Pro in Klammern.
- Legatobögen mit H/P.
- Akkordnamen, Tempo und Textanweisungen („Auftakt“).
- Taktnummern, auch ab 0 bei Auftakt.
- Wiederholungen, Volta-Klammern, Triolen, Tonart- und Taktwechsel.
- Zeilenumbrüche an denselben Stellen wie in Guitar Pro (sonst alle 4 Takte).

**Verwendung**
- Im Dokument: `\usepackage{xcolor}` und `\usepackage{musixtex}`, siehe `samples/Rahmendokument.tex`.
- Übersetzen: `musixtex -l -p datei.tex` (entspricht pdflatex → musixflx → pdflatex, mit Blocksatz der Notenzeilen).
- Ein einzelner pdflatex-Lauf funktioniert auch, dann sind die Zeilen aber nicht ausgeglichen.

**Behobene Fallstricke**
- **Bundzahlen:** MusiXTeX setzt den weißen Hintergrund per PostScript; unter pdflatex wurden daraus schwarze Kästchen. Das Snippet zeichnet den Hintergrund jetzt mit `\color`.
- **Zeilenbreite:** Leerzeichen um `&` erzeugen in MusiXTeX Zwischenraum, den musixflx nicht einrechnet, sodass jede Zeile zu breit wurde. Sie werden daher weggelassen.
- **Bögen:** Guitar Pro setzt an Bassnoten Bogen-Start-Kennzeichen ohne passendes Ende. Der Generator erlaubt pro Anschlag nur einen Bogen, bevorzugt an der Note mit Hammer-on/Pull-off.

**Noch nicht umgesetzt**
- Bends und Vibrato als Zeichen.
- Taktangabe im TAB ausblenden.
- Instrumentname (kollidierte mit dem Schlüssel; Hinweis als Kommentar im Snippet).

**Beispiele**
- `samples/Greensleeves.tex`: Snippet, aus dem Programm gespeichert.
- `samples/Greensleeves_MusiXTeX.pdf`: Ergebnis mit `samples/Rahmendokument.tex`.

Die bisherige Datenübersicht bleibt als `generateDataDump()` zur Fehlersuche erhalten.

### Parser an echtem Guitar-Pro-8.1-Export geprüft (Greensleeves)

Die Datei aus Guitar Pro 8.1.4 hat Eigenheiten gezeigt, die jetzt berücksichtigt werden:

- **Zwei Systeme:** Guitar Pro exportiert jede Note doppelt, als Notensystem (Violinschlüssel, eine Oktave höher notiert) und als TAB-System (klingende Tonhöhe).
  - `Part::clefs`, `Part::tabStaff()` und `Part::primaryStaff()` wählen das TAB-System.
  - `noteCount()` zählt nicht mehr doppelt.
  - `Part::octaveChange` hält die Oktavverschiebung des Notensystems fest.
- **Saite/Bund im Notensystem:** Guitar Pro speichert sie dort in einer speziellen Anweisung `<?GP <root><string>…</string><fret>…</fret></root> ?>`. Diese wird jetzt ebenfalls gelesen.
- **Pull-offs:** Guitar Pro exportiert auch Pull-offs als `<hammer-on>H</hammer-on>`. `resolveLegato()` macht daraus einen Pull-off, wenn der Zielton auf derselben Saite tiefer liegt.
- **Akkordsymbole:** `<harmony>` wird mit Griffbild gelesen (`Measure::harmonies`, `Harmony::name()` → „Am“, „C“, „E“ …), inklusive Position im Takt. Die Kopie des TAB-Systems wird entfernt.
  - **Echte Slash-Akkorde** werden aus dem Griffbild abgeleitet, wenn der tiefste gespielte Ton nicht der Grundton ist (→ „C/G“).
  - **Einschränkung:** Das „C/g“ aus dem PDF exportiert Guitar Pro nicht als Namen, sondern als C-Dur mit G als höchstem Ton (Griff x32013). Es erscheint deshalb als „C“ mit eigenem Griffbild.
- **Weitere Angaben:** Auftakt (`Measure::pickup`, Takt 0), Textanweisungen (z. B. „Auftakt“) und Dynamik (`Measure::texts`).

Gegen das PDF geprüft: H in Takt 12/13/20/22/28/29, P in 13/29, E-Dur in Takt 6 auf Zählzeit 3, 218 Noten.
Testdatei: `samples/Greensleeves.xml`.

### New / Neu… (Button und Menü)

- **Zurücksetzen:** Setzt alle Widgets zurück: `lineEditXML`, `lineEditLaTeX` und `textEdit`.
  Auch der zuletzt verwendete Speicherpfad wird vergessen, sodass der nächste Save wieder nachfragt.
- **Sicherheitsabfrage** nur, wenn die LaTeX-Ausgabe noch nicht gespeichert ist, also frisch konvertiert oder nach dem Speichern von Hand geändert.
  Standardknopf ist „Nein“, damit Enter nichts verwirft. Ist der Editor leer oder alles gespeichert, wird ohne Rückfrage zurückgesetzt.
- **Bleibt erhalten:** Einstellungen, Theme und Sprache.

### Konverter: vollständiger Parser

`musicxmlconverter.h/.cpp` liest jetzt alles ein, was ein LaTeX-Generator voraussichtlich braucht:

- **Pro Stimme (Part):** Stimmung der Gitarre (`staff-tuning`, nach Saite sortiert), Anzahl Linien, Kapodaster.
- **Pro Takt:** Divisions, Tonart (mit Namen, z. B. „G major“), Taktart, Tempo, Wiederholungszeichen `|:` und `:|` samt Anzahl, Volta-Klammern.
- **Pro Note:**
  - Tonhöhe, Dauer, Typ, Punktierung;
  - **Startposition im Takt**, korrekt auch bei Akkorden und bei `backup`/`forward` für mehrere Stimmen;
  - Voice, Staff, Saite/Bund;
  - Haltebogen, Legatobogen, Triolen/N-Tolen;
  - Hammer-on, Pull-off, Slide/Glissando, Bend (Halbtöne), Flageolett, Vibrato, Palm Mute, Staccato, Akzent.

`generateLatex()` (MusiXTeX, siehe oben) setzt diese Daten; `generateDataDump()` zeigt sie als Übersicht.
Sie ist die **einzige** Stelle, die das Zielformat kennt, und wird ersetzt, sobald das Format feststeht.

Neue Testdatei: `samples/Technik-Test.musicxml`.

## Themes (Prefs > Theme)

- **Menüaufbau:** Das Untermenü wird dynamisch gefüllt. Zuerst kommen die auf dem System verfügbaren Qt-Stile (`QStyleFactory::keys()`, unter Windows z. B. windows11, windowsvista, Windows, Fusion). Nach einem Trennstrich folgen die beiden Dark Themes aus AmigaED 4.0: **Dark** und **Visual Studio Code Dark**.
- **Namen:** Die Theme-Namen bleiben unübersetzt, wie in AmigaED. Der angezeigte Name ist zugleich der gespeicherte Wert.
- **Häkchen:** Genau ein Eintrag ist angehakt, wie im Sprachmenü. Häkchen statt Radio-Punkt auch unter Fusion.
- **Übernahme aus AmigaED (1:1):**
  - `darkApplicationPalette()` und `vscodeApplicationPalette()`, beide mit Fusion-Stil, weil native Stile eigene Paletten ignorieren;
  - das VS-Code-Stylesheet: blaue Statusleiste #007ACC, Menü- und Menüleisten-Farben, Toolbar;
  - die Dark-Farben für Margin, Cursorzeile und Auswahl;
  - die Syntaxfarben aus `applyLexerDarkColors()`, zugeordnet auf die LaTeX-Styles:
    - Kommentar → comment
    - Befehl → keyword
    - Umgebung → „Typ“-Farbe (Dark: lila, VS Code: türkis)
    - Klammern → preprocessor
    - Mathe → string
    - Zahl → number
    - Sonderzeichen → „Funktions“-Farbe (Dark: bernstein, VS Code: blassgelb)
- **Zurück zu einem hellen Stil:** Palette, Stylesheet und Lexer-Farben werden vollständig zurückgesetzt.
- **Speichern:** Unter `GUI/Theme`, sofort wirksam, auch beim nächsten Start. Leerer Wert = der Stil, den Qt beim Start gewählt hat. Ein auf einem anderen System gespeicherter, hier nicht vorhandener Stil (z. B. windowsvista unter Linux) fällt auf diesen Systemstandard zurück.
- **.ui-Änderung:** Die bisherige Aktion `actionTheme` ist durch das Untermenü `menuTheme` ersetzt.

Screenshots: `screenshots/`

## Laden, Konvertieren, Speichern, Zwischenablage

Button 📂 bzw. File > Load MusicXML File (`on_actionLoad_MusicXML_File_triggered()`):

1. **Datei wählen:** Ein Dateidialog öffnet sich im gespeicherten MusicXML-Standardordner (Fallback: Home). Filter: `*.xml`, `*.musicxml`.
2. **Quelle anzeigen:** Pfad und Name landen in `lineEditXML`.
3. **Zielname vorschlagen:** Derselbe Name mit `.tex` landet in `lineEditLaTeX`, z. B. `Song.musicxml` → `Song.tex`. Ersetzt wird nur der letzte Suffix.
4. **Konvertieren:** Der Inhalt wird konvertiert und in `textEdit` angezeigt.
5. **Zwischenablage:** Copy to Clipboard (Button oder Strg+Shift+C) kopiert den kompletten Inhalt von `textEdit`.

### Speichern

Save und Save As verwenden den Namen aus `lineEditLaTeX`. Der Benutzer darf ihn ändern; `.tex` wird bei Bedarf ergänzt, Pfadanteile werden ignoriert.

- **Save:** Der Dateidialog erscheint beim ersten Mal. Er startet im LaTeX-Standardordner (Fallback: Ordner der MusicXML-Datei, dann Home), der Name ist vorbelegt. Danach überschreibt Save ohne Nachfrage, solange der Name unverändert bleibt. Wird der Name geändert oder eine neue Datei geladen, fragt Save wieder nach.
- **Save As:** fragt immer nach.
- **Nach dem Speichern:** Ein im Dialog gewählter Name wird zurück nach `lineEditLaTeX` übernommen.
- **Sicher schreiben:** Über `QSaveFile` (UTF-8), sodass bei Fehlern keine halb geschriebene `.tex` entsteht.

### Pfad eintippen

Ein Pfad in `lineEditXML` wird mit Enter geladen und konvertiert (früher über „Convert“).

## Konverter (`musicxmlconverter.h/.cpp`)

- **Einlesen:** `score-partwise`-MusicXML wird per `QXmlStreamReader` in ein Datenmodell eingelesen.
  - Kopfdaten: Titel und Komponist.
  - Je Stimme: Takte und Noten mit allen Details (siehe „Konverter: vollständiger Parser“ oben).
- **Fehler:** Klare, übersetzte Meldungen für kaputtes XML (mit Zeilennummer), Nicht-MusicXML, `score-timewise`, fehlende oder leere Dateien. Der Editorinhalt bleibt dabei unverändert.
- **Ausgabe:** MusiXTeX-Snippet über `generateLatex()` in `musixtexgenerator.cpp`.
- **Nicht unterstützt:** komprimiertes `.mxl` (ZIP). Guitar Pro kann unkomprimiert exportieren.

Testdateien: `samples/Song Test.musicxml` und `samples/Technik-Test.musicxml`.

## Einspielen

1. Qt Creator schließen.
2. Alles nach `D:\Projekte\Qt6\GuitarPROtoTeX-Convert\` kopieren und **alles überschreiben**. Neu dazu kommt `musixtexgenerator.cpp`.
3. Den Build-Ordner löschen.
4. Qt Creator öffnen, qmake ausführen, dann bauen.

**Kontrolle:** `class MainWindow : public QMainWindow` steht in `mainwindow.h` in Zeile 25.

## Verifikation (Linux, Qt 6.4, QScintilla, qmake6/g++)

- **Build:** frisch aus genau diesem Ordner, 0 Fehler, 0 Warnungen, 79/79 Übersetzungen (inkl. Plural).
- **Neu/Reset-Test:** 8/8 Prüfungen bestanden.
  - Leerer Editor ohne Rückfrage; ungespeichert mit Rückfrage, „Nein“ behält alles, „Ja“ setzt zurück.
  - Gespeichert ohne Rückfrage; nach Handänderung wieder mit Rückfrage; Einstellungen bleiben erhalten.
- **Notenwerte/Techniken/Liedtext-Test:** 23/23 (inkl. Amazing Grace: Schlüssel, Liedtext, Platzhalter) (inkl. Rhythmus-Triolen, Liedtext) (inkl. Triolenklammern, Marcato, Swing-Symbol) Prüfungen bestanden.
  - Alle Noten- und Pausenwerte; 3 Akzente; „full“-Bending; Triolen.
  - Akkordstaffelung in Ring Of Fire.
  - Flageolett, P.M., Vibrato, Slide, Staccato, Akzent.
- **Testbuch mit allen drei Songs aus den XML-Dateien** (`book` und `scrbook`, sauberer Dreischritt, `\raggedbottom`): 0 Fehler, keine überlange Notenzeile, kein „Underfull \vbox“.
- **Save-Button-Test:** 7/7 Prüfungen bestanden.
  - Beschriftung und Symbol; Position rechts neben „Neu…“.
  - Erster Klick mit Dialog, zweiter Klick ohne Dialog.
  - Sprachwechsel lässt die Dateinamen stehen.
- **Testbuch** (`book` und `scrbook`, Dreischritt): 0 Fehler, keine überlange Notenzeile.
- **Beispiel-Song-Test (zweistimmig):** 16/16 (inkl. keine doppelte Bassnote, übertragener Hammer-on-Bogen) Prüfungen bestanden.
  - PIMA-Zählung; Stimmen; Arpeggio, Vorschlag, Pause.
  - Taktnummern; Stimme 2 als Halbe mit Hals nach unten; gestapeltes PIMA; Pause der Stimme 2.
  - Triole mit P; Vorschlag mit H; Arpeggio + 15 Haltebögen; alle Bögen geschlossen.
  - MusiXTeX: 0 Fehler, 0 überlange Zeilen.
- **About/Convert-Test:** 4/4 Prüfungen bestanden.
  - Keine Convert-Aktion und kein Convert-Button mehr.
  - About-Text auf Englisch und Deutsch korrekt.
- **Generator-Test:** 30/30 (inkl. TAB-Rhythmus) (neu: TAB-Größe/Ziffernschrift, dünne Balken, ausschließlich waagerechte Balken, KOMA-Schriftbefehle) (inkl. Griffbilder: Anzahl, Inhalt, höhere Lage, Paketzeile, keine Legende ohne Griffbilder) Prüfungen bestanden.
  - Aufbau und Oktavierung; Blocksatz-taugliche Syntax (keine Leerzeichen um `&`).
  - Akkorde, Vorzeichen, 16tel-Balken; 6× H und 2× P; alle Bögen geschlossen; Haltebögen.
  - Wiederholung, Volta, Triole, Tonart, 3/4-Takt.
  - Greensleeves mit Griffbildern: 0 Fehler, 0 überlange Zeilen.
- **MusiXTeX-Übersetzung:** Greensleeves, Technik-Test und Song-Test übersetzen mit 0 Fehlern und 0 überlangen Zeilen; `Rahmendokument.tex` ebenso.
- **End-to-End:** In der GUI laden → speichern → MusiXTeX übersetzen: fehlerfrei.
- **Greensleeves-Test (echter Guitar-Pro-Export):** 20/20 Prüfungen bestanden.
- **Parser-Test:** 19/19 Prüfungen bestanden.
  - Alle neuen Felder, darunter Positionen nach `backup`, Hammer-on „stop“ ≠ Start, Tuning-Sortierung und Wiederholungen.
  - Die alte Testdatei wird unverändert eingelesen.
- **Theme-Test:** 28/28 Prüfungen bestanden.
  - Menüaufbau; Häkchen und Exklusivität; erneuter Klick auf das aktive Theme.
  - Stil, Palette, Stylesheet und Lexer-Farben für Dark, VS Code und Fusion; Rücksetzen auf hell.
  - Speichern, Neustart mit Dark, ungültiger gespeicherter Stil.
  - Sprachwechsel lässt das Theme-Menü intakt.
- **End-to-End-Test Laden/Speichern:** 22/22 Prüfungen bestanden, Dateidialoge automatisch bedient.
  - Button öffnet den Dialog im Standardordner; Abbrechen ändert nichts.
  - Felder, Namensableitung (`my.song.v2.xml` → `my.song.v2.tex`) und Konvertierungsinhalt stimmen.
  - Zwischenablage funktioniert.
  - Speichern: erster Save mit Dialog, zweiter Save ohne Dialog, Save nach Namensänderung wieder mit Dialog, Save As immer mit Dialog.
  - Eingetippter Pfad + Enter lädt und konvertiert.
  - 4 Fehlerfälle zeigen eine Meldung, der Editor bleibt unverändert.
