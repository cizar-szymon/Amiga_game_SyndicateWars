# Pomysły optymalizacji zgłoszone przez użytkownika

Każdy pomysł ma być osobną opcją (dziś ENV: / Tool Type, później menu opcji w grze, w którym
się je włącza i wyłącza). Stan: ⏳ do zrobienia, 🔧 w pracy, ✅ zrobione.

| Data | Pomysł | Stan | Uwagi |
|---|---|---|---|
| 15.11.2026 | Przy dużym obciążeniu (duży wybuch na ekranie) wolniej odświeżać HUD | ⏳ | bezpieczne – nie zmienia logiki gry |
| 15.11.2026 | Przy dużym obciążeniu liczyć tylko osoby widoczne na ekranie | ⏳ | zmienia przebieg misji (wrogowie/agenci poza ekranem by stanęli) – tylko ostrożnie, np. wzmocnić istniejące pomijanie dalekich cywilów (`SyndWarsCivLOD`) |
| 15.11.2026 | Znacznik „gra spada poniżej 16 kl./s” i wtedy w czasie rzeczywistym włączać dodatkowe optymalizacje | ⏳ | z histerezą, żeby nie migało |
| 15.11.2026 | HUD odświeżany 2 razy rzadziej niż obraz 3D (też punkty ludzi na radarze migają za agresywnie) | 🔧 | radar: opcja **RADAR HALF RATE** (menu opcji, `SyndWarsRadar2`) – od 15.11.2026 domyślnie włączona na Amidze i ARM ✅; reszta paneli (agenci, broń): ⏳ – trzeba zapamiętywać ich piksele jak przy radarze, nowa opcja „HUD HALF RATE” |
| 15.11.2026 | Przy wielu wybuchach tempo spada do 12 z 16 tur/s – w takich chwilach oszczędzać czas procesora kosztem jakości albo optymalizacją | 🔧 | w grze jest przełącznik DETAIL HI/LO w menu pauzy (mniej odłamków wybuchów); plan: scena testowa z wybuchami, pomiar profilerem na 68060, potem automatyczne obniżanie szczegółów przy spadku tempa (pomysł ze znacznikiem < 16) i optymalizacja najdroższego elementu |
