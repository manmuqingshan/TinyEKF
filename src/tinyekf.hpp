/*
 * TinyEKF: Extended Kalman Filter for Arduino and TeensyBoard.
 *
 * Copyright (C) 2015 Simon D. Levy
 *
 * MIT License
 */

#define _FOO
#define _float_t float
#include <tinyekf.h>

class TinyEKF {

    public:

        void initialize(
                const float pdiag[EKF_N],
                const float min_covariance=0, 
                const float max_covariance=0)
        {
            _isUpdated = false;

            _min_covariance = min_covariance;
            _max_covariance = max_covariance;

            ekf_initialize(&_ekf, pdiag);

            for (uint8_t i=0; i<EKF_N; ++i) {

                _x[i] = 0;
            }
        }

        void predict(
                const float fx[EKF_N],
                const float F[EKF_N*EKF_N],
                const float Q[EKF_N*EKF_N])
        { 
            // $\hat{x}_k = f(\hat{x}_{k-1})$
            for (uint8_t i=0; i<EKF_N; ++i) {
                _x[i] = fx[i];
            }

            multiplyCovariance(F);
            accum(_ekf.P, Q, EKF_N, EKF_N);
            cleanupCovariance();
        }

         void update_with_scalar(
                const float z,
                const float hx,
                const float h[EKF_N], 
                const float r)
        {
            float ph[EKF_N] = {};
            mulvec(_ekf.P, h, ph, EKF_N, EKF_N);
            const auto hphr = r + dot(h, ph); // HPH' + R

            float g[EKF_N] = {};
            for (uint8_t i=0; i<EKF_N; ++i) {
                g[i] = ph[i] / hphr;
            }

            // $\hat{x}_k = \hat{x_k} + G_k(z_k - h(\hat{x}_k))$
            for (uint8_t i=0; i<EKF_N; ++i) {
                _x[i] += g[i] * (z - hx);
            }

            float GH[EKF_N*EKF_N] = {};
            outer(g, h, GH); 

            for (int i=0; i<EKF_N; i++) { 
                GH[i*EKF_N+i] -= 1;
            }

            // $P_k = (I - G_k H_k) P_k$
            multiplyCovariance(GH);

            // Add the measurement variance 
            for (int i=0; i<EKF_N; i++) {
                for (int j=0; j<EKF_N; j++) {
                    _ekf.P[i*EKF_N+j] += j < i ? 0 : r * g[i] * g[j];
                }
            }

            cleanupCovariance();

            _isUpdated = true;
        }


        void finalize(
                const float newx[EKF_N], 
                const float A[EKF_N*EKF_N],
                const bool isErrorSufficient) 
        {
            if (_isUpdated) {

                for (uint8_t i=0; i<EKF_N; ++i) {
                    _x[i] = newx[i];
                }

                if (isErrorSufficient) {

                    multiplyCovariance(A);

                    cleanupCovariance();
                }

                _isUpdated = false;
            }
        }

        float * get(void) 
        { 
            static float x[EKF_N] = {};

            for (int i=0; i<EKF_N; ++i) {
                x[i] = _x[i];
            }

            return x; 
        }

    private:

        ekf_t _ekf;

        // State
        float _x[EKF_N];

        float _min_covariance;
        float _max_covariance;

        bool _isUpdated;

        void multiplyCovariance(const float a[EKF_N*EKF_N])
        {
            float at[EKF_N*EKF_N] = {};
            transpose(a, at, EKF_N, EKF_N);  
            float ap[EKF_N*EKF_N] = {};
            mulmat(a, _ekf.P,  ap, EKF_N, EKF_N, EKF_N);
            mulmat(ap, at, _ekf.P, EKF_N, EKF_N, EKF_N);
        }

        void cleanupCovariance(void)
        {
            if (_min_covariance < _max_covariance) {

                // Enforce symmetry of the covariance matrix, and ensure the
                // values stay bounded
                for (int i=0; i<EKF_N; i++) {

                    for (int j=i; j<EKF_N; j++) {

                        const auto pval = (_ekf.P[i*EKF_N+j] + _ekf.P[EKF_N*j+i]) / 2;

                        _ekf.P[i*EKF_N+j] = _ekf.P[j*EKF_N+i] = 
                            pval > _max_covariance ?  _max_covariance :
                            (i==j && pval < _min_covariance) ?  _min_covariance :
                            pval;
                    }
                }
            }
        } 

        static void outer(
                const float x[EKF_N],
                const float y[EKF_N],
                float a[EKF_N*EKF_N]) 
        {
            for (uint8_t i=0; i<EKF_N; i++) {
                for (uint8_t j=0; j<EKF_N; j++) {
                    a[i*EKF_N+j] = x[i] * y[j];
                }
            }
        }

        static float dot(const float x[EKF_N], const float y[EKF_N]) 
        {
            float d = 0;

            for (uint8_t k=0; k<EKF_N; k++) {
                d += x[k] * y[k];
            }

            return d;
        }
};
