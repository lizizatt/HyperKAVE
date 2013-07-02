//********************************************************
// Syzygy is licensed under the BSD license v2
// see the file SZG_CREDITS for details
//********************************************************

// precompiled header include MUST appear as the first non-comment line
#include "arPrecompiled.h"
// MUST come before other szg includes. See arCallingConventions.h for details.
#define SZG_DO_NOT_EXPORT
#include <stdio.h>
#include <stdlib.h>
#include "arMasterSlaveFramework.h"
#include "arInteractableThing.h"
#include "arInteractionUtilities.h"
#include "arGlut.h"

// Unit conversions.  Tracker (and cube screen descriptions) use feet.
// Atlantis, for example, uses 1/2-millimeters, so the appropriate conversion
// factor is 12*2.54*20.
const float FEET_TO_LOCAL_UNITS = 1.;


// Near & far clipping planes.
const float nearClipDistance = .1*FEET_TO_LOCAL_UNITS;
const float farClipDistance = 10000.*FEET_TO_LOCAL_UNITS;

//contant values
const float PI = 3.14159;
double cherenkovElectronThreshold = .768; // in MeV
double electronMass = 9.11e-31; //kilos  //ID -11 (11 = positron)
double cherenkovMuonThreshold = 158.7; //MeV
double muonMass = 1.88352473e-28; //kilos  //ID 13 (antiparticle = 13)
double cherenkovPionThreshold = 209.7;  //MeV
double pionMass = 2.483e-28;  //kilos  //ID is 211 / -211
double speedOfLight = 299792458.; //in m/s
//Sizing Constants
static float RADIUS = 17 * 3.28;     //Constants for sizing IN FEED
static float HEIGHT = 40 * 3.28;
static float OUTERRADIUS = 17.61 * 3.28;  
static float OUTERHEIGHT = 41.22*  3.28;
static float innerDotRad = 0.3 * 3.28;
static float outerDotRad = 0.2 * 3.28;
static double threshold = 1.;

//master debug mode variable
bool debug = false;
//global function definitions (mostly utility)
//definition of absolute value
double abs(double in){
	if(in > 0.0){
		return in;
	}
	return in * -1.0;
}
//debug helper function
void debugText(string s){
	if(debug){
		cout << s;
		cout << "\n";
	}
}

double dotProduct(arVector3 a, arVector3 b){
	return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

arVector3 crossProduct(arVector3 a, arVector3 b){
	return arVector3 ( (a[1]*b[2]-a[2]*b[1]) , ( a[2]*b[0]-a[0]*b[2]),  (a[0]*b[1]-a[1]*b[0]));
}

arVector3 normalize(arVector3 a){
	float mag = sqrt(pow(a[0],2)+pow(a[1],2)+pow(a[2],2));
	return arVector3(a[0]/mag,a[1]/mag,a[2]/mag);
}

//display function declarations (defined in functions section)
void drawDisplay(int index, bool highlighted, char * content[10],int numLines, int startOffSetX, int startOffSetY, float scale);
void doInterface(arMasterSlaveFramework& framework);
bool updateMenuIndexState(int i);

//Class Declarations:  
class dot {  // The class containing all the relevant information about each dot
public:
	float cx, cy, cz, dx, dy, dz, charge, time, radius;
	dot(float x, float y, float z, float xd, float yd, float zd, float q, float t, float r) {
		cx = x * 3.28;
		cy = y * 3.28;
		cz = z * 3.28;
		dx = xd;
		dy = yd;
		dz = zd;
		charge = q;
		time = t;
		radius = r;
	};
	dot(){};
	void draw(bool isOD);
	arVector3 dotColor();
};

typedef struct ringPointHolder{  //just a wrapped vector of arVector3s
	vector<arVector3> ringPoints;
}ringPointHolder;

class dotVector {  //a dotVector is an individual event.
public:
	double startTime;
	double endTime;
	double length;
	vector<double> particleType;  //each particleType as a double, which is encoded as GEANT particle code (http://hepunx.rl.ac.uk/BFROOT/www/Computing/Environment/NewUser/htmlbug/node51.html)
	vector<string> particleName;  //just for ease of access, if we know what the type is (eg, muon electron pion) we'll write it in here, else we'll just enter "???"
	vector<double> coneAngle;
	vector<arVector3> coneDirection;
	double vertexPosition[3];
	vector<bool> haveRingPoints;
	vector<bool> doDisplay;
	vector<vector<ringPointHolder> > ringPoints;
	vector<float> momentum;  //momentum (in MeV ? )
	vector<float> energy; //in the case of time-compression (supernova file), each consecutive 3 entries in this will be vertex positions ... else, energy in MeV
	vector<dot> dots;  //holds the inner cylinder
	vector<dot> outerDots; //holds the outer cylinder
	dotVector(vector<dot> in, vector<dot> outer, float st) {
		dots = in;
		outerDots = outer;
		startTime = st;
	};
	dotVector(){};
	void draw(arMasterSlaveFramework& fw);
};

 
  
GLUquadricObj * quadObj;  //quadric object for object drawing
  
// Class definitions & imlpementations. We'll have just one one class, a 2-ft colored square that
// can be grabbed & dragged around. We'll also have an effector class for doing the grabbing
//
class ColoredSquare : public arInteractableThing {
  public:
    // set the initial position of any ColoredSquare to 5 ft up & 2 ft. back
    ColoredSquare() : arInteractableThing() {}
    ~ColoredSquare() {}
    void draw( arMasterSlaveFramework* fw=0 );
};
void ColoredSquare::draw( arMasterSlaveFramework* /*fw*/ ) {
  glPushMatrix();
    glMultMatrixf( getMatrix().v );
    // set one of two colors depending on if this object has been selected for interaction
    if (getHighlight()) {
      glColor3f( 0,1,0 );
    } else {
     glColor3f( 1,1,0 );
    }
    // draw a 2-ft upright square
    glBegin( GL_QUADS );	
      glVertex3f( -1,-1,0 );
      glVertex3f( -1,1,0 );
      glVertex3f( 1,1,0 );
      glVertex3f( 1,-1,0 );
    glEnd();
  glPopMatrix();
}

class RodEffector : public arEffector {
  public:
    // set it to use matrix #1 (#0 is normally the user's head) and 3 buttons starting at 0 
    // (no axes, i.e. joystick-style controls; those are generally better-suited for navigation
    // than interaction)
    RodEffector() : arEffector( 1, 3, 0, 0, 0, 0, 0 ) {

      // set length to 5 ft.
      setTipOffset( arVector3(0,0,-5) );

      // set to interact with closest object within 1 ft. of tip
      // (see interaction/arInteractionSelector.h for alternative ways to select objects)
      setInteractionSelector( arDistanceInteractionSelector(2.) );

      // set to grab an object (that has already been selected for interaction
      // using rule specified on previous line) when button 0 or button 2
      // is pressed. Button 0 will allow user to drag the object with orientation
      // change, button 2 will allow dragging but square will maintain orientation.
      // (see interaction/arDragBehavior.h for alternative behaviors)
      // The arGrabCondition specifies that a grab will occur whenever the value
      // of the specified button event # is > 0.5.
      setDrag( arGrabCondition( AR_EVENT_BUTTON, 0, 0.5 ), arWandRelativeDrag() );
      setDrag( arGrabCondition( AR_EVENT_BUTTON, 2, 0.5 ), arWandTranslationDrag() );
    }
    // Draw it. Maybe share some data from master to slaves.
    // The matrix can be gotten from updateState() on the slaves' postExchange().
    void draw() const;
	void draw(arMasterSlaveFramework & fw) const;
  private:
};
void RodEffector::draw() const {
  glPushMatrix();
    glMultMatrixf( getCenterMatrix().v );
    // draw grey rectangular solid 2"x2"x5'
    glScalef( 2./12, 2./12., 5. );
    glColor3f( .5,.5,.5 );
    glutSolidCube(1.);
    // superimpose slightly larger black wireframe (makes it easier to see shape)
    glColor3f(0,0,0);
    glutWireCube(1.03);
  glPopMatrix();
}

class Detector : public arInteractableThing {
public:
	arVector3 center; //for rendering
	arVector3 axis; //for rendering
	
	//these used to make vertex array at initialization
	float length;
	float vertical_radius;
	float horizontal_radius;
	float height;
	float cap_width; 
	
	float num_subdivisions_ellipse;
	float num_subdivisions_cap;
	float num_slices;
	
	GLfloat verticies[];
	
	Detector() : arInteractableThing() {}
    ~Detector() {}
	
	void initialize(); //populates vertex arrays
    void draw( arMasterSlaveFramework* fw=0 );
};

void Detector::initialize(){  //will be drawn going down z axis, with up being y
	//tunable parameters
	num_subdivisions_ellipse = 50;
	num_subdivisions_cap = 50;
	num_slices = 50;
	length = 49.490 * 3.28;
	vertical_radius = 25.000 * 3.28;
	horizontal_radius = 42.190/2 * 3.28;
	height = 42.190 * 3.28;
	cap_width = 23.5 * 3.28;

	arVector3 previousPoint;
	arVector3 currentPoint;
	float z_stepSize = length / num_slices;
	//first generate endcaps, as if we were doing it in super-k.	
}
void Detector::draw(arMasterSlaveFramework* fw){
	glPushMatrix();
	//offet to get us placed properly
	glRotatef(180, 0, 1, 0);
	glTranslatef(0,0,-length/2);
	for(int numCavernsPerRow = 0; numCavernsPerRow < 5; numCavernsPerRow++){
		glPushMatrix();
			glTranslatef(0,0,length*numCavernsPerRow);
			for(int numRows = 0; numRows < 2; numRows++){
				glPushMatrix();
					glTranslatef(length*numRows, 0,0);
					//draw from precomputed vertex array defining one chunk of detector structure
					
					glColor3f(1,1,1);
					
					for(int doItTwice = 0; doItTwice < 2; doItTwice++){
						float min_rad = sqrt( (cap_width / 2) * (cap_width / 2) + (height / 2) * (height / 2) );
						float offset_angle = atan( (cap_width / 2) / (height / 2) );
						float min_angle = -PI/2 + offset_angle + doItTwice * PI;
						float max_angle = PI/2 -  offset_angle + doItTwice * PI;
						for(int i = 0; i < num_subdivisions_ellipse; i++){
							float angle = min_angle + (max_angle-min_angle) * (i / num_subdivisions_ellipse);
							float prevx = min_rad * cos(angle);
							float prevy = min_rad * sin(angle);
							float prevz = 0;
							for(float j = length/num_slices; j < length; j+= length/num_slices){
								float z = j;
								float y = min_rad * sin(angle);
								float ellipse_factor = .01;  //this is a function of our horizontal radius versus our vertical radius
								float k = 1 - ellipse_factor*sin((y+height/2) / height * PI); //this is the ellipse parameter controlling scaling of the radius.  factor of sin(angle)
								float x = k * min_rad * cos(angle);
								glBegin(GL_LINES);
									glVertex3f(prevx,prevy,prevz);
									glVertex3f(x,y,z);
								glEnd();
								prevx = x;
								prevy = y;
								prevz = z;
							}
						}
						for(int i = 0; i < num_subdivisions_ellipse; i++){
							
						}
					}
					
					
					//wireframe top and bottom of the detector
					for(int i = 0; i < 2; i++){
						for(float j = 0; j < length; j+= length/num_slices){
							glBegin(GL_LINES);
							glVertex3f(-cap_width/2, i*height - height/2, j);
							glVertex3f(cap_width/2, i*height - height/2, j);
							glEnd();    
						}
					}
					for(int i = 0; i < 2; i++){
						for(float j = -cap_width/2; j <= cap_width/2; j+= cap_width / num_subdivisions_cap){
							glBegin(GL_LINES);
							glVertex3f(j, i*height - height/2, 0);
							glVertex3f(j, i*height-height/2, length);
							glEnd();   
						} 		
					}
					
					//wireframe caps are drawn per frame since it's a lot cleaner this way:
					
					/*
					for(int i = 0; i < 2; i++){
						for(double j = PI/num_subdivisions_cap; j < PI; j+=(2*PI/num_subdivisions_cap)){
							glBegin(GL_LINES);
							glVertex3f(cap_width*cos(j),cap_width*sin(j),i*height-0.1);
							glVertex3f(cap_width*cos(j),-cap_width*sin(j),i*height-0.1);
							glEnd();    
						}
					}
					for(int i = 0; i < 2; i++){
						for(double j = PI/num_subdivisions_cap; j < 2*PI; j+=(2*PI/num_subdivisions_cap)){
							glBegin(GL_LINES);
							glVertex3f(cap_width*cos(j),cap_width*sin(j),i*height-0.1);
							glVertex3f(-cap_width*cos(j),cap_width*sin(j),i*height-0.1);
							glEnd();   
						} 		
					}*/
					
				glPopMatrix();
			}	
		glPopMatrix();
	}
	glPopMatrix();
}

// End of classes

ColoredSquare theSquare;
RodEffector theEffector;
Detector myDetector;

//GLOBAL VARIABLES (what a mess)
//menu variables
bool doHUD = false;  //outdated
bool doMenu = true;  //whether or not the menu is currently displayed
bool doMainMenu = true; 
bool doOptionsMenu = false;
bool doLoadMenu = false;
bool doCherenkovConeMenu = false;
bool triggerDepressed = false;
int cherenkovConeMenuIndex = 0;  //there will be (num charenkov cones) / 3 submenus if the number of cherenkov cones is greated than 4
int menuIndex = 0;  //current index in the menu, defaults to 0 .. can be -2,-1,0,1,2 for 5 windows
bool doColorKey = false;  //window next to the primary tablet with information for charge or time display ... not currently implemented
int modifiedCherenkovConeIndex = -1;  //if we modify a cone index, instead of sharing the entire doDisplay vector, we'll change this to a value 0 - doDisplay.size()-1.  If it's -1, no change

bool doCherenkovCone = true;   //toggle for cherenkov cones lines connecting particle to projection on wall.
bool doScaleByCharge = true;   //scales the radii of circles by their respective charges.  Hard coded scale factor at the moment.
double currentTime = 0.0;
double timeScaleFactor = .5;  //scale factor for playback.  At 1, length of the supernova is 7 seconds.  There's a bug or me being stupid somewhere that's requiring this to not be 1.0.
double timeHolder1 = 0.0;
bool doCylinderDivider = true;  //do outer detector true/false
int autoPlay = 0;   //autoplay back = -1, autoplay forward = 1;
float lastJoyStickMove = 0;  //last time the joystick was moved, used for double-press activation, not currently working
bool joyStickMoveDir = true;  //last joystick move direction, true = right, false = left, not implemented
float ax, az;
int index;  //current location in the event list
int indexTransfer;  
vector<dotVector> dotVectors;     //Vector to hold all generated dot vectors in loaded order
dotVector currentDots;               //Class to hold unknown number of dots (just wraps the dotVector)
arVector3 currentPosition;
float viewer_distance=100.0;
float viewer_angle=0.0;
bool l_button=false;
bool r_button=false;
arVector3 l_position;
dot tempDot;                    //dot held between loading new events. This is a one-dot buffer
bool stored = false;            //Telling us whether a dot is stored in tempDot
bool colorByCharge = false;      //whether to color by charge or time
ifstream dataFile;              //data input
char* filename;
bool doTimeCompressed = false;
arVector3 deltaPosition, originalPosition;
arVector3 deltaDirection, originalDirection;
bool isTouchingVertex = false;
bool isGrabbingVertex = false;
int itemTouching = 0;  //0 corresponds to vertex, else subtract 1 and is index of cone
arVector3 vertexOffset;
//string bufferLine;  



//function definitions
//returns color of dots based on whether charge or time are the metrics for coloring, based on these color arrays
//PRE_PICKED COLOR ARRAYS
int red_values [] =		{132	,74,	22,		4,		4,		6,		40,		115,	193,	249,	253,	249,	219,	219,	219,	219};  //r
int green_values [] =	{4		,12,	4,		124,	175,	184,	183,	114,	193,	249,	213,	191,	143,	129,	102,	33};    //g
int blue_values [] =	{186	,178,	186,	186,	186,	86,		7,		0,		6,		21,		80,		1,		12,		12,		12,		12};   //b
arVector3 dot::dotColor() {
	float red, green, blue;
	float numDivs = 16;
	int section = 0;
	if(colorByCharge) {
		float value = charge;
		if(value > 26.7) section = 15;
		else if (value > 23.3) section = 14;
		else if (value > 20.2) section = 13;
		else if (value > 17.3) section = 12;
		else if (value > 14.7) section = 11;
		else if (value > 12.2) section = 10;
		else if (value > 10) section = 9;
		else if (value > 8.0) section = 8;
		else if (value > 6.2) section = 7;
		else if (value > 4.7) section = 6;
		else if (value > 3.3) section = 5;
		else if (value > 2.2) section = 4;
		else if (value > 1.3) section = 3;
		else if (value > 0.7) section = 2;
		else if (value > 0.2) section = 1;

		red = red_values[section] / 255.0;
		blue = blue_values[section] / 255.0;
		green = green_values[section] / 255.0;
	} else {
		float value = time;
		/*
		if(value > 1125) section = 16;
		else if (value > 1110) section = 14;
		else if (value > 1095) section = 13;
		else if (value > 1080) section = 12;
		else if (value > 1065) section = 11;
		else if (value > 1050) section = 10;
		else if (value > 1035) section = 9;
		else if (value > 1020) section = 8;
		else if (value > 1005) section = 7;
		else if (value > 990) section = 6;
		else if (value > 975) section = 5;
		else if (value > 960) section = 4;
		else if (value > 945) section = 3;
		else if (value > 930) section = 2;
		else if (value > 915) section = 1;
		*/
		/*
		if(value < 1001) section = 16;
		else if (value < 1012) section = 14;
		else if (value < 1023) section = 13;
		else if (value < 1034) section = 12;
		else if (value < 1045) section = 11;
		else if (value < 1056) section = 10;
		else if (value < 1067) section = 9;
		else if (value < 1078) section = 8;
		else if (value < 1089) section = 7;
		else if (value < 1100) section = 6;
		else if (value < 1111) section = 5;
		else if (value < 1122) section = 4;
		else if (value < 1133) section = 3;
		else if (value < 1144) section = 2;
		else if (value < 1155) section = 1;
		*/
		if(value < 893) section = 15;
		else if (value < 909) section = 14;
		else if (value < 925) section = 13;
		else if (value < 941) section = 12;
		else if (value < 957) section = 11;
		else if (value < 973) section = 10;
		else if (value < 989) section = 9;
		else if (value < 1005) section = 8;
		else if (value < 1021) section = 7;
		else if (value < 1037) section = 6;
		else if (value < 1053) section = 5;
		else if (value < 1069) section = 4;
		else if (value < 1085) section = 3;
		else if (value < 1101) section = 2;
		else if (value < 1117) section = 1;

		//now we will access an array of pre-picked values corresponding with the superscan mode, based on the SECTION
		red = red_values[section] / 255.0;
		blue = blue_values[section] / 255.0;
		green = green_values[section] / 255.0;
	}
	return arVector3(red,green,blue);
}
void dot::draw(bool isOD){
	//cout << cx / 30.48 << " " << cy / 30.48 << " " << cz / 30.48 << "\n";
	glPushMatrix();
		float radius = innerDotRad;
		float radiusScaleFactor = 1.;
		if(doScaleByCharge && abs(charge) < 26.7){
			float scaleMin = .5;
			if(doTimeCompressed){
				scaleMin = .25;
			}
			radiusScaleFactor = scaleMin/26.7 * charge + scaleMin;
			radius = radius * radiusScaleFactor;
		}
		//translate to right position and rotation
		glTranslatef(cx / 100, cy / 100, -cz / 100);
		//current direction is -z, want to rotate to vector <dx, dy, dz>
		arVector3 currentDir(0,0,-1);
		arVector3 targetDir = normalize(arVector3(dx,dy,dz));
		arVector3 axisRotation = crossProduct(currentDir, targetDir);
		float angle = acos(dotProduct(axisRotation, currentDir))*180/PI;
		
		glRotatef(angle, axisRotation[0], axisRotation[1], axisRotation[2]);
		
		arVector3 myColor = dotColor();
		glColor3f(myColor[0], myColor[1], myColor[2]);
		gluDisk(quadObj,0,radius,20,1);
	glPopMatrix();
}

void dotVector::draw(arMasterSlaveFramework& fw){
	debugText("Began Draw Dots");
	for(std::vector<dot>::size_type i = 0; i != currentDots.dots.size(); i++) {
		currentDots.dots[i].draw(false);
	}
	for(std::vector<dot>::size_type i = 0; i != currentDots.outerDots.size();i++){
		currentDots.outerDots[i].draw(true);
	}
	debugText("Ended draw dots");
}

//helper function, returns true if i == menu index
bool updateMenuIndexState(int i){
	if(i == menuIndex){
		return true;
	}
	return false;
}

void RodEffector::draw(arMasterSlaveFramework& framework) const {
	debugText("began drawing rod effector");
	glPushMatrix();
	glMultMatrixf( getCenterMatrix().v );  //transforms to hand position
	

	//we're going to draw the text on the wand, bound to hand, like a tablet
	//draw the tablet surface, which will be a scaled cube
	glColor3f(.5,.5,.5);
	glScalef(2./3,6./120,1.5);
	glTranslatef(0,1.5,1);
	glutSolidCube(1.);
	glColor3f(0,0,0);
	glutWireCube(1.01); //black wireframe, makes it easier to see
	glTranslatef(0,.1,0);
	glutSolidCube(.9);  //screen (black surface)
	
	//making a pincer-style addition at the top to point to the center of our "grabber"
	glPushMatrix();
		glTranslatef(.33,-.1,-.55);
		glColor3f(.5,.5,.5);
		glScalef(1,3,1);
		glutSolidCube(.25);
		glColor3f(0,0,0);
		glutWireCube(.26);
	glPopMatrix();
	glPushMatrix();
		glTranslatef(-.33,-.1,-.55);
		glColor3f(.5,.5,.5);
		glScalef(1,3,1);
		glutSolidCube(.25);
		glColor3f(0,0,0);
		glutWireCube(.26);
	glPopMatrix();
	


	//HERE WE DO THE TABLET DISPLAY
	//WHICH is currently hardcoded, sorry about that
	glScalef(.007,.007,.007); 
	glRotatef(90,1,0,0);  
	glColor3f(1.,1,1);  
	glTranslatef(0,0,-90.0);
	glRotatef(180,0,0,1);
	glRotatef(180,0,1,0);
	glTranslatef(-50,-50,0);
	//at this point we're centered on bottom left of display, and oriented correctly
	glScalef(.1,.1,.1);  //this will determine text size
	glLineWidth(1.9);  //and width

	//at the current settings, the top right is (1000,1000,0)
	glPushMatrix();
	glTranslatef(-50,950,0);
	char * text = "Event Data";
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(0,925,0);
	text = "__________";
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(-50,700,0);
	text = "Color By: ";
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	if(colorByCharge){
		glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN,'Q');
	}
	else{
		glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN,'T');
	}
	glPopMatrix();
	glPushMatrix();
	glTranslatef(-50,550,0);
	text = "Scale by Q?: ";
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	if(doScaleByCharge){
		text = "ON";
	}
	else{
		text = "OFF";
	}
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(-50,400,0);
	text = "OD?: ";
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	if(doCylinderDivider){
		text = "ON";
	}
	else{
		text = "OFF";
	}
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(-50,250,0);
	text = "Cone visible?: ";
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	if(doCherenkovCone){
		text = "ON";
	}
	else{
		text = "OFF";
	}
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	glPopMatrix();
	/*
	glPushMatrix();
	glTranslatef(0,300,0);
	text = "Particle: ";
	for (char * p = text; *p; p++)
	glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	//glPopMatrix();
	//glPushMatrix();
	//	glTranslatef(0,200,0);
	text = "???";
	if(currentDots.particleType[0] == 11){
	text = "Electron";
	}
	if(currentDots.particleType[0] == -11){
	text = "Positron";
	}
	if(currentDots.particleType[0] == 13){
	text = "Muon";
	}
	if(currentDots.particleType[0] == -13){
	text = "Antimuon";
	}
	if(currentDots.particleType[0] == 211){
	text = "Pion+";
	}
	if(currentDots.particleType[0] == -211){
	text = "Pion-";
	}
	for (char * p = text; *p; p++)
	glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
	glPopMatrix();
	*/
	glPushMatrix();
	glTranslatef(-50,100,0);
	text = "Event ";
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);

	char buffer[30];
	itoa(index+1,buffer,10);
	text = buffer;
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);

	text = " / ";
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);

	itoa(dotVectors.size(),buffer,10);
	text = buffer;
	for (char * p = text; *p; p++)
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);

	glPopMatrix();
	glLineWidth(1.0);
	glPopMatrix();

	//now for individual displays, again hardcoded for the moment.  Sorry again.
	if(doMenu){
		if(doHUD){
			glPushMatrix();
			doInterface(framework);
			glPopMatrix();
		}else{
			glPushMatrix();
			glMultMatrixf( getCenterMatrix().v );
			doInterface(framework);
			glPopMatrix();
		}	
	}
	debugText("Ended drawing wand");

}

//function to draw a display.  Helper function for the GUI.  This will draw one display  with given parameters.
void drawDisplay(int index, bool highlighted, char * content[10],int numLines, int startOffSetX, int startOffSetY, float scale){
	glPushMatrix();  //do one display ... test process
	//for rotations, let's say we're 2 units away from the hand.
	glColor3f(.75,.75,.75);
	if(doHUD){
		glTranslatef(0,0,-5);
		glTranslatef(4,0,0);
		glRotatef(-atan(5/3.)*180/PI,0,1,0);
		glRotatef(atan(index/3.)*180/PI/3.,1,0,0);
		glTranslatef(0,index,0);
	}
	else{
		glTranslatef(index,0,0);
		glRotatef(-atan(index/3.)*180/PI,0,1,0);
		glRotatef(-20,1,0,0);
	}
	glScalef(1.0,1.0,.1);
	glutSolidCube(1.0);
	if(highlighted){
		glPushMatrix();
		glColor4f(0,1,0,.5);
		if(triggerDepressed){
			glScalef(1.2,1.2,.5);
		}else{
			glScalef(1.1,1.1,.5);
		}
		glutSolidCube(1);
		glColor3f(0,0,0);
		glutWireCube(1.001);
		glPopMatrix();
	}
	glColor3f(0,0,0);
	glLineWidth(1);
	glutWireCube(1.001);
	glScalef(.9,.9,1.2);
	glutSolidCube(1.0);

	//now we have the distance-tablet deal, let's try to write some text
	glScalef(.001,.001,.001);  //same scaling as we had to do before ish
	glTranslatef(-300,0,600);
	glColor3f(1.,1.,1.);
	glLineWidth(2);
	glTranslatef(startOffSetX,startOffSetY,0);
	for(int i = 0; i < numLines;i++){
		glPushMatrix();
		glScalef(scale,scale,scale);
		glTranslatef(0,-150 * i,0);
		char * text = content[i];
		for (char * p = text; *p; p++)
			glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
		glPopMatrix();
	}
	glPopMatrix();
}

void doInterface(arMasterSlaveFramework& framework){
	// framework.setEyeSpacing( 0.0f );
	/*
	if(doHUD){ 
		//if we're doing the HUD (which is now disabled and not accessible from in program), we'll do the matrix pushing here.
		glMultMatrixf(ar_getNavMatrix().v);
		glMultMatrixf(framework.getMatrix(0).v);
	}
	*/
	
	bool state = false;  //this is used to determine where the green box goes
	char * content[10];  //no more than 10 items per line.  Gets illegible if we try to get too much smaller
	int indexVal;
	if(doOptionsMenu){  //hardcoded menus.
		state = updateMenuIndexState(-2);  //each of these updateMenuIndexState lines is checking if this is currently the selected item.  If it is, this becomes true, and passes in to the drawDisplay function which adds a green box to it.  Honestly there's no reason I couldn't have just done this from within drawDisplay entirely, but it isn't computationally expensive anyhow.  TODO, make that smarter.
		content[0] = "Main";
		content[1] = "Menu";
		drawDisplay(-2,state, content ,2,0,50, 2);

		state = updateMenuIndexState(-1);
		content[0] = "Toggle";
		content[1] = "Outer";
		content[2] = "Detector";
		drawDisplay(-1,state, content ,3,-35,150, 1.5);

		state = updateMenuIndexState(0);
		content[0] = " Toggle";
		content[1] = "Cherenkov";
		content[2] = "Radiation";
		drawDisplay(0,state, content ,3,-140,150, 1.5);

		state = updateMenuIndexState(1);
		content[0] = "  Q/T";
		content[1] = " Toggle";
		drawDisplay(1,state, content ,2,-300,100, 2);

		state = updateMenuIndexState(2);
		content[0] = "Scale";
		content[1] = "Radius";
		content[2] = "By Q";
		drawDisplay(2,state, content ,3,-75,200, 1.8);
	}
	if(doCherenkovConeMenu){
		state = updateMenuIndexState(-2);
		if(cherenkovConeMenuIndex == 0){
			content[0] = "Main";
			content[1] = "Menu";
			drawDisplay(-2,state, content ,2,0,50, 2);
		}
		else{
			content[0] = "Back";
			drawDisplay(-2,state,content,1,0,-50,2);
		}

		int numHold = 3;  //this is 3.  If we don't have anything to write on a display, set it to 0.  We know all the other ones after that will also be 0
		//special case, if there are 4 (or less) particles, we don't need a "more" button
		if(currentDots.particleType.size() <= 4){
			for(int l = -1; l<=2;l++){
				state = updateMenuIndexState(l);
				indexVal = cherenkovConeMenuIndex*3+(l+1);
				if(indexVal >= currentDots.particleType.size()){
					numHold = 0;
				}
				content[0] = (char*)currentDots.particleName[indexVal].c_str();
				char dest[50];
				sprintf(dest, "%i MeV", (int)currentDots.energy[indexVal]);
				content[1] = dest;
				if(currentDots.doDisplay[indexVal]){
					content[2] = "On";
				}else{
					content[2] = "Off";
				}
				drawDisplay(l,state, content ,numHold,-100,100, 1.2);
			}
		}
		else{
			for(int l = -1; l<=1;l++){
				state = updateMenuIndexState(l);
				indexVal = cherenkovConeMenuIndex*3+(l+1);
				if(indexVal >= currentDots.particleType.size()){
					numHold = 0;
				}
				content[0] = (char*)currentDots.particleName[indexVal].c_str();
				char dest[50];
				sprintf(dest, "~%i MeV", (int)currentDots.energy[indexVal]);
				content[1] = dest;
				if(currentDots.doDisplay[indexVal]){
					content[2] = "On";
				}else{
					content[2] = "Off";
				}
				drawDisplay(l,state, content ,numHold,-100,100, 1.1);
			}
			/*
			state = updateMenuIndexState(0);
			drawDisplay(0,state, content ,0,0,0, 1);

			state = updateMenuIndexState(1);
			drawDisplay(1,state, content ,0,0,0, 1);
			*/

			state = updateMenuIndexState(2);
			content[0] = "Next";
			if(numHold == 3){
				drawDisplay(2,state, content ,1,0,0, 2);
			}else{
				drawDisplay(2,state,content,0,0,0,2);
			}
		}
	}
	if(doMainMenu){  //main menu
		state = updateMenuIndexState(-2);
		content[0] = " -";
		content[1] = "Event";
		content[2] = " -";
		drawDisplay(-2,state,content,3,0,200,2);

		state = updateMenuIndexState(-1);
		content[0] = "Options";
		drawDisplay(-1,state,content,1,-125,-75,2);

		state = updateMenuIndexState(0);
		content[0] = "Exit";
		content[1] = "Menu";
		drawDisplay(0,state, content ,2,0,100,2);

		state = updateMenuIndexState(1);
		content[0] = "Cherenkov";
		content[1] = " Cones";
		drawDisplay(1,state,content,2,-130,50, 1.5);

		state = updateMenuIndexState(2);
		content[0] = " +";
		content[1] = "Event";
		content[2] = " +";
		drawDisplay(2,state,content,3,0,200, 2);
	}
	glLineWidth(1.0);
}

/*  logic to fill a dotVector.  Reads from the file and populates a dotVector until it hits a NEXTEVENT line, where it will do the physics calculation for each final particle to determine cone angle, and then stores the dot vector and exits */
void loadNextEvent(void) {
	int hit;
	arVector3 theta;
	float x,y,z,q,t,xd,yd,zd;
	string type;
	float filler;
	float time;
	float vx, vy,vz;
	vector<float> particleType, dx, dy, dz, momentum, id;

	//float particleType, dx, dy, dz, momentum, id;
	float particleType2, dx2, dy2, dz2, momentum2, id2;
	//momentum = 0;
	float momentumHold = 0;
	vector<dot> dots;
	vector<dot> outerDots;
	bool debug = false;
	if(dataFile.is_open()) {
		while (dataFile.good()) {
			dataFile >> type;
			if(type == "ID"){  //parse inner detector
				dataFile >> hit >> x >> y >> z >> xd >> yd >> zd >> q >> t;
				tempDot = dot(x,y,z, xd, yd, zd,q,t,innerDotRad);
				dots.push_back(tempDot);
			}
			if(type == "OD"){  //parse outer detector
				dataFile >> hit >> x >> y >> z >> xd >> yd >> zd >> q >> t;
				x = x / 100.0;
				y = y / 100.0;
				z = z * 20.0 / 1810.0 + 20.0;
				tempDot = dot(x,y,z,xd, yd, zd,q,t,outerDotRad);
				outerDots.push_back(tempDot);
			}
			if(type == "TIME"){ //parse time info
				dataFile >> time;
			}
			if(type == "VERTEX"){  //vertex location of particle
				dataFile >> vx >> vy >> vz;
				vx = vx / 100 * 3.28;
				vy = vy / 100 * 3.28;
				vz = vz * 20.0 / 1810.0 + 20.0;
				vz = vz * 3.28;
			}
			if(type == "PARTICLE"){  //particle information -- momentum and direction of cone
				dataFile >> particleType2 >> dx2 >> dy2 >> dz2 >> momentum2 >> id2;

				particleType.push_back(particleType2);
				dx.push_back(dx2);
				dy.push_back(dy2);
				dz.push_back(dz2);
				momentum.push_back(momentum2);
				id.push_back(id2);
			}
			if(type == "NEXTEVENT"){  //store everything, create a new event
				currentDots.dots = dots;
				currentDots.outerDots = outerDots;
				currentDots.endTime = time;
				currentDots.vertexPosition[0] = vx;
				currentDots.vertexPosition[1] = vy;
				currentDots.vertexPosition[2] = vz;

				//store type
				for(int i = 0; i < particleType.size();i++){
					currentDots.particleType.push_back(particleType[i]);

					//normalize conedirection and store
					double mag = pow(dx[i],2) + pow(dy[i],2) + pow(dz[i],2);
					mag = sqrt(mag);
					arVector3 coneDirHold = arVector3(dx[i]/mag,dy[i]/mag,dz[i]/mag);
					currentDots.coneDirection.push_back(coneDirHold);

					//calculate angle
#ifdef WINNEUTRINO
					double momentumConverted = momentum[i] * pow(10.0,6) * 1.6e-19 / 2.998e8;
#else
					double momentumConverted = momentum[i] * pow(10,6) * 1.6e-19 / 2.998e8;
#endif
					double mass = electronMass;  //assume electron to start .. ID of electron is 11 / -11
					double cherenkovThreshold = cherenkovElectronThreshold;
					if(abs(currentDots.particleType[i]) == 13){  //id 13 and -13 is muon
						mass = muonMass;
						cherenkovThreshold = cherenkovMuonThreshold;
					}
					if(abs(currentDots.particleType[i]) == 211){  //id 211 and -211 is pion
						mass = pionMass;
						cherenkovThreshold = cherenkovPionThreshold;
					}
					double velocity = sqrt(pow(momentumConverted,2) / (pow(mass,2)+pow(momentumConverted,2)/pow(speedOfLight,2)));  //calculating velocity from momentum and mass, have to take in to account lorentz factor
					double energy = sqrt(pow(momentumConverted,2)*pow(speedOfLight,2)+pow(mass,2)*pow(speedOfLight,4)) / 1.602e-13;  //calculate total energy from velocity, convert to MeV

					if(energy > cherenkovThreshold){ //checks if it's over cherenkov energy threshold  ... I don't think this is actually doing anything meaningful right now
						double beta = velocity / speedOfLight;  //in m/s  
						double n = 1.33; 
						double angle = acos(1.0 / (beta * n)) * 180. / PI;  //angle in degrees, using equation cos(theta) = 1 / (n*beta) for cherenkov energy
						currentDots.coneAngle.push_back(angle);
					}
					else{
						currentDots.coneAngle.push_back(0);
					}
					currentDots.momentum.push_back(momentum[i]);
					currentDots.energy.push_back(energy);
					//and start an empty ringPoints class to be filled later
					vector<ringPointHolder> me;
					ringPointHolder me2;
					me.push_back(me2);
					me.push_back(me2);
					currentDots.ringPoints.push_back(me);
					currentDots.haveRingPoints.push_back(false); //since we haven't generated any ring points, fill this with false
					currentDots.doDisplay.push_back(false); //turn off all displays, in the next step we'll go ahead and turn on only the highest momentum particle
					//set haveRingPoints to false, this will have them be generated on the first frame

					string text = "???";
					if(currentDots.particleType[i] == 11){
						text = "Electron";
					}
					else if(currentDots.particleType[i] == -11){
						text = "Positron";
					}
					else if(currentDots.particleType[i] == 13){
						text = "Muon";
					}
					else if(currentDots.particleType[i] == -13){
						text = "Antimuon";
					}
					else if(currentDots.particleType[i] == 211){
						text = "Pion+";
					}
					else if(currentDots.particleType[i] == -211){
						text = "Pion-";
					}
					else{
						char me[100];
						printf(me,"%i",(int)currentDots.particleType[i]);
						text = me;
					}
					currentDots.particleName.push_back(text);

				}

				//we'll go ahead and load in the START TIME which is the END TIME of the previous event
				if(dotVectors.size() == 0){ //if we're the first event, start time is 0, length is thus 'time'
					currentDots.startTime = 0.0;
					currentDots.length = time;
				}else{
					int tempIndex = dotVectors.size()-1;  //this is the most recently added event (before the currently generated one)
					currentDots.startTime = dotVectors[tempIndex].endTime; 
					currentDots.length = time - currentDots.startTime;
				}
				return;
			}
			if(dataFile.fail()) break;

		}
		if(dataFile.fail() || !dataFile.good()) dataFile.close();
	} else {
		printf("file was not open! \n");
		throw 9001; //really?
	}
}

//reads in file, looping over loadNextEvent until an error is thrown (eg, the file has no more data)
void readInFile(arMasterSlaveFramework& fw){
	debugText("started read in file");
	char me[100];
	strcpy(me, filename);
	//strcpy(me,"data/");
	//strcat(me,filename);
	//dataFile.open("C:/users/owner/desktop/glut_cylinder/temp");
	dataFile.open(me);  //relative pathing, may be really temperamental ... run from SRC
	if(!dataFile.is_open()) {
		cout << "Unable to open file";
		exit(0);
	}
	index = 0;
	while(true){
		try{
			loadNextEvent();
			dotVectors.push_back(currentDots);
			currentDots = dotVector();
		}
		catch (int e){
			break;
		}
	}

	//file is read by now.  Now we're going to go ahead and compress events if we're doing time compression

	float timeStep = .05;
	index = 0;
	if(doTimeCompressed){  //here we have to compress all events in to a smaller number of events
		vector<dotVector> newDotVectors;
		newDotVectors.push_back(dotVectors[0]);
		currentDots = dotVector();
		currentDots.startTime = 0;
		for(int i = 1; i < dotVectors.size(); i++){
			if(dotVectors[i].startTime >= timeStep * index){
				currentDots.endTime = timeStep*index;
				currentDots.length = currentDots.endTime - currentDots.startTime;
				newDotVectors.push_back(currentDots);
				currentDots = dotVector();
				currentDots.startTime = timeStep*index;
				index++;
				continue;
			}
			for(int k = 0; k < dotVectors[i].dots.size(); k++){
				bool found = false;
				//search all existing dots in this event, see if any have the same vertex position, if so, just add this one's charge to that one
				for(int l = 0; l < currentDots.dots.size(); l++){
					if(dotVectors[i].dots[k].cx == currentDots.dots[l].cx && dotVectors[i].dots[k].cy == currentDots.dots[l].cy && dotVectors[i].dots[k].cz == currentDots.dots[l].cz){
						found = true;
						currentDots.dots[l].charge += dotVectors[i].dots[k].charge;
						break;
					}
				}
				if(!found){
					currentDots.dots.push_back(dotVectors[i].dots[k]);
				}
			}
			for(int k = 0; k < dotVectors[i].outerDots.size();k++){
				bool found = false;
				//same deal with outer dots.
				for(int l = 0; l < currentDots.outerDots.size(); l++){
					if(dotVectors[i].outerDots[k].cx == currentDots.outerDots[l].cx && dotVectors[i].outerDots[k].cy == currentDots.outerDots[l].cy && dotVectors[i].outerDots[k].cz == currentDots.outerDots[l].cz){
						found = true;
						currentDots.outerDots[l].charge += dotVectors[i].outerDots[k].charge;
						break;
					}
				}
				if(!found){
					currentDots.outerDots.push_back(dotVectors[i].outerDots[k]);
				}
			}
			currentDots.energy.push_back(dotVectors[i].vertexPosition[0]);
			currentDots.energy.push_back(dotVectors[i].vertexPosition[1]);
			currentDots.energy.push_back(dotVectors[i].vertexPosition[2]);
		}
		currentDots.endTime = dotVectors[dotVectors.size()-1].endTime;
		currentDots.length = currentDots.endTime - currentDots.startTime;
		newDotVectors.push_back(currentDots);
		dotVectors = newDotVectors;
	}

	index = 0;


	//now, we turn on display for first listed final state particle of each event
	//we do it here and not in Loadevent because I was getting a weird bug trying to do it there and got lazy
	for(int e = 0; e < dotVectors.size(); e++){
		int holder = 0;
		/*
		for(int i = 0; i < dotVectors[e].doDisplay.size(); i++){
		if(dotVectors[e].energy[i] > dotVectors[e].energy[holder]){
		holder = i;
		}
		}
		*/    if(dotVectors[e].doDisplay.size() > 0){
			dotVectors[e].doDisplay[0] = true;
		}

		//also go ahead and draw the scene to pre-load cones
		currentDots = dotVectors[e];
		//drawScene(fw);
	}

	currentDots = dotVectors[index];
	debugText("ended read in file");
}


// Master-slave transfer variables
// All we need to explicity transfer in this program is the square's placement matrix and
// whether or not the it is highlighted (the effector's matrix can be updated by calling 
// updateState(), see below).
int squareHighlightedTransfer = 0;
arMatrix4 squareMatrixTransfer;

// start callback (called in arMasterSlaveFramework::start()
//
// Note: DO NOT do OpenGL initialization here; this is now called
// __before__ window creation. Do it in the WindowStartGL callback.
//
bool start( arMasterSlaveFramework& framework, arSZGClient& /*cli*/ ) {
  // Register shared memory.
  //  framework.addTransferField( char* name, void* address, arDataType type, int numElements ); e.g.
  framework.addTransferField("squareHighlighted", &squareHighlightedTransfer, AR_INT, 1);
  framework.addTransferField("squareMatrix", squareMatrixTransfer.v, AR_FLOAT, 16);
	framework.addTransferField("indexTransferField",&index,AR_INT,1);
	framework.addTransferField("autoPlayTransfer",&autoPlay,AR_INT,1);
	framework.addTransferField("colorByChargeTransfer",&colorByCharge,AR_INT,1);
	framework.addTransferField("doCylinderDividerTransfer",&doCylinderDivider,AR_INT,1);
	framework.addTransferField("doScaleByChargeTransfer",&doScaleByCharge,AR_INT,1);
	framework.addTransferField("doMenuTransfer",&doMenu,AR_INT,1);
	framework.addTransferField("menuIndexTransfer",&menuIndex,AR_INT,1);
	framework.addTransferField("optionsTransfer",&doOptionsMenu,AR_INT,1);
	framework.addTransferField("menuIndexX",&menuIndex,AR_INT,1);
	framework.addTransferField("colorKeyX",&doColorKey,AR_INT,1);
	framework.addTransferField("doCherenkovMenuTransfer",&doCherenkovConeMenu,AR_INT,1);
	framework.addTransferField("doMainMenuTransfer",&doMainMenu,AR_INT,1);
	framework.addTransferField("CherenkovConeMenuIndexIndexTransfer",&cherenkovConeMenuIndex,AR_INT,1);
	framework.addTransferField("modifiedCherenkovConeItem",&modifiedCherenkovConeIndex,AR_INT,1);	
	framework.addTransferField("isTouchingVertex",&isTouchingVertex,AR_INT,1);
	framework.addTransferField("isGrabbingVertex",&isGrabbingVertex,AR_INT,1);
	framework.addTransferField("itemTouching",&itemTouching ,AR_INT,1);
	framework.addTransferField("starttransfer", &triggerDepressed, AR_INT, 1);

  // Setup navigation, so we can drive around with the joystick
  //
  // Tilting the joystick by more than 20% along axis 1 (the vertical on ours) will cause
  // translation along Z (forwards/backwards). This is actually the default behavior, so this
  // line isn't necessary.
  framework.setNavTransCondition( 'z', AR_EVENT_AXIS, 1, 0.2 );      

  // Tilting joystick left or right will rotate left/right around vertical axis (default is left/right
  // translation)
  framework.setNavRotCondition( 'y', AR_EVENT_AXIS, 0, 0.2 );      

  // Set translation & rotation speeds to 5 ft/sec & 30 deg/sec (defaults)
  framework.setNavTransSpeed( 50. );
  framework.setNavRotSpeed( 30. );
  
  // set square's initial position
  theSquare.setMatrix( ar_translationMatrix(0,5,-6) );
 

  return true;
}


// Callback to initialize each window (because now a Syzygy app can
// have more than one).
void windowStartGL( arMasterSlaveFramework& fw, arGUIWindowInfo* ) {
  // OpenGL initialization
  glClearColor(0,0,0,0);
  
  //for drawing quadrics
  quadObj = gluNewQuadric(); 
  
  readInFile(fw);  //opens the file
  
  myDetector.initialize();
}

// Callback called before data is transferred from master to slaves. Only called
// on the master. This is where anything having to do with
// processing user input or random variables should happen.
void preExchange( arMasterSlaveFramework& fw ) {
  // Do stuff on master before data is transmitted to slaves.

  // handle joystick-based navigation (drive around). The resulting
  // navigation matrix is automagically transferred to the slaves.
  fw.navUpdate();

  // update the input state (placement matrix & button states) of our effector.
  theEffector.updateState( fw.getInputState() );

  // Handle any interaction with the square (see interaction/arInteractionUtilities.h).
  // Any grabbing/dragging happens in here.
  ar_pollingInteraction( theEffector, (arInteractable*)&theSquare );

  // Pack data destined for slaves into appropriate variables
  // (bools transfer as ints).
  squareHighlightedTransfer = (int)theSquare.getHighlight();
  squareMatrixTransfer = theSquare.getMatrix();

	//do button presses
		if(fw.getOnButton(0)){  // on yellow button, step event back one if menus aren't up, step menu index back one if menus are up...replace stepping with autoplay if we're doing time compression
			if(doMenu){
				if(!(menuIndex < -1)){
					menuIndex--;
				} else {
					menuIndex = 2;
				}
			}
			else{
				doMenu = true;
				/*
				if(!doTimeCompressed){
					if(index > 0){
						index--;
					}
					autoPlay = 0;
				} else {
					if(autoPlay == 0){
						autoPlay = -1;
					}
					else if(autoPlay == 1){
						autoPlay = 0;
					}
				}
				*/
			}
		}
		if(fw.getOnButton(1)){  //on red button, do nothing
		}
		if(fw.getOnButton(2)){  // on green button, step event forward one, or if menus are up, step menuIndex forwar done  .. or, if time compressed, autoplay forward
			if(doMenu){
				if(!(menuIndex > 1)){
					menuIndex++;
				} else {
					menuIndex = -2;
				}
			}
			else{
				doMenu = true;
				/*
				if(!doTimeCompressed){
					if(index < dotVectors.size() - 1){
						index++;
					}
					autoPlay = 0;
				}else{
					if(autoPlay == 0){
						autoPlay = 1;
					}
					else if(autoPlay == -1){
						autoPlay = 0;
					}
				}
				*/
			}
			
		}
		if(fw.getOnButton(3)){ // on blue button, do nothing
		}
		if(fw.getOnButton(4)){  //on joystick button press (todo:  joystick compressed + left/right will autoplay)
		}
		if(!fw.getButton(5)){
			isGrabbingVertex = false;
		}
		if(fw.getButton(5) && isTouchingVertex){
			isGrabbingVertex = true;
			doMenu = false;
		}
		else
		if(fw.getOnButton(5)){  //this is the master control button.  It turns on the menu, and selects menu items
			/*if(!doMenu && !isTouchingVertex){
				doMenu = true;
			}else */
			if(doMenu && !isTouchingVertex){  //the menu is on, here write code to toggle menu items
				if(doMainMenu){ //main menu
					if(menuIndex == -2){
						if(index > 0){
							index--;
						}
						autoPlay = 0;
					}
					if(menuIndex == -1){
						doOptionsMenu = true;
						doMainMenu = false;
					}
					if(menuIndex == 0){
						doMenu = false;
					}
					if(menuIndex == 1){
						doCherenkovConeMenu = true;
						doMainMenu = false;
					}
					if(menuIndex == 2){
						if(index < dotVectors.size() - 1){
							index++;
						}
						autoPlay = 0;
					}
				}
				else if(doOptionsMenu){
					if(menuIndex == -2){
						doOptionsMenu = false;
						doMainMenu = true;
					}
					if(menuIndex == -1){
						doCylinderDivider = !doCylinderDivider;
					}
					if(menuIndex == 0){
						doCherenkovCone = !doCherenkovCone;
					}
					if(menuIndex == 1){
						colorByCharge = !colorByCharge;
					}
					if(menuIndex == 2){
						doScaleByCharge = !doScaleByCharge;
					}
				}
				else if(doCherenkovConeMenu){  //
					if(menuIndex == -2){
						if(cherenkovConeMenuIndex == 0){
							doCherenkovConeMenu = false;
							doMainMenu = true;
						}
						else{
							cherenkovConeMenuIndex--;
						}
					}
					if(menuIndex == -1){
						if((cherenkovConeMenuIndex * 3 + 0) < currentDots.particleType.size()){
							dotVectors[index].doDisplay[cherenkovConeMenuIndex*3 + 0] = !currentDots.doDisplay[cherenkovConeMenuIndex*3 + 0];
							modifiedCherenkovConeIndex = cherenkovConeMenuIndex*3 + 0;
						}
					}
					if(menuIndex == 0){
						if((cherenkovConeMenuIndex * 3 + 1) < currentDots.particleType.size()){
							dotVectors[index].doDisplay[cherenkovConeMenuIndex*3 + 1] = !currentDots.doDisplay[cherenkovConeMenuIndex*3 + 1];
							modifiedCherenkovConeIndex = cherenkovConeMenuIndex*3 + 1;
						}
					}
					if(menuIndex == 1){
						if((cherenkovConeMenuIndex * 3 + 2) < currentDots.particleType.size()){
							dotVectors[index].doDisplay[cherenkovConeMenuIndex*3 + 2] = !currentDots.doDisplay[cherenkovConeMenuIndex*3 + 2];
							modifiedCherenkovConeIndex = cherenkovConeMenuIndex*3 + 2;
						}
					}
					if(menuIndex == 2){
						if(cherenkovConeMenuIndex <= (currentDots.particleType.size()-1) / 3){
							cherenkovConeMenuIndex++;
						}
					}
				}

			}
		}
}

// Callback called after transfer of data from master to slaves. Mostly used to
// synchronize slaves with master based on transferred data.
void postExchange( arMasterSlaveFramework& fw ) {
  // Do stuff after slaves got data and are again in sync with the master.
  if (!fw.getMaster()) {
    
    // Update effector's input state. On the slaves we only need the matrix
    // to be updated, for rendering purposes.
    theEffector.updateState( fw.getInputState() );

    // Unpack our transfer variables.
    theSquare.setHighlight( (bool)squareHighlightedTransfer );
    theSquare.setMatrix( squareMatrixTransfer.v );
  }
  
  currentDots = dotVectors[index];
}

void display( arMasterSlaveFramework& fw ) {
  // Load the navigation matrix.
  fw.loadNavMatrix();
  
  //tell dots to draw themselves
  currentDots.draw(fw);
  
  // Draw stuff.
  theEffector.draw(fw);
  myDetector.draw();
}

// Catch key events.
void keypress( arMasterSlaveFramework& fw, arGUIKeyInfo* keyInfo ) {
  cout << "Key ascii value = " << keyInfo->getKey() << endl;
  cout << "Key ctrl  value = " << keyInfo->getCtrl() << endl;
  cout << "Key alt   value = " << keyInfo->getAlt() << endl;
  string stateString;
  arGUIState state = keyInfo->getState();
  if (state == AR_KEY_DOWN) {
    stateString = "DOWN";
  } else if (state == AR_KEY_UP) {
    stateString = "UP";
  } else if (state == AR_KEY_REPEAT) {
    stateString = "REPEAT";
  } else {
    stateString = "UNKNOWN";
  }
  cout << "Key state = " << stateString << endl;
}
    
// This is how we have to catch reshape events now, still
// dealing with the fallout from the GLUT->arGUI conversion.
// Note that the behavior implemented below is the default.
void windowEvent( arMasterSlaveFramework& fw, arGUIWindowInfo* winInfo ) {
  // The values are defined in src/graphics/arGUIDefines.h.
  // arGUIWindowInfo is in arGUIInfo.h
  // The window manager is in arGUIWindowManager.h
  if (winInfo->getState() == AR_WINDOW_RESIZE) {
    const int windowID = winInfo->getWindowID();
#ifdef UNUSED
    const int x = winInfo->getPosX();
    const int y = winInfo->getPosY();
#endif
    const int width = winInfo->getSizeX();
    const int height = winInfo->getSizeY();
    fw.getWindowManager()->setWindowViewport( windowID, 0, 0, width, height );
  }
}

int main(int argc, char** argv) {
	filename = "temp_hk.txt";
	if(argc > 1){
		filename = argv[1];
	}
	if(argc > 2){
		cout << argv[2];
		cout << "\n";
	}
	arMasterSlaveFramework framework;
	// Tell the framework what units we're using.
	framework.setUnitConversion(FEET_TO_LOCAL_UNITS);
	framework.setClipPlanes(nearClipDistance, farClipDistance);
	framework.setStartCallback(start);
	framework.setWindowStartGLCallback( windowStartGL );
	framework.setPreExchangeCallback(preExchange);
	framework.setPostExchangeCallback(postExchange);
	framework.setDrawCallback(display);
	framework.setKeyboardCallback( keypress );
	framework.setWindowEventCallback( windowEvent );
	// also setExitCallback(), setUserMessageCallback()
	// in demo/arMasterSlaveFramework.h


	if (!framework.init(argc, argv)) {
	return 1;
	}

	// Never returns unless something goes wrong
	return framework.start() ? 0 : 1;
}
