# Temperature Logs
The logs are made during experiments on Nucleo L552ZE-Q (Cortex-M33 of STMicroelectronics) for ML-KEM and RSA for 7 zones that were measured

Note on log inspection: ML-KEM generates a log file roughly 5x larger than RSA over the same 8-minute window because it executes significantly more iterations.

## Log Files:
1. thermal_log_Substrate_below_kem.txt
2. thermal_log_Nucleo_usb_ldo_u6_rsa.txt
3. thermal_log_Nucleo_usb_ldo_u6_kem.txt
4. thermal_log_Nucleo_substrate_right_cpu_kem.txt
5. thermal_log_Nucleo_ldo_u5_substrate_left_rsa.txt
6. thermal_log_Nucleo_ldo__u5_substrate_left_kem.txt
7. thermal_log_Nucleo_cpu_substrate_left_idle.txt
8. thermal_log_nrf_cpu_substrate_left_idle.txt

The files 2-8 have temperature for 2 zones at once.
For e.g. "thermal_log_Nucleo_usb_ldo_u6_rsa" has temperature for micro "usb" and "ldo u6" for RSA algorithm.

Also, here already present experiments for nRF5340 SoC additionally to Nucleo L552ZE-Q but for now only in boards idle state when no crypto algorithm is executed.
