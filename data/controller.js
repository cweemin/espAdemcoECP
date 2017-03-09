function onBodyLoad(){ 
  var colorList = ['red', 'yellow', 'green', 'blue'];
  //Create the audio tag
  var soundFile = document.createElement("audio");
  soundFile.preload = "auto";

  //Load the sound file (using a source element for expandability)
  var src = document.createElement("source");
  src.src = "Beep.mp3";
  soundFile.appendChild(src);

  //Load the audio tag
  //It auto plays as a fallback
  soundFile.load();
  soundFile.volume = 0.000000;
  soundFile.play();

  //Plays the sound
  var playBeep = function() {
    //Set the current time for the audio file to the beginning
    soundFile.currentTime = 0.01;
    soundFile.volume = 1.0;

    //Due to a bug in Firefox, the audio needs to be played after a delay
    setTimeout(function(){soundFile.play();},1);
  }
  //WS connection
  var wsUri = 'ws://' + self.location.hostname+ ':10112';
  var wsconn = new WebSocket(wsUri);
  var setFaults = function( check ) {
    for(let i =1; i<=5; i++) {
      document.getElementById('zone'+i).checked = check;
    }
  };

  wsconn.onopen = function(evt) {
    document.getElementById("wsconnected").checked = true;
  };
  wsconn.onclose = function(evt) {
    document.getElementById("wsconnected").checked = false;
  };
  wsconn.onerror =  function(evt) {
    console.log("Error connection to ws");
  }
  wsconn.onmessage = function(evt) {
    var alarmdata = JSON.parse(evt.data);
    if (alarmdata.READY >0 ) 
      setFaults(false);
    else 
      document.getElementById('zone' + alarmdata.zone).checked = true;
    document.getElementById('msg').value = alarmdata.msg;
    document.getElementById('ready').checked = (alarmdata.READY > 0);
    document.getElementById('armedStay').checked = (alarmdata.ARMED_STAY > 0);
    document.getElementById('armedAway').checked = (alarmdata.ARMED_AWAY > 0);
    document.getElementById('chime').checked = (alarmdata.chime > 0);
    if(alarmdata.beep > 0) {
      playBeep();
    }
  };
  var changeLed = function(color, id){

    let tmplist = document.getElementById(id).classList;
    // Remove all class
    ['red','yellow','green','blue'].map((idx)=> {
	tmplist.remove('led-'+idx);
	});
    tmplist.add('led-'+color);
  };
  var stopButton = document.getElementById("stop-button");
  stopButton.onclick = function(e){
    document.getElementById("zone1").checked = false;
  }
  var startButton = document.getElementById("start-button");
  startButton.onclick = function(e){
    document.getElementById("zone1").checked = true;
  }
}
