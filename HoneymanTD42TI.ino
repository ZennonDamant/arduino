//=====================================================================================================================
// Code for Ardinuio board - for Patrol 4.2TD ECU
//=====================================================================================================================
// Date written: August 2014
// Authors: SevenAngryPenguins & ZENNON



//=====================================================================================================================
// Global Constants
//=====================================================================================================================
const long SERIAL_SPEED = 9600;                 // Serial speed (bits per second)

const int MAX_RPM = 3900;                       // if this is changed will also have to update "DUTY_CYCLES_TABLE"
const int MAX_BOOST = 14;                       // if this is changed will also have to update "DUTY_CYCLES_TABLE"
const int MAX_RPM_POINTS = 12;                  // if this is changed will also have to update "DUTY_CYCLES_TABLE"
const int MAX_BOOST_POINTS = MAX_BOOST + 1;     // if this is changed will also have to update "DUTY_CYCLES_TABLE"
const int DUTY_CYCLES_TABLE[MAX_BOOST_POINTS][MAX_RPM_POINTS] =
{
    //  0    1    2    3    4    5    6    7    8    9   10   11  rpmIndex
    //600  900 1200 1500 1800 2100 2400 2700 3000 3300 3600 3900  roundedRpm
    { 255, 255, 190, 160, 150, 110, 90,   50,  30,  15,   0,   0 },      //  0 psi
    { 255, 240, 180, 140, 130, 100, 80,   50,  20,  10,   0,   0 },      //  1 psi
    { 235, 230, 130, 100, 100, 85,  70,   50,  20,  10,   0,   0 },      //  2 psi
    { 180, 180, 95,  85,  85,  85,  70,   40,  15,  7,    0,   0 },      //  3 psi
    { 100, 100, 90,  80,  80,  70,  70,   40,  15,  7,    0,   0 },      //  4 psi
    { 80,  80,  75,  70,  70,  70,  60,   40,  10,  7,    0,   0 },      //  5 psi
    { 80,  80,  65,  65,  65,  65,  60,   40,  10,  7,    0,   0 },      //  6 psi
    { 80,  80,  55,  55,  55,  65,  60,   30,  10,  7,    0,   0 },      //  7 psi
    { 70,  70,  50,  50,  50,  50,  45,   20,  10,  7,    0,   0 },      //  8 psi
    { 60,  60,  40,  35,  35,  35,  35,   15,  8,   6,    0,   0 },      //  9 psi
    { 40,  40,  30,  30,  30,  30,  30,   10,  5,   5,    0,   0 },      // 10 psi
    { 20,  20,  15,  15,  15,  15,  15,   10,  3,   2,    0,   0 },      // 11 psi
    { 10,  10,  10,  10,  10,  10,  10,   10,  2,   2,    0,   0 },      // 12 psi
    { 5,   5,   5,   5,   5,   5,   5,    5,   0,   0,    0,   0 },      // 13 psi
    { 0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,   0 },      // 14 psi
};

const long GLOW_PLUG_TIMEOUT = 18000;           // Glow plug time-out (ie how long they should be on for)

const int TIMING_OUTPUT_PIN = 3;                // timing control valve output pin
const int PULSE_INPUT_PIN = 5;                  // ECU frequency RPM input pin
const int GLOW_CONTROL_PIN = 8;                 // Glow glowplugs if ECU grounds digital pin 8
const int WATER_LEVEL_SAFETY_SWITCH_PIN = 10;   // water level safety switch
const int WATER_INJECTION_PUMP_PIN = 11;        // PWM control for water/meth injection pump
const int GLOW_RELAY_OUTPUT_PIN = 12;           // glow plug relay output pin

const int MIN_ANALOG_WRITE_VALUE = 0;           // Used for duty cycle and PWM validation
const int MAX_ANALOG_WRITE_VALUE = 255;         // Used for duty cycle and PWM validation



//=====================================================================================================================
// Global Variables
//=====================================================================================================================
int lowerRpm = -1;                              // lower bound for current RPM value - used for interpolation
int upperRpm = -1;                              // upper bound for current RPM value - used for interpolation
int lowerRpmIndex = -1;                         // index of above lower bound - used for lookup
int upperRpmIndex = -1;                         // index of above upper bound - used for lookup



//=====================================================================================================================
// The two primary Arduino functions
//=====================================================================================================================

/// <summary>
/// The set-up function will only run once, after each power-up or reset of the Arduino board.
/// </summary>
void setup()
{
    Serial.begin(SERIAL_SPEED);

    pinMode(PULSE_INPUT_PIN, INPUT);
    pinMode(TIMING_OUTPUT_PIN, OUTPUT);
    pinMode(GLOW_RELAY_OUTPUT_PIN, OUTPUT);
    
    pinMode(A1, INPUT);
    pinMode(8, INPUT_PULLUP);
    
    pinMode(WATER_INJECTION_PUMP_PIN, OUTPUT);
    pinMode(WATER_LEVEL_SAFETY_SWITCH_PIN, INPUT_PULLUP);
}

/// <summary>
/// Loop function will be called again and again. Use it to actively control the Arduino board.
/// </summary>
void loop()
{
    // Enable/disable glow plugs as needed
    GlowPlugs();
    
    // Get boost & RPM
    int rpm = GetRpm();
    float boost = GetBoost();
    
    // Calculate and set duty cycle
    DutyCycle(rpm, boost);

    // Perform Water Injection
    WaterInjection(rpm, boost);

    // Sleep for 2 milliseconds
    delay(2);
}



//=====================================================================================================================
// Our helper functions
//=====================================================================================================================

/// <summary>
/// Enable/disable Glow Plugs as needed.
/// </summary>
void GlowPlugs()
{
    //Glow plugs run for GLOW_PLUG_TIMEOUT milliseconds from startup if ecu grounds digital pin 8
    int glowControl = digitalRead(GLOW_CONTROL_PIN);
    long glowTimeOut = millis();

    if ((glowControl == 0) && (glowTimeOut <= GLOW_PLUG_TIMEOUT))
    {
        digitalWrite(GLOW_RELAY_OUTPUT_PIN, HIGH);
    }
    else
    {
        digitalWrite(GLOW_RELAY_OUTPUT_PIN, LOW);
    }
}

/// <summary>
/// Calculate and set duty cycle.
/// </summary>
/// <param name="rpm">The engine RPM.</param>
/// <param name="boost">The boost (in PSI).</param>
void DutyCycle(int rpm, float boost)
{
    //Calculate & set duty cycle
    int dutyCycle = GetInterpolatedDutyCycle(rpm, boost);
    analogWrite(TIMING_OUTPUT_PIN, dutyCycle);
}

/// <summary>
/// Gets the actual RPM.
/// </summary>
/// <returns>The engine RPM</returns>
int GetRpm()
{
    const long TimeOut = 100000; //in microseconds

    //Get time signal stays high (in microseconds)
    float pulseWidth = pulseIn(PULSE_INPUT_PIN, HIGH, TimeOut);

    //Calculate RPM
    int rpm = ((1000000 / (pulseWidth)) * 20);

    return rpm;
}

/// <summary>
/// Gets the RPM range so we can interpolate the duty cycle.
/// Note: This function doesn't return anything - instead it populates these four global variables:
///       lowerRpmIndex, upperRpmIndex, lowerRpm & upperRpm
/// </summary>
/// <param name="rpm">The actual RPM.</param>
void GetRpmRange(int rpm)
{
    if (rpm < 900)
    {
        lowerRpmIndex = 0;
        upperRpmIndex = 1;
        lowerRpm = 600;
        upperRpm = 900;
    }
    else if (rpm >= 900 && rpm < 1200)
    {
        lowerRpmIndex = 1;
        upperRpmIndex = 2;
        lowerRpm = 900;
        upperRpm = 1200;
    }
    else if (rpm >= 1200 && rpm < 1500)
    {
        lowerRpmIndex = 2;
        upperRpmIndex = 3;
        lowerRpm = 1200;
        upperRpm = 1500;
    }
    else if (rpm >= 1500 && rpm < 1800)
    {
        lowerRpmIndex = 3;
        upperRpmIndex = 4;
        lowerRpm = 1500;
        upperRpm = 1800;
    }
    else if (rpm >= 1800 && rpm < 2100)
    {
        lowerRpmIndex = 4;
        upperRpmIndex = 5;
        lowerRpm = 1800;
        upperRpm = 2100;
    }
    else if (rpm >= 2100 && rpm < 2400)
    {
        lowerRpmIndex = 5;
        upperRpmIndex = 6;
        lowerRpm = 2100;
        upperRpm = 2400;
    }
    else if (rpm >= 2400 && rpm < 2700)
    {
        lowerRpmIndex = 6;
        upperRpmIndex = 7;
        lowerRpm = 2400;
        upperRpm = 2700;
    }
    else if (rpm >= 2700 && rpm < 3000)
    {
        lowerRpmIndex = 7;
        upperRpmIndex = 8;
        lowerRpm = 2700;
        upperRpm = 3000;
    }
    else if (rpm >= 3000 && rpm < 3300)
    {
        lowerRpmIndex = 8;
        upperRpmIndex = 9;
        lowerRpm = 3000;
        upperRpm = 3300;
    }
    else if (rpm >= 3300 && rpm < 3600)
    {
        lowerRpmIndex = 9;
        upperRpmIndex = 10;
        lowerRpm = 3300;
        upperRpm = 3600;
    }
    else
    {
        lowerRpmIndex = 10;
        upperRpmIndex = 11;
        lowerRpm = 3600;
        upperRpm = 3900;
    }

    //Validate
    if (lowerRpmIndex < 0) lowerRpmIndex = 0;
    if (upperRpmIndex > MAX_RPM_POINTS - 1) upperRpmIndex = MAX_RPM_POINTS - 1;
    if (lowerRpm < 0) lowerRpm = 0;
    if (upperRpm > MAX_RPM) upperRpm = MAX_RPM;
}

/// <summary>
/// Function which looks up duty cycle based on given RPM and boost.
/// Note: will interpolate table values based on RPM
/// </summary>
/// <param name="rpm">The engine RPM.</param>
/// <param name="boost">The boost (in PSI).</param>
/// <returns>The duty cycle. Number between 0 (always off) and 255 (always on).</returns>
int GetInterpolatedDutyCycle(int rpm, int boost)
{
    // Get the RPM range (including indices)
    GetRpmRange(rpm);

    // Get boost index
    int roundedBoost = GetRoundedBoost(boost);
    int boostIndex = GetBoostIndex(roundedBoost);

    // Lookup upper and lower duty cycles using boost and RPM indices
    int lowerDutyCycle = DUTY_CYCLES_TABLE[boostIndex][lowerRpmIndex];
    int upperDutyCycle = DUTY_CYCLES_TABLE[boostIndex][upperRpmIndex];
    
    // Validate
    if (lowerDutyCycle < MIN_ANALOG_WRITE_VALUE) lowerDutyCycle = MIN_ANALOG_WRITE_VALUE;
    if (lowerDutyCycle > MAX_ANALOG_WRITE_VALUE) lowerDutyCycle = MAX_ANALOG_WRITE_VALUE;
    if (upperDutyCycle < MIN_ANALOG_WRITE_VALUE) upperDutyCycle = MIN_ANALOG_WRITE_VALUE;
    if (upperDutyCycle > MAX_ANALOG_WRITE_VALUE) upperDutyCycle = MAX_ANALOG_WRITE_VALUE;

    // Use linear interpolation to calculate duty cycle to return
    int interpolatedDutyCycle = InterpolateInts(rpm, lowerRpm, upperRpm, lowerDutyCycle, upperDutyCycle);
    
    return interpolatedDutyCycle;
}

/// <summary>
/// Uses linear interpolation to calculate Y.
/// </summary>
/// <param name="X">X (actual RPM)</param>
/// <param name="x0">x0 (lowerRpm)</param>
/// <param name="x1">x1 (upperRpm)</param>
/// <param name="y0">y0 (lowerDutyCycle)</param>
/// <param name="y1">y1 (upperDutyCycle)</param>
/// <returns>Y (interpolated duty cycle)</returns>
int InterpolateInts(int X, int x0, int x1, int y0, int y1)
{
    float interpolatedFloat = y0 + ((y1 - y0) * ((X - x0) / (x1 - x0)));
    int interpolatedInt = (int)(round(interpolatedFloat));
    return interpolatedInt;
}

/// <summary>
/// Gets the actual boost (in PSI).
/// </summary>
/// <returns>The boost.</returns>
float GetBoost()
{
    float mapValue = analogRead(A1);
    float boost = ((mapValue * 0.004887586) - 1.00) / 0.064;    //in PSI
    
    // Validate
    if (boost < 0) boost = 0;
    
    return boost;
}

/// <summary>
/// Rounds the actual boost to the nearest rounded boost value.
/// </summary>
/// <param name="boost">The actual boost.</param>
/// <returns>The rounded boost.</returns>
int GetRoundedBoost(float boost)
{
    // Round to nearest whole number
    int roundedBoost = int(round(boost));

    // Validate
    if (roundedBoost < 0) roundedBoost = 0;
    if (roundedBoost > MAX_BOOST) roundedBoost = MAX_BOOST;

    return roundedBoost;
}

/// <summary>
/// Gets the boost index to use for the DUTY_CYCLES_TABLE array.
/// </summary>
/// <param name="roundedBoost">The rounded boost.</param>
/// <returns>The boost index.</returns>
int GetBoostIndex(int roundedBoost)
{
    // Since our roundedBoost values match boostIndex values this is easy
    int boostIndex = roundedBoost;

    // Note: If we change the boost values in the DUTY_CYCLES_TABLE
    // (so they don't start at 0 and go up in increments of 1)
    // then we'll need to do a proper lookup here.

    // Validate
    if (boostIndex < 0) boostIndex = 0;
    if (boostIndex > MAX_BOOST_POINTS - 1) boostIndex = MAX_BOOST_POINTS - 1;

    return boostIndex;
}

/// <summary>
/// Perform water injection as needed.
/// </summary>
/// <param name="rpm">The engine RPM.</param>
/// <param name="boost">The boost (in PSI).</param>
void WaterInjection(int rpm, float boost)
{
    int calculatedPwm = 0;
    int lowWaterLevel = digitalRead(WATER_LEVEL_SAFETY_SWITCH_PIN);
   
    if (lowWaterLevel == 0 || rpm < 1500)
    {
        calculatedPwm = 0;
    }
    else 
    {
        calculatedPwm = GetInterpolatedPwm(boost);
    }
    
    analogWrite(WATER_INJECTION_PUMP_PIN, calculatedPwm);
}

/// <summary>
/// Function which interpolats PWM based on given boost.
/// </summary>
/// <param name="boost">The boost (in PSI).</param>
/// <returns>The PWM. Number between 0 (always off) and 255 (always on).</returns>
int GetInterpolatedPwm(float boost)
{
    int interpolatedPwm = 0;
    if (boost < 8)
    {
        interpolatedPwm = 0;
    }
    else if (boost >= 8 && boost <= 12)
    {
        interpolatedPwm = InterpolateFloats(boost, 8.0, 12.0, 15, 255);
    }
    else if (boost > 12)
    {
        interpolatedPwm = 255;
    }
    
    // Validate
    if (interpolatedPwm < MIN_ANALOG_WRITE_VALUE) interpolatedPwm = MIN_ANALOG_WRITE_VALUE;
    if (interpolatedPwm > MAX_ANALOG_WRITE_VALUE) interpolatedPwm = MAX_ANALOG_WRITE_VALUE;
    
    return interpolatedPwm;
}

/// <summary>
/// Uses linear interpolation to calculate Y.
/// </summary>
/// <param name="X">X (actual boost)</param>
/// <param name="x0">x0 (lower boost)</param>
/// <param name="x1">x1 (upper boost)</param>
/// <param name="y0">y0 (lower PWM)</param>
/// <param name="y1">y1 (upper PWM)</param>
/// <returns>Y (interpolated PWM)</returns>
int InterpolateFloats(float X, float x0, float x1, int y0, int y1)
{
    float interpolatedFloat = y0 + ((y1 - y0) * ((X - x0) / (x1 - x0)));
    int interpolatedInt = (int)(round(interpolatedFloat));
    return interpolatedInt;
}
