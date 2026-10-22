# Onderzoek: trage opstart van VESC Tool op Windows

Status: 22 oktober 2026, branch `ci/windows-msys2` van `charclo/vesc_tool`.
Dit is werk in uitvoering. De branch bevat tijdelijke tijdmetingen en is niet bedoeld om zo naar upstream te gaan.

## Achtergrond

- VESC Tool start op Windows soms pas na een halve minuut. Op macOS en Linux gaat het veel sneller.
- De officiële Windows-build wordt op Linux gecrosscompileerd met [MXE](https://github.com/mxe/mxe) (zie `build_win`). Het resultaat is een statisch gelinkte 32-bit exe (`i686-w64-mingw32.static`) met Qt 5.
- In deze branch zit een workflow (`.github/workflows/build_win_msys2.yml`) die VESC Tool native op Windows bouwt met [MSYS2](https://www.msys2.org/) en Qt 5.15.19, 64-bit. Hij bouwt een dynamische en een statische variant en meet daarna de opstarttijd op de Windows-runner van GitHub.

## Wat er in deze branch is veranderd

| Bestand | Wijziging | Tijdelijk? |
|---|---|---|
| `.github/workflows/build_win_msys2.yml` | Native Windows-build (MSYS2, Qt 5.15.19), dynamisch en statisch, plus opstartmetingen | Nee, de build zelf is bruikbaar. De meetstap is tijdelijk. |
| `startuptimer.h` | `startupMark()` schrijft `STARTUP <epoch ms> <ms sinds main> <label>` naar stderr als `VT_STARTUP_TIMING` gezet is | Ja |
| `main.cpp`, `mainwindow.cpp` | Meetpunten in `main()`, de `MainWindow`-constructor, elke pagina in `reloadPages()` en rond `checkVersion()` | Ja |
| `utility.cpp` | `Utility::checkVersion()` is asynchroon gemaakt, met een timeout van 10 s | **Nee, dit is de fix** |
| `utility.cpp` | Oude blokkerende versie blijft tijdelijk beschikbaar via `VT_BLOCKING_VERSION_CHECK=1` | Ja |

## Metingen op de GitHub-runner

Run [#8](https://github.com/charclo/vesc_tool/actions/runs/39850620847): Windows Server 2025, dynamische build, `QT_QUICK_BACKEND=software` (de runner heeft geen GPU).

| Stap | Tijd |
|---|---|
| Proces gestart tot `main()` | ongeveer 20 ms |
| `MainWindow` aangemaakt | 1,3 tot 3,6 s |
| waarvan `reloadPages()` | 1,1 tot 3,2 s |
| waarvan `PageWelcome` (laadt `WelcomeQmlPanel.qml` met wizards en profielen) | 0,4 tot 1,6 s, de zwaarste pagina |
| Venster getoond | ongeveer 1,4 s na start |
| `checkVersion()`, normaal netwerk | 0,5 tot 1,6 s |
| `checkVersion()`, `vesc-project.com` onbereikbaar, oude code | **42 s** |
| `checkVersion()`, `vesc-project.com` onbereikbaar, met fix | keert na 0,06 s terug, het antwoord stopt na precies 10 s |

Andere waarnemingen:

- **De eerste start na het bouwen is trager dan de volgende.** Bijvoorbeeld 5,4 s tegen 2,2 s. Dat komt vermoedelijk doordat Qt bij de eerste start de QML-bestanden compileert en in een cache zet.
- **De statische MSYS2-build is trager dan de dynamische.** `reloadPages` is ongeveer twee keer zo traag en het zetten van de iconen gaat van ongeveer 11 naar ongeveer 180 ms. De officiële MXE-build is ook statisch.
- **De statische MSYS2-build crasht op de runner** bij het tonen van het venster (`0xC0000005`). Met softwarematige rendering crasht hij iets later (`0xC000041D`). Dat heeft vermoedelijk met OpenGL zonder GPU te maken en is niet verder onderzocht.
- **Op de runner stond de real-time scan van Windows Defender uit** (`RealTimeProtectionEnabled: False`). Het effect van een virusscanner kon daar dus niet gemeten worden.
- **Windows gaf het venster steeds als "reageert" aan**, ook terwijl de oude `checkVersion()` 42 s wachtte. Die wacht in een geneste event-loop. Op de runner bleef het venster in beide gevallen wit, met en zonder fix, omdat de runner niet goed rendert. Of de gebruiker die 42 s echt merkt, is daarom nog niet bevestigd.

## Conclusie tot nu toe

1. Op de runner kon ik de halve minuut niet nabootsen. De opstart zelf duurt er 1,5 tot 4 s.
2. De versiecheck kan bij een onbereikbare of trage server 42 s hangen. Dat is opgelost met de asynchrone check en de timeout. Of het de klacht verklaart, moet de test op een echte pc laten zien.
3. Andere kandidaten, die op de runner niet te testen waren:
   - de real-time scan van de virusscanner op het grote, niet-ondertekende exe-bestand;
   - de statische 32-bit MXE-build;
   - tragere hardware of een tragere GPU, die het werk in `reloadPages()` vertraagt.

## Test op een Windows-pc (morgen)

### 1. Downloaden

- Ga naar de [Actions-pagina van de fork](https://github.com/charclo/vesc_tool/actions/workflows/build_win_msys2.yml). Download van de laatste geslaagde run het artifact `vesc_tool_windows_msys2_dynamic`. Je moet ingelogd zijn op GitHub.
  - Bijvoorbeeld uit run #8: [vesc_tool_windows_msys2_dynamic](https://github.com/charclo/vesc_tool/actions/runs/39850620847/artifacts/12458020628)
- Pak de zip uit en houd de hele map bij elkaar. Het exe-bestand heeft de DLL's en submappen naast zich nodig.
- Gebruik **niet** het statische artifact, want dat crasht.
- Download voor de vergelijking ook de officiële versie van [vesc-project.com](https://vesc-project.com/).

### 2. Opstarttijd meten met de testbuild

Open PowerShell in de uitgepakte map en voer dit uit. Sluit VESC Tool zodra hij helemaal geladen is.

```powershell
$env:VT_STARTUP_TIMING = 1
"launch $([DateTimeOffset]::Now.ToUnixTimeMilliseconds())" | Set-Content startup.log
Start-Process -Wait -FilePath .\vesc_tool_7.01.exe -RedirectStandardError startup2.log
Get-Content startup2.log | Add-Content startup.log
```

- Doe dit **twee keer**: de eerste start en een tweede start direct daarna. Bewaar de logs als `startup_1.log` en `startup_2.log`.
- Elke regel heeft het formaat `STARTUP <epoch ms> <ms sinds main> <label>`. Het verschil tussen `launch` en de eerste `STARTUP`-regel is de tijd vóór `main()`: het laden van de DLL's en de scan van de virusscanner.
- Noteer ook met de hand **na hoeveel seconden het venster bruikbaar is**.

### 3. De oude, blokkerende versiecheck vergelijken

```powershell
$env:VT_BLOCKING_VERSION_CHECK = 1
# zelfde commando's als bij stap 2, log opslaan als startup_blocking.log
Remove-Item Env:VT_BLOCKING_VERSION_CHECK
```

Optioneel, als beheerder: voeg tijdelijk `10.255.255.1 vesc-project.com` toe aan `C:\Windows\System32\drivers\etc\hosts`. Dan is de server onbereikbaar. Start met en zonder `VT_BLOCKING_VERSION_CHECK` en kijk of het venster in die 42 s bruikbaar is. **Haal de regel daarna weer weg.**

### 4. De officiële versie timen

- Start de officiële `vesc_tool_*.exe` en meet met een stopwatch hoe lang het duurt tot het venster bruikbaar is. Doe dat twee keer.
- Kijk in Taakbeheer of `MsMpEng.exe` (Defender) tijdens het starten veel CPU gebruikt.

### 5. Is de virusscanner de oorzaak?

Hiervoor heb je beheerdersrechten nodig.

1. Ga naar Windows-beveiliging → Virus- en bedreigingsbeveiliging → Instellingen beheren → Uitsluitingen. Voeg de map met de officiële exe toe.
2. Start de officiële versie opnieuw en meet de tijd.
3. **Verwijder de uitsluiting daarna weer.**

Als hij met de uitsluiting snel start, ligt het aan de virusscanner. Een mogelijke oplossing is dan het exe-bestand te ondertekenen, of een dynamische build te gebruiken.

### Wat ik terug wil krijgen

- `startup_1.log`, `startup_2.log` en eventueel `startup_blocking.log`
- de gemeten tijden van de officiële versie, met en zonder uitsluiting
- de versie van Windows, de virusscanner en of de pc via een proxy of bedrijfsnetwerk werkt

## Mogelijke vervolgstappen

- De fix voor `checkVersion()` los aanbieden als pull request voor upstream, zonder de tijdmetingen.
- `PageWelcome` en andere zware pagina's pas laden wanneer ze geopend worden.
- `CONFIG += qtquickcompiler` proberen. Dat staat nu uitgecommentarieerd in `vesc_tool.pro`. Hiermee wordt QML al bij het bouwen gecompileerd in plaats van bij de eerste start.
- De crash van de statische MSYS2-build onderzoeken.
