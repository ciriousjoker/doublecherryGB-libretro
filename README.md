## DoubleCherryGB

<img src="/assets/readme/header.png">


[![Double Cherry GB Latest Release][dcgb-release-badge]][dcgb-latest-release]
[![Gitlab Pipeline Status](https://img.shields.io/gitlab/pipeline-status/libretro%2FdoublecherryGB-libretro?gitlab_url=https%3A%2F%2Fgit.libretro.com%2F&branch=master&style=for-the-badge&logo=gitlab&label=Libretro%20Build)](https://git.libretro.com/libretro/doublecherryGB-libretro/-/pipelines)


**author:** Tim Oelrichs<br>



> DoubleCherryGB is an open source (GPLv3) GB/GBC emulator with up to 16 Player support, based on the TGBDual libretro core by GIGO and Hii.
> It emulates the GAMEBOY 4 PLAYER ADAPTER (DMG-07), adds 3-16 Player Link Hacks for two-player games, enables Pokemon-Trading and much more.
> It's named after the Double Cherry Item from Super Mario 3D World.


<hr>

# Installation

Installation instructions may vary depending on your chosen libretro frontend.

## Native builds and MBC30

MBC30 cartridges support up to 4 MiB ROM and 64 KiB SRAM, including 4 MiB ROM
hacks that retain 32 KiB saves. See [native build and regression test instructions](tests/README.md)
for macOS/Linux, generated mapper fixtures and multiplayer state coverage.

## RetroArch

You can install DoubleCherryGB through the built-in core downloader where supported.
If your build of RetroArch doesn't include it (e.g. Steam)
or if you want to use a development build,
you can install it [RetroArch][retroarch] manually like so:

1. Place `DoubleCherryGB_libretro.dll` (or `.so` or `.dylib`, depending on the platform) in RetroArch's `cores` directory.
2. Place `DoubleCherryGB_libretro.info` in the same directory as the other `.info` files,
   which is usually `cores` or `info` depending on the platform.

<hr>

## Features

### Pokemon Stuff
- [Builtin Mew and Celebi Maschine and weekly GEN1 Pokemon Distributions](#Builtin-Mew-and-Celebi-Maschine-and-weekly-GEN1-Pokemon-Distributions)
- [Link Cable Trading over network](#Link-Cable-Trading-over-network), easy Pokemon Gen1 and Gen2 online trading via Lobby (Retroarch 1.17 needed)

  <br>

### Multiplayer
- GAMEBOY 4 PLAYER ADAPTER (DMG-07) emulation (F1-Race, Wave Race, Yoshi's Cookie...) 
- up to 16-Player Faceball 2000 emulation (1-15 without bugfix-patch, 16 players needs [patched](https://github.com/Zarithya/Faceball2000DX) rom)
  
- up to 16-Player Tetris Battle Royal Hack (virtual link device, no modified rom needed)
- up to 16-Player Kwirk Multiplayer Hack (virtual link device, no modified rom needed)
- up to 16 Player 4x4Player Adapter
- Of course up to 16 Player Singleplayer for speedrun-battles or if you have arguing kids :D
- local splitscreen
- netplay support

  <br>
  
### Gameboy Peripherals Emulation
- Gameboy Printer Emulation with PNG-Print and DIN A4, DIN A5, DIN A6 upscale
- Gameboy 4 Player Adapter (DMG-07)
- Mobile Adapter GB Emulation
- [TV REMOTE](#TV-REMOTE) Emulation
- [BARCODE BOY](#BARCODE-BOY) Emulation full support (all barcodes are hardcoded, use the numkeys 0-9 for selection)
- [BARCODE TAISEN BARDIGUN](#BARCODE-TAISEN-BARDIGUN) Emulation full Support (all barcodes are hardcoded, use the numkeys 0-8 for selection)
- [POWER ANTENNA/BUGSENSOR](#power-antenna-and-bugsensor) Emulation (LED via led driver overlay & controller rumble on led flash)

<br>

### unreleased Gameboy Peripherals Emulation
- Faceball 2000 16 Player RingLink-Cable
- [Alleyway Analog Controller](#Unreleased-Alleyway-Analog-Controller)

<br>

### custom Linkcable Peripherals
- Multiplayer Hacks
- PKMBuddy Boy
- LinkcableHub, auto switching peripherals for Games that support multiple Linkcable Devices (GB Printer, Linkcable, etc.)

### Retroarch-Features
- Retroarchivments support
- Netplay

<br>

## How to

You can download the core via RetroArch Online-Updater on Windows, Linux, Android and IOS.
For Android make sure to use the retroarch APK from retroarch.com and not the GooglePlay Version.

For Multiplayer you have to increase the amount of emulated Gameboy in the core options in your frontend and reload.

<br>

## Builtin Mew and Celebi Maschine and weekly  Pokemon Distributions 

<img src="/assets/readme/pkbuddyboy.png">

DoubleCherryGB has a **built-in virtual link cable device** that can trade Pokémon. I called it the **PKMBuddy Boy**<br>
Visit the Cable Club to trigger the PKMBuddy Boy Trade.
You can uses the Numkeys on your keyboard or the SELECT Button to change Pokemon you want to get.

---

### Key Features

* **Auto Mew / Celebi Distribution**
  If you don't own a Mew or Celebi, it will trade you a **Lv 5 Mew/Celebi**.
* **Weekly Pokémon Distribution Events**
  If you already own a Mew, it will offer **weekly Pokémon Distribution Events** with a selection of different Pokémon *(see table below)*.
  > **Note:** Mew Detection might not work with all game versions yet, as different versions save Pokédex data differently. If it does not work for you, disable the "Auto Mew" option under:
  > `Core Options` → `GameBoy Linkcable Emulation`
* **Trade Evolutions**
  You can also use this feature to trade Pokémon back and forth to trigger **trade-based evolutions**.

* **Gen 2 PCNYa Events**
  For Gen 2, I recreated many of the **PCNYa Distribution Events** *(still a work in progress)*.
    * **Reference:** [Bulbapedia - List of PCNY event Pokémon distributions in Generation II](https://bulbapedia.bulbagarden.net/wiki/List_of_PCNY_event_Pok%C3%A9mon_distributions_in_Generation_II)

* **Time Capsule (Gen 1 & Gen 2)**
  You can now also access Gen 1 events in *Gold*, *Silver*, and *Crystal* via the **Time Capsule** by changing the **Trading Mode** by pressing the **`LT` / `L2` -Button**.

It's already preconfigured. Leave emulated Gameboys at 1 and just visit the cable club ingame.

Weekly Pokemon Distribution List
https://docs.google.com/spreadsheets/d/1SMcpTz_4Mwo1eVAKWB3AbaLHJNthkB5sKHlOPnQw2UI/edit?gid=0#gid=0

<img src="/assets/readme/screenshots/gotMew.png">

<br>

## Link Cable Trading over network
#### Easy gen1/gen2 pokemon online trading

Why is this easier? No setup and no workarounds. No need to own the same roms (region, edition). Should work cross platform.
You can find trading partners over the netplay lobby (netplay -> refresh Host List), and can use the netplay text-chat by pressing ^ on your keyboard.
Also works in LAN. 

I recommend to use the [RetroArch Discord server](https://discord.com/channels/184109094070779904/326960443157381120) to find players and use voice-chats.

Atleast Retroarch 1.17 is needed for the netpacket api.
Only the linkcable data is transfered over network, so you won't see the others player's screen.

- make sure emulated gameboys is set to 1 in the Core Options 
- load your rom
- start or join a netplay (a second player can join any time)

The netpacket api is activated when you set emulated gameboys to 1. 
This is only meant for trading purposes and may be too slow for actual Multiplayer.
For Multiplayer-sessions please set the amount of emulated gameboys to 2 or higher, because this will activate the old savestate sync for lagfree netplay.  

Trading/Battling works on:

- Pokemon R/B/Y/S/G/C (trading works better than battling, which can be unstable)
- Pokemon Trading Card Game 1 and 2  (Hacky workaround, may lag a bit, but it works)

Untested or not working yet: 
- RoboPon Sun/Star
- Medarot 1/2/3 Kabuto/Kuwagata/Parts Collection
- Telefang Speed/Power
- Network Adventure Bugsite Alpha/Beta
- Dragon Warrior (Quest) Monster
- Dragon Warrior (Quest) Monster 2 Tara's Adventure / Cobi's Journey
- Monster Rancher Battle Card Game
- Zelda OoA / OoS ring trade
- ...

Infrared over network is currently not supported (GBE+ does) but maybe in a future release.

Have fun!

<br>

## 4-Player Adapter Emulation

<img src="/assets/readme/4playeradapter.png">

![](/assets/readme/screenshots/F-1%20Race.png)
![](/assets/readme/screenshots/SuperRC.png)
![](/assets/readme/screenshots/TopRankTennis.png)
![](/assets/readme/screenshots/YoshisCookie.png)
![](/assets/readme/screenshots/WaveRace.png)

Netplay is working. Make sure to have all the same amount of emulated gameboys in your core options.

[compatibility list](https://peppermint-theater-276.notion.site/4-player-adapter-compatibility-8d33abe17f1d4bf5a361b368af6622b7)

## Unreleased Alleyway Analog Controller

<img src="/assets/readme/alleywaycontroller.png">


This was a planned but never released peripheral, and to my knowledge, it's only known today because the game constantly polls the serial port.
It turns out that the game can be controlled via the link port when it receives correctly formatted data.
I've implemented several control styles — you can press SELECT to toggle between them:

- Right analog stick — should even work with a steering wheel! 😄
- Mouse control
- Touchscreen controls

Thanks to Jarhmander and lidnariq for figuring this out.
See: https://forums.nesdev.org/viewtopic.php?t=18348

<br>

## TV REMOTE

<img src="/assets/readme/tvremote.png">

The GBC has a IR receicer and some games used this to unlock cotent with a TV REMOTE.
You can choose between 4 different JVC TV-Remotes. Use SELECT to switch between the Remotes and use the Numpad to press a button.
If you are playing on a handheld you can activate the "autorandom IR Signal" in the Core options.

- Robopon Sun (HUC3) - works
- Bomberman MAX Blue/RED - works
- Bomberman Daman Bakugaiden V - Final Mega Tune - works
- Doraemon Memories - Nobita no Omoide Daibouken - not working

<br>

## Mobile Adapter GB - ReonReady

DoubleCherryGB implements the libmobile library by pfero for emulating the japanese exclusive Mobile Adapter GB.

Team Reon is currently working on creating custom servers and translating roms to bringing back the experinnce. As this is still a work in progress (WIP), there are no public servers available yet. However, you can host one yourself by cloning https://github.com/REONTeam/reon.

**Setup Instructions**
 1. Make sure to configure the settings in the **Core Options**.

 2. If you want to connect to a server on another device, set the option to **Custom** and edit the **mobile_adapter_gb_custom_server_config.ini** file located in your RetroArch system folder:

```
dns_ip = "INSERT_IP_FROM_YOUR_CUSTOM_SERVER"
dns_port = "INSERT_YOUR_CONFIGURED_PORT"
relay_ip = "INSERT_IP_FROM_YOUR_CUSTOM_SERVER"
relay_port = "INSERT_YOUR_CONFIGURED_PORT"
```

Games with Mobile Adapter GB support: 

- Pokemon Crystal (Japan or mobile adapter patched)
- Game Boy Wars 3
- Hello Kitty Happy House  
- Mobile Golf 
- Mobile Trainer   
- Net de Get Note: The MBC6 chip is not yet emulated by DoubleCherryGB)

❤️ Many thanks to pfero for creating libmobile, and to the entire Team Reon for their hard work and dedication in making the Mobile Adapter GB accessible to everyone!

<br>
 
## BARCODE BOY

<img src="/assets/readme/barcodeboy.png">

The BARCODE BOY is a japan exclusive peripheral (See https://shonumi.github.io/articles/art7.html).

The following games have support or even need the BARCODE BOY to be played:

- BATTLE SPACE (needed)
- Monster Maker Barcode Saga (needed)
- Kattobi Road (additional content)
- Family Jockey 2 (additional content)
- Famista 3 (additional content)

All Barcode Cards are hardcoded. You can select different cards with the Numkeys 0-9.
If you don't have a keyboard, the the barcode cards will be changed randomly.

<br>

## BARCODE TAISEN BARDIGUN

A japan exclusive GBC game that came with Barcode Reader(See https://shonumi.github.io/articles/art6.html).

All Barcode Cards are hardcoded. You can select different cards with the Numkeys 0-8 or use the SELECT Button.

<br>

## POWER ANTENNA and BUGSENSOR

These are devices, that came with the games **Keitai Denjuu Telefang** and **Network Adventure Bugsite**  and are plugged into the linkport. It's simply a led that flashes when, something special happens ingame.
DoubleCherryGB uses the controller rumble and the retroarch led api to simulate the led flashing on an overlay.

<img src="/assets/readme/screenshots/Bugsensor-overlay.gif">

[Here](https://github.com/TimOelrichs/doublecherryGB-libretro/blob/master/overlays/gb-bugsensor.zip) is the example overlay i used over the mega-bezels gbc shader. But you can also make your own.

You need to edit your **retroarch.cfg** file the get the overlay to work. (See https://docs.libretro.com/guides/led-drivers/)

led_driver = "overlay" <br>
led1_map = "1"

<br>

## Faceball 2000 - 16 Player Ring-Link Cable

DoubleCherryGB is happy to be the first emulator to support the 16-player Ring-Link setup for Faceball 2000.

<br>

## Support DoubleCherryGB
I have tons of ideas—but not nearly enough time to bring them all to life. Your support helps me dedicate more time to development and make DoubleCherry the cherry on top of your retro gaming experience 🍒

https://patreon.com/double_cherry_projects<br>
https://www.buymeacoffee.com/timoelrichs<br>

Thanks to
- Shonumi for doing the research and technical documentation for Gameboy and many peripherals/accessories/add-ons  https://shonumi.github.io/index.html
- pfero and the entire TeamReon :heart:
- PSYRaven for the netpacket api and davidgf for his implemtation on gpSP
- Ninoh-Fox for his work on Koriki cfw
- GIGO and Hii for making TGBDual and the libretro port

![](/assets/readme/screenshots/Faceball%202000.png)
![](/assets/readme/screenshots/Tetris.png)
![](/assets/readme/screenshots/tetris_winner.png)
![](/assets/readme/screenshots/Kwirk.png)

Original sources from
- https://github.com/libretro/tgbdual-libretro
- http://gigo.retrogames.com/download.html#tgb-dual
- http://shinh.skr.jp/tgbdualsdl/

## License
This project is licensed under the **GNU General Public License v3.0**.  
See the [LICENSE](LICENSE) file for details.

[retroarch]: https://www.retroarch.com
[dcgb-release-badge]: https://img.shields.io/github/v/release/TimOelrichs/doublecherryGB-libretro?style=for-the-badge&&logo=github&label=Double%20Cherry%20GB&link=https%3A%2F%2Fgithub.com%2FTimOelrichs%2FdoublecherryGB-libretro%2Freleases%2Flatest
[dcgb-latest-release]: https://github.com/TimOelrichs/doublecherryGB-libretro/releases/latest
