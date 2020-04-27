void streamDisconnect(void);
void streamConnect(void);
#define VERT 1
#define HORIZ 0
void servoMove(int axis, int dir);
void flashLight(int val);
extern float temperatureF;
extern struct tm  timeinfo;
//asctime(&timeinfo);
