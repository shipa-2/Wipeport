**Wipeport** runs *WipEout Omega Collection* (PS4, CUSA05670, update 1.07) natively on Linux,
on a monitor or in a PC VR headset through SteamVR. No game files are included.

### Install

1. Download `Wipeport-x86_64.AppImage`, make it executable and run it:
   ```bash
   chmod +x Wipeport-x86_64.AppImage && ./Wipeport-x86_64.AppImage
   ```
2. In the launcher, either *Install from .pkg* (the base game's and update 1.07's packages from
   your dump; about 47 GB is unpacked) or choose an unpacked `CUSA05670` folder with update 1.07
   copied over the base game. The launcher checks it and says what is missing.
3. *Start*. For VR, start SteamVR with the headset first (launcher → *VR*).

Settings, saves and the shader cache live in `~/.local/share/wocport`. `--play` starts the game
without the launcher window (Steam: *Add a Non-Steam Game*).

### Установка

1. Скачайте `Wipeport-x86_64.AppImage`, сделайте исполняемым и запустите (команда выше).
2. В лаунчере нажмите «Установить из .pkg» (пакеты базовой игры и обновления 1.07 из вашего
   дампа; распакуется около 47 ГБ) или выберите уже распакованную папку `CUSA05670` с
   обновлением 1.07 поверх базовой игры. Лаунчер проверит её и скажет, чего не хватает.
3. *Запустить*. Для VR сначала запустите SteamVR со шлемом (лаунчер → *VR*).

Checksums: `SHA256SUMS`. Details: [README](https://github.com/shipa-2/Wipeport#readme).
