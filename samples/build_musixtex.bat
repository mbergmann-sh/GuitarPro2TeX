@echo off
rem ------------------------------------------------------------------
rem  MusiXTeX-Dreischritt fuer ein LaTeX-Dokument mit Snippets aus
rem  GuitarPROtoTeX-Convert (ausgeglichene Notenzeilen):
rem      alte .mx1/.mx2 loeschen -> pdflatex -> musixflx -> pdflatex
rem
rem  Die alte .mx2 MUSS weg: MusiXTeX liest sie sonst schon im ersten
rem  Lauf. Passt sie nicht mehr zum Inhalt (z.B. neuer Song dazu),
rem  bricht pdflatex mit "Emergency stop" bei einem \bar ab.
rem
rem  Aufruf:   build_musixtex.bat testsonbook_main
rem            (Hauptdatei, mit oder ohne .tex; ohne Angabe: testsonbook_main)
rem ------------------------------------------------------------------
setlocal
set NAME=%~n1
if "%NAME%"=="" set NAME=testsonbook_main

if exist "%NAME%.mx1" del "%NAME%.mx1"
if exist "%NAME%.mx2" del "%NAME%.mx2"

pdflatex -interaction=nonstopmode "%NAME%.tex"
if errorlevel 1 goto :error
musixflx "%NAME%"
if errorlevel 1 goto :error
pdflatex -interaction=nonstopmode "%NAME%.tex"
if errorlevel 1 goto :error

echo.
echo Fertig: %NAME%.pdf
goto :eof

:error
echo.
echo Fehler - siehe %NAME%.log
exit /b 1
