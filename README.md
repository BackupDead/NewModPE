# NewModPE
Adds new hooks and methods for ModPE. <br />
It can break the ModPE's limit.
## Current API:
```
event listeners:
    function onNewModPELoaded() - You can use NewModPE APIs after this event is fired
global classes:
    N_Player - provides some player related utilities
    methods:
        static void setRoundAttack(boolean enable, boolean hitMob, int distance); - Enable kill aura
            parameters:
                enable - Enable kill aura?
                hitMob - Hit mob?
                distance - Target mob filtering with distance
        static void setCanFly(boolean enable); - Enable flying
        parameters:
                enable - Enable flying?
    N_Proxy - provides some java & android exploits
    methods:
        static void setFixedScripting(boolean enable); - Let scriptingEnabled always be true
            parameters:
                enable - Enabled fixed scripting?
        static java.lang.Thread startThread(Function run); - Start a thread and return it; because instant threading at onNewModPELoaded has a bug
            parameters:
                run - Function to run
```