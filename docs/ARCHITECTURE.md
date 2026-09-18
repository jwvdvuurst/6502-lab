> Phase 3 and the small text debugger are complete and verified on 13 September 2026. See [PHASE3.md](PHASE3.md) and [DEBUGGER.md](DEBUGGER.md). Earlier phase statuses below are historical.

> Phase 2 completed and verified on 13 September 2026. See [PHASE2.md](PHASE2.md) for the current implementation; earlier phase status below is historical.

> Phase 1 completed and verified on 13 September 2026. See [PHASE1.md](PHASE1.md). Earlier status and code findings below are historical.

# 6502 Lab — inbouwplan

Status: ontwerpvoorstel, geen nieuwe CPU-functionaliteit geïmplementeerd.
Datum: 12 september 2026. Werknaam: 6502 Lab.

## Doel en uitgangspunt

Een experimentele 6502-machine met 1 KiB-geheugenvensters, coöperatieve
threads, meerdere virtuele CPU's met gedeeld geheugen en een globale
terugstapfunctie. De bestaande emulator blijft als afzonderlijk project bestaan.

Begin bij het bestaande gedrag met één CPU en een identieke 64 KiB-mapping.
Voer structurele wijzigingen en functionele uitbreidingen in afzonderlijke
stappen uit. Noem dit niet automatisch een volledig correcte NMOS6502:
de huidige beperkte zelftests bewijzen geen volledige instructieconformiteit.

De actuele werkboom is gekopieerd, niet alleen de laatst gecommitte versie.
Zie COPY-MANIFEST.json voor oorspronkelijke paden, hashes en Git-status.

## Bevindingen in de huidige code

| Onderdeel | Huidige situatie | Benodigde verandering |
| --- | --- | --- |
| src/6502cpu.c en .h | Eén static cpu_t; fetch/execute/tick zonder context | Expliciete CPU- en machinecontext doorgeven |
| CPU-hulpfuncties | Registermacro's verwijzen naar globale cpu | Handlers en adresresolutie laten werken op de meegegeven context |
| Overige CPU-status | stack_depth, status_requested en stat_message buiten cpu_t | Uitvoeringsstatus per context; presentatie buiten de CPU |
| Opcode-dispatch | Tabel met handlers plus fallback-switch | Extensies op één gecontroleerde dispatchroute toevoegen |
| src/6502memory.c | Globale memory[65536], ook directe woordaccessen | Fysieke blokken, mapping en centrale byteaccessen |
| Stack | Directe pointer naar memory + $0100 | Stack leest/schrijft via de bus op $0100 + SP |
| src/6502io.c | displaymem wijst direct naar memory + $C000 | Expliciete framebufferbinding en gelogde gastgeheugenmutaties |
| src/6502test.c | Laden, tests, CLI en uitvoeringsloop samen | Later machine-runner van CLI/frontend scheiden |
| tools/asm6502.py | Vaste opcode- en instructielengtetabellen | Expliciete lab-modus en operandvalidatie |
| Timing | --ticks telt instructies; cyclustabel onvolledig | Deterministische instructiescheduling, geen MHz-claims |

De loader en schermdump gebruiken al fetch/store-functies: goede aansluitpunten.
Stack en curses-schermhelpers omzeilen deze juist. Alleen fetch_byte aanpassen
zou daardoor een gedeeltelijk werkende en moeilijk te debuggen banklaag geven.

## Voorgestelde componenten en eigendom

Dit zijn voorgenomen modules/interfaces; deze bestanden/API's bestaan nog niet.

- machine_t: fysieke blokken, CPU's, globale scheduler, apparaten en tijdlijn.
- cpu_t: CPU-identiteit, actieve thread en lokaal apparaat-/signaalbeleid.
- thread_t: A, X, Y, P, SP, PC, huidige instructie, mapping en run/wait/halt-status.
- address_space_t: 64 mappingentries met fysieke blok-ID's.
- bus: adresvertaling, lezen/schrijven en registratie van veranderingen.
- history: begin/commit/undo van één globale stap en periodieke snapshots.
- frontend: leest de machine en geeft opdrachten; bepaalt niet de gasttiming.

Tijdens de eerste refactor hoeft thread_t nog niet te bestaan: verplaats eerst
alle bestaande toestand naar cpu_t. Splits pas wanneer threads worden toegevoegd.
Gedeelde onveranderlijke opcodetabellen mogen globaal blijven; actieve registers
en mapping niet. Gebruik geen nieuwe globale active_cpu als vervanging.

Conceptuele interface:

```c
step_result_t machine_step(machine_t *machine);
step_result_t cpu_step(machine_t *machine, cpu_t *cpu);
uint8_t bus_read8(machine_t *machine, thread_t *thread, uint16_t address);
void bus_write8(machine_t *machine, thread_t *thread,
                uint16_t address, uint8_t value);
bool machine_step_back(machine_t *machine);
```

Het uiteindelijke foutmodel moet read/write-fouten kunnen doorgeven; de signatures
hierboven tonen verantwoordelijkheid, geen definitief headercontract.

## 1 KiB-banking

Een gastadres blijft 16 bits. Een venster is 1024 bytes; dit verandert de
gebruikelijke 256-byte 6502-page en page-crossingregels niet.

```text
slot          = address >> 10          // 0..63
offset        = address & 0x03FF       // 0..1023
physical_bank = mapping[slot]
value         = banks[physical_bank][offset]
```

Voorstel: 16-bits bank-ID, configureerbaar aantal aanwezige banken. Daarmee is
het representabele maximum 64 MiB; begin bijvoorbeeld met 256 banken (256 KiB)
en alloceer geen volledig maximum zonder noodzaak.

MAP slot, bank wisselt een verwijzing, geen geheugenkopie. Uitgemapte data blijft
bestaan. MAPR slot, bank, count kan later aaneengesloten vensters wisselen.

Voorgesteld semantisch contract:

1. Lees alle instructiebytes en operanden onder de oude mapping.
2. Valideer slot, bank, aantal en toegangsbeleid voordat iets verandert.
3. Pas de mapping aan op één instructiegrens.
4. De volgende instructie-fetch gebruikt de nieuwe mapping.
5. Registers en flags blijven bij een geslaagde MAP ongewijzigd.
6. Bij ongeldige operanden: rapporteer een lab-fault zonder gedeeltelijke mapping.
   De transactielaag herstelt de toestand tot vóór deze instructie, inclusief PC.

Ook remapping van het uitvoerende codevenster krijgt zo een exact gedefinieerd
effect. Een overgangsroutine in een stabiel venster voorkomt verrassingen.

Woordaccessen moeten uit twee byteaccessen bestaan: $83FF/$8400 kan twee
verschillende banken raken. $FFFF/$0000 vereist 16-bits wraparound. Specifieke
6502-adresmodi, bijvoorbeeld zero-page wraparound en JMP indirect-gedrag, blijven
expliciete CPU-regels; ze horen niet verborgen in een algemene bus_read16.

Slot 0 bevat zero page, stack en $0200–$03FF. Geef elke thread een eigen slot 0.
Voor v1 kan gast-MAP naar slot 0 verboden zijn; contextwissels mogen het wel
herstellen. Dat beleid moet zichtbaar gedocumenteerd worden.

Behoud blokken tijdens de sessie. Persistentie over emulatorherstarts is een
latere expliciete save/load-functie met formaatversie. Schrijf niet automatisch
een gewijzigde bank naar disk: dat zou undo en experimenten vertroebelen.

## Meerdere CPU's en privé-/gedeeld geheugen

Privé betekent in de eerste versie: standaard naar verschillende blokken gemapt,
niet een beveiligingsgarantie. Als isolatie gewenst is, voeg een per-CPU lijst
toegestane fysieke blokken toe en valideer ook MAP hiertegen.

Begin met twee CPU's en één gedeeld mailboxblok. Threads blijven aan hun CPU
gebonden; migratie is geen eerste vereiste.

Voorbeeldindeling per CPU:

| Bereik | Rol |
| --- | --- |
| $0000–$03FF | Privé threadwerkgebied |
| $0400–$7FFF | Eigen data/code |
| $8000–$83FF | Gedeelde mailbox |
| $8400–$BFFF | Eigen geheugen |
| $C000–$C3FF | Framebufferbank van deze CPU |
| $C400–$FFFF | Eigen code en vectoren; Conway start op $D000 |

Bind de framebuffer aan een expliciet fysiek blok. De displaycontroller blijft
dat blok tonen als de CPU zijn $C000-venster ommapt. Geef de debugger daarnaast
een afzonderlijke weergave van het huidige virtuele adresbereik.

Elke CPU krijgt een eigen resetvector via zijn mapping. De loader moet CPU-ID
en bestemming kennen. Later kan een machinebeschrijving vastleggen welke
programma's, threads en banken bij het opstarten worden aangemaakt.

## Scheduling en coöperatieve threads

De host voert aanvankelijk alles op één thread uit. Op elk globaal tickmoment
krijgt één uitvoerbare virtuele CPU één volledige instructie, in vaste
round-robinvolgorde. Gastinstructies zijn ondeelbaar in dit model.

Binnen iedere CPU loopt de actieve thread totdat die YIELD, WAIT of HALT doet.
Een thread die nooit afstaat houdt andere threads op dezelfde CPU tegen, maar
niet de andere CPU. Eén CPU zonder uitvoerbare threads wordt overgeslagen.

Contextwissel bewaart registers en mapping gezamenlijk. Stacks worden niet
gekopieerd: de andere slot-0-bank wordt actief. Leg het beleid voor bestaande
stack_depth-controles apart vast; dit is emulatorboekhouding en kan afwijken
van de werkelijke 6502-stackwraparound. Behoud eerst bestaand gedrag.

Maak de eerste threads aan vanuit de machineconfiguratie. Gast-SPAWN, timers,
preëmptie, migratie en cycle-accurate scheduling komen later.

Alle CPU's wachtend zonder mogelijke volgende gebeurtenis betekent een
quiescente/deadlocktoestand. Meld die expliciet; blijf niet eindeloos draaien.
Hostinvoer kan als nieuwe gelogde gebeurtenis uitvoering hervatten.

## Communicatie en voorgestelde extensies

Begin met twee eenrichtingsmailboxes in hetzelfde gedeelde blok. Per mailbox:
één producer, één consumer, een payload en een statusbyte. De producer schrijft
de payload vóór de ready-vlag; de consumer bevestigt ontvangst vóór hergebruik.

In het instructie-atomische model zijn individuele reads/writes geordend.
Een reeks LDA/vergelijk/STA is niet atomisch en kan races hebben. SEI is lokaal
en kan andere CPU's niet uitsluiten.

| Mnemonic (voorstel) | Betekenis | Fase |
| --- | --- | --- |
| MAP slot, bank | Eén mapping wijzigen | Banking |
| MAPR slot, bank, count | Bereik in één transactie wijzigen | Later |
| YIELD | Volgende uitvoerbare thread op deze CPU | Threads |
| HALT | Huidige thread stoppen | Threads |
| WAIT mask | Pending event consumeren of atomisch wachten | Events |
| SIGNAL cpu, thread, mask | Eventbits zetten en passende waiter wekken | Events |
| TAS address | A = oude byte; schrijf 1; Z/N volgens A | Optioneel locks |

Voor WAIT/SIGNAL zijn sticky pending bits per thread een eenvoudig voorstel:
check-and-block gebeurt atomisch, zodat een signaal vlak vóór WAIT niet verloren
gaat. Herhaalde identieke signalen mogen samenklappen; gebruik voor aantallen
een mailboxqueue. Laat SEI deze lab-events niet impliciet blokkeren.

Een TAS is één globale stap. Gewone read-modify-write-instructies zijn in dit
eerste model ook instructie-atomisch; dit is geen claim over echte 6502-bussen.

Kies de binaire opcodewaarden pas nadat de dispatch en compatibiliteitsmodus
zijn vastgesteld. Kandidaten uit de huidige reserved-switch zijn niet universeel
vrij op echte 6502-varianten. Een aparte lab-modus met versie voorkomt dat
bestaande programma's ongemerkt nieuwe instructies uitvoeren.

De assembler moet de nieuwe operandvormen en lengtes expliciet kennen, grenzen
controleren en lab-instructies in legacy-modus afwijzen. Er is momenteel geen
uitgewerkt loaderformaat voor meerdere CPU's/banken; kies dat vóór de demo.

## Globale tijdmachine

De tijdlijn hoort bij machine_t, niet bij afzonderlijke CPU's.

Een stap omvat: selectie van CPU/thread, fetch/execute, register- en bankwijzigingen,
schedulerwijzigingen en eventeffecten. Ook faults moeten een herhaalbare uitkomst
hebben. Begin met journaling van oude waarden en beperkte snapshots.

Registreer:
- globale stap-ID, CPU/thread-ID en instructie-identiteit;
- eerdere registertoestand en relevante uitvoeringsboekhouding;
- fysieke bank-ID, offset en oude waarde voor elke write;
- eerdere mappingentries;
- vorige schedulerpositie, actieve thread, wachtstatus en pending events;
- invoergebeurtenissen en de positie in het replaylog.

Bewaar writes in volgorde en herstel ze in omgekeerde volgorde. Dat ondersteunt
meerdere writes naar dezelfde locatie binnen één instructie. Een virtueel adres
alleen is onvoldoende nadat banking heeft plaatsgevonden.

Voorbeeld: CPU 0 schrijft, CPU 1 leest en schrijft een resultaat. Teruggaan tot
vóór CPU 0's write moet ook de latere acties van CPU 1 ongedaan maken.

Vooruit na undo: zolang er geen wijziging is, volg de opgeslagen gebeurtenissen.
Bij een debuggeredit of afwijkende invoer ontstaat een nieuwe tak: verwijder of
archiveer de toekomstige historie expliciet. Debuggerwrites zijn ook transacties.

Schermweergave is afgeleid van gasttoestand: na undo redraw/cache-invalidation.
Niet-terugdraaibare externe effecten (bestanden/netwerk) blijven buiten de eerste
versie. Wall-clock framerates en hosttiming mogen gastuitvoering niet sturen.

Stel een geheugenlimiet in voor historie. Toon de oudste terugstapbare stap.
Voor causale vragen zoals 'welke write leverde deze read?' is daarnaast een
optioneel read-/last-writertrace nodig; een undo-journal alleen bewijst dit niet.

## Gefaseerde uitvoering met acceptatiecriteria

### Fase 0 — huidige baseline (nu uitgevoerd)
- Werkboom kopiëren en hashes vastleggen.
- Headless build, bestaande zelftests en Conway-smoketest.
- Geen functionele wijzigingen.

### Fase 1 — expliciete CPU-context
- Verwijder globale actieve CPU/status en maak reset/tick contextafhankelijk.
- Twee instanties afzonderlijk initialiseren zonder elkaar te overschrijven.
- Ongewijzigd programma behoudt bestaande register-/geheugenresultaten.

### Fase 2 — geheugenbus en fysieke blokken
- Identity mapping voor de bestaande 64 KiB.
- Alle stack-, woord-, loader- en frontendwrites via gecontroleerde routes.
- Tests voor 1 KiB-grenzen, 16-bits wraparound, stack en schermbinding.

### Fase 3 — undo voor één CPU
- machine_step-transacties en fysieke write-journaling.
- N stappen vooruit, N terug geeft exact dezelfde volledige machinetoestand.
- Herhaald vooruit geeft dezelfde eindtoestand.

### Fase 4 — MAP en meerdere banken
- Apart lab-profiel en assemblerondersteuning.
- Bank A beschrijven, naar B wisselen en terug: A blijft intact.
- Test mapping van codevenster, ongeldige banken en undo van MAP.

### Fase 5 — twee CPU's en mailbox
- Deterministische globale scheduler en expliciet gedeeld blok.
- Privéwrites blijven privé; gedeelde writes zichtbaar bij beide mappings.
- Producer/consumer-demo; terugspoelen herstelt beide CPU's en mailbox.
- Bewust gebroken handshake als reproduceerbare raceproef.

### Fase 6 — coöperatieve threads en events
- Twee threads per CPU met aparte zero page en stack.
- Geneste JSR/RTS over YIELD behoudt retouradressen.
- WAIT vóór/na SIGNAL, alle threads wachtend en wakeup/undo testen.
- Eventueel TAS voor een tweede synchronisatieproef.

### Fase 7 — visuele debugger en grotere experimenten
- CPU/threadselectie, mappingviewer, fysiek/virtueel geheugen en globale tijdlijn.
- Optioneel last-writertrace en snapshots naar disk.
- Parallelle Conway met dubbele buffers, randuitwisseling en generatiebarrière.

## Open ontwerpkeuzes vóór functionele implementatie

Werkbare defaults zijn hierboven voorgesteld, maar nog niet als ISA vastgelegd:
- lab-opcodecodering en formaatversie;
- aantal banken en wel/geen afgedwongen banktoegang;
- blokkeren van gast-MAP naar slot 0;
- expliciete save/load versus alleen sessiegeheugen;
- programmalaadformaat en statisch aangemaakte CPU's/threads;
- omvang van undo-buffer en later snapshotformaat.

Preëmptie, echte hostparalleliteit, buscyclusnauwkeurigheid en een volledig
besturingssysteem zijn bewuste vervolgstappen. Ze zijn niet nodig om de eerste
onderzoeksvragen over banking, communicatie en terugstappen te beantwoorden.




