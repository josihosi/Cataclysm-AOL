# Cataclysm: Arsenic and Old Lace

C-AOL is a fork of Cataclysm: Dark Days Ahead and Cataclysm-TLG. If you already play CDDA, the biggest change is what happens around your shelter. Other groups can pick up signs that someone lives there, send scouts, and act on what those scouts bring home.

**[Download a build](https://github.com/josihosi/Cataclysm-AOL/releases)** · **[Watch the gameplay video](https://www.youtube.com/watch?v=3-3gJbJabyI)** · [Setup and build guide](doc/SETUP.md)

## What’s different from CDDA?

- **Your camp can give itself away.** Smoke by day, exposed light after dark, and loud sounds can give groups on the overmap clues on your location. Weather, terrain, and where you put the fire affect who can pick up the signal.
- **Hostile camps send people to find out what’s there.** Bandits and cannibals can send a pair to a lookout. Scouts travel on the overmap, can be spotted or driven off, and bring home observations the camp can act on.
- **A shakedown can turn into a return visit.** Bandits can demand payment, search for reachable supplies, or fight if things go badly. If the visit pays off, they can remember the place and come back later.
- **Some threats hunt differently.** Mi-go patrols and towers can investigate smoke, and some roaming zombie hordes can be drawn toward visible light.
- **More to do with companions at camp.** Assign followers to patrol or use shared camp lockers to supply them. You can also connect an LLM for free-text dialogue and a limited set of follower actions, such as asking someone to follow, wait, or hand over an item. No LLM setup is required to play.

## Get started

Pick an archive for your platform from [GitHub Releases](https://github.com/josihosi/Cataclysm-AOL/releases). For source builds and optional LLM setup, see the [setup guide](doc/SETUP.md). Start a fresh world for your first run and press **?** in game to see commands and key bindings.

C-AOL is an **alpha in active development**. Excpect performance issues and bugs. And tell me about them, please.

## Credits and license

Thank you to Whales, the original Cataclysm author, and to Kevin Granade and the developers and contributors of [Cataclysm: Dark Days Ahead](https://github.com/CleverRaven/Cataclysm-DDA). Thanks also to the [Cataclysm-TLG](https://github.com/Cataclysm-TLG/Cataclysm-TLG) contributors. The current release doesnt cover TLG, but theres a legacy LLM release on TLG. C-AOL builds on their work and modifies the game systems described above. See the [in-game credits](data/credits/en.credits) for more names and contributions.

Cataclysm is licensed under [Creative Commons Attribution-ShareAlike 3.0 Unported](https://creativecommons.org/licenses/by-sa/3.0/). See [LICENSE.txt](LICENSE.txt) for the project’s license and bundled component notices.

Bug reports and playtest stories are welcome. If you include an LLM debug log, review it for private content first.

<details>
<summary>ASCII title art</summary>

```text
 @@@@@          @                  @@                       @                                      .--::.-=-=-*+=.
 @   @   @@    @@@    @@@    @@     @@           @@@   @@@                                      .....  .:*#*-:   .
 @         @@   @        @ @@   @   @@   @   @  @     @@@@@@                                  .:..-::..:.::
 @     @@  @@   @    @@  @ @@       @@   @   @   @@@@ @@@@@@                                ==-:-:==:. ..:-+==-*%::--..    .
 @@@@@  @@@@@   @@@  @@@@@  @@@@@   @@   @@@@@  @@@@@ @ @@@@                              -......::--:....::... .-+.   .    ..
                         @@   @@@@@      @@@@@                                           =#=.  .:-=+=::.     .:.
       @@     @@ @@    @@  @@ @@      @@   @@  @@    @@ @@                              :+*#-:..   :-:..     .::.
       @@      @   @@  @      @@      @@@  @@  @@   @@                                   .-=+-=:.     :.      .. ..
       @@      @   @@  @@     @@      @@@  @@  @@  @@                                  ...:::-*%#=:.     .          . .
      @ @@     @   @@  @@@    @@@@@   @@@@ @@  @@  @@                                  .:::--*@@@@%#*-::.
      @ @@     @  @@    @@@   @@      @ @@ @@  @@  @@                                .:.:-:-+%@@@@@@@%%#*+=-:.      ...:--.
     @   @@    @@@        @@  @@      @  @@@@  @@  @@                                : .:---+%@@@@@@@%%%%%%%#=:.::-**+*++==-
     @   @@    @ @@        @@ @@      @  @@@@  @@  @@                                .  :::=*%%@@@@%@@%%%%##%%%###********++=:
    @    @@    @ @@        @@ @@   @  @   @@@  @@  @@                                :  :-:=*%%%@@@@@@%%%#%%%%%%%#######***++-.
    @     @@   @  @@  @@   @  @@  @@  @    @@  @@   @@                               -..::-=*%%%@@@@@%%###%%%%%%%%%%###**++++=:
   @@     @@  @@        @@     @  @   @        @@    @@@@                          #@=:...-*%%@@@@@%%%%%%##%%%%%%%%%%#*****+++=.
                                   @                                              #=-+...=#%%%%#=:-=*#%%%%##########**=-:...-+*+  :--
                        @ @@@@@@@@ @                                              *-*%=.:#%%%*=-::.   .:-=+**##***-:.  .:::--=++.:*+
                        @@ @   @ @@@                                              =+#*+-:#%%#+****+========+***++=--========++++.-*:
        @ @   @@    @@@@      @@       @     @ @@ @@ @                    +#*:    .##-=+=#%%#*++=-***+=:--=+**++=------:---===++:+*.     =%%%=
       @   @  @@    @@ @@     @@       @@   @@    @@                    +@@@@@@-   *%=+%*%%##*+=-:+: .*=---*%#*=---==  :*:.=++++=**    .@@@@@@@.
      @@   @@ @@    @@  @     @@       @@   @     @@                   .@@@@@@@@.   @%*#%%%%%%##%%%#+*%#*#*%@%#*++++##***++++++++*+   .@@@@@@@@=
      @@   @@ @@    @@  @@    @@      @ @  @@     @@@@                  @@@@@@@@@   #%%####%%%%%%%#*+++*#%%@@%#*++*++====++**+++++-   #@@@@@@@@:
      @@   @@ @@    @@  @@    @@      @ @@ @@     @@                  .#@%%@@@@@@@:  :*+=###%%%@@%%####%%@@@@%#**+**********+++=:   :%@@@%%%%%%%+
      @@   @@ @@    @@  @     @@     @@@@@ @@     @@                 #%%%%%%%%@@@@@@+.   =###%%%%%%%%@%%%@@@@%#***+++*****+++++: .*@@@@@%%%%%%%%%%=
      @@   @  @@ @  @@ @      @@  @  @   @@ @@    @@  @             +%###%%%%%%%@%@@@@@#::######%%%%%%%#%%%@@%#*++++++***++===+#@@@@%%%%%%%%%%####*
        @ @   @@@@  @@@       @@@@@ @@   @@  @@@  @@@@@             .%######%%%%%%%%%@@@@@##########****#@@@@%#***+===++======*@%@%%%%%%%%%######%=
                                                                      *#%%#+.    =#%%%%%@@%####**#*++++**+=#%#**=:+*+=========#@%%%%*:     .=###=
                                                                                      *%%%%#####**+++#%%%%%#+::==+++++========*#=
                                                                                          =###**#*##%%%%%%#*+-===++++++======-
                                                                                            -#*###%%#*******+**++==-===+====-
                                                                                             :**###*+--==-::::::::-+--=====.
                                                                                              .***+++***#%####*+++=-=----=.
                                                                                               ++++=++==--::...:--------=
                                                                              .--.        :*%@@@#+****#*%%@%###*+======+@@%+.
                                                                            =@@@@@@@%##%@@@@%%%%%#*+*#%%%%%**#*+++++==#%%@@@@@@%*+*%@@@@@@+
                                                                            @@@@@@@@@@@@@%%%%%%%*.  .+****+==+++===- :#%%%%%%@@@@@@@@@@@@@@+
                                                                            %@@@@@@@@@@@%%%%%=          ..:::-:::.      .+%%%%%%@@@@@@@@@@@=
                                                                             *@@@@@@@@%%%%=                                 =%%%%@@@@@@@@@-
                                                                               *@%@@@%%%+                                     =%%%@@%@@%:
                                                                               *%%%%%%%#                                       =%%%%%%%@.
                                                                               +%####%%                                         #%#%%#%%.
                                                                                =###%#                                           *%###%
```
</details>
