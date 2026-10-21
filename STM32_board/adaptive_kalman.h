#ifndef ADAPTIVE_KALMAN_H
#define ADAPTIVE_KALMAN_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float Q_angle;   
    float Q_bias;    
    float R_base;      // Nhiễu đo lường danh định (lúc tĩnh)
    float R_adaptive;  // Nhiễu tự thích ứng theo gia tốc động

    float angle;     
    float bias;      
    float P[2][2];   
} AdaptiveKalman_t;

void AKF_Init(AdaptiveKalman_t *kf, float q_angle, float q_bias, float r_base);
float AKF_Update(AdaptiveKalman_t *kf, float newAngle, float newRate, float dt, float acc_magnitude);

#endif