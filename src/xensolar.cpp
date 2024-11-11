

#include "Particle.h"
#include "MCP9808.h"
#include "PublishQueueAsyncRK.h"


#define SETTING_ResetInterval           (int)1814400000  // 21 days
#define SETTING_CadenceInterval         (int)1000        // 1 seconds
#define SETTING_TempfsLength            (int)10
#define SETTING_TempfsMidIndex          (int)5
#define SETTING_Tempf_Low_Threshold     (int)38
#define SETTING_Tempf_High_Threshold    (int)39
#define SETTING_HEATING_PWM_DUTYCYCLE   (int)255
#define SETTING_PUBlISHINTERVAL         (int)1200000     // 20 minutes
#define SETTING_PUBlISHSTRSIZE          (int)512


uint8_t const GPIO_HEAT_PIN = A5;

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);


uint8_t           _publishQueueRetainedBuffer[8192];
PublishQueueAsync _publishQueue(_publishQueueRetainedBuffer, sizeof(_publishQueueRetainedBuffer));


int _currentmillis = 0;
int _cadencemillis = 0;
int _publishmillis = 0;
int _heatingonmillis = 0;
int _isactive = 1;
int _tempfs[SETTING_TempfsLength];
int _tempf = 0;
int _tempfindex = 0;
int _isheating = 0;
int _interval_highf = 0;
int _interval_lowf = 0;
int _interval_heating_on_count = 0;
int _interval_heating_duration = 0;
SerialLogHandler logHandler(LOG_LEVEL_INFO);
MCP9808 mcp = MCP9808();




void swap(int* xp, int* yp) { 
    int temp = *xp; 
    *xp = *yp; 
    *yp = temp; 
} 




void sortit(int arr[], int n) { 
    int i, j, min_in; 
 
    for(i=0; i<n; i++) { 
        min_in = i; 
 
        for(j=i+1;j<n;j++) 
            if (arr[j] < arr[min_in]) min_in = j; 
 
        swap(&arr[i], &arr[min_in]); 
    } 
} 




void publish_to_cloud(int interval_heating_duration, int _interval_heating_on_count, int interval_highf, int interval_lowf) {

    int ts = Time.now();
    int heating_duration_as_seconds = (int)(interval_heating_duration / 1000);

    char pstr[SETTING_PUBlISHSTRSIZE] = {'\0'};
    sprintf(pstr, "t%d,%d;h%d,%d;d%d", _interval_highf, _interval_lowf, _interval_heating_on_count, heating_duration_as_seconds, ts);

    _publishQueue.publish("solar", pstr, PRIVATE, WITH_ACK);
}




int configit(String pstr) {

    uint8_t p = (uint8_t)pstr.toInt();

    if (p == 1) {
        _isactive = 1;
    } else {
        _isactive = 0;
    }

    return 1;
}




void setup() {

    Serial.begin();
    Serial1.begin(9600);

    pinMode(GPIO_HEAT_PIN, OUTPUT);

	mcp.begin();
	    
    mcp.setResolution(MCP9808_SLOWEST);

    Particle.variable("tempf", _tempf);
    Particle.variable("isheating", _isheating);
    Particle.variable("interval_highf", _interval_highf);
    Particle.variable("interval_lowf", _interval_lowf);
    Particle.variable("interval_heating_on_count", _interval_heating_on_count);
    Particle.variable("interval_heating_duration", _interval_heating_duration);

    Particle.function("pw_initiateSendStatus", configit);

}




void loop() {

    _currentmillis = millis();

    if (_currentmillis - _cadencemillis >= SETTING_CadenceInterval) {  

        _cadencemillis = _currentmillis;

        _tempfs[_tempfindex] = (int)(mcp.getTemperature() * 1.8) + 32;

        sortit(_tempfs, SETTING_TempfsLength);

        _tempf = _tempfs[SETTING_TempfsMidIndex];

        _tempfindex = _tempfindex + 1;
        if (_tempfindex >= SETTING_TempfsLength) _tempfindex = 0;

        Serial.print("tempf: ");
        Serial.println(_tempf);

        if (_tempf <= SETTING_Tempf_Low_Threshold) {

            if (_isheating == 0) {
                _interval_heating_on_count = _interval_heating_on_count + 1;
                _heatingonmillis = _currentmillis;
            }
            
            if (_isactive == 1)
                _isheating = 1; 
            else
                _isheating = 0;
        }

        if (_tempf >= SETTING_Tempf_High_Threshold) {

            if (_isheating == 1) {
                _interval_heating_duration = _interval_heating_duration + (_currentmillis - _heatingonmillis);
                _heatingonmillis = 0;
            }
            _isheating = 0;
        }

        if (_tempf > _interval_highf) _interval_highf = _tempf;
        if (_tempf < _interval_lowf) _interval_lowf = _tempf;

        analogWrite(GPIO_HEAT_PIN, (_isheating ? SETTING_HEATING_PWM_DUTYCYCLE : 0) );

        Serial.print("_isheating: ");
        Serial.println(_isheating);
    }

    if (_currentmillis - _publishmillis >= SETTING_PUBlISHINTERVAL) {
        _publishmillis = _currentmillis;

        if (_isheating == 1) {
            _interval_heating_duration = _interval_heating_duration + (_currentmillis - _heatingonmillis);
            _heatingonmillis = 0;
        }

        publish_to_cloud(_interval_heating_duration, _interval_heating_on_count, _interval_highf, _interval_lowf);

        _interval_heating_duration = 0;
        _interval_heating_on_count = 0;
    }

    if (_currentmillis > SETTING_ResetInterval)   System.reset();    
}







