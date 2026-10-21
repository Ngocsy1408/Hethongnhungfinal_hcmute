#include "adaptive_kalman.h"
#include <math.h>

void AKF_Init(AdaptiveKalman_t *kf, float q_angle, float q_bias, float r_base) {
    kf->Q_angle = q_angle;
    kf->Q_bias = q_bias;
    kf->R_base = r_base;
    kf->R_adaptive = r_base;
    kf->angle = 0.0f;
    kf->bias = 0.0f;
    kf->P[0][0] = 1.0f; kf->P[0][1] = 0.0f;
    kf->P[1][0] = 0.0f; kf->P[1][1] = 1.0f;
}

float AKF_Update(AdaptiveKalman_t *kf, float newAngle, float newRate, float dt, float acc_magnitude) {
    // 1. Thích ứng ma trận R dựa trên vector gia tốc ||a|| = sqrt(ax^2 + ay^2 + az^2)
    // Trọng lực chuẩn = 1.0g. Càng xa 1.0g nghĩa là xe đang xóc/ôm cua/phanh -> Tăng R để phạt Accel
    float acc_error = fabsf(acc_magnitude - 1.0f);
    
    // Hệ số phạt lũy tiến (tuning factor: 10.0f)
    kf->R_adaptive = kf->R_base * (1.0f + 10.0f * (acc_error * acc_error));

    // 2. Predict (Dự báo)
    kf->rate = newRate - kf->bias;
    kf->angle += dt * kf->rate;

    kf->P[0][0] += dt * (dt * kf->P[1][1] - kf->P[0][1] - kf->P[1][0] + kf->Q_angle);
    kf->P[0][1] -= dt * kf->P[1][1];
    kf->P[1][0] -= dt * kf->P[1][1];
    kf->P[1][1] += kf->Q_bias * dt;

    // 3. Update (Cập nhật với R thích ứng)
    float S = kf->P[0][0] + kf->R_adaptive;
    float K[2];
    K[0] = kf->P[0][0] / S;
    K[1] = kf->P[1][0] / S;

    float y = newAngle - kf->angle;
    kf->angle += K[0] * y;
    kf->bias  += K[1] * y;

    float P00_temp = kf->P[0][0];
    float P01_temp = kf->P[0][1];

    kf->P[0][0] -= K[0] * P00_temp;
    kf->P[0][1] -= K[0] * P01_temp;
    kf->P[1][0] -= K[1] * P00_temp;
    kf->P[1][1] -= K[1] * P01_temp;

    return kf->angle;
}