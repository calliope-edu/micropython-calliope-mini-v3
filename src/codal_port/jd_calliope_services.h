/*
 * Header for Jacdac Calliope mini V3 service initialisers.
 *
 * The MIT License (MIT)
 * Copyright (c) 2025 Calliope gGmbH
 */

#ifndef JD_CALLIOPE_SERVICES_H
#define JD_CALLIOPE_SERVICES_H

void calliope_temperature_init(void);
void calliope_accelerometer_init(void);
void calliope_button_init(int button_index);

#endif // JD_CALLIOPE_SERVICES_H
