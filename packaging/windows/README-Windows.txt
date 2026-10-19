Wipeport for Windows 10 and 11 (64-bit)
=======================================

Wipeport runs WipEout Omega Collection (PS4, CUSA05670, update 1.07) natively on the PC, on a
monitor or in a PC VR headset through SteamVR. No game files are included.

1. Unpack the archive anywhere (not into C:\Program Files) and start Wipeport.exe.
2. In the launcher, either "Install from .pkg" (the base game's and update 1.07's packages from
   your dump; about 47 GB is unpacked) or choose an unpacked CUSA05670 folder with update 1.07
   copied over the base game. The launcher checks it and says what is missing.
3. Start. For VR, start SteamVR with the headset first (launcher -> VR).

Settings, saves and the shader cache are in %LOCALAPPDATA%\Wipeport. "Wipeport.exe --play"
starts the game with the saved settings, without the launcher window (Steam: Add a Non-Steam
Game).

Needs a GPU with Vulkan 1.3 (AMD, NVIDIA or Intel, current drivers). The Windows version is new:
if something does not work, the log (launcher -> Log, "Save the log") helps with a report:
https://github.com/shipa-2/Wipeport/issues

Русский
-------
1. Распакуйте архив в любую папку (не в C:\Program Files) и запустите Wipeport.exe.
2. В лаунчере нажмите «Установить из .pkg» (пакеты базовой игры и обновления 1.07 из вашего
   дампа; распакуется около 47 ГБ) или выберите распакованную папку CUSA05670 с обновлением
   1.07 поверх базовой игры. Лаунчер проверит её и скажет, чего не хватает.
3. «Запустить». Для VR сначала запустите SteamVR со шлемом (лаунчер → VR).

Настройки, сохранения и кэш шейдеров — в %LOCALAPPDATA%\Wipeport. «Wipeport.exe --play»
запускает игру с сохранёнными настройками без окна лаунчера.

Licenses: LICENSE (GPL-2.0), CREDITS.md (the projects this port is built from and their
licenses); tools\pkgtool\LICENSE.txt (LibOrbisPkg, LGPL-3.0).
