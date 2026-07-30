/**
  ******************************************************************************
  * @file           : main_logic_control.cpp
  * @brief          : Core State Machine & Closed-Loop Control Logic
  *                   Engine for STM32H7 Bioreactor Node.
  ******************************************************************************
  */

#include "stm32h7xx_hal.h"
#include <math.h>

/* --- SYSTEM STATES --- */
typedef enum {
    STATE_INIT = 0,
    STATE_NORMAL_OPERATION,
    STATE_WARNING_DRIFT,
    STATE_EMERGENCY_SHUTDOWN
} SystemState_t;

/* --- PID PARAMETERS --- */
typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float prev_error;
    float integral;
    float output_limit;
} PID_Controller_t;

/* --- BIOCHEMICAL BOUNDARIES --- */
#define TEMP_TARGET      37.0f
#define TEMP_CRITICAL_HIGH 42.0f
#define PH_TARGET        7.20f
#define PH_DEADBAND      0.05f
#define DO_CRITICAL_LOW  20.0f  // Minimum safe dissolved oxygen (%)

/* --- GLOBAL CONTROL VARIABLES --- */
static SystemState_t currentState = STATE_INIT;
static PID_Controller_t tempPID = {2.5f, 0.1f, 0.5f, 0.0f, 0.0f, 100.0f};

/* --- ACTUATOR CONTROL PINS --- */
#define HEATER_PORT      GPIOB
#define HEATER_PIN       GPIO_PIN_0
#define ACID_PUMP_PORT   GPIOB
#define ACID_PUMP_PIN    GPIO_PIN_1
#define BASE_PUMP_PORT   GPIOB
#define BASE_PUMP_PIN    GPIO_PIN_2
#define ALARM_BUZZER_PORT GPIOC
#define ALARM_BUZZER_PIN  GPIO_PIN_13

/* --- FUNCTION PROTOTYPES --- */
void System_State_Machine_Update(float temp, float ph, float do_level);
float Compute_PID(PID_Controller_t *pid, float setpoint, float current_val, float dt);
void Execute_pH_Control(float current_ph);
void Safety_Emergency_Shutdown(void);

/**
  * @brief Primary Logic Execution Hook called by system loop
  */
void Run_Main_Control_Logic(float temp, float ph, float do_level) {
    // 1. Evaluate State Machine & Safety Boundaries
    System_State_Machine_Update(temp, ph, do_level);

    // 2. State-Dependent Execution Logic
    switch (currentState) {
        case STATE_NORMAL_OPERATION:
        case STATE_WARNING_DRIFT:
            {
                // Thermal Control via PID (calculates PWM duty cycle or SSR cycle)
                float thermal_duty = Compute_PID(&tempPID, TEMP_TARGET, temp, 0.5f);
                if (thermal_duty > 0.0f) {
                    HAL_GPIO_WritePin(HEATER_PORT, HEATER_PIN, GPIO_PIN_SET);
                } else {
                    HAL_GPIO_WritePin(HEATER_PORT, HEATER_PIN, GPIO_PIN_RESET);
                }

                // pH Control via Deadband Pulsed Actuation
                Execute_pH_Control(ph);
            }
            break;

        case STATE_EMERGENCY_SHUTDOWN:
            Safety_Emergency_Shutdown();
            break;

        default:
            break;
    }
}

/**
  * @brief System State Machine for Fault & Boundary Isolation
  */
void System_State_Machine_Update(float temp, float ph, float do_level) {
    // Critical Condition Trigger -> Hard Shutdown
    if (temp >= TEMP_CRITICAL_HIGH || do_level < DO_CRITICAL_LOW) {
        currentState = STATE_EMERGENCY_SHUTDOWN;
        return;
    }

    // Parameter Drift Trigger -> Warning State
    if (fabsf(ph - PH_TARGET) > 0.4f || fabsf(temp - TEMP_TARGET) > 2.0f) {
        currentState = STATE_WARNING_DRIFT;
        HAL_GPIO_WritePin(ALARM_BUZZER_PORT, ALARM_BUZZER_PIN, GPIO_PIN_SET); // Warning Alert
    } else {
        currentState = STATE_NORMAL_OPERATION;
        HAL_GPIO_WritePin(ALARM_BUZZER_PORT, ALARM_BUZZER_PIN, GPIO_PIN_RESET);
    }
}

/**
  * @brief Closed-Loop Discrete PID Calculation
  */
float Compute_PID(PID_Controller_t *pid, float setpoint, float current_val, float dt) {
    float error = setpoint - current_val;
    
    // Proportional Term
    float P = pid->Kp * error;

    // Integral Term with Anti-Windup Guard
    pid->integral += error * dt;
    if (pid->integral > pid->output_limit) pid->integral = pid->output_limit;
    if (pid->integral < -pid->output_limit) pid->integral = -pid->output_limit;
    float I = pid->Ki * pid->integral;

    // Derivative Term
    float derivative = (error - pid->prev_error) / dt;
    float D = pid->Kd * derivative;

    pid->prev_error = error;

    float output = P + I + D;
    if (output > pid->output_limit) output = pid->output_limit;
    if (output < 0.0f) output = 0.0f; // Heating element only

    return output;
}

/**
  * @brief Non-Linear pH Control Engine with Deadband Filtering
  */
void Execute_pH_Control(float current_ph) {
    float error = current_ph - PH_TARGET;

    // Ignore minor fluctuations within safe biological deadband
    if (fabsf(error) <= PH_DEADBAND) {
        HAL_GPIO_WritePin(ACID_PUMP_PORT, ACID_PUMP_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BASE_PUMP_PORT, BASE_PUMP_PIN, GPIO_PIN_RESET);
        return;
    }

    // High pH -> Dose Acid
    if (error > PH_DEADBAND) {
        HAL_GPIO_WritePin(BASE_PUMP_PORT, BASE_PUMP_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(ACID_PUMP_PORT, ACID_PUMP_PIN, GPIO_PIN_SET);
        HAL_Delay(150); // Metered 150ms dosing burst
        HAL_GPIO_WritePin(ACID_PUMP_PORT, ACID_PUMP_PIN, GPIO_PIN_RESET);
    } 
    // Low pH -> Dose Base
    else if (error < -PH_DEADBAND) {
        HAL_GPIO_WritePin(ACID_PUMP_PORT, ACID_PUMP_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BASE_PUMP_PORT, BASE_PUMP_PIN, GPIO_PIN_SET);
        HAL_Delay(150);
        HAL_GPIO_WritePin(BASE_PUMP_PORT, BASE_PUMP_PIN, GPIO_PIN_RESET);
    }
}

/**
  * @brief Fail-Safe Fail-Closed Hardware Lockdown
  */
void Safety_Emergency_Shutdown(void) {
    // Cut all actuator power outputs immediately
    HAL_GPIO_WritePin(HEATER_PORT, HEATER_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ACID_PUMP_PORT, ACID_PUMP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BASE_PUMP_PORT, BASE_PUMP_PIN, GPIO_PIN_RESET);

    // Latch Critical Alarm Output
    HAL_GPIO_WritePin(ALARM_BUZZER_PORT, ALARM_BUZZER_PIN, GPIO_PIN_SET);
}
