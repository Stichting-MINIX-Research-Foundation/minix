// Main javascript for the BeagleBone Weather Cape demo.
// 
// This code handles drawing the thermometer, gauges, and light level dot.
// Most of this code is from the original weatherstation demo. There is some
// room for improvement (i.e. changing the scale on the gauges requires a lot
// of tweaking, not just changing pmax and pmin), but it gets the job done.

// global vars
var PI = 3.14;
var HALF_PI = 1.57;
var TWO_PI = 6.28;

// set defaults
var pressure = 0;
var pmax = 1200;
var pmin = 200;
var pdata = 0;
var punit = "hPa";

var temp = 0.0;
var tmax = 40.0;
var tmin = -20.0;
var tunit = "C";

var humidity = 0.0;
var hmax = 100.0;
var hmin = 0.0;
var hdata = 0.0;
var hunit = "% Humidity";

var lux = 0;
var lmax = 2000;
var lmin = 0;
var lunit = "lux";

var setWidth = function() {
 var canvasWidth = window.innerWidth * 0.9;
 if ( canvasWidth > (0.8 * window.innerHeight)) {
  canvasWidth = 1.8 * window.innerHeight;
  $('#heading').hide();
 } else
  $('#heading').show();

 barometerWidth = canvasWidth*0.5;
 barometerHeight = barometerWidth;
 thermometerWidth = canvasWidth*0.25;
 lightWidth = canvasWidth*0.25;
};
setWidth();

var barometerSketchProc = function(p) {
 p.size(barometerWidth, barometerWidth);
 p.draw = function() {
 p.size(barometerWidth, barometerWidth);
 barometerWidth=p.width;
 p.background(0,0);
  
  // barometer
  p.fill(220);
  p.noStroke();
  // Angles for sin() and cos() start at 3 o'clock;
  // subtract HALF_PI to make them start at the top
  p.ellipse(barometerWidth/2, barometerWidth/2, barometerWidth*0.8, barometerWidth*0.8);
  
   var angle = -HALF_PI + HALF_PI / 3;
   var inc = TWO_PI / 12;
   p.stroke(0);
   p.strokeWeight(barometerWidth*0.015);
   p.arc(barometerWidth/2, barometerWidth/2, barometerWidth*0.8, barometerWidth*0.8, -(4/3)*PI, PI/3);

   // we want a range from 200 hPa to 1200 hPa
   // we want a range from ±950 - ±1050 milibar
   // 1-5=1010-1050, 7-12=950-1000
   p.textSize(Math.round(barometerWidth*0.04));
   for ( i = 1; i <= 12; i++, angle += inc ) {
       if(i!=6) {
         p.line(barometerWidth/2 + Math.cos(angle) * barometerWidth*0.35,barometerWidth/2 + Math.sin(angle) * barometerWidth*.35,barometerWidth/2 + Math.cos(angle) * barometerWidth*0.4,barometerWidth/2 + Math.sin(angle) * barometerWidth*.4);
         if (i < 6) {
           myText = 700 + 100*i;
         } else {
           myText = 100*i - 500;
         }     
         myWidth = p.textWidth(myText);
         p.fill(0, 102, 153);
         p.text(myText, Math.round(barometerWidth/2 + Math.cos(angle) * barometerWidth*0.3 - myWidth/2),Math.round(barometerWidth/2 + Math.sin(angle) * barometerWidth*0.3));
       } else {
         myText = pdata + ' ' + punit;  
         myWidth = p.textWidth(myText);
         p.fill(0, 102, 153);
         p.text(myText, Math.round(barometerWidth/2 + Math.cos(angle) * barometerWidth*0.3 - myWidth/2),Math.round(barometerWidth/2 + Math.sin(angle) * barometerWidth*0.3));
       }
   }
   
  
   // RH scale
   p.fill(220);
   p.stroke(0);
   p.strokeWeight(barometerWidth*0.005);
   p.arc(barometerWidth/2, barometerWidth/2, barometerWidth*0.4, barometerWidth*0.4, -(4/3)*PI, PI/3);
   
   // we want a range from 0 - 100%
   // 1-5=60-100, 7-12=0-50
   p.textSize(Math.round(barometerWidth*0.02));
   for ( i = 1; i <= 12; i++, angle += inc ) {
       if(i!=6) {
         p.line(barometerWidth/2 + Math.cos(angle) * barometerWidth*0.18,barometerWidth/2 + Math.sin(angle) * barometerWidth*.18,barometerWidth/2 + Math.cos(angle) * barometerWidth*0.2,barometerWidth/2 + Math.sin(angle) * barometerWidth*.2);
         if (i < 6) {
           myText = 50 +10*i;
         } else {
           myText = 10*i - 70;
         }     
         myWidth = p.textWidth(myText);
         p.fill(0, 102, 153);
         p.text(myText, Math.round(barometerWidth/2 + Math.cos(angle) * barometerWidth*0.16 - myWidth/2),Math.round(barometerWidth/2 + Math.sin(angle) * barometerWidth*0.16));
       } else {
         myText = hdata + ' ' + hunit;  
         myWidth = p.textWidth(myText);
         p.fill(0, 102, 153);
         p.text(myText, Math.round(barometerWidth/2 + Math.cos(angle) * barometerWidth*0.12 - myWidth/2),Math.round(barometerWidth/2 + Math.sin(angle) * barometerWidth*0.12));
       }
   }
   
        //humidity needle
  p.stroke(60);
  p.strokeWeight(barometerWidth*0.005);
  p.line(-Math.cos(humidity) * barometerWidth*0.05  + barometerWidth/2, -Math.sin(humidity) * barometerWidth*0.05 + barometerWidth/2, Math.cos(humidity) * barometerWidth*0.15 + barometerWidth/2, Math.sin(humidity) * barometerWidth*0.15 + barometerWidth/2);
  //p.ellipse(barometerWidth/2, barometerWidth/2, barometerWidth/20, barometerWidth/20);
   
     // pressure needle
  p.stroke(60);
  p.strokeWeight(barometerWidth*0.015);
  p.line(-Math.cos(pressure) * barometerWidth*0.05  + barometerWidth/2, -Math.sin(pressure) * barometerWidth*0.05 + barometerWidth/2, Math.cos(pressure) * barometerWidth*0.35 + barometerWidth/2, Math.sin(pressure) * barometerWidth*0.35 + barometerWidth/2);
  p.ellipse(barometerWidth/2, barometerWidth/2, barometerWidth/20, barometerWidth/20);
  
 };
 p.noLoop();
}

var thermometerSketchProc = function(p) {
 p.size(thermometerWidth, barometerHeight);
 p.draw = function() {
  p.size(thermometerWidth, barometerHeight);
  thermometerWidth=p.width;
  p.background(0,0);

  // thermometer
  // contour
  p.stroke(0);
  p.strokeWeight(thermometerWidth*0.27);
  p.line(thermometerWidth/2,thermometerWidth*1.75,thermometerWidth/2,thermometerWidth/4);
  p.ellipse(thermometerWidth/2, thermometerWidth*1.75, thermometerWidth/5, thermometerWidth/5);
  
  p.strokeWeight(thermometerWidth*0.22);
  p.stroke(255);
  p.line(thermometerWidth/2,thermometerWidth*1.75,thermometerWidth/2,thermometerWidth/4);
  // mercury bubble
  if(temp > 0) {
    p.stroke(255,0,0);
  } else {
    p.stroke(0,0,255)
  }  
  p.ellipse(thermometerWidth/2, thermometerWidth*1.75, thermometerWidth/5, thermometerWidth/5);
  // temperature line
  if (temp < 44) {
    p.strokeCap("butt");
  } else {
    // don't exceed thermometer bounds.  
    temp = 44;  
    p.strokeCap("round");  
  }      
  p.line(thermometerWidth/2,thermometerWidth*1.75,thermometerWidth/2,thermometerWidth*1.1 - (thermometerWidth/50) * temp);
  // scale
  p.strokeCap("round");
  p.stroke(0);
  p.strokeWeight(thermometerWidth*0.03);
  var theight = thermometerWidth*1.5;
  var inc = thermometerWidth/5;
  
  p.textSize(Math.round(thermometerWidth*0.06));
  
  for ( i = 1; i <= 7; i++, theight -= inc ) {
    // longer bar at zero degrees C  
    if(i==3) {
        extra = thermometerWidth/10;
    } else {
        extra = thermometerWidth/20;
    }    
    p.line((thermometerWidth/2) - thermometerWidth/8,theight,(thermometerWidth/2) - thermometerWidth/8 + extra, theight );
    
    myText = -30 + i*10;
    p.fill(0, 0, 0);
    p.text(myText, (thermometerWidth/2) - thermometerWidth*0.09 + extra, theight + 4);
    
    // min/max marks
    p.strokeWeight(thermometerWidth*0.01);
    p.line((thermometerWidth/2) + thermometerWidth/8,thermometerWidth*1.1 - (thermometerWidth/50) * tmin,(thermometerWidth/2) + thermometerWidth/8 - thermometerWidth/20, thermometerWidth*1.1 - (thermometerWidth/50) * tmin );
    p.line((thermometerWidth/2) + thermometerWidth/8,thermometerWidth*1.1 - (thermometerWidth/50) * tmax,(thermometerWidth/2) + thermometerWidth/8 - thermometerWidth/20, thermometerWidth*1.1 - (thermometerWidth/50) * tmax );
    p.strokeWeight(thermometerWidth*0.03);
}
  myText = temp + ' ' + tunit;
  p.fill(0, 0, 0);
  p.textSize(Math.round(thermometerWidth*0.09));
  myWidth = p.textWidth(myText);
  p.text(myText, thermometerWidth/2 - myWidth/2, thermometerWidth*1.75 + thermometerWidth*0.045);
 };
 p.noLoop();
}


var lightSketchProc = function(p) {
 var fill_lux;
 p.size(lightWidth, barometerHeight);
 p.draw = function() {
  p.size(lightWidth, barometerHeight);
  lightWidth=p.width;
  p.background(0,0);

  // contour
  p.stroke(0);
  p.strokeWeight(lightWidth*0.01);
  fill_lux = lux;
  if(fill_lux > (3*255 - 10))
  	fill_lux = (3*255 - 10);
  p.fill(fill_lux/3 + 10);
  p.ellipse(lightWidth/2,lightWidth,lightWidth/2,lightWidth/2);
  
  myText = lux + ' ' + lunit;
  p.fill(0, 0, 0);
  p.textSize(Math.round(lightWidth*0.09));
  myWidth = p.textWidth(myText);
  p.text(myText, lightWidth/2 - myWidth/2, lightWidth*1.4 + lightWidth*0.045);
 };
 p.noLoop();
}

var canvas = document.getElementById("barometerCanvas");
var thermometerCanvas = document.getElementById("thermometerCanvas");
var lightCanvas = document.getElementById("lightCanvas");
var barometer = new Processing(canvas, barometerSketchProc);
var thermometer = new Processing(thermometerCanvas, thermometerSketchProc);
var light = new Processing(lightCanvas, lightSketchProc);

function set_pressure(data) {

	var myData = parseFloat(data);

	// Angles for sin() and cos() start at 3 o'clock;
	// subtract HALF_PI to make them start at the top
	pressure = ((myData - 700) / 10) * (TWO_PI / 120) - HALF_PI;
	pdata = myData;

	if (myData > pmax) pmax = myData;
	if (myData < pmin) pmin = myData;
 
	barometer.redraw();
}

function set_humidity(data) {

	var myData = parseFloat(data);

	// Angles for sin() and cos() start at 3 o'clock;
	// subtract HALF_PI to make them start at the top
	// 30% = HALF_PI
	humidity = (myData - 50) * (TWO_PI / 120) - HALF_PI;
	hdata = myData;

	if (myData > hmax) hmax = myData;
	if (myData < hmin) hmin = myData;
 
	barometer.redraw();
}

function set_temp(data) {

	var myData = parseFloat(data);

	temp = myData;

	if (myData > tmax) tmax = myData;
	if (myData < tmin) tmin = myData;

	thermometer.redraw();
}

function set_lux(data) {

	var myData = parseFloat(data);

	lux = myData;
	light.redraw();
}

function resizeHandler() {

	setWidth();
	barometer.redraw();
	thermometer.redraw();
	light.redraw();
}

window.onresize=resizeHandler;
