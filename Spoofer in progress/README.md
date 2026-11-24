# REDMOON

Injecteur interne Fortnite, thème lune rouge.

## Compiler

```bat
cmake -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Sortie : `build/Release/redmoon.exe` + `sideloaded.dll`.
Prérequis : VS 2022 (C++), `assets/payload_astrax.dll`.

## Lancer

Administrateur obligatoire. Refus en VM. Clé PWF Auth requise.

## Traces

- registre : `HKLM\SOFTWARE\RedMoon`
- session : `%LOCALAPPDATA%\RedMoon\session.bin`
- log : `redmoon.log`
- payload : `C:\redmoon.dll`