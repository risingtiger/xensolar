

#include "Particle.h"
#include "MCP9808.h"
#include "PublishQueueAsyncRK.h"


#define SETTING_ResetInterval        (uint32_t)1814400000  // 21 days
#define SETTING_CadenceInterval      (uint32_t)1000        // 5 seconds
#define SETTING_TempfsLength         (uint8_t)30
#define SETTING_TempfsMidIndex       (uint8_t)15
#define SETTING_Tempf_Low_Threshold  (uint8_t)37
#define SETTING_Tempf_High_Threshold (uint8_t)39
#define SETTING_HeatPWMDutyCycle     (uint8_t)64
#define SETTING_PUBlISHINTERVAL      (uint32_t)1200000     // 20 minutes
#define SETTING_PUBlISHSTRSIZE       (uint16_t)512


SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);


uint32_t _currentmillis = 0;
uint32_t _cadencemillis = 0;
uint32_t _publishmillis = 0;
uint32_t _heatingonmillis = 0;
uint8_t _tempfs[SETTING_TempfsLength];
uint8_t _tempf = 0;
uint8_t _tempfindex = 0;
uint8_t _isheating = 0;
uint8_t GPIO_Heat = A5;
uint8_t _interval_highf = 0;
uint8_t _interval_lowf = 0;
uint8_t _interval_heating_on_count = 0;
uint8_t _interval_heating_duration = 0;
SerialLogHandler logHandler(LOG_LEVEL_INFO);
MCP9808 mcp = MCP9808();
uint8_t           _publishQueueRetainedBuffer[8192];
PublishQueueAsync _publishQueue(_publishQueueRetainedBuffer, sizeof(_publishQueueRetainedBuffer));




void swap(uint8_t* xp, uint8_t* yp) { 
    uint8_t temp = *xp; 
    *xp = *yp; 
    *yp = temp; 
} 




void sortit(uint8_t arr[], uint8_t n) { 
    uint8_t i, j, min_in; 
 
    for(i=0; i<n; i++) { 
        min_in = i; 
 
        for(j=i+1;j<n;j++) 
            if (arr[j] < arr[min_in]) min_in = j; 
 
        swap(&arr[i], &arr[min_in]); 
    } 
} 




void setup() {

    pinMode(GPIO_Heat, OUTPUT);

    delay(5000);
    //
	// Wait for the sensor to come up
	while(! mcp.begin()){
	    delay(500);
	}

    mcp.setResolution(MCP9808_SLOWEST);
}




void loop() {

    _currentmillis = millis();

    if (_currentmillis - _cadencemillis >= SETTING_CadenceInterval) {   

        _cadencemillis = _currentmillis;

        _tempfs[_tempfindex] = (uint8_t)(mcp.getTemperature() * 1.8) + 32;

        sortit(_tempfs, SETTING_TempfsLength);

        _tempf = _tempfs[SETTING_TempfsMidIndex];

        _tempfindex = _tempfindex + 1;

        if (_tempfindex >= SETTING_TempfsLength) _tempfindex = 0;

        if (_tempf <= SETTING_Tempf_Low_Threshold) {

            if (_isheating == 0) {
                _interval_heating_on_count = _interval_heating_on_count + 1;
                _heatingonmillis = _currentmillis;
            }

            _isheating = 1;
            analogWrite(GPIO_Heat, SETTING_HeatPWMDutyCycle);
        }

        if (_tempf >= SETTING_Tempf_High_Threshold) {

            if (_isheating == 1) {
                _interval_heating_duration = _interval_heating_duration + (_currentmillis - _heatingonmillis);
                _heatingonmillis = 0;
            }

            _isheating = 0;
            analogWrite(GPIO_Heat, 0);
        }

        if (_tempf > _interval_highf) _interval_highf = _tempf;
        if (_tempf < _interval_lowf) _interval_lowf = _tempf;
    }

    if (_currentmillis - _publishmillis >= SETTING_PUBlISHINTERVAL) {

        _publishmillis = _currentmillis;


        int ts = Time.now();
        uint8_t heating_duration_as_seconds = (uint8_t)(_interval_heating_duration / 1000);

        char pstr[SETTING_PUBlISHSTRSIZE] = {'\0'};
        sprintf(pstr, "t%d,%d;h%d,%d;d%d", _interval_highf, _interval_lowf, _interval_heating_on_count, heating_duration_as_seconds, ts);

        _publishQueue.publish("solar", pstr, PRIVATE, WITH_ACK);
    }


    if (_currentmillis > SETTING_ResetInterval)   System.reset();
}







