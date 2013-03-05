//------------------------------------------------
//	RC SERVO library
//	Arthur Benemann 19/12/2011
//------------------------------------------------
#define DEFAULT_SERVO_VALUE 1500
#define MAX_SERVO_VALUE     2100
#define MIN_SERVO_VALUE     900
#define SERVO_UPDATE_PERIOD 20000

enum RC_CH{
    RC_CH1,
    RC_CH2
};

void initRC();
void writeRC(enum RC_CH ch, unsigned int position);
