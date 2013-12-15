node-ca
=======

A node.js addon to interface with the EPICS Channel Access library.

Usage
-----
Require the addon, then create a PV object.  The callback will run whenever the PV gets new data.

    var epicsAddon = require("./build/Release/nodeca.node");
    
    var obj = epicsAddon.PVObject("BPMS:LI23:401:X1H",function(data){
      console.log(data);
    });