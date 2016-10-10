for (i=0,data=[];i<360; ++i) data[i] = null;
var box_data;

var average;
for (i=0,average=[];i<10; ++i) average[i] = null;

window.onload = function () {
  var line = new RGraph.Line({
    id: 'cvs',
    data: data,
    options: {
      labels: [ '60', '55', '50', '45', '40', '35', '30', '25', '20', '15', '10', '5', '0' ],
      backgroundHbars: [
        [23.49,0.02,'rgba(0,0,0,1)'],
        [23,1,'rgba(0,255,0,0.2)'],
      ],
      xaxispos: 'bottom',
      ymax: 30,
      ymin: 20,
      linewidth: 1,
      hmargin: 5,
      colors: ['black'],
      textAccessible: true
    }
  }).draw();
  
  function draw() {
    var sum = 0;
    var count = 0;
    for (var i = 0; i < average.length; i++) {
      if( average[i] != null ) {
        sum += average[i];
        count++;
      }
    }
    var graph_point = sum / count;

    line.original_data[0].push(graph_point);
    line.original_data[0].shift();
    RGraph.clear(line.canvas);
    line.draw();
    setTimeout(draw, 10000);
  }
  setTimeout(draw, 10000);
};

function update() {
  $.getJSON( "./json" )
    .done(function( data ) {
      box_data = data;
      var logic_mode;
      if( data.mode == "T" ) {
        logic_mode = "Thermostat";
      } else if ( data.mode == "P" ) {
        logic_mode = "PID";
      } else {
        logic_mode = "Unknown";
      }
      var percent = data.rate * 100 / data.windowSize;
      $("#window_size").text(data.windowSize/1000);
      $("#rate").text(parseFloat(Math.round(data.rate*1000)/1000000).toFixed(5));
      $("#percent").text(Math.round(percent*10000)/10000);
      $("#target").text(data.target);
      $("#temperature").text(data.temperature);
      $("#logic_mode").text(logic_mode);
      average.push(data.temperature);
      average.shift();
    });
};
window.setInterval(update, 1000);
