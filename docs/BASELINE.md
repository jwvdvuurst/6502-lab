# Herkomst en verificatie

Datum: 12 september 2026.

Bron: C:\Users\JWvdV\OneDrive\Documenten\workspace\6502emulator
Doel: C:\Users\JWvdV\Documents\workspace\6502-lab
Bron-HEAD: 7dc166ea05a77bfe396177e922930da66068c666

De bronwerkboom bevat wijzigingen en niet-getrackte bestanden. Daarom is een
bestandkopie gebruikt, geen clone van alleen de Git-commit. Het bronproject is
niet aangepast. COPY-MANIFEST.json bevat bronhashes, kopiehashes en Git-status.
De oorspronkelijke README is bewaard als UPSTREAM-README.md.

Niet meegenomen: .git, de lege .agents-map en top-level .exe/.o/.obj/.pdb.
De .vscode-configuratie is meegekopieerd; de bestaande taak bouwt één actief
bestand met cl.exe en is geen gevalideerde projectbuild. Gebruik de README.

## Uitgevoerde controles

- Headless broncodebuild met lokaal geïnstalleerde Clang: geslaagd.
- Bestaande --self-test: Self-tests passed.
- Conway met --ticks 1000000 --dump-screen: exitcode 0.
- Gerapporteerde bestaande cyclusteller: 2.981.297.
- Schermdump toont een vijfcellige configuratie; dit is een smoketest,
  geen bewijs voor de correctheid van iedere 6502-instructie.
- Eén compilerwaarschuwing: Windows-CRT deprecation van fopen in de loader.
- Interactieve curses-build: niet getest.
- Nieuwe banking/thread/multiprocessor/undo-functies: nog niet aanwezig;
  daarvoor staan acceptatiecriteria in ARCHITECTURE.md.

De nieuwe 6502headless.exe en bijbehorende .pdb/.ilk zijn gegenereerde buildoutput,
geen bronkopieën, en vallen buiten het kopie-manifest.

## Vervolgvalidatie

Leg vóór de contextrefactor reproduceerbare register-/geheugentraces van kleine
instructieprogramma's vast. Vergelijk de refactor met de baseline en toets
instructiecorrectheid afzonderlijk aan passende referentietests. De bestaande
zelftests dekken met name geheugenaccessen, stack en enkele flags.

Het project krijgt een nieuwe lokale Git-repository zonder remote en commits.
De oorspronkelijke Git-historie is niet gekopieerd.

