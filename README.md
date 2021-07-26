# steam-webgame-launcher

*Launches a web game in Firefox and stays alive until no tabs with the specified
URL exist, allowing better Steam status behaviour when added as a non-Steam
game.*

## Usage

1. Install Firefox, and download steam-webgame-launcher.exe from the
   [Releases page](https://github.com/chuahou/steam-webgame-launcher/releases).
1. Find `recovery.jsonlz4`. It should be located somewhere like
   `C:\Users\[username]\AppData\Roaming\Mozilla\Firefox\Profiles\[profile].default-release\sessionstore-backups\recovery.jsonlz4`
1. In Steam, click on `Games -> Add a Non-Steam Game to my Library...`.
1. Choose steam-webgame-launcher.exe.
1. Open the `Properties` of `steam-webgame-launcher` in Steam. Change the
   name/icon as desired, and set the Launch Options as follows:

   ```
   [URL] "[path to firefox.exe]" "[path to recovery.jsonlz4]"
   ```

1. For example, to launch YouTube you might have

   ```
   https://www.youtube.com "C:\Program Files\Mozilla Firefox\firefox.exe" "C:\Users\[user]\AppData\Roaming\Mozilla\Firefox\Profiles\[profile].default-release\sessionstore-backups\recovery.jsonlz4"
   ```

1. Launch the game.
