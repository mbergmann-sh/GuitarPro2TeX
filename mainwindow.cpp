#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileInfo>
#include <QLocale>
#include <QLibraryInfo>
#include <QFontDatabase>
#include <QCoreApplication>
#include <Qsci/qsciscintilla.h>
#include "latexlexer.h"
#include "musicxmlconverter.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QSaveFile>
#include <QStyle>
#include <QStyleFactory>

/* --- QSettings keys (grouped like in AmigaED) --- */
namespace {
const char *const KEY_LANGUAGE     = "GUI/Language";        // "en" | "de"
const char *const KEY_THEME        = "GUI/Theme";           // theme name, "" = system default
const char *const KEY_XML_FOLDER   = "Paths/MusicXMLFolder";// default MusicXML input folder
const char *const KEY_TEX_FOLDER   = "Paths/LaTeXFolder";   // default LaTeX output folder
const char *const KEY_GEOMETRY     = "MainWindow/Geometry"; // window position/size
const char *const KEY_WINDOW_STATE = "MainWindow/State";    // toolbar position etc.
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // The translator must be active BEFORE setupUi(), so the initial texts
    // are already in the stored language (no visible switch after startup).
    m_guiLanguage = storedLanguage();
    applyLanguage(m_guiLanguage);

    ui->setupUi(this);
    m_uiReady = true;

    // Remember the style Qt picked for this platform before any theme changes it
    // (e.g. "windowsvista"/"windows11" on Windows) - used as "system default"
    m_systemStyleName = QApplication::style() ? QApplication::style()->name() : QStringLiteral("Fusion");

    // English/Deutsch are mutually exclusive and marked with a CHECKMARK.
    // Note: an exclusive QActionGroup makes several styles (e.g. Fusion) draw a
    // radio dot instead of a checkmark. Therefore the group is non-exclusive and
    // the exclusion is enforced in updateLanguageMenuChecks().
    languageGroup = new QActionGroup(this);
    languageGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);
    ui->actionEnglish->setCheckable(true);
    ui->actionDeutsch->setCheckable(true);
    languageGroup->addAction(ui->actionEnglish);
    languageGroup->addAction(ui->actionDeutsch);

    setupEditor();

    // The Convert action/button are gone (loading converts immediately).
    // A path typed into lineEditXML is loaded + converted with Enter.
    connect(ui->lineEditXML, &QLineEdit::returnPressed, this, [this] {
        const QString path = QDir::fromNativeSeparators(ui->lineEditXML->text().trimmed());
        if(path.isEmpty())
            on_actionLoad_MusicXML_File_triggered();
        else
            loadMusicXmlFile(path);
    });

    // Must run AFTER setupUi() and after all member pointers used in it exist
    readSettings();

    // Theme menu needs the loaded theme name; then apply it (style, palette, editor colours)
    buildThemeMenu();
    applyTheme();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionQuit_triggered()
{
    this->close();
}


void MainWindow::on_actionNew_triggered()
{
    if(myDebug)
        qDebug() << "on_actionNew_triggered()";

    // Safety request only if there is a snippet that has not been saved yet
    // (freshly converted or edited by hand after the last save)
    if(!ui->textEdit->text().isEmpty() && ui->textEdit->isModified())
    {
        QMessageBox box(this);
        box.setWindowTitle(tr("Reset all content"));
        box.setIcon(QMessageBox::Question);
        box.setText(tr("The LaTeX output has not been saved yet."));
        box.setInformativeText(tr("Do you really want to discard it and reset all content?"));
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::No);      // Enter must not discard work
        if(box.exec() != QMessageBox::Yes)
            return;
    }

    resetAllContent();
}


void MainWindow::resetAllContent()
{
    // Widgets
    ui->lineEditXML->clear();
    ui->lineEditLaTeX->clear();
    ui->textEdit->clear();
    ui->textEdit->setModified(false);
    updateLineNumberMarginWidth();

    // State that belongs to the previous file (prefs, theme and language stay)
    m_savedTexPath.clear();

    ui->lineEditXML->setFocus();
    statusBar()->showMessage(tr("All content has been reset."), 5000);
}


void MainWindow::on_actionLoad_MusicXML_File_triggered()
{
    if(myDebug)
        qDebug() << "on_actionLoad_MusicXML_File_triggered()";

    // 1. File requester, starting in the stored MusicXML default folder
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Load MusicXML File"),
        startDirFor(p_xml),
        tr("MusicXML files (*.xml *.musicxml);;All files (*)"));

    if(path.isEmpty())
        return;                                     // cancelled

    loadMusicXmlFile(path);                         // 2. - 4.
}


void MainWindow::on_actionSave_LaTeX_Snippet_triggered()
{
    if(myDebug)
        qDebug() << "on_actionSave_LaTeX_Snippet_triggered()";

    saveLatex(false);   // asks only if not yet saved under the name in lineEditLaTeX
}


void MainWindow::on_actionAbout_triggered()
{    if(myDebug)
        qDebug() << "on_actionAbout_triggered()";

    QMessageBox::information(
        this,
        tr("About GuitarPROtoTeX-Convert %1").arg(QStringLiteral(APP_VERSION)),
        tr("<h3>GuitarPROtoTeX-Convert %1</h3>"
           "<p><b>Author:</b> Michael Bergmann</p>"
           "<p>This program converts MusicXML files into LaTeX files (MusiXTeX) "
           "that can be included in existing LaTeX projects.</p>"
           "<p>License: GNU General Public License v3.<br>"
           "Uses Qt 6 and QScintilla.</p>").arg(QStringLiteral(APP_VERSION))
        );
}


void MainWindow::on_actionAbout_Qt_triggered()
{
    if(myDebug)
        qDebug() << "on_actionAbout_Qt_triggered()";

    QMessageBox::aboutQt(this, tr("About Qt"));
}


void MainWindow::on_actionSave_As_triggered()
{
    if(myDebug)
        qDebug() << "on_actionSave_As_triggered()";

    saveLatex(true);    // always asks
}


void MainWindow::on_actionCopy_to_Clipboard_triggered()
{
    if(myDebug)
        qDebug() << "on_actionCopy_to_Clipboard_triggered()";

    const QString text = ui->textEdit->text();
    if(text.isEmpty())
    {
        statusBar()->showMessage(tr("Nothing to copy - the LaTeX output is empty."), 5000);
        return;
    }
    QGuiApplication::clipboard()->setText(text);
    statusBar()->showMessage(tr("LaTeX snippet copied to clipboard (%n line(s)).", nullptr,
                                ui->textEdit->lines()), 5000);
}


void MainWindow::on_actionEnglish_triggered()
{
    if(myDebug)
        qDebug() << "on_actionEnglish_triggered()";

    setGuiLanguage(QStringLiteral("en"));
    statusBar()->showMessage(tr("GUI language set to English."), 5000);
}


void MainWindow::on_actionDeutsch_triggered()
{
    if(myDebug)
        qDebug() << "on_actionDeutsch_triggered()";

    setGuiLanguage(QStringLiteral("de"));
    statusBar()->showMessage(tr("GUI language set to German."), 5000);
}


void MainWindow::on_actionDefault_MusicXML_Files_Folder_triggered()
{
    if(myDebug)
        qDebug() << "on_actionDefault_MusicXML_Files_Folder_triggered()";

    // Use a local variable: cancelling the dialog must NOT wipe the stored folder
    const QString folder = requestXMLFolderPath();
    if(!folder.isEmpty())
    {
        p_xml = QDir::cleanPath(folder);
        writeSettings();
        updateFolderActionTips();
        statusBar()->showMessage(tr("Default MusicXML folder: %1").arg(QDir::toNativeSeparators(p_xml)), 5000);
        if(myDebug)
            qDebug() << "MusicXML folder saved:" << p_xml;
    }
    else if(myDebug)
        qDebug() << "MusicXML folder selection cancelled, keeping" << p_xml;
}


void MainWindow::on_actionDefault_LaTeX_Files_Folder_triggered()
{
    if(myDebug)
        qDebug() << "on_actionDefault_LaTeX_Files_Folder_triggered()";

    // Use a local variable: cancelling the dialog must NOT wipe the stored folder
    const QString folder = requestTexFolderPath();
    if(!folder.isEmpty())
    {
        p_tex = QDir::cleanPath(folder);
        writeSettings();
        updateFolderActionTips();
        statusBar()->showMessage(tr("Default LaTeX folder: %1").arg(QDir::toNativeSeparators(p_tex)), 5000);
        if(myDebug)
            qDebug() << "LaTeX folder saved:" << p_tex;
    }
    else if(myDebug)
        qDebug() << "LaTeX folder selection cancelled, keeping" << p_tex;
}


void MainWindow::on_pushButtonXML_clicked()
{
    if(myDebug)
        qDebug() << "on_pushButtonXML_clicked()";

    // Lazy as we are, we recycle the correspondending menu action for this job!
    on_actionLoad_MusicXML_File_triggered();
}


void MainWindow::on_pushButtonNew_clicked()
{
    if(myDebug)
        qDebug() << "on_pushButtonNew_clicked()";

    // Lazy as we are, we recycle the correspondending menu action for this job!
    on_actionNew_triggered();
}


void MainWindow::on_pushButtonSave_clicked()
{
    if(myDebug)
        qDebug() << "on_pushButtonSave_clicked()";

    // Same behaviour as File > Save LaTeX Snippet (asks only if not yet saved under this name)
    on_actionSave_LaTeX_Snippet_triggered();
}


void MainWindow::on_pushButtonClipboard_clicked()
{
    if(myDebug)
        qDebug() << "on_pushButtonClipboard_clicked()";

    // Lazy as we are, we recycle the correspondending menu action for this job!
    on_actionCopy_to_Clipboard_triggered();
}


void MainWindow::closeEvent(QCloseEvent *event) {
    // Create the message box object
    QMessageBox quitBox(this);

    // Set the window title and translatable question text
    quitBox.setWindowTitle(tr("Quit Application"));
    quitBox.setText(tr("Do you really want to quit?"));

    // Explicitly set the question mark icon
    quitBox.setIcon(QMessageBox::Question);

    // Add the OK and Cancel buttons
    quitBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

    // Set Cancel as the default button to prevent accidental closing via Enter key
    quitBox.setDefaultButton(QMessageBox::Cancel);

    // Show the dialog and get the user's response
    int reply = quitBox.exec();

    if (reply == QMessageBox::Ok) {
        writeSettings(); // persist window geometry and all prefs
        event->accept(); // Close the application
    } else {
        event->ignore(); // Cancel the closing process
    }
}


QString MainWindow::requestXMLFolderPath() {
    // Open the native folder selection dialog
    // Parameters: parent, window title, default directory, options
    QString selectedFolder = QFileDialog::getExistingDirectory(
        this,
        tr("Select default MusicXML Input Directory"),
        startDirFor(p_xml), // stored folder, falls back to the user's home directory
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    // Return the selected folder path (will be empty if user clicks "Cancel")
    return selectedFolder;
}

QString MainWindow::requestTexFolderPath() {
    // Open the native folder selection dialog
    // Parameters: parent, window title, default directory, options
    QString selectedFolder = QFileDialog::getExistingDirectory(
        this,
        tr("Select default LaTeX files Output Directory"),
        startDirFor(p_tex), // stored folder, falls back to the user's home directory
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    // Return the selected folder path (will be empty if user clicks "Cancel")
    return selectedFolder;
}


/* =====================================================================
 *  Preferences
 * ===================================================================== */

void MainWindow::readSettings()
{
    QSettings settings;   // identity is set in main.cpp

    // GUI language (already applied in the constructor before setupUi())
    m_guiLanguage = storedLanguage();

    // Theme name (applied by the theme code once the Prefs/Theme menu exists)
    m_guiTheme = settings.value(KEY_THEME, QString()).toString();
    // A native style saved on another system (e.g. "windowsvista" on Linux) may not
    // exist here: fall back to the system default instead of an unknown style
    if(!m_guiTheme.isEmpty() && !syntheticThemes().contains(m_guiTheme)
       && !QStyleFactory::keys().contains(m_guiTheme, Qt::CaseInsensitive))
    {
        if(myDebug)
            qDebug() << "readSettings(): theme" << m_guiTheme << "not available, using system default";
        m_guiTheme.clear();
    }

    // Default folders (kept even if they don't exist right now, e.g. unplugged drive;
    // the file dialogs fall back to the home directory in that case)
    p_xml = settings.value(KEY_XML_FOLDER, QString()).toString();
    p_tex = settings.value(KEY_TEX_FOLDER, QString()).toString();

    // Window geometry / toolbar state
    const QByteArray geometry = settings.value(KEY_GEOMETRY).toByteArray();
    if(!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray state = settings.value(KEY_WINDOW_STATE).toByteArray();
    if(!state.isEmpty())
        restoreState(state);

    updateLanguageMenuChecks();
    updateFolderActionTips();

    if(myDebug)
        qDebug() << "readSettings():" << settings.fileName()
                 << "lang =" << m_guiLanguage << "theme =" << m_guiTheme
                 << "xml =" << p_xml << "tex =" << p_tex;
}

void MainWindow::writeSettings()
{
    QSettings settings;

    settings.setValue(KEY_LANGUAGE,     m_guiLanguage);
    settings.setValue(KEY_THEME,        m_guiTheme);
    settings.setValue(KEY_XML_FOLDER,   p_xml);
    settings.setValue(KEY_TEX_FOLDER,   p_tex);
    settings.setValue(KEY_GEOMETRY,     saveGeometry());
    settings.setValue(KEY_WINDOW_STATE, saveState());
    settings.sync();

    if(settings.status() != QSettings::NoError)
        qWarning() << "writeSettings(): could not write preferences to" << settings.fileName();
    else if(myDebug)
        qDebug() << "writeSettings(): saved to" << settings.fileName();
}

void MainWindow::setGuiLanguage(const QString &lang)
{
    const QString newLang = (lang == QLatin1String("de")) ? QStringLiteral("de") : QStringLiteral("en");
    const bool changed = (newLang != m_guiLanguage);
    m_guiLanguage = newLang;
    if(changed)
        applyLanguage(m_guiLanguage);   // -> LanguageChange -> changeEvent() -> retranslateUi()
    updateLanguageMenuChecks();
    writeSettings();
}

void MainWindow::setGuiTheme(const QString &theme)
{
    m_guiTheme = theme;
    applyTheme();
    writeSettings();
}

void MainWindow::updateLanguageMenuChecks()
{
    // Guard: may be called before the actions/group exist
    if(!languageGroup)
        return;
    // Exactly one entry is checked - also re-checks the current language if the
    // user clicks its (already checked) entry again, which would otherwise uncheck it.
    ui->actionEnglish->setChecked(m_guiLanguage == QLatin1String("en"));
    ui->actionDeutsch->setChecked(m_guiLanguage == QLatin1String("de"));
}

void MainWindow::updateFolderActionTips()
{
    const QString none = tr("(not set)");
    ui->actionDefault_MusicXML_Files_Folder->setStatusTip(
        tr("Current: %1").arg(p_xml.isEmpty() ? none : QDir::toNativeSeparators(p_xml)));
    ui->actionDefault_LaTeX_Files_Folder->setStatusTip(
        tr("Current: %1").arg(p_tex.isEmpty() ? none : QDir::toNativeSeparators(p_tex)));
}

QString MainWindow::startDirFor(const QString &path)
{
    if(!path.isEmpty() && QFileInfo(path).isDir())
        return path;
    return QDir::homePath();
}


/* =====================================================================
 *  GUI language
 * ===================================================================== */

QString MainWindow::storedLanguage()
{
    QSettings settings;
    const QString sysLang = (QLocale::system().language() == QLocale::German)
                                ? QStringLiteral("de") : QStringLiteral("en");
    const QString lang = settings.value(KEY_LANGUAGE, sysLang).toString();
    return (lang == QLatin1String("de")) ? QStringLiteral("de") : QStringLiteral("en");  // sanitize
}

bool MainWindow::applyLanguage(const QString &lang)
{
    // English is the source language: no translator needed.
    // Removing an installed translator sends QEvent::LanguageChange automatically.
    QCoreApplication::removeTranslator(&appTranslator);
    QCoreApplication::removeTranslator(&qtTranslator);

    if(lang != QLatin1String("de"))
        return true;

    // Our own translation, embedded via CONFIG += embed_translations (prefix :/i18n/)
    const bool ok = appTranslator.load(QStringLiteral(":/i18n/GuitarPROtoTeX-Convert_de"));
    if(ok)
        QCoreApplication::installTranslator(&appTranslator);
    else
        qWarning() << "applyLanguage(): German translation not found in resources";

    // Qt's own strings (OK/Cancel, file dialog, ...). Optional: windeployqt copies
    // them into <exe dir>/translations; missing files are simply ignored.
    if(qtTranslator.load(QStringLiteral("qtbase_de"), QLibraryInfo::path(QLibraryInfo::TranslationsPath))
       || qtTranslator.load(QStringLiteral("qtbase_de"), QCoreApplication::applicationDirPath() + QStringLiteral("/translations")))
        QCoreApplication::installTranslator(&qtTranslator);
    else if(myDebug)
        qDebug() << "applyLanguage(): qtbase_de.qm not found, standard buttons stay English";

    return ok;
}

void MainWindow::changeEvent(QEvent *event)
{
    // Guard: LanguageChange may arrive before setupUi() has created the widgets
    if(event->type() == QEvent::LanguageChange && m_uiReady)
    {
        ui->retranslateUi(this);        // all texts from mainwindow.ui
        updateFolderActionTips();       // texts built in code
        if(myDebug)
            qDebug() << "changeEvent(): GUI re-translated to" << m_guiLanguage;
    }
    QMainWindow::changeEvent(event);
}


/* =====================================================================
 *  QScintilla editor
 * ===================================================================== */

void MainWindow::setupEditor()
{
    QsciScintilla *ed = ui->textEdit;

    // Real monospace font of the platform (Consolas on Windows, DejaVu Sans Mono etc. on Linux)
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);

    // LaTeX syntax highlighting for the converted snippet
    texLexer = new LatexLexer(ed);
    texLexer->setBaseFont(font);        // same font for all styles, comments italic
    ed->setLexer(texLexer);

    ed->setUtf8(true);
    ed->setTabWidth(2);                 // snippets are indented with 2 spaces
    ed->setIndentationsUseTabs(false);
    ed->setAutoIndent(true);
    ed->setCaretLineVisible(true);

    // Line numbers in margin 0. Set AFTER setLexer(): setLexer() resets margin styles.
    ed->setMarginType(0, QsciScintilla::NumberMargin);
    ed->setMarginLineNumbers(0, true);
    ed->setMarginsFont(font);
    connect(ed, &QsciScintilla::linesChanged, this, &MainWindow::updateLineNumberMarginWidth);
    updateLineNumberMarginWidth();
}

void MainWindow::updateLineNumberMarginWidth()
{
    // Width for at least 3 digits, grows with the number of lines (+1 digit as padding)
    const int digits = qMax(3, int(QString::number(ui->textEdit->lines()).length()));
    ui->textEdit->setMarginWidth(0, QString(digits + 1, QLatin1Char('0')));
}


/* =====================================================================
 *  Load / convert / save
 * ===================================================================== */

QString MainWindow::texNameFor(const QString &xmlPath)
{
    // "D:/Songs/My Song.musicxml" -> "My Song.tex" (only the last suffix is replaced)
    const QString base = QFileInfo(xmlPath).completeBaseName();
    return (base.isEmpty() ? QStringLiteral("snippet") : base) + QStringLiteral(".tex");
}

bool MainWindow::loadMusicXmlFile(const QString &path)
{
    // 2. path and name of the chosen file
    ui->lineEditXML->setText(QDir::toNativeSeparators(path));

    // 3. same name with suffix .tex - may be edited by the user before saving
    ui->lineEditLaTeX->setText(texNameFor(path));
    m_savedTexPath.clear();                         // new source: next Save asks for the target

    // 4. convert and show the result
    return convertMusicXmlFile(path);
}

bool MainWindow::convertMusicXmlFile(const QString &path)
{
    const MusicXmlConverter::Result r = MusicXmlConverter::convertFile(path);
    if(!r.ok)
    {
        QMessageBox::warning(this, tr("Conversion failed"), r.error);
        statusBar()->showMessage(tr("Conversion failed."), 5000);
        return false;
    }

    ui->textEdit->setText(r.latex);
    ui->textEdit->setModified(true);                // new, not yet saved output (see New)
    ui->textEdit->setCursorPosition(0, 0);
    if(ui->lineEditLaTeX->text().trimmed().isEmpty())
        ui->lineEditLaTeX->setText(texNameFor(path));

    statusBar()->showMessage(tr("Converted %1: %2 part(s), %3 measure(s), %4 note(s).")
                                 .arg(QFileInfo(path).fileName())
                                 .arg(r.score.parts.size())
                                 .arg(r.score.measureCount())
                                 .arg(r.score.noteCount()), 8000);
    return true;
}

bool MainWindow::saveLatex(bool alwaysAsk)
{
    if(ui->textEdit->text().isEmpty())
    {
        statusBar()->showMessage(tr("Nothing to save - the LaTeX output is empty."), 5000);
        return false;
    }

    // Name from lineEditLaTeX (user may have changed it); no path parts, always .tex
    QString name = QFileInfo(ui->lineEditLaTeX->text().trimmed()).fileName();
    if(name.isEmpty())
        name = texNameFor(ui->lineEditXML->text());
    if(!name.endsWith(QLatin1String(".tex"), Qt::CaseInsensitive))
        name += QStringLiteral(".tex");

    QString target;
    if(!alwaysAsk && !m_savedTexPath.isEmpty() && QFileInfo(m_savedTexPath).fileName() == name)
    {
        target = m_savedTexPath;                    // already saved under this name: overwrite
    }
    else
    {
        // Start in the LaTeX default folder, else next to the MusicXML file, else home
        QString dir = p_tex;
        if(dir.isEmpty() || !QFileInfo(dir).isDir())
        {
            const QString xmlDir = QFileInfo(QDir::fromNativeSeparators(ui->lineEditXML->text().trimmed())).absolutePath();
            dir = (!ui->lineEditXML->text().trimmed().isEmpty() && QFileInfo(xmlDir).isDir())
                      ? xmlDir : QDir::homePath();
        }
        target = QFileDialog::getSaveFileName(
            this,
            alwaysAsk ? tr("Save LaTeX Snippet As") : tr("Save LaTeX Snippet"),
            QDir(dir).filePath(name),
            tr("LaTeX files (*.tex);;All files (*)"));
        if(target.isEmpty())
            return false;                           // cancelled
        if(QFileInfo(target).suffix().isEmpty())
            target += QStringLiteral(".tex");
    }

    if(!writeTexFile(target))
        return false;

    m_savedTexPath = target;
    ui->textEdit->setModified(false);               // saved: New/Reset needs no confirmation
    ui->lineEditLaTeX->setText(QFileInfo(target).fileName());   // name chosen in the dialog
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(target)), 5000);
    return true;
}

bool MainWindow::writeTexFile(const QString &path)
{
    // QSaveFile: writes to a temp file and renames it -> no half-written .tex on errors
    QSaveFile f(path);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Text)
       || f.write(ui->textEdit->text().toUtf8()) < 0
       || !f.commit())
    {
        QMessageBox::warning(this, tr("Save failed"),
                             tr("Cannot write file \"%1\":\n%2")
                                 .arg(QDir::toNativeSeparators(path), f.errorString()));
        return false;
    }
    return true;
}


/* =====================================================================
 *  Themes
 *  The two dark themes are taken 1:1 from AmigaED 4.0
 *  (darkApplicationPalette(), vscodeApplicationPalette(),
 *   applyApplicationStyle(), initializeMargin(), initializeCaretLine(),
 *   applyLexerDarkColors()).
 * ===================================================================== */

QStringList MainWindow::syntheticThemes()
{
    // Not real QStyleFactory keys; untranslated like in AmigaED, because
    // the displayed text is also the value stored in the prefs
    return { QStringLiteral("Dark"), QStringLiteral("Visual Studio Code Dark") };
}

bool MainWindow::isDarkTheme() const
{
    return m_guiTheme == QLatin1String("Dark");
}

bool MainWindow::isVSCodeTheme() const
{
    return m_guiTheme == QLatin1String("Visual Studio Code Dark");
}

QString MainWindow::effectiveThemeName() const
{
    if(!m_guiTheme.isEmpty())
        return m_guiTheme;
    // Map the system style's name to the spelling used by QStyleFactory::keys()
    for(const QString &key : QStyleFactory::keys())
        if(key.compare(m_systemStyleName, Qt::CaseInsensitive) == 0)
            return key;
    return m_systemStyleName;
}

void MainWindow::buildThemeMenu()
{
    // Guard against a second call (the menu is built exactly once)
    if(themeGroup)
        return;

    QMenu *menu = ui->menuTheme;
    themeGroup = new QActionGroup(this);
    // Like the language menu: non-exclusive group so every style draws a
    // CHECKMARK (Fusion - used by the dark themes - would draw a radio dot
    // for exclusive groups); exclusion is enforced in syncThemeMenuCheckedState()
    themeGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

    auto addEntry = [&](const QString &name) {
        QAction *act = menu->addAction(name);
        act->setCheckable(true);
        act->setData(name);                         // stored value (text may get a "&" from styles)
        themeGroup->addAction(act);
        connect(act, &QAction::triggered, this, &MainWindow::onThemeActionTriggered);
    };

    // 1. native styles available on this system (Windows: windows11, windowsvista, Windows, Fusion)
    for(const QString &styleName : QStyleFactory::keys())
        addEntry(styleName);

    // 2. separator + the two dark themes from AmigaED
    menu->addSeparator();
    for(const QString &name : syntheticThemes())
        addEntry(name);

    syncThemeMenuCheckedState();
}

void MainWindow::syncThemeMenuCheckedState()
{
    if(!themeGroup)
        return;
    const QString current = effectiveThemeName();
    for(QAction *act : themeGroup->actions())
        act->setChecked(act->data().toString().compare(current, Qt::CaseInsensitive) == 0);
}

void MainWindow::onThemeActionTriggered()
{
    QAction *act = qobject_cast<QAction *>(sender());
    if(!act)
        return;

    const QString name = act->data().toString();
    if(myDebug)
        qDebug() << "onThemeActionTriggered():" << name;

    setGuiTheme(name);                              // applies + saves + re-syncs the checkmarks
    statusBar()->showMessage(tr("Theme: %1").arg(name), 5000);
}

QPalette MainWindow::darkApplicationPalette() const
{
    // 1:1 from AmigaED 4.0 - used together with the "Fusion" style
    QPalette palette;

    palette.setColor(QPalette::Window,            QColor(0x35, 0x35, 0x35));
    palette.setColor(QPalette::WindowText,        QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Base,              QColor(0x23, 0x23, 0x23));
    palette.setColor(QPalette::AlternateBase,     QColor(0x35, 0x35, 0x35));
    palette.setColor(QPalette::ToolTipBase,       QColor(0x35, 0x35, 0x35));
    palette.setColor(QPalette::ToolTipText,       QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Text,              QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::Button,            QColor(0x3c, 0x3c, 0x3c));
    palette.setColor(QPalette::ButtonText,        QColor(0xd4, 0xd4, 0xd4));
    palette.setColor(QPalette::BrightText,        QColor(0xf4, 0x47, 0x47));
    palette.setColor(QPalette::Link,              QColor(0x56, 0x9c, 0xd6));
    palette.setColor(QPalette::LinkVisited,       QColor(0xb3, 0x92, 0xf0));
    palette.setColor(QPalette::Highlight,         QColor(0x26, 0x4f, 0x78));
    palette.setColor(QPalette::HighlightedText,   QColor(0xff, 0xff, 0xff));

    palette.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x7f, 0x7f, 0x7f));
    palette.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x7f, 0x7f, 0x7f));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x7f, 0x7f, 0x7f));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x50, 0x50, 0x50));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x7f, 0x7f, 0x7f));

    return palette;
}

QPalette MainWindow::vscodeApplicationPalette() const
{
    // 1:1 from AmigaED 4.0 - VS Code "Dark+" UI colours, used with "Fusion"
    QPalette palette;

    palette.setColor(QPalette::Window,            QColor(0x25, 0x25, 0x26));   // sideBar.background
    palette.setColor(QPalette::WindowText,        QColor(0xcc, 0xcc, 0xcc));   // foreground
    palette.setColor(QPalette::Base,              QColor(0x1e, 0x1e, 0x1e));   // editor.background
    palette.setColor(QPalette::AlternateBase,     QColor(0x2d, 0x2d, 0x2d));
    palette.setColor(QPalette::ToolTipBase,       QColor(0x25, 0x25, 0x26));
    palette.setColor(QPalette::ToolTipText,       QColor(0xcc, 0xcc, 0xcc));
    palette.setColor(QPalette::Text,              QColor(0xd4, 0xd4, 0xd4));   // editor.foreground
    palette.setColor(QPalette::Button,            QColor(0x3c, 0x3c, 0x3c));
    palette.setColor(QPalette::ButtonText,        QColor(0xcc, 0xcc, 0xcc));
    palette.setColor(QPalette::BrightText,        QColor(0xf4, 0x84, 0x71));
    palette.setColor(QPalette::Link,              QColor(0x37, 0x94, 0xff));   // textLink.foreground
    palette.setColor(QPalette::LinkVisited,       QColor(0xb1, 0x80, 0xd7));
    palette.setColor(QPalette::Highlight,         QColor(0x09, 0x47, 0x71));   // list.activeSelectionBackground
    palette.setColor(QPalette::HighlightedText,   QColor(0xff, 0xff, 0xff));

    palette.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x6a, 0x6a, 0x6a));
    palette.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x6a, 0x6a, 0x6a));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x6a, 0x6a, 0x6a));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x3c, 0x3c, 0x3c));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x6a, 0x6a, 0x6a));

    return palette;
}

void MainWindow::applyTheme()
{
    if(isAnyDarkTheme())
    {
        // Native styles (e.g. windowsvista) ignore custom palettes -> Fusion
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(isDarkTheme() ? darkApplicationPalette() : vscodeApplicationPalette());
    }
    else
    {
        const QString styleName = effectiveThemeName();
        if(QStyle *st = QStyleFactory::create(styleName))   // nullptr for unknown names
            QApplication::setStyle(st);
        // Undo a previously applied dark palette
        if(QApplication::style())
            QApplication::setPalette(QApplication::style()->standardPalette());
    }

    if(isVSCodeTheme())
    {
        // As in AmigaED: palette for the bulk, stylesheet for the iconic details
        // (blue status bar, VS Code menu/menubar colours, toolbar)
        this->setStyleSheet(
            "QStatusBar { background-color: #007ACC; color: #ffffff; }"
            "QStatusBar::item { border: none; }"
            "QStatusBar QLabel { color: #ffffff; }"
            "QMenuBar { background-color: #3c3c3c; color: #cccccc; }"
            "QMenuBar::item { background-color: #3c3c3c; color: #cccccc; }"
            "QMenuBar::item:selected, QMenuBar::item:pressed { background-color: #094771; color: #ffffff; }"
            "QMenu { background-color: #252526; color: #cccccc; border: 1px solid #454545; }"
            "QMenu::item:selected { background-color: #094771; color: #ffffff; }"
            "QToolBar { background-color: #333333; border: none; spacing: 2px; }"
        );
    }
    else
    {
        this->setStyleSheet(QString());             // clear a stale VS Code stylesheet
    }

    syncThemeMenuCheckedState();
    applyEditorColors();

    if(myDebug)
        qDebug() << "applyTheme():" << effectiveThemeName()
                 << "style =" << (QApplication::style() ? QApplication::style()->name() : QString());
}

void MainWindow::applyEditorColors()
{
    QsciScintilla *ed = ui->textEdit;
    if(!ed)
        return;

    if(isAnyDarkTheme())
    {
        // Margin / caret line / selection: AmigaED's exact VS Code Dark+ values
        ed->setMarginsBackgroundColor(QColor("#252526"));
        ed->setMarginsForegroundColor(QColor("#858585"));
        ed->setCaretLineBackgroundColor(QColor("#2d2d2d"));
        ed->setCaretForegroundColor(QColor("#ffffff"));
        ed->setSelectionBackgroundColor(QColor("#264f78"));
        ed->setIndentationGuidesForegroundColor(QColor("#3b3b3b"));
        ed->setIndentationGuidesBackgroundColor(QColor("#1e1e1e"));
    }
    else
    {
        // Light: gutter follows the style's own palette
        const QPalette pal = QApplication::palette();
        ed->setMarginsBackgroundColor(pal.color(QPalette::Window));
        ed->setMarginsForegroundColor(QColor("#6e6e6e"));
        ed->setCaretLineBackgroundColor(QColor("#e8f2fe"));
        ed->setCaretForegroundColor(QColor("#000000"));
        ed->setSelectionBackgroundColor(QColor("#add6ff"));
        ed->setIndentationGuidesForegroundColor(QColor("#c0c0c0"));
        ed->setIndentationGuidesBackgroundColor(QColor("#ffffff"));
    }

    if(!texLexer)
        return;

    if(!isAnyDarkTheme())
    {
        // Back to LatexLexer's own light colours
        texLexer->setPaper(Qt::white);
        for(int st = LatexLexer::Default; st <= LatexLexer::Special; ++st)
            texLexer->setColor(texLexer->defaultColor(st), st);
        return;
    }

    // AmigaED's dark syntax palette (applyLexerDarkColors()), mapped to LaTeX styles.
    // Base colours are shared by both dark themes; the two "Amiga-specific"
    // colours differ exactly as in AmigaED (Dark: purple/amber, VS Code: teal/pale yellow).
    texLexer->setPaper(QColor(0x1e, 0x1e, 0x1e));
    texLexer->setColor(QColor(0xd4, 0xd4, 0xd4));   // all styles first: no light leftovers

    const QColor comment(0x6a, 0x99, 0x55);
    const QColor keyword(0x56, 0x9c, 0xd6);
    const QColor number(0xb5, 0xce, 0xa8);
    const QColor string(0xce, 0x91, 0x78);
    const QColor preprocessor(0xc5, 0x86, 0xc0);
    const QColor typeColor = isVSCodeTheme() ? QColor(0x4e, 0xc9, 0xb0) : QColor(0xb3, 0x92, 0xf0);
    const QColor funcColor = isVSCodeTheme() ? QColor(0xdc, 0xdc, 0xaa) : QColor(0xe5, 0xc0, 0x7b);

    texLexer->setColor(comment,      LatexLexer::Comment);
    texLexer->setColor(keyword,      LatexLexer::Command);
    texLexer->setColor(typeColor,    LatexLexer::Environment);
    texLexer->setColor(preprocessor, LatexLexer::Brace);
    texLexer->setColor(string,       LatexLexer::Math);
    texLexer->setColor(number,       LatexLexer::Number);
    texLexer->setColor(funcColor,    LatexLexer::Special);
}
